#include "compiler/ast_optimizer.h"
#include "builtin/builtin.h"
#include "builtin/string.h"
#include "common/errors.h"
#include "common/memory.h"
#include "common/vector.h"
#include "compiler/constant.h"
#include "compiler/scope.h"
#include "compiler/token.h"
#include "runtime/concatenation.h"
#include "runtime/string_object.h"
#include <stdint.h>

static void removeDiscardedExpressionReads(ASTNode* ast);
static void removeDiscardedStatementReads(ASTNode* ast);
static void markCalledFunctions(ASTNode* ast);
static void resetFunctionCallCounts(ASTNode* ast);
static void optimizeNode(ASTNode* ast);
static void optimizeMatchPattern(ASTNode* pattern);
static void optimizeConditional(ASTNode* ast, bool fold);
static void replaceWithExpression(ASTNode* ast, ASTNode* expression);

static void writeTextLiteral(Writer* writer, ASTNode* ast)
{
    if (ast->type == AST_STRING) {
        formatString(writer, ast->stringLiteral.value);

        return;
    }

    formatChar(writer, ast->characterLiteral.value);
}

static void writeConcatenationLiterals(Writer* writer, const void* context)
{
    const ASTNode* ast = context;

    writeTextLiteral(writer, ast->binary.leftExpr);
    writeTextLiteral(writer, ast->binary.rightExpr);
}

static size_t textLiteralCapacity(const ASTNode* ast)
{
    if (ast->type == AST_STRING) {
        return ast->stringLiteral.value->length;
    }

    return 4;
}

static size_t concatenationLiteralCapacity(const ASTNode* ast)
{
    size_t left = textLiteralCapacity(ast->binary.leftExpr);
    size_t right = textLiteralCapacity(ast->binary.rightExpr);

    if (right > SIZE_MAX - left) {
        outOfMemoryError();
    }

    return left + right;
}

static bool foldConcatenation(ASTNode* ast)
{
    if (ast->binary.typeId != TYPE_STRING || ast->binary.operator.type != TOKEN_PLUS) {
        return false;
    }

    if (!isTextLiteral(ast->binary.leftExpr) || !isTextLiteral(ast->binary.rightExpr)) {
        return false;
    }

    Token token = ast->binary.operator;
    size_t capacity = concatenationLiteralCapacity(ast);
    StringObject* string = concatenateStringParts(capacity, writeConcatenationLiterals, ast);

    freeASTNode(ast->binary.leftExpr);
    freeASTNode(ast->binary.rightExpr);

    ast->type = AST_STRING;
    ast->stringLiteral.value = string;
    ast->stringLiteral.token = token;

    return true;
}

static void foldBinary(ASTNode* ast)
{
    if (foldConcatenation(ast)) {
        return;
    }

    ConstantValue value = {0};
    bool constant = evaluateConstant(ast, &value);

    if (!constant) {
        return;
    }

    ASTNode* replacement = createConstantNode(value, ast->binary.typeId, ast->binary.operator);

    freeASTNode(ast->binary.leftExpr);
    freeASTNode(ast->binary.rightExpr);
    replaceWithExpression(ast, replacement);
}

static bool isLiteralOfType(ASTNode* ast, DataType type)
{
    if (!isLiteral(ast)) {
        return false;
    }

    DataType literalType = getNodeDataType(ast);

    return literalType == type;
}

static void setLiteralToken(ASTNode* ast, Token token)
{
    switch (ast->type) {
        case AST_INTEGER:
            ast->integerLiteral.token = token;
            break;
        case AST_BOOLEAN:
            ast->booleanLiteral.token = token;
            break;
        case AST_FLOAT:
            ast->floatLiteral.token = token;
            break;
        case AST_STRING:
            ast->stringLiteral.token = token;
            break;
        case AST_CHARACTER:
            ast->characterLiteral.token = token;
            break;
        default:
            break;
    }
}

static ASTNode copyLiteralInitializer(ASTNode* initializer)
{
    ASTNode replacement = *initializer;

    if (initializer->type == AST_STRING) {
        StringObject* string = initializer->stringLiteral.value;
        replacement.stringLiteral.value = copyStringObject(string->chars, string->length);
    }

    return replacement;
}

static void propagateLiteralLet(ASTNode* ast)
{
    ASTNode* symbol = ast->variable.symbol;

    if (!symbol || symbol->type != AST_VARIABLE_DEFINITION) {
        return;
    }

    if (!symbol->variableDefinition.fixed || symbol->variableDefinition.addressTaken
        || symbol->variableDefinition.referenceType != REFERENCE_NONE) {
        return;
    }
    
    Scope* definitionScope = symbol->variableDefinition.scope;
    if (definitionScope->frameScope != ast->variable.scope->frameScope) {
        return;
    }

    ASTNode* initializer = symbol->variableDefinition.expr;
    DataType type = symbol->variableDefinition.typeId;

    if (!initializer) {
        return;
    }

    bool literal = isLiteralOfType(initializer, type);

    if (!literal) {
        return;
    }

    ASTNode replacement = copyLiteralInitializer(initializer);
    Token token = ast->variable.token;

    setLiteralToken(&replacement, token);
    freeStringObject(ast->variable.id);

    symbol->variableDefinition.readCount--;
    *ast = replacement;
}

static void foldPrefix(ASTNode* ast)
{
    ConstantValue value = {0};
    bool constant = evaluateConstant(ast, &value);

    if (!constant) {
        return;
    }

    DataType type = getNodeDataType(ast);
    ASTNode* replacement = createConstantNode(value, type, ast->prefix.operator);

    freeASTNode(ast->prefix.expr);
    freeVector(&ast->prefix.referenceOrigins);
    replaceWithExpression(ast, replacement);
}

static void optimizeNodes(Vector* nodes)
{
    size_t count = countVector(nodes);

    for (size_t i = 0; i < count; i++) {
        ASTNode* node = getVectorAt(nodes, i);

        optimizeNode(node);
    }
}

static void optimizeConditionalBranch(ASTNode* branch)
{
    if (!branch) {
        return;
    }

    if (branch->type == AST_CONDITIONAL) {
        optimizeConditional(branch, false);

        return;
    }

    optimizeNodes(&branch->compound.statements);
}

static ASTNode* getLiteralBranchResult(ASTNode* branch)
{
    while (branch && branch->type == AST_CONDITIONAL) {
        ASTNode* condition = branch->conditional.condition;

        if (condition->type != AST_BOOLEAN) {
            return NULL;
        }

        branch = condition->booleanLiteral.value ? branch->conditional.thenBranch : branch->conditional.elseBranch;
    }

    if (!branch || branch->type != AST_COMPOUND) {
        return NULL;
    }

    size_t count = countVector(&branch->compound.statements);

    if (count != 1) {
        return NULL;
    }

    ASTNode* result = getVectorAt(&branch->compound.statements, 0);

    if (result->type != AST_INTEGER && result->type != AST_FLOAT && result->type != AST_BOOLEAN) {
        return NULL;
    }

    return result;
}

static void foldConditional(ASTNode* ast)
{
    ASTNode* condition = ast->conditional.condition;

    if (!ast->conditional.expression || ast->conditional.referenceType != REFERENCE_NONE
        || condition->type != AST_BOOLEAN) {
        return;
    }

    ASTNode* selected = condition->booleanLiteral.value ? ast->conditional.thenBranch : ast->conditional.elseBranch;
    ASTNode* result = getLiteralBranchResult(selected);

    if (!result) {
        return;
    }

    DataType type = getNodeDataType(result);

    if (type != ast->conditional.typeId) {
        return;
    }

    ASTNode replacement = *result;

    freeASTNode(ast->conditional.condition);
    freeASTNode(ast->conditional.thenBranch);
    freeASTNode(ast->conditional.elseBranch);
    freeVector(&ast->conditional.referenceOrigins);

    *ast = replacement;
}

static void optimizeConditional(ASTNode* ast, bool fold)
{
    optimizeNode(ast->conditional.condition);
    optimizeConditionalBranch(ast->conditional.thenBranch);
    optimizeConditionalBranch(ast->conditional.elseBranch);

    if (!fold) {
        return;
    }

    foldConditional(ast);
}

static void optimizeMatchPattern(ASTNode* pattern)
{
    if (pattern->type != AST_BINARY || pattern->binary.operator.type != TOKEN_PIPE) {
        optimizeNode(pattern);

        return;
    }

    optimizeMatchPattern(pattern->binary.leftExpr);
    optimizeMatchPattern(pattern->binary.rightExpr);
}

static void optimizeMatchArm(MatchArm* arm)
{
    if (arm->pattern) {
        optimizeMatchPattern(arm->pattern);
    }

    if (arm->guard) {
        optimizeNode(arm->guard);
    }

    optimizeConditionalBranch(arm->branch);
}

static void optimizeMatch(ASTNode* ast)
{
    if (ast->match.subject) {
        optimizeNode(ast->match.subject);
    }

    size_t count = countVector(&ast->match.arms);

    for (size_t i = 0; i < count; i++) {
        MatchArm* arm = getVectorAt(&ast->match.arms, i);

        optimizeMatchArm(arm);
    }

    optimizeConditionalBranch(ast->match.defaultBranch);
}

static void replaceWithExpression(ASTNode* ast, ASTNode* expression)
{
    ASTNode replacement = *expression;

    freeMemory(expression);

    *ast = replacement;
}

static void optimizeBuiltinCall(ASTNode* ast)
{
    optimizeNodes(&ast->builtinCall.args);

    const Builtin* builtin = ast->builtinCall.builtin;

    if (builtin->entry != builtinString || ast->builtinCall.args.count != 1) {
        return;
    }

    ASTNode* expression = ast->builtinCall.args.data[0];

    if (getNodeDataType(expression) != builtin->returnTypeId || getReferenceType(expression) != REFERENCE_NONE) {
        return;
    }

    freeVector(&ast->builtinCall.args);
    replaceWithExpression(ast, expression);
}

static void foldExplicitConversion(ASTNode* ast)
{
    ConstantValue value = {0};
    bool constant = evaluateConstant(ast, &value);

    if (!constant) {
        return;
    }

    ASTNode* replacement = createConstantNode(value, ast->conversion.typeId, ast->conversion.token);
    ASTNode* expression = ast->conversion.expr;

    freeASTNode(expression);

    ast->conversion.expr = replacement;
}

static void optimizeConversion(ASTNode* ast)
{
    optimizeNode(ast->conversion.expr);
    foldExplicitConversion(ast);

    ASTNode* expression = ast->conversion.expr;
    bool literal = expression->type == AST_INTEGER || expression->type == AST_FLOAT
        || expression->type == AST_CHARACTER;
    bool identity = getNodeDataType(expression) == ast->conversion.typeId;

    if (!literal && !identity) {
        return;
    }

    replaceWithExpression(ast, expression);
}

static void optimizeBinary(ASTNode* ast)
{
    optimizeNode(ast->binary.leftExpr);
    optimizeNode(ast->binary.rightExpr);
    foldBinary(ast);
}

static void optimizePrefix(ASTNode* ast)
{
    if (ast->prefix.operator.type == TOKEN_CONST) {
        return;
    }

    optimizeNode(ast->prefix.expr);
    foldPrefix(ast);
}

static void optimizeNode(ASTNode* ast)
{
    switch (ast->type) {
        case AST_ASSIGNMENT:
            optimizeNode(ast->assignment.expr);
            break;
        case AST_BINARY:
            optimizeBinary(ast);
            break;
        case AST_BUILTIN_CALL:
            optimizeBuiltinCall(ast);
            break;
        case AST_WHILE:
            optimizeNode(ast->whileStatement.condition);
            optimizeNodes(&ast->whileStatement.body->compound.statements);
            break;
        case AST_CONDITIONAL:
            optimizeConditional(ast, true);
            break;
        case AST_CONVERSION:
            optimizeConversion(ast);
            break;
        case AST_FUNCTION_CALL:
            optimizeNodes(&ast->functionCall.args);
            break;
        case AST_FUNCTION_DEFINITION:
            optimizeNodes(&ast->functionDefinition.body->compound.statements);
            break;
        case AST_MATCH:
            optimizeMatch(ast);
            break;
        case AST_PREFIX:
            optimizePrefix(ast);
            break;
        case AST_RETURN:
            optimizeNode(ast->returnStatement.expr);
            break;
        case AST_VARIABLE:
            propagateLiteralLet(ast);
            break;
        case AST_VARIABLE_DEFINITION:
            optimizeNode(ast->variableDefinition.expr);
            break;
        default:
            break;
    }
}

void optimizeAST(ASTNode* ast, size_t start)
{
    size_t count = countVector(&ast->compound.statements);

    for (size_t i = start; i < count; i++) {
        ASTNode* node = getVectorAt(&ast->compound.statements, i);

        optimizeNode(node);
    }
}

static bool hasNoValueReads(ASTNode* ast)
{
    return ast->variableDefinition.readCount == ast->variableDefinition.discardedReadCount;
}

static void removeUnreadInitializerReads(ASTNode* ast)
{
    if (!hasNoValueReads(ast) || ast->variableDefinition.discardedReadsAnalyzed) {
        return;
    }

    ast->variableDefinition.discardedReadsAnalyzed = true;

    removeDiscardedExpressionReads(ast->variableDefinition.expr);
}

static void removeDiscardedVariableRead(ASTNode* ast)
{
    ASTNode* symbol = ast->variable.symbol;

    if (!isVariableDefinition(symbol)) {
        return;
    }

    symbol->variableDefinition.discardedReadCount++;

    removeUnreadInitializerReads(symbol);
}

static void removeDiscardedExpressionReads(ASTNode* ast)
{
    switch (ast->type) {
        case AST_VARIABLE:
            removeDiscardedVariableRead(ast);
            break;
        case AST_PREFIX:
            removeDiscardedExpressionReads(ast->prefix.expr);
            break;
        default:
            break;
    }
}

static void removeDiscardedStatementVectorReads(Vector* nodes)
{
    size_t count = countVector(nodes);

    for (size_t i = 0; i < count; i++) {
        ASTNode* node = getVectorAt(nodes, i);

        removeDiscardedStatementReads(node);
    }
}

static void removeDiscardedStatementReads(ASTNode* ast)
{
    if (ast->type == AST_FUNCTION_DEFINITION) {
        removeDiscardedStatementVectorReads(&ast->functionDefinition.body->compound.statements);

        return;
    }

    if (ast->type == AST_VARIABLE_DEFINITION) {
        removeUnreadInitializerReads(ast);

        return;
    }

    removeDiscardedExpressionReads(ast);
}

void removeDiscardedReads(Vector* nodes, size_t start)
{
    size_t count = countVector(nodes);

    for (size_t i = start; i < count; i++) {
        ASTNode* node = getVectorAt(nodes, i);

        removeDiscardedStatementReads(node);
    }
}

static void markCalledFunctionsInVector(Vector* nodes)
{
    size_t count = countVector(nodes);

    for (size_t i = 0; i < count; i++) {
        ASTNode* node = getVectorAt(nodes, i);

        markCalledFunctions(node);
    }
}

static void markCalledFunction(ASTNode* ast)
{
    ASTNode* function = ast->functionCall.symbol;

    markCalledFunctionsInVector(&ast->functionCall.args);

    if (!function) {
        return;
    }

    function->functionDefinition.callCount++;

    if (function->functionDefinition.callCount > 1) {
        return;
    }

    markCalledFunctions(function->functionDefinition.body);
}

static void markMatchCalledFunctions(ASTNode* ast)
{
    markCalledFunctions(ast->match.subject);

    size_t count = countVector(&ast->match.arms);

    for (size_t i = 0; i < count; i++) {
        MatchArm* arm = getVectorAt(&ast->match.arms, i);

        markCalledFunctions(arm->pattern);
        markCalledFunctions(arm->guard);
        markCalledFunctions(arm->branch);
    }

    markCalledFunctions(ast->match.defaultBranch);
}

static void markCalledFunctions(ASTNode* ast)
{
    if (!ast) {
        return;
    }

    switch (ast->type) {
        case AST_ASSIGNMENT:
            markCalledFunctions(ast->assignment.expr);
            break;
        case AST_BINARY:
            markCalledFunctions(ast->binary.leftExpr);
            markCalledFunctions(ast->binary.rightExpr);
            break;
        case AST_BUILTIN_CALL:
            markCalledFunctionsInVector(&ast->builtinCall.args);
            break;
        case AST_WHILE:
            markCalledFunctions(ast->whileStatement.condition);
            markCalledFunctions(ast->whileStatement.body);
            break;
        case AST_COMPOUND:
            markCalledFunctionsInVector(&ast->compound.statements);
            break;
        case AST_CONDITIONAL:
            markCalledFunctions(ast->conditional.condition);
            markCalledFunctions(ast->conditional.thenBranch);
            markCalledFunctions(ast->conditional.elseBranch);
            break;
        case AST_CONVERSION:
            markCalledFunctions(ast->conversion.expr);
            break;
        case AST_FUNCTION_CALL:
            markCalledFunction(ast);
            break;
        case AST_MATCH:
            markMatchCalledFunctions(ast);
            break;
        case AST_PREFIX:
            markCalledFunctions(ast->prefix.expr);
            break;
        case AST_RETURN:
            markCalledFunctions(ast->returnStatement.expr);
            break;
        case AST_VARIABLE_DEFINITION:
            markCalledFunctions(ast->variableDefinition.expr);
            break;
        default:
            break;
    }
}

static void resetFunctionCallCountsInVector(Vector* nodes)
{
    size_t count = countVector(nodes);

    for (size_t i = 0; i < count; i++) {
        ASTNode* node = getVectorAt(nodes, i);

        resetFunctionCallCounts(node);
    }
}

static void resetMatchFunctionCallCounts(ASTNode* ast)
{
    size_t count = countVector(&ast->match.arms);

    for (size_t i = 0; i < count; i++) {
        MatchArm* arm = getVectorAt(&ast->match.arms, i);

        resetFunctionCallCounts(arm->pattern);
        resetFunctionCallCounts(arm->guard);
        resetFunctionCallCounts(arm->branch);
    }

    resetFunctionCallCounts(ast->match.defaultBranch);
}

static void resetFunctionCallCounts(ASTNode* ast)
{
    if (!ast) {
        return;
    }

    if (ast->type == AST_FUNCTION_DEFINITION) {
        ast->functionDefinition.callCount = 0;

        resetFunctionCallCounts(ast->functionDefinition.body);

        return;
    }

    if (ast->type == AST_COMPOUND) {
        resetFunctionCallCountsInVector(&ast->compound.statements);

        return;
    }

    if (ast->type == AST_CONDITIONAL) {
        resetFunctionCallCounts(ast->conditional.thenBranch);
        resetFunctionCallCounts(ast->conditional.elseBranch);

        return;
    }

    if (ast->type == AST_WHILE) {
        resetFunctionCallCounts(ast->whileStatement.condition);
        resetFunctionCallCounts(ast->whileStatement.body);

        return;
    }

    if (ast->type == AST_MATCH) {
        resetMatchFunctionCallCounts(ast);
    }
}

static void markRootCalledFunctions(ASTNode* ast)
{
    if (ast->type == AST_FUNCTION_DEFINITION) {
        return;
    }

    markCalledFunctions(ast);
}

void markReachableFunctions(Vector* nodes)
{
    size_t count = countVector(nodes);

    resetFunctionCallCountsInVector(nodes);

    for (size_t i = 0; i < count; i++) {
        ASTNode* ast = getVectorAt(nodes, i);

        markRootCalledFunctions(ast);
    }
}
