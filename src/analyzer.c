#include "analyzer.h"
#include "builtin.h"
#include "scope.h"
#include "string_object.h"
#include "token.h"
#include "vector.h"
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool nodeUsesId(ASTNode* ast, StringObject* id);
static void analyzeNode(Analyzer* analyzer, ASTNode* ast);
static void validateNodeEffects(ASTNode* ast);
static void activateReferenceAccess(ASTNode* ast);
static void transferExclusiveReference(ASTNode* expression);

typedef struct OwnershipState
{
    ASTNode* symbol;
    ASTNode* referenceOrigin;
    size_t sharedAccessCount;
    bool exclusiveAccessActive;
    bool initialized;
    bool moved;
    bool referenceAccessActive;
} OwnershipState;

static void semanticError(char* message, Token token)
{
    fprintf(stderr, "Error: %s", message);
    printTokenValue(token);
    fprintf(stderr, " on line %d:%d\n", token.line, token.column);
    exit(1);
}

static void bindingAccessError(char* message, Token token, char* detail)
{
    fprintf(stderr, "Error: %s", message);
    printTokenValue(token);
    fprintf(stderr, "%s on line %d:%d\n", detail, token.line, token.column);
    exit(1);
}

static void semanticErrorAt(char* message, Token token)
{
    fprintf(stderr, "Error: %s", message);
    fprintf(stderr, " on line %d:%d\n", token.line, token.column);
    exit(1);
}

static void integerLiteralTooLargeForTypeError(Token token, TokenType type)
{
    fprintf(stderr, "Error: Integer literal is too large for type %s", getTokenTypeName(type));
    fprintf(stderr, " on line %d:%d\n", token.line, token.column);
    exit(1);
}

static void integerLiteralRequiresTypeError(Token token)
{
    fprintf(stderr, "Error: Integer literal requires an explicit type");
    fprintf(stderr, " on line %d:%d\n", token.line, token.column);
    exit(1);
}

static void negativeUnsignedIntegerError(Token token)
{
    fprintf(stderr, "Error: Negative integer literal cannot have an unsigned type");
    fprintf(stderr, " on line %d:%d\n", token.line, token.column);
    exit(1);
}

static void redefinitionError(char* kind, Token token)
{
    fprintf(stderr, "Error: %s ", kind);
    printTokenValue(token);
    fprintf(stderr, " is already defined on line %d:%d\n", token.line, token.column);
    exit(1);
}

static void returnOutsideFunctionError(Token token)
{
    fprintf(stderr, "Error: Return statement is only valid inside a function");
    fprintf(stderr, " on line %d:%d\n", token.line, token.column);
    exit(1);
}

static ASTNode* getIntegerLiteral(ASTNode* expression, bool* negative)
{
    *negative = false;

    if (expression->type == AST_INTEGER) {
        return expression;
    }

    if (expression->type != AST_PREFIX || expression->prefix.operator.type != TOKEN_MINUS
        || expression->prefix.expr->type != AST_INTEGER) {
        return NULL;
    }

    *negative = true;

    return expression->prefix.expr;
}

static bool integerLiteralFitsType(uint64_t value, bool negative, TokenType type)
{
    size_t bits = getIntegerTypeSize(type) * CHAR_BIT;

    if (!bits || (!isSignedIntegerTypeToken(type) && negative)) {
        return false;
    }

    if (!isSignedIntegerTypeToken(type)) {
        return bits == 64 || value <= (UINT64_C(1) << bits) - 1;
    }

    uint64_t limit = UINT64_C(1) << (bits - 1);

    return negative ? value <= limit : value < limit;
}

static bool applyIntegerLiteralType(ASTNode* expression, TokenType type)
{
    bool negative;
    ASTNode* literal = getIntegerLiteral(expression, &negative);

    if (!literal) {
        return false;
    }

    if (negative && isIntegerTypeToken(type) && !isSignedIntegerTypeToken(type)) {
        negativeUnsignedIntegerError(literal->integerLiteral.token);
    }

    if (!integerLiteralFitsType(literal->integerLiteral.value, negative, type)) {
        integerLiteralTooLargeForTypeError(literal->integerLiteral.token, type);
    }

    literal->integerLiteral.typeId = type;

    return true;
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

static size_t* getSharedAccessCount(ASTNode* symbol)
{
    if (isParameter(symbol)) {
        return &symbol->parameter.sharedAccessCount;
    }

    return &symbol->variableDefinition.sharedAccessCount;
}

static bool hasExclusiveAccess(ASTNode* symbol)
{
    if (isParameter(symbol)) {
        return symbol->parameter.exclusiveAccessActive;
    }

    return symbol->variableDefinition.exclusiveAccessActive;
}

static void setExclusiveAccess(ASTNode* symbol, bool active)
{
    if (isParameter(symbol)) {
        symbol->parameter.exclusiveAccessActive = active;

        return;
    }

    symbol->variableDefinition.exclusiveAccessActive = active;
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

static void setMovedState(ASTNode* symbol, bool moved)
{
    if (isParameter(symbol)) {
        symbol->parameter.moved = moved;

        return;
    }

    symbol->variableDefinition.moved = moved;
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

static bool conditionalBranchUsesId(ASTNode* branch, StringObject* id)
{
    if (!branch) {
        return false;
    }

    if (branch->type == AST_CONDITIONAL) {
        return nodeUsesId(branch, id);
    }

    return nodesUseId(&branch->compound.statements, id);
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
        case AST_CONDITIONAL:
            return nodeUsesId(ast->conditional.condition, id)
                || conditionalBranchUsesId(ast->conditional.thenBranch, id)
                || conditionalBranchUsesId(ast->conditional.elseBranch, id);
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

static void releaseReferenceAccess(ASTNode* reference)
{
    ASTNode* origin = reference->variableDefinition.referenceOrigin;

    if (reference->variableDefinition.referenceType == REFERENCE_SHARED) {
        (*getSharedAccessCount(origin))--;
    } else {
        setExclusiveAccess(origin, false);
    }

    reference->variableDefinition.referenceAccessActive = false;
}

static void releaseInactiveReferenceAccess(Vector* nodes, size_t next)
{
    for (size_t i = 0; i < next; i++) {
        ASTNode* ast = getVectorAt(nodes, i);

        if (!isVariableDefinition(ast) || !ast->variableDefinition.referenceAccessActive) {
            continue;
        }

        if (!nodesUseIdAfter(nodes, next, ast->variableDefinition.id)) {
            releaseReferenceAccess(ast);
        }
    }
}

static void analyzeNodes(Analyzer* analyzer, Vector* nodes, size_t start)
{
    size_t count = countVector(nodes);

    for (size_t i = start; i < count; i++) {
        analyzer->currentNodes = nodes;
        analyzer->nextNode = i + 1;
        analyzeNode(analyzer, getVectorAt(nodes, i));
        releaseInactiveReferenceAccess(nodes, i + 1);
    }
}

static void analyzeExpressionNodes(Analyzer* analyzer, Vector* nodes)
{
    analyzeNodes(analyzer, nodes, 0);
}

static TokenType analyzeConditionalBranch(Analyzer* analyzer, ASTNode* branch)
{
    Scope* previousScope = analyzer->currentScope;
    branch->compound.scope = createScope(previousScope);
    analyzer->currentScope = branch->compound.scope;
    analyzeExpressionNodes(analyzer, &branch->compound.statements);
    analyzer->currentScope = previousScope;

    size_t count = countVector(&branch->compound.statements);
    if (!count) {
        return TOKEN_VOID;
    }

    return getTypeId(getVectorAt(&branch->compound.statements, count - 1));
}

static TokenType analyzeElseBranch(Analyzer* analyzer, ASTNode* branch)
{
    if (!branch) {
        return TOKEN_VOID;
    }

    if (branch->type == AST_CONDITIONAL) {
        analyzeNode(analyzer, branch);

        return getTypeId(branch);
    }

    return analyzeConditionalBranch(analyzer, branch);
}

static void captureSymbolOwnership(Vector* states, ASTNode* symbol)
{
    if (!isVariableType(symbol)) {
        return;
    }

    OwnershipState* state = malloc(sizeof(OwnershipState));
    state->symbol = symbol;
    state->referenceOrigin = getReferenceOrigin(symbol);
    state->sharedAccessCount = *getSharedAccessCount(symbol);
    state->exclusiveAccessActive = hasExclusiveAccess(symbol);
    state->initialized = isInitialized(symbol);
    state->moved = isMoved(symbol);
    state->referenceAccessActive = isVariableDefinition(symbol) && symbol->variableDefinition.referenceAccessActive;

    pushVectorItem(states, state);
}

static void captureScopeOwnership(Vector* states, Scope* scope)
{
    for (size_t i = 0; i < scope->symbols.capacity; i++) {
        TableItem* item = scope->symbols.data[i];

        while (item) {
            captureSymbolOwnership(states, item->value);
            item = item->next;
        }
    }
}

static void captureOwnershipState(Vector* states, Scope* scope)
{
    initVector(states);

    while (scope) {
        captureScopeOwnership(states, scope);
        scope = scope->parent;
    }
}

static void copyOwnershipState(Vector* destination, Vector* source)
{
    initVector(destination);

    for (size_t i = 0; i < countVector(source); i++) {
        OwnershipState* sourceState = getVectorAt(source, i);
        OwnershipState* destinationState = malloc(sizeof(OwnershipState));

        *destinationState = *sourceState;
        pushVectorItem(destination, destinationState);
    }
}

static void applyOwnershipState(OwnershipState* state)
{
    ASTNode* symbol = state->symbol;

    *getSharedAccessCount(symbol) = state->sharedAccessCount;
    setExclusiveAccess(symbol, state->exclusiveAccessActive);
    setMovedState(symbol, state->moved);

    if (!isVariableDefinition(symbol)) {
        return;
    }

    symbol->variableDefinition.referenceOrigin = state->referenceOrigin;
    symbol->variableDefinition.initialized = state->initialized;
    symbol->variableDefinition.referenceAccessActive = state->referenceAccessActive;
}

static void applyOwnershipStates(Vector* states)
{
    for (size_t i = 0; i < countVector(states); i++) {
        applyOwnershipState(getVectorAt(states, i));
    }
}

static void updateOwnershipStates(Vector* states)
{
    for (size_t i = 0; i < countVector(states); i++) {
        OwnershipState* state = getVectorAt(states, i);
        ASTNode* symbol = state->symbol;

        state->referenceOrigin = getReferenceOrigin(symbol);
        state->sharedAccessCount = *getSharedAccessCount(symbol);
        state->exclusiveAccessActive = hasExclusiveAccess(symbol);
        state->initialized = isInitialized(symbol);
        state->moved = isMoved(symbol);
        state->referenceAccessActive = isVariableDefinition(symbol)
            && symbol->variableDefinition.referenceAccessActive;
    }
}

static void freeOwnershipStates(Vector* states)
{
    for (size_t i = 0; i < countVector(states); i++) {
        free(getVectorAt(states, i));
    }

    freeVector(states);
}

static bool branchUsesReference(ASTNode* branch, StringObject* id)
{
    if (!branch) {
        return false;
    }

    return conditionalBranchUsesId(branch, id);
}

static bool continuationUsesReference(Analyzer* analyzer, StringObject* id)
{
    if (!analyzer->currentNodes) {
        return false;
    }

    return nodesUseIdAfter(analyzer->currentNodes, analyzer->nextNode, id);
}

static void releaseReferencesDeadOnBranch(Analyzer* analyzer, Vector* states, ASTNode* branch)
{
    for (size_t i = 0; i < countVector(states); i++) {
        OwnershipState* state = getVectorAt(states, i);
        ASTNode* symbol = state->symbol;

        if (!isVariableDefinition(symbol) || !symbol->variableDefinition.referenceAccessActive) {
            continue;
        }

        StringObject* id = symbol->variableDefinition.id;
        if (branchUsesReference(branch, id) || continuationUsesReference(analyzer, id)) {
            continue;
        }

        releaseReferenceAccess(symbol);
    }
}

static void analyzeOwnershipBranch(
    Analyzer* analyzer,
    Vector* states,
    ASTNode* branch,
    TokenType* type)
{
    applyOwnershipStates(states);
    releaseReferencesDeadOnBranch(analyzer, states, branch);
    *type = analyzeElseBranch(analyzer, branch);
    updateOwnershipStates(states);
}

static ASTNode* mergedReferenceOrigin(OwnershipState* thenState, OwnershipState* elseState)
{
    if (thenState->referenceOrigin == elseState->referenceOrigin) {
        return thenState->referenceOrigin;
    }

    if (thenState->referenceAccessActive || elseState->referenceAccessActive) {
        semanticError(
            "Reference binding cannot have different origins across branches ",
            thenState->symbol->variableDefinition.token
        );
    }

    return NULL;
}

static size_t maxSize(size_t left, size_t right)
{
    if (left > right) {
        return left;
    }

    return right;
}

static void mergeOwnershipState(OwnershipState* thenState, OwnershipState* elseState)
{
    OwnershipState merged = *thenState;
    merged.referenceOrigin = mergedReferenceOrigin(thenState, elseState);
    merged.sharedAccessCount = maxSize(thenState->sharedAccessCount, elseState->sharedAccessCount);
    merged.exclusiveAccessActive = thenState->exclusiveAccessActive || elseState->exclusiveAccessActive;
    merged.initialized = thenState->initialized && elseState->initialized;
    merged.moved = thenState->moved || elseState->moved;
    merged.referenceAccessActive = thenState->referenceAccessActive || elseState->referenceAccessActive;

    applyOwnershipState(&merged);
}

static void mergeOwnershipStates(Vector* thenStates, Vector* elseStates)
{
    size_t count = countVector(thenStates);

    for (size_t i = 0; i < count; i++) {
        mergeOwnershipState(getVectorAt(thenStates, i), getVectorAt(elseStates, i));
    }
}

static void analyzeConditional(Analyzer* analyzer, ASTNode* ast)
{
    Vector* continuationNodes = analyzer->currentNodes;
    size_t continuationStart = analyzer->nextNode;

    analyzeNode(analyzer, ast->conditional.condition);

    if (getTypeId(ast->conditional.condition) != TOKEN_BOOL) {
        semanticErrorAt("If condition must have type bool", ast->conditional.token);
    }

    Vector thenStates;
    Vector elseStates;

    captureOwnershipState(&thenStates, analyzer->currentScope);
    copyOwnershipState(&elseStates, &thenStates);

    analyzer->currentNodes = continuationNodes;
    analyzer->nextNode = continuationStart;
    releaseReferencesDeadOnBranch(analyzer, &thenStates, ast->conditional.thenBranch);
    
    TokenType thenType = analyzeConditionalBranch(analyzer, ast->conditional.thenBranch);
    
    updateOwnershipStates(&thenStates);

    analyzer->currentNodes = continuationNodes;
    analyzer->nextNode = continuationStart;

    TokenType elseType;

    analyzeOwnershipBranch(analyzer, &elseStates, ast->conditional.elseBranch, &elseType);
    mergeOwnershipStates(&thenStates, &elseStates);
    freeOwnershipStates(&thenStates);
    freeOwnershipStates(&elseStates);

    if (!ast->conditional.expression) {
        ast->conditional.typeId = TOKEN_VOID;

        return;
    }

    if (!ast->conditional.elseBranch) {
        semanticError("If expression requires an else branch ", ast->conditional.token);
    }

    if (thenType == TOKEN_VOID || elseType == TOKEN_VOID) {
        semanticError("If expression branch must produce a value ", ast->conditional.token);
    }

    if (thenType != elseType) {
        semanticError("If expression branches have incompatible types ", ast->conditional.token);
    }

    ast->conditional.typeId = thenType;
}

static void analyzeBinary(Analyzer* analyzer, ASTNode* ast)
{
    analyzeNode(analyzer, ast->binary.leftExpr);
    analyzeNode(analyzer, ast->binary.rightExpr);

    TokenType leftType = getTypeId(ast->binary.leftExpr);
    TokenType rightType = getTypeId(ast->binary.rightExpr);

    if (isIntegerTypeToken(leftType)) {
        applyIntegerLiteralType(ast->binary.rightExpr, leftType);
        rightType = getTypeId(ast->binary.rightExpr);
    }

    if (isIntegerTypeToken(rightType)) {
        applyIntegerLiteralType(ast->binary.leftExpr, rightType);
        leftType = getTypeId(ast->binary.leftExpr);
    }

    if (leftType != rightType) {
        semanticError("Invalid operands to binary ", ast->binary.operator);
    }

    if (isLogicalToken(ast->binary.operator.type) && leftType != TOKEN_BOOL) {
        semanticError("Logical operators require bool operands ", ast->binary.operator);
    }

    if (isComparisonToken(ast->binary.operator.type) && !isIntegerTypeToken(leftType)) {
        semanticError("Comparison operators require integer operands ", ast->binary.operator);
    }

    if (ast->binary.operator.type == TOKEN_POWER && leftType != TOKEN_I32) {
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
        && hasExclusiveAccess(symbol)) {
        bindingAccessError("Cannot read binding ", ast->variable.token, " while exclusive access is active");
    }

    ast->variable.scope = analyzer->currentScope;
    ast->variable.symbol = symbol;
}

static ASTNode* getReferenceVariable(ASTNode* ast)
{
    if (!isVariable(ast->prefix.expr)) {
        semanticError("References require a binding for ", ast->prefix.operator);
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

static void validateReferenceAccess(ASTNode* ast, ASTNode* variable, ASTNode* symbol)
{
    ReferenceType requested = getReferenceType(ast);
    ReferenceType existing = getReferenceType(symbol);

    if (existing != REFERENCE_NONE) {
        semanticError("Cannot create a reference to another reference ", ast->prefix.operator);
    }

    if (requested == REFERENCE_SHARED && hasExclusiveAccess(symbol)) {
        bindingAccessError("Cannot share binding ", variable->variable.token, " while exclusive access is active");
    }

    if (requested == REFERENCE_EXCLUSIVE
        && (hasExclusiveAccess(symbol) || *getSharedAccessCount(symbol))) {
        bindingAccessError("Cannot take exclusive access to binding ", variable->variable.token, " while another access is active");
    }
}

static void analyzeReference(Analyzer* analyzer, ASTNode* ast)
{
    ASTNode* variable = getReferenceVariable(ast);
    ASTNode* symbol = findReferenceSymbol(analyzer, variable);
    validateReferenceAccess(ast, variable, symbol);

    variable->variable.scope = analyzer->currentScope;
    variable->variable.symbol = symbol;
}

static void analyzeParameter(Analyzer* analyzer, ASTNode* ast)
{
    if (getLocalSymbol(analyzer->currentScope, ast->parameter.id)) {
        redefinitionError("Parameter", ast->parameter.token);
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
        applyIntegerLiteralType(arg, builtin->params[i]);

        TokenType argumentType = getTypeId(arg);
        TokenType parameterType = builtin->params[i];

        if (argumentType != parameterType
            && !canImplicitlyWidenInteger(argumentType, parameterType)) {
            semanticError("Invalid arguments to function ", token);
        }
    }
}

static void convertBuiltinCall(ASTNode* ast, Builtin* builtin)
{
    Vector args = ast->functionCall.args;
    
    freeStringObject(ast->functionCall.id);

    ast->type = AST_BUILTIN_CALL;
    ast->builtinCall.builtin = builtin;
    ast->builtinCall.args = args;
}

static Builtin* resolveBuiltinCall(ASTNode* ast)
{
    size_t count = countVector(&ast->functionCall.args);
    if (count > BUILTIN_PARAMS_MAX) {
        return NULL;
    }

    TokenType argumentTypes[BUILTIN_PARAMS_MAX];

    for (size_t i = 0; i < count; i++) {
        ASTNode* argument = getVectorAt(&ast->functionCall.args, i);

        argumentTypes[i] = getTypeId(argument);
    }

    return resolveBuiltin(ast->functionCall.id->chars, argumentTypes, count);
}

static void analyzeBuiltinCall(ASTNode* ast)
{
    Builtin* builtin = resolveBuiltinCall(ast);
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
        applyIntegerLiteralType(arg, param->parameter.typeId);

        ReferenceType argumentType = getReferenceType(arg);
        ReferenceType parameterType = getReferenceType(param);
        bool valueParameter = parameterType == REFERENCE_NONE;
        bool implicitAccess = argumentType == REFERENCE_NONE;
        bool compatibleReference = valueParameter
            || implicitAccess
            || argumentType == parameterType
            || (argumentType == REFERENCE_EXCLUSIVE && parameterType == REFERENCE_SHARED);

        TokenType argumentValueType = getTypeId(arg);
        TokenType parameterValueType = param->parameter.typeId;
        bool compatibleValue = argumentValueType == parameterValueType
            || (argumentType == REFERENCE_NONE
                && parameterType == REFERENCE_NONE
                && canImplicitlyWidenInteger(argumentValueType, parameterValueType));

        if (!compatibleValue || !compatibleReference) {
            semanticError("Invalid arguments to function ", token);
        }
    }
}

static void validateEffectAccess(ASTNode* origin, ReferenceType type, Token token)
{
    if (!origin || !isTopLevelScope(getScope(origin))) {
        return;
    }

    if (type == REFERENCE_SHARED && hasExclusiveAccess(origin)) {
        bindingAccessError("Function requires shared access to binding ", token, " while exclusive access is active");
    }

    if (type == REFERENCE_EXCLUSIVE
        && (hasExclusiveAccess(origin) || *getSharedAccessCount(origin))) {
        bindingAccessError("Function requires exclusive access to binding ", token, " while another access is active");
    }
}

static bool referenceArgumentRequiresBinding(ReferenceType type)
{
    return type == REFERENCE_EXCLUSIVE;
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

        if (getReferenceType(arg) != REFERENCE_NONE) {
            continue;
        }

        ASTNode* origin = referenceOriginFromExpression(arg);

        if (!origin && isVariable(arg)) {
            origin = getReferenceOriginOrSelf(arg->variable.symbol);
        }

        if (!origin && referenceArgumentRequiresBinding(type)) {
            semanticError("Reference access requires a binding for ", token);
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

static void validateConditionalBranchEffects(ASTNode* branch)
{
    if (!branch) {
        return;
    }

    if (branch->type == AST_CONDITIONAL) {
        validateNodeEffects(branch);

        return;
    }

    validateEffectsInVector(&branch->compound.statements);
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
        case AST_CONDITIONAL:
            validateNodeEffects(ast->conditional.condition);
            validateConditionalBranchEffects(ast->conditional.thenBranch);
            validateConditionalBranchEffects(ast->conditional.elseBranch);
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

static ReferenceType getArgumentReferenceType(ASTNode* argument, ASTNode* parameter)
{
    ReferenceType type = getReferenceType(argument);

    if (type != REFERENCE_NONE) {
        return type;
    }

    return getReferenceType(parameter);
}

static ASTNode* getArgumentReferenceOrigin(ASTNode* argument)
{
    ASTNode* origin = referenceOriginFromExpression(argument);

    if (origin || !isVariable(argument)) {
        return origin;
    }

    return getReferenceOriginOrSelf(argument->variable.symbol);
}

static bool referenceAccessConflicts(ASTNode* leftOrigin, ReferenceType leftType,
    ASTNode* right, ASTNode* rightParameter)
{
    ASTNode* rightOrigin = getArgumentReferenceOrigin(right);
    ReferenceType rightType = getArgumentReferenceType(right, rightParameter);

    return leftOrigin == rightOrigin
        && (leftType == REFERENCE_EXCLUSIVE || rightType == REFERENCE_EXCLUSIVE);
}

static void validateArgumentAccess(Vector* arguments, Vector* parameters,
    size_t position, Token token)
{
    ASTNode* left = getVectorAt(arguments, position);
    ASTNode* leftParameter = getVectorAt(parameters, position);
    ASTNode* leftOrigin = getArgumentReferenceOrigin(left);
    ReferenceType leftType = getArgumentReferenceType(left, leftParameter);

    if (!leftOrigin || leftType == REFERENCE_NONE) {
        return;
    }

    size_t count = countVector(arguments);

    for (size_t i = position + 1; i < count; i++) {
        ASTNode* right = getVectorAt(arguments, i);
        ASTNode* rightParameter = getVectorAt(parameters, i);

        if (!referenceAccessConflicts(leftOrigin, leftType, right, rightParameter)) {
            continue;
        }

        semanticError("Conflicting reference access in call ", token);
    }
}

static void validateArgumentAccesses(ASTNode* caller, ASTNode* callee)
{
    Vector* arguments = &caller->functionCall.args;
    Vector* parameters = &callee->functionDefinition.params;
    size_t count = countVector(arguments);

    for (size_t i = 0; i < count; i++) {
        validateArgumentAccess(arguments, parameters, i, caller->functionCall.token);
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
    validateArgumentAccesses(ast, symbol);
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
    TokenType returnType = ast->functionDefinition.returnTypeId;

    bool valueReturn = ast->functionDefinition.returnReferenceType == REFERENCE_NONE;

    if (returnType != type
        && (!valueReturn || !canImplicitlyWidenInteger(type, returnType))) {
        semanticError("Invalid return type for function ", ast->functionDefinition.token);
    }
}

static void setParameterPositions(ASTNode* ast)
{
    size_t count = countVector(&ast->functionDefinition.params);
    size_t position = 0;

    for (size_t i = 0; i < count; i++) {
        ASTNode* param = getVectorAt(&ast->functionDefinition.params, i);
        param->parameter.position = position;
        position += getReferenceType(param) == REFERENCE_NONE
            ? getTypeSlotCount(getTypeId(param))
            : 1;
    }
}

static void applyFunctionReturnLiteralType(ASTNode* ast)
{
    if (!ast->functionDefinition.hasExplicitReturnType
        || !isIntegerTypeToken(ast->functionDefinition.returnTypeId)) {
        return;
    }

    Vector* statements = &ast->functionDefinition.body->compound.statements;
    size_t count = countVector(statements);

    for (size_t i = 0; i < count; i++) {
        ASTNode* statement = getVectorAt(statements, i);

        if (statement->type == AST_RETURN) {
            applyIntegerLiteralType(
                statement->returnStatement.expr, ast->functionDefinition.returnTypeId);
        }
    }
}

static void resolveFunctionReturnValueType(ASTNode* ast)
{
    applyFunctionReturnLiteralType(ast);
    TokenType type = getFunctionReturnValueType(
        &ast->functionDefinition.body->compound.statements);

    if (ast->functionDefinition.hasExplicitReturnType) {
        validateFunctionReturnValueType(ast, type);
        return;
    }

    ast->functionDefinition.returnTypeId = type;
}

static void analyzeFunction(Analyzer* analyzer, ASTNode* ast)
{
    if (getLocalSymbol(analyzer->currentScope, ast->functionDefinition.id)) {
        redefinitionError("Function", ast->functionDefinition.token);
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

    if (getReferenceType(symbol) == REFERENCE_NONE
        && (hasExclusiveAccess(symbol) || *getSharedAccessCount(symbol))) {
        bindingAccessError("Cannot mutate binding ", token, " while shared access is active");
    }

    if (getReferenceType(symbol) == REFERENCE_NONE && isVariableDefinition(symbol)
        && symbol->variableDefinition.fixed && isInitialized(symbol)) {
        semanticError("Cannot reassign fixed binding ", token);
    }
}

static void validateAssignmentType(ASTNode* ast, ASTNode* symbol)
{
    ASTNode* expression = ast->assignment.expr;
    TokenType expressionType = getTypeId(expression);
    TokenType bindingType = getTypeId(symbol);
    bool compatibleValue = bindingType == expressionType
        || (getReferenceType(symbol) == REFERENCE_NONE
            && getReferenceType(expression) == REFERENCE_NONE
            && canImplicitlyWidenInteger(expressionType, bindingType));

    if (!compatibleValue || getReferenceType(symbol) != getReferenceType(expression)) {
        semanticError("Assignment type does not match binding ", ast->assignment.token);
    }
}

static void updateReferenceBinding(ASTNode* ast, ASTNode* symbol, bool initializesBinding)
{
    if (getReferenceType(symbol) == REFERENCE_NONE) {
        return;
    }

    if (!initializesBinding && symbol->variableDefinition.referenceAccessActive) {
        releaseReferenceAccess(symbol);
    }

    symbol->variableDefinition.referenceOrigin = referenceOriginFromExpression(ast->assignment.expr);

    if (getReferenceType(symbol) == REFERENCE_EXCLUSIVE && isVariable(ast->assignment.expr)) {
        transferExclusiveReference(ast->assignment.expr);
    } else {
        activateReferenceAccess(symbol);
    }

    symbol->variableDefinition.referenceAccessActive = true;
    ast->assignment.initializesBinding = initializesBinding;
}

static void analyzeAssignment(Analyzer* analyzer, ASTNode* ast)
{
    ASTNode* symbol = findAssignmentSymbol(analyzer, ast);
    bool initializesBinding = !isInitialized(symbol);
    validateAssignmentTarget(analyzer, ast, symbol);

    if (isIntegerTypeToken(getTypeId(symbol))) {
        applyIntegerLiteralType(ast->assignment.expr, getTypeId(symbol));
    }

    analyzeNode(analyzer, ast->assignment.expr);
    validateAssignmentType(ast, symbol);

    if (ast->assignment.operator.type == TOKEN_POWER_EQUAL && getTypeId(symbol) != TOKEN_I32) {
        semanticError("Invalid operands to assignment ", ast->assignment.operator);
    }

    ast->assignment.scope = analyzer->currentScope;
    ast->assignment.symbol = symbol;

    updateReferenceBinding(ast, symbol, initializesBinding);
    initializeVariable(symbol);
}

static void transferExclusiveReference(ASTNode* expression)
{
    ASTNode* source = expression->variable.symbol;
    setMoved(source);

    if (!isVariableDefinition(source)) {
        return;
    }

    source->variableDefinition.referenceAccessActive = false;
}

static void activateReferenceAccess(ASTNode* ast)
{
    ASTNode* origin = ast->variableDefinition.referenceOrigin;

    if (ast->variableDefinition.referenceType == REFERENCE_SHARED) {
        (*getSharedAccessCount(origin))++;

        return;
    }

    setExclusiveAccess(origin, true);
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
    TokenType declaredType = ast->variableDefinition.typeId;
    ReferenceType declaredReferenceType = ast->variableDefinition.referenceType;

    if (isNone(expression)) {
        return;
    }

    if (declaredType != TOKEN_UNKNOWN && isIntegerTypeToken(declaredType)) {
        applyIntegerLiteralType(expression, declaredType);
    }

    analyzeNode(analyzer, expression);
    TokenType expressionType = getTypeId(expression);
    ReferenceType expressionReferenceType = getReferenceType(expression);

    if (declaredType == TOKEN_UNKNOWN) {
        if (expressionType == TOKEN_UNKNOWN) {
            integerLiteralRequiresTypeError(ast->variableDefinition.token);
        }

        ast->variableDefinition.typeId = expressionType;
        ast->variableDefinition.referenceType = expressionReferenceType;
    } else if ((declaredType != expressionType
        && (declaredReferenceType != REFERENCE_NONE
            || expressionReferenceType != REFERENCE_NONE
            || !canImplicitlyWidenInteger(expressionType, declaredType)))
        || declaredReferenceType != expressionReferenceType) {
        semanticError("Initializer type does not match variable ", ast->variableDefinition.token);
    }

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
    if (ast->variableDefinition.referenceType == REFERENCE_NONE
        || !ast->variableDefinition.initialized) {
        return;
    }

    ASTNode* expression = ast->variableDefinition.expr;
    bool transfersExclusive = ast->variableDefinition.referenceType == REFERENCE_EXCLUSIVE
        && isVariable(expression);

    if (transfersExclusive) {
        transferExclusiveReference(expression);
    } else {
        activateReferenceAccess(ast);
    }

    ast->variableDefinition.referenceAccessActive = true;
}

static void analyzeVariableDefinition(Analyzer* analyzer, ASTNode* ast)
{
    if (getLocalSymbol(analyzer->currentScope, ast->variableDefinition.id)) {
        redefinitionError("Variable", ast->variableDefinition.token);
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
        returnOutsideFunctionError(ast->returnStatement.token);
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

    if (ast->prefix.operator.type == TOKEN_NOT && getTypeId(ast->prefix.expr) != TOKEN_BOOL) {
        semanticError("Logical not requires a bool operand ", ast->prefix.operator);
    }
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
        case AST_CONDITIONAL:
            analyzeConditional(analyzer, ast); break;
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
    analyzer->currentNodes = NULL;
    analyzer->nextNode = 0;
}

bool analyze(Analyzer* analyzer, size_t start)
{
    analyzer->currentScope = analyzer->topLevel->compound.scope;
    analyzeNodes(analyzer, &analyzer->topLevel->compound.statements, start);

    return true;
}
