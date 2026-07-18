#include "analyzer.h"
#include "builtin.h"
#include "scope.h"
#include "string_object.h"
#include "token.h"
#include "vector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool nodeUsesId(ASTNode* ast, StringObject* id);
static void analyzeNode(Analyzer* analyzer, ASTNode* ast);
static void validateNodeEffects(ASTNode* ast);

static void semanticError(char* message, Token token)
{
    fprintf(stderr, "Error: %s", message);
    printTokenValue(token);
    fprintf(stderr, " on line %d:%d\n", token.line, token.column);
    exit(1);
}

static void symbolError(char* message, Token token)
{
    fprintf(stderr, "Error: ");
    printTokenValue(token);
    fprintf(stderr, " is %s on line %d:%d\n", message, token.line, token.column);
    exit(1);
}

static ASTNode* findSymbol(Analyzer* analyzer, StringObject* id)
{
    ASTNode* symbol = getLocalSymbol(analyzer->currentScope, id);
    if (symbol) {
        return symbol;
    }

    return getLocalSymbol(analyzer->topLevel->compound.scope, id);
}

static size_t* getSharedBorrowCount(ASTNode* symbol)
{
    if (isParameter(symbol)) {
        return &symbol->parameter.sharedBorrowCount;
    }

    return &symbol->variableDefinition.sharedBorrowCount;
}

static bool isExclusivelyBorrowed(ASTNode* symbol)
{
    if (isParameter(symbol)) {
        return symbol->parameter.exclusivelyBorrowed;
    }

    return symbol->variableDefinition.exclusivelyBorrowed;
}

static void setExclusivelyBorrowed(ASTNode* symbol, bool borrowed)
{
    if (isParameter(symbol)) {
        symbol->parameter.exclusivelyBorrowed = borrowed;

        return;
    }

    symbol->variableDefinition.exclusivelyBorrowed = borrowed;
}

static bool isMoved(ASTNode* symbol)
{
    if (isParameter(symbol)) {
        return symbol->parameter.moved;
    }

    return symbol->variableDefinition.moved;
}

static void setMoved(ASTNode* symbol)
{
    if (isParameter(symbol)) {
        symbol->parameter.moved = true;

        return;
    }

    symbol->variableDefinition.moved = true;
}

static ASTNode* getReferenceOriginOrSelf(ASTNode* symbol)
{
    ASTNode* origin = getReferenceOrigin(symbol);

    if (origin) {
        return origin;
    }

    return symbol;
}

static ASTNode* referenceOriginFromExpression(ASTNode* ast)
{
    if (ast->type == AST_PREFIX && isVariable(ast->prefix.expr)) {
        ASTNode* symbol = ast->prefix.expr->variable.symbol;

        return getReferenceOriginOrSelf(symbol);
    }

    return getReferenceOrigin(ast);
}

static bool nodesUseId(Vector* nodes, StringObject* id)
{
    size_t count = countVector(nodes);

    for (size_t i = 0; i < count; i++) {
        if (nodeUsesId(getVectorAt(nodes, i), id)) {
            return true;
        }
    }

    return false;
}

static bool nodeUsesId(ASTNode* ast, StringObject* id)
{
    if (!ast) {
        return false;
    }

    switch (ast->type) {
        case AST_ASSIGNMENT:
            return (ast->assignment.token.length == id->length
                && memcmp(ast->assignment.token.chars, id->chars, id->length) == 0)
                || nodeUsesId(ast->assignment.expr, id);
        case AST_BINARY:
            return nodeUsesId(ast->binary.leftExpr, id)
                || nodeUsesId(ast->binary.rightExpr, id);
        case AST_BUILTIN_CALL:
            return nodesUseId(&ast->builtinCall.args, id);
        case AST_FUNCTION_CALL:
            return nodesUseId(&ast->functionCall.args, id);
        case AST_FUNCTION_DEFINITION:
            return nodesUseId(&ast->functionDefinition.body->compound.statements, id);
        case AST_PREFIX:
            return nodeUsesId(ast->prefix.expr, id);
        case AST_RETURN:
            return nodeUsesId(ast->returnStatement.expr, id);
        case AST_VARIABLE:
            return compareStringObject(ast->variable.id, id);
        case AST_VARIABLE_DEFINITION:
            return nodeUsesId(ast->variableDefinition.expr, id);
        default:
            return false;
    }
}

static bool nodesUseIdAfter(Vector* nodes, size_t start, StringObject* id)
{
    size_t count = countVector(nodes);

    for (size_t i = start; i < count; i++) {
        if (nodeUsesId(getVectorAt(nodes, i), id)) {
            return true;
        }
    }

    return false;
}

static void releaseBorrow(ASTNode* reference)
{
    ASTNode* origin = reference->variableDefinition.referenceOrigin;

    if (reference->variableDefinition.referenceType == REFERENCE_SHARED) {
        (*getSharedBorrowCount(origin))--;
    } else {
        setExclusivelyBorrowed(origin, false);
    }

    reference->variableDefinition.borrowActive = false;
}

static void releaseDeadBorrows(Vector* nodes, size_t next)
{
    for (size_t i = 0; i < next; i++) {
        ASTNode* ast = getVectorAt(nodes, i);

        if (!isVariableDefinition(ast) || !ast->variableDefinition.borrowActive) {
            continue;
        }

        if (!nodesUseIdAfter(nodes, next, ast->variableDefinition.id)) {
            releaseBorrow(ast);
        }
    }
}

static void analyzeNodes(Analyzer* analyzer, Vector* nodes, size_t start)
{
    size_t count = countVector(nodes);

    for (size_t i = start; i < count; i++) {
        analyzeNode(analyzer, getVectorAt(nodes, i));
        releaseDeadBorrows(nodes, i + 1);
    }
}

static void analyzeExpressionNodes(Analyzer* analyzer, Vector* nodes)
{
    analyzeNodes(analyzer, nodes, 0);
}

static void analyzeBinary(Analyzer* analyzer, ASTNode* ast)
{
    analyzeNode(analyzer, ast->binary.leftExpr);
    analyzeNode(analyzer, ast->binary.rightExpr);

    TokenType leftType = getTypeId(ast->binary.leftExpr);
    TokenType rightType = getTypeId(ast->binary.rightExpr);

    if (leftType != rightType) {
        semanticError("Invalid operands to binary ", ast->binary.operator);
    }

    ast->binary.typeId = leftType;

    if (isBoolOperatorToken(ast->binary.operator.type)) {
        ast->binary.typeId = TOKEN_BOOL;
    }
}

static void analyzeVariable(Analyzer* analyzer, ASTNode* ast)
{
    ASTNode* symbol = findSymbol(analyzer, ast->variable.id);

    if (!symbol || !isVariableType(symbol)) {
        symbolError("undefined", ast->variable.token);
    }

    if (!isInitialized(symbol)) {
        symbolError("uninitialized", ast->variable.token);
    }

    if (isMoved(symbol)) {
        symbolError("moved", ast->variable.token);
    }

    if (getReferenceType(symbol) == REFERENCE_NONE
        && isExclusivelyBorrowed(symbol)) {
        semanticError("Cannot access exclusively borrowed binding ", ast->variable.token);
    }

    ast->variable.scope = analyzer->currentScope;
    ast->variable.symbol = symbol;
}

static ASTNode* getReferenceVariable(ASTNode* ast)
{
    if (!isVariable(ast->prefix.expr)) {
        semanticError("References require a binding near ", ast->prefix.operator);
    }

    return ast->prefix.expr;
}

static ASTNode* findReferenceSymbol(Analyzer* analyzer, ASTNode* variable)
{
    ASTNode* symbol = findSymbol(analyzer, variable->variable.id);

    if (!symbol || !isVariableType(symbol)) {
        symbolError("undefined", variable->variable.token);
    }

    if (!isInitialized(symbol)) {
        symbolError("uninitialized", variable->variable.token);
    }

    if (isMoved(symbol)) {
        symbolError("moved", variable->variable.token);
    }

    return symbol;
}

static void validateReferenceBorrow(ASTNode* ast, ASTNode* variable, ASTNode* symbol)
{
    ReferenceType requested = getReferenceType(ast);
    ReferenceType existing = getReferenceType(symbol);

    if (existing != REFERENCE_NONE) {
        semanticError("Cannot borrow a reference again ", ast->prefix.operator);
    }

    if (requested == REFERENCE_SHARED && isExclusivelyBorrowed(symbol)) {
        semanticError("Cannot share exclusively borrowed binding ", variable->variable.token);
    }

    if (requested == REFERENCE_EXCLUSIVE
        && (isExclusivelyBorrowed(symbol) || *getSharedBorrowCount(symbol))) {
        semanticError("Cannot exclusively borrow active binding ", variable->variable.token);
    }
}

static void analyzeReference(Analyzer* analyzer, ASTNode* ast)
{
    ASTNode* variable = getReferenceVariable(ast);
    ASTNode* symbol = findReferenceSymbol(analyzer, variable);
    validateReferenceBorrow(ast, variable, symbol);

    variable->variable.scope = analyzer->currentScope;
    variable->variable.symbol = symbol;
}

static void analyzeParameter(Analyzer* analyzer, ASTNode* ast)
{
    if (getLocalSymbol(analyzer->currentScope, ast->parameter.id)) {
        semanticError("Redefinition of ", ast->parameter.token);
    }

    ast->parameter.scope = analyzer->currentScope;
    ast->parameter.referenceOrigin = NULL;

    if (getReferenceType(ast) != REFERENCE_NONE) {
        ast->parameter.referenceOrigin = ast;
    }

    setLocalSymbol(analyzer->currentScope, ast->parameter.id, ast);
}

static void validateBuiltinCall(ASTNode* ast, Builtin* builtin, Token token)
{
    size_t count = countVector(&ast->functionCall.args);

    if (count != (size_t)builtin->paramCount) {
        semanticError("Invalid arguments to function ", token);
    }

    for (size_t i = 0; i < count; i++) {
        ASTNode* arg = getVectorAt(&ast->functionCall.args, i);

        if (getTypeId(arg) != builtin->params[i]) {
            semanticError("Invalid arguments to function ", token);
        }
    }
}

static void convertBuiltinCall(ASTNode* ast, Builtin* builtin)
{
    Vector args = ast->functionCall.args;
    
    freeStringObject(ast->functionCall.id);

    ast->type = AST_BUILTIN_CALL;
    ast->builtinCall.id = builtin->id;
    ast->builtinCall.builtin = builtin;
    ast->builtinCall.args = args;
}

static void analyzeBuiltinCall(ASTNode* ast)
{
    Builtin* builtin = getBuiltinByName(ast->functionCall.id->chars);
    if (!builtin) {
        symbolError("undefined", ast->functionCall.token);
    }

    validateBuiltinCall(ast, builtin, ast->functionCall.token);
    convertBuiltinCall(ast, builtin);
}

static void validateFunctionCall(ASTNode* caller, ASTNode* callee, Token token)
{
    size_t count = countVector(&caller->functionCall.args);
    size_t paramCount = countVector(&callee->functionDefinition.params);

    if (count != paramCount) {
        semanticError("Invalid arguments to function ", token);
    }

    for (size_t i = 0; i < count; i++) {
        ASTNode* arg = getVectorAt(&caller->functionCall.args, i);
        ASTNode* param = getVectorAt(&callee->functionDefinition.params, i);

        ReferenceType argumentType = getReferenceType(arg);
        ReferenceType parameterType = getReferenceType(param);
        bool valueParameter = parameterType == REFERENCE_NONE;
        bool implicitBorrow = argumentType == REFERENCE_NONE;
        bool compatibleReference = valueParameter
            || implicitBorrow
            || argumentType == parameterType
            || (argumentType == REFERENCE_EXCLUSIVE && parameterType == REFERENCE_SHARED);

        if (getTypeId(arg) != param->parameter.typeId || !compatibleReference) {
            semanticError("Invalid arguments to function ", token);
        }
    }
}

static void validateEffectAccess(ASTNode* origin, ReferenceType type, Token token)
{
    if (!origin || !isTopLevelScope(getScope(origin))) {
        return;
    }

    if (type == REFERENCE_SHARED && isExclusivelyBorrowed(origin)) {
        semanticError("Function requires shared access to borrowed binding ", token);
    }

    if (type == REFERENCE_EXCLUSIVE
        && (isExclusivelyBorrowed(origin) || *getSharedBorrowCount(origin))) {
        semanticError("Function requires exclusive access to borrowed binding ", token);
    }
}

static void validateCallArgumentAccess(ASTNode* caller, ASTNode* callee, Token token)
{
    size_t count = countVector(&caller->functionCall.args);

    for (size_t i = 0; i < count; i++) {
        ASTNode* param = getVectorAt(&callee->functionDefinition.params, i);
        ReferenceType type = getReferenceType(param);

        if (type == REFERENCE_NONE) {
            continue;
        }

        ASTNode* arg = getVectorAt(&caller->functionCall.args, i);
        ASTNode* origin = referenceOriginFromExpression(arg);

        if (!origin && isVariable(arg)) {
            origin = getReferenceOriginOrSelf(arg->variable.symbol);
        }

        validateEffectAccess(origin, type, token);
    }
}

static void validateEffectsInVector(Vector* nodes)
{
    size_t count = countVector(nodes);

    for (size_t i = 0; i < count; i++) {
        validateNodeEffects(getVectorAt(nodes, i));
    }
}

static void validateNodeEffects(ASTNode* ast)
{
    switch (ast->type) {
        case AST_ASSIGNMENT: {
            ASTNode* origin = getReferenceOriginOrSelf(ast->assignment.symbol);
            validateEffectAccess(origin, REFERENCE_EXCLUSIVE, ast->assignment.token);
            validateNodeEffects(ast->assignment.expr);
            break;
        }
        case AST_BINARY:
            validateNodeEffects(ast->binary.leftExpr);
            validateNodeEffects(ast->binary.rightExpr);
            break;
        case AST_BUILTIN_CALL:
            validateEffectsInVector(&ast->builtinCall.args);
            break;
        case AST_FUNCTION_CALL:
            validateEffectsInVector(&ast->functionCall.args);
            validateEffectsInVector(&ast->functionCall.symbol->functionDefinition.body->compound.statements);
            break;
        case AST_PREFIX: {
            ASTNode* origin = referenceOriginFromExpression(ast);
            ReferenceType type = getReferenceType(ast);
            validateEffectAccess(origin, type, ast->prefix.operator);
            break;
        }
        case AST_RETURN:
            validateNodeEffects(ast->returnStatement.expr);
            break;
        case AST_VARIABLE: {
            ASTNode* origin = getReferenceOriginOrSelf(ast->variable.symbol);
            validateEffectAccess(origin, REFERENCE_SHARED, ast->variable.token);
            break;
        }
        case AST_VARIABLE_DEFINITION:
            validateNodeEffects(ast->variableDefinition.expr);
            break;
        default:
            break;
    }
}

static bool borrowsConflict(ASTNode* leftOrigin, ReferenceType leftType, ASTNode* right)
{
    ASTNode* rightOrigin = referenceOriginFromExpression(right);
    ReferenceType rightType = getReferenceType(right);

    return leftOrigin == rightOrigin
        && (leftType == REFERENCE_EXCLUSIVE || rightType == REFERENCE_EXCLUSIVE);
}

static void validateArgumentBorrow(Vector* arguments, size_t position, Token token)
{
    ASTNode* left = getVectorAt(arguments, position);
    ASTNode* leftOrigin = referenceOriginFromExpression(left);
    ReferenceType leftType = getReferenceType(left);

    if (!leftOrigin || leftType == REFERENCE_NONE) {
        return;
    }

    size_t count = countVector(arguments);

    for (size_t i = position + 1; i < count; i++) {
        ASTNode* right = getVectorAt(arguments, i);

        if (!borrowsConflict(leftOrigin, leftType, right)) {
            continue;
        }

        semanticError("Conflicting borrows in call ", token);
    }
}

static void validateArgumentBorrows(ASTNode* ast)
{
    Vector* arguments = &ast->functionCall.args;
    size_t count = countVector(arguments);

    for (size_t i = 0; i < count; i++) {
        validateArgumentBorrow(arguments, i, ast->functionCall.token);
    }
}

static ASTNode* getCallReferenceOrigin(ASTNode* caller, ASTNode* callee)
{
    ASTNode* returned = callee->functionDefinition.returnReferenceOrigin;

    if (!returned || !isParameter(returned)) {
        return returned;
    }

    size_t count = countVector(&callee->functionDefinition.params);

    for (size_t i = 0; i < count; i++) {
        if (getVectorAt(&callee->functionDefinition.params, i) == returned) {
            ASTNode* argument = getVectorAt(&caller->functionCall.args, i);
            return referenceOriginFromExpression(argument);
        }
    }

    return NULL;
}

static void analyzeFunctionCall(Analyzer* analyzer, ASTNode* ast)
{
    analyzeExpressionNodes(analyzer, &ast->functionCall.args);

    ASTNode* symbol = findSymbol(analyzer, ast->functionCall.id);
    if (!symbol) {
        analyzeBuiltinCall(ast);
        return;
    }

    if (!isFunctionDefinition(symbol)) {
        symbolError("undefined", ast->functionCall.token);
    }

    ast->functionCall.scope = analyzer->currentScope;
    ast->functionCall.symbol = symbol;
    validateFunctionCall(ast, symbol, ast->functionCall.token);
    validateCallArgumentAccess(ast, symbol, ast->functionCall.token);
    validateArgumentBorrows(ast);
    validateEffectsInVector(&symbol->functionDefinition.body->compound.statements);
    ast->functionCall.referenceOrigin = getCallReferenceOrigin(ast, symbol);
}

static TokenType getFunctionReturnValueType(Vector* statements)
{
    size_t count = countVector(statements);

    for (size_t i = 0; i < count; i++) {
        ASTNode* statement = getVectorAt(statements, i);

        if (statement->type == AST_RETURN) {
            return getTypeId(statement->returnStatement.expr);
        }
    }

    return TOKEN_VOID;
}

static void validateFunctionReturnValueType(ASTNode* ast, TokenType type)
{
    if (ast->functionDefinition.typeId != type) {
        semanticError("Invalid return type for function ", ast->functionDefinition.token);
    }
}

static void setParameterPositions(ASTNode* ast)
{
    size_t count = countVector(&ast->functionDefinition.params);

    for (size_t i = 0; i < count; i++) {
        ASTNode* param = getVectorAt(&ast->functionDefinition.params, i);
        param->parameter.position = i;
    }
}

static void resolveFunctionReturnValueType(ASTNode* ast)
{
    TokenType type = getFunctionReturnValueType(
        &ast->functionDefinition.body->compound.statements);

    if (ast->functionDefinition.hasExplicitReturnType) {
        validateFunctionReturnValueType(ast, type);
        return;
    }

    ast->functionDefinition.typeId = type;
}

static void analyzeFunction(Analyzer* analyzer, ASTNode* ast)
{
    if (getLocalSymbol(analyzer->currentScope, ast->functionDefinition.id)) {
        semanticError("Redefinition of ", ast->functionDefinition.token);
    }

    Scope* parent = analyzer->currentScope;
    ASTNode* previousFunction = analyzer->function;
    Scope* scope = createScope(parent);
    ast->functionDefinition.scope = scope;
    ast->functionDefinition.body->compound.scope = scope;
    analyzer->currentScope = scope;
    analyzer->function = ast;
    analyzeExpressionNodes(analyzer, &ast->functionDefinition.params);
    setParameterPositions(ast);
    analyzeExpressionNodes(analyzer, &ast->functionDefinition.body->compound.statements);
    analyzer->currentScope = parent;
    analyzer->function = previousFunction;
    resolveFunctionReturnValueType(ast);
    setLocalSymbol(parent, ast->functionDefinition.id, ast);
}

static ASTNode* findAssignmentSymbol(Analyzer* analyzer, ASTNode* ast)
{
    StringObject* id = copyStringObject(ast->assignment.token.chars, ast->assignment.token.length);
    ASTNode* symbol = findSymbol(analyzer, id);

    freeStringObject(id);

    if (!symbol || !isVariableType(symbol)) {
        symbolError("undefined", ast->assignment.token);
    }

    return symbol;
}

static void validateAssignmentTarget(Analyzer* analyzer, ASTNode* ast, ASTNode* symbol)
{
    Token token = ast->assignment.token;

    if (analyzer->currentScope != getScope(symbol) && !isInitialized(symbol)) {
        symbolError("uninitialized", token);
    }

    if (isMoved(symbol)) {
        symbolError("moved", token);
    }

    ReferenceType referenceType = getReferenceType(symbol);

    if (referenceType == REFERENCE_SHARED) {
        semanticError("Cannot mutate through shared reference ", token);
    }

    if (referenceType == REFERENCE_NONE
        && (isExclusivelyBorrowed(symbol) || *getSharedBorrowCount(symbol))) {
        semanticError("Cannot mutate borrowed binding ", token);
    }

    if (referenceType == REFERENCE_NONE && isVariableDefinition(symbol)
        && symbol->variableDefinition.fixed && isInitialized(symbol)) {
        semanticError("Cannot reassign fixed binding ", token);
    }
}

static void analyzeAssignment(Analyzer* analyzer, ASTNode* ast)
{
    ASTNode* symbol = findAssignmentSymbol(analyzer, ast);
    validateAssignmentTarget(analyzer, ast, symbol);

    analyzeNode(analyzer, ast->assignment.expr);
    ast->assignment.scope = analyzer->currentScope;
    ast->assignment.symbol = symbol;
    initializeVariable(symbol);
}

static void transferExclusiveReference(ASTNode* expression)
{
    ASTNode* source = expression->variable.symbol;
    setMoved(source);

    if (!isVariableDefinition(source)) {
        return;
    }

    source->variableDefinition.borrowActive = false;
}

static void activateReferenceBorrow(ASTNode* ast)
{
    ASTNode* origin = ast->variableDefinition.referenceOrigin;

    if (ast->variableDefinition.referenceType == REFERENCE_SHARED) {
        (*getSharedBorrowCount(origin))++;

        return;
    }

    setExclusivelyBorrowed(origin, true);
}

static void initializeVariableDefinition(Analyzer* analyzer, ASTNode* ast)
{
    ast->variableDefinition.scope = analyzer->currentScope;
    ast->variableDefinition.position = getLocalCount(analyzer->currentScope);
    ast->variableDefinition.initialized = false;
}

static void analyzeVariableInitializer(Analyzer* analyzer, ASTNode* ast)
{
    ASTNode* expression = ast->variableDefinition.expr;

    if (isNone(expression)) {
        return;
    }

    analyzeNode(analyzer, expression);
    ast->variableDefinition.typeId = getTypeId(expression);
    ast->variableDefinition.referenceType = getReferenceType(expression);
    ast->variableDefinition.referenceOrigin = referenceOriginFromExpression(expression);
    initializeVariable(ast);
}

static void validateVariableType(ASTNode* ast)
{
    if (ast->variableDefinition.typeId == TOKEN_VOID) {
        semanticError("Invalid type for variable ", ast->variableDefinition.token);
    }
}

static void trackVariableReference(ASTNode* ast)
{
    if (ast->variableDefinition.referenceType == REFERENCE_NONE) {
        return;
    }

    ASTNode* expression = ast->variableDefinition.expr;
    bool transfersExclusive = ast->variableDefinition.referenceType == REFERENCE_EXCLUSIVE
        && isVariable(expression);

    if (transfersExclusive) {
        transferExclusiveReference(expression);
    } else {
        activateReferenceBorrow(ast);
    }

    ast->variableDefinition.borrowActive = true;
}

static void analyzeVariableDefinition(Analyzer* analyzer, ASTNode* ast)
{
    if (getLocalSymbol(analyzer->currentScope, ast->variableDefinition.id)) {
        semanticError("Redefinition of ", ast->variableDefinition.token);
    }

    initializeVariableDefinition(analyzer, ast);
    analyzeVariableInitializer(analyzer, ast);
    validateVariableType(ast);
    setLocalVariableSymbol(analyzer->currentScope, ast->variableDefinition.id, ast);
    trackVariableReference(ast);
}

static bool canReturnReferenceOrigin(ASTNode* origin)
{
    if (!origin) {
        return false;
    }

    if (isTopLevelScope(getScope(origin))) {
        return true;
    }

    return isParameter(origin) && getReferenceType(origin) != REFERENCE_NONE;
}

static ReferenceType getExpectedReturnReferenceType(Analyzer* analyzer)
{
    if (!analyzer->function) {
        return REFERENCE_NONE;
    }

    return analyzer->function->functionDefinition.returnReferenceType;
}

static void validateReturnScope(Analyzer* analyzer, ASTNode* ast)
{
    if (analyzer->currentScope->level < 2) {
        semanticError("Expected return inside a function but found ", ast->returnStatement.token);
    }
}

static ReferenceType validateReturnReferenceType(Analyzer* analyzer, ASTNode* ast)
{
    ReferenceType expected = getExpectedReturnReferenceType(analyzer);

    if (getReferenceType(ast->returnStatement.expr) != expected) {
        semanticError("Invalid reference return from function ", ast->returnStatement.token);
    }

    return expected;
}

static void trackReturnReferenceOrigin(Analyzer* analyzer, ASTNode* ast)
{
    ASTNode* origin = referenceOriginFromExpression(ast->returnStatement.expr);

    if (!canReturnReferenceOrigin(origin)) {
        semanticError("Reference to local binding cannot escape function ",
            ast->returnStatement.token);
    }

    ASTNode* function = analyzer->function;
    ASTNode* previous = function->functionDefinition.returnReferenceOrigin;

    if (previous && previous != origin) {
        semanticError("Reference returns must share one lifetime in function ",
            ast->returnStatement.token);
    }

    function->functionDefinition.returnReferenceOrigin = origin;
}

static void analyzeReturn(Analyzer* analyzer, ASTNode* ast)
{
    validateReturnScope(analyzer, ast);
    analyzeNode(analyzer, ast->returnStatement.expr);

    if (validateReturnReferenceType(analyzer, ast) == REFERENCE_NONE) {
        return;
    }

    trackReturnReferenceOrigin(analyzer, ast);
}

static void analyzePrefix(Analyzer* analyzer, ASTNode* ast)
{
    if (getReferenceType(ast) != REFERENCE_NONE) {
        analyzeReference(analyzer, ast);

        return;
    }

    analyzeNode(analyzer, ast->prefix.expr);
}

static void analyzeNode(Analyzer* analyzer, ASTNode* ast)
{
    switch (ast->type) {
        case AST_ASSIGNMENT:
            analyzeAssignment(analyzer, ast); break;
        case AST_BINARY:
            analyzeBinary(analyzer, ast); break;
        case AST_BUILTIN_CALL:
            analyzeExpressionNodes(analyzer, &ast->builtinCall.args); break;
        case AST_FUNCTION_CALL:
            analyzeFunctionCall(analyzer, ast); break;
        case AST_FUNCTION_DEFINITION:
            analyzeFunction(analyzer, ast); break;
        case AST_PARAMETER:
            analyzeParameter(analyzer, ast); break;
        case AST_PREFIX:
            analyzePrefix(analyzer, ast); break;
        case AST_RETURN:
            analyzeReturn(analyzer, ast); break;
        case AST_VARIABLE_DEFINITION:
            analyzeVariableDefinition(analyzer, ast); break;
        case AST_VARIABLE:
            analyzeVariable(analyzer, ast); break;
        default: break;
    }
}

void initAnalyzer(Analyzer* analyzer, ASTNode* ast)
{
    analyzer->topLevel = ast;
    analyzer->function = NULL;
    analyzer->currentScope = ast->compound.scope;
}

bool analyze(Analyzer* analyzer, size_t start)
{
    analyzer->currentScope = analyzer->topLevel->compound.scope;
    analyzeNodes(analyzer, &analyzer->topLevel->compound.statements, start);

    return true;
}
