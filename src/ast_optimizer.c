#include "ast_optimizer.h"
#include "scope.h"
#include "string_object.h"
#include "token.h"
#include "vector.h"
#include <math.h>
#include <stdint.h>

static void optimizeNode(ASTNode* ast);
static void optimizeMatchPattern(ASTNode* pattern);
static void optimizeConditional(ASTNode* ast, bool fold);

static int64_t signedI32Value(uint32_t value)
{
    if (value <= INT32_MAX) {
        return value;
    }

    return (int64_t)value - (UINT64_C(1) << 32);
}

static bool foldDivision(uint32_t left, uint32_t right, uint64_t* value)
{
    if (!right) {
        return false;
    }

    *value = (uint32_t)(signedI32Value(left) / signedI32Value(right));

    return true;
}

static bool foldRemainder(uint32_t left, uint32_t right, uint64_t* value)
{
    if (!right) {
        return false;
    }

    *value = (uint32_t)(signedI32Value(left) % signedI32Value(right));
    
    return true;
}

static bool foldBinaryValue(ASTNode* ast, uint64_t* value)
{
    uint32_t left = (uint32_t)ast->binary.leftExpr->integerLiteral.value;
    uint32_t right = (uint32_t)ast->binary.rightExpr->integerLiteral.value;

    switch (ast->binary.operator.type) {
        case TOKEN_PLUS:
            *value = left + right;
            return true;
        case TOKEN_MINUS:
            *value = left - right;
            return true;
        case TOKEN_STAR:
            *value = left * right;
            return true;
        case TOKEN_SLASH:
            return foldDivision(left, right, value);
        case TOKEN_PERCENT:
            return foldRemainder(left, right, value);
        case TOKEN_POWER:
            *value = (int)pow(left, right);
            return true;
        case TOKEN_AMPERSAND:
            *value = left & right;
            return true;
        case TOKEN_PIPE:
            *value = left | right;
            return true;
        case TOKEN_CIRCUMFLEX:
            *value = left ^ right;
            return true;
        default: return false;
    }
}

static bool getLiteralComparisonOperand(ASTNode* ast, int64_t* value)
{
    if (ast->type == AST_BOOLEAN) {
        *value = ast->booleanLiteral.value;

        return true;
    }

    if (ast->type != AST_INTEGER) {
        return false;
    }

    uint64_t integer = ast->integerLiteral.value;

    switch (ast->integerLiteral.typeId) {
        case TOKEN_I8:
            *value = (int8_t)integer;
            break;
        case TOKEN_I16:
            *value = (int16_t)integer;
            break;
        case TOKEN_I32:
            *value = signedI32Value(integer);
            break;
        case TOKEN_U8:
            *value = (uint8_t)integer;
            break;
        case TOKEN_U16:
            *value = (uint16_t)integer;
            break;
        case TOKEN_U32:
            *value = (uint32_t)integer;
            break;
        default: return false;
    }

    return true;
}

static bool foldBooleanValue(TokenType operator, int64_t left, int64_t right, bool* value)
{
    switch (operator) {
        case TOKEN_EQUAL_EQUAL:
            *value = left == right;
            break;
        case TOKEN_NOT_EQUAL:
            *value = left != right;
            break;
        case TOKEN_LESS:
            *value = left < right;
            break;
        case TOKEN_LESS_EQUAL:
            *value = left <= right;
            break;
        case TOKEN_GREATER:
            *value = left > right;
            break;
        case TOKEN_GREATER_EQUAL:
            *value = left >= right;
            break;
        case TOKEN_AND:
            *value = left && right;
            break;
        case TOKEN_OR:
            *value = left || right;
            break;
        default: return false;
    }

    return true;
}

static bool foldBooleanBinary(ASTNode* ast)
{
    int64_t left;
    int64_t right;
    bool value;

    if (ast->binary.typeId != TOKEN_BOOL
        || !getLiteralComparisonOperand(ast->binary.leftExpr, &left)
        || !getLiteralComparisonOperand(ast->binary.rightExpr, &right)
        || !foldBooleanValue(ast->binary.operator.type, left, right, &value)) {
        return false;
    }

    Token token = ast->binary.operator;

    freeASTNode(ast->binary.leftExpr);
    freeASTNode(ast->binary.rightExpr);

    ast->type = AST_BOOLEAN;
    ast->booleanLiteral.value = value;
    ast->booleanLiteral.token = token;

    return true;
}

static void foldBinary(ASTNode* ast)
{
    if (foldBooleanBinary(ast)) {
        return;
    }

    if (ast->binary.leftExpr->type != AST_INTEGER) {
        return;
    }

    if (ast->binary.rightExpr->type != AST_INTEGER) {
        return;
    }

    if (getTypeId(ast->binary.leftExpr) != TOKEN_I32
        || getTypeId(ast->binary.rightExpr) != TOKEN_I32) {
        return;
    }

    uint64_t value;
    Token token = ast->binary.operator;
    
    if (!foldBinaryValue(ast, &value)) {
        return;
    }

    freeASTNode(ast->binary.leftExpr);
    freeASTNode(ast->binary.rightExpr);

    ast->type = AST_INTEGER;
    ast->integerLiteral.value = value;
    ast->integerLiteral.token = token;
    ast->integerLiteral.typeId = TOKEN_I32;
}

static bool isLiteralOfType(ASTNode* ast, TokenType type)
{
    if (ast->type != AST_INTEGER && ast->type != AST_BOOLEAN && ast->type != AST_FLOAT) {
        return false;
    }

    TokenType literalType = getTypeId(ast);

    return literalType == type;
}

static void setLiteralToken(ASTNode* ast, Token token)
{
    switch (ast->type) {
        case AST_INTEGER: ast->integerLiteral.token = token; break;
        case AST_BOOLEAN: ast->booleanLiteral.token = token; break;
        case AST_FLOAT: ast->floatLiteral.token = token; break;
        default: break;
    }
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

    // Keep captured bindings dynamic across later interactive reference accesses
    Scope* definitionScope = symbol->variableDefinition.scope;
    if (definitionScope->frameScope != ast->variable.scope->frameScope) {
        return;
    }

    ASTNode* initializer = symbol->variableDefinition.expr;
    TokenType type = symbol->variableDefinition.typeId;

    if (!initializer) {
        return;
    }

    bool literal = isLiteralOfType(initializer, type);

    if (!literal) {
        return;
    }

    ASTNode replacement = *initializer;
    Token token = ast->variable.token;

    setLiteralToken(&replacement, token);
    freeStringObject(ast->variable.id);

    symbol->variableDefinition.readCount--;
    *ast = replacement;
}

static void replacePrefixWithLiteral(ASTNode* ast, ASTNode* literal)
{
    freeASTNode(ast->prefix.expr);
    freeVector(&ast->prefix.referenceOrigins);

    *ast = *literal;
}

static void foldLogicalNot(ASTNode* ast)
{
    ASTNode* operand = ast->prefix.expr;

    if (operand->type != AST_BOOLEAN) {
        return;
    }

    ASTNode replacement = *operand;

    replacement.booleanLiteral.value = !operand->booleanLiteral.value;
    replacement.booleanLiteral.token = ast->prefix.operator;

    replacePrefixWithLiteral(ast, &replacement);
}

static uint64_t negateIntegerValue(uint64_t value, TokenType type)
{
    uint64_t result = UINT64_C(0) - value;

    switch (type) {
        case TOKEN_I8:
            return (uint64_t)(int64_t)(int8_t)result;
        case TOKEN_I16:
            return (uint64_t)(int64_t)(int16_t)result;
        case TOKEN_I32:
            return (uint64_t)(int64_t)(int32_t)result;
        default:
            return result;
    }
}

static void foldIntegerNegation(ASTNode* ast, TokenType type)
{
    ASTNode* operand = ast->prefix.expr;

    if (operand->type != AST_INTEGER) {
        return;
    }

    ASTNode replacement = *operand;
    uint64_t value = negateIntegerValue(operand->integerLiteral.value, type);

    replacement.integerLiteral.value = value;
    replacement.integerLiteral.token = ast->prefix.operator;

    replacePrefixWithLiteral(ast, &replacement);
}

static void foldFloatNegation(ASTNode* ast, TokenType type)
{
    ASTNode* operand = ast->prefix.expr;

    if (type != TOKEN_F32 && type != TOKEN_F64) {
        return;
    }

    if (operand->type != AST_FLOAT && operand->type != AST_INTEGER) {
        return;
    }

    double value = operand->type == AST_FLOAT ? operand->floatLiteral.value : operand->integerLiteral.value;
    ASTNode replacement = {.type = AST_FLOAT};

    if (type == TOKEN_F32) {
        value = (float)value;
    }

    replacement.floatLiteral.value = -value;
    replacement.floatLiteral.typeId = type;
    replacement.floatLiteral.token = ast->prefix.operator;

    replacePrefixWithLiteral(ast, &replacement);
}

static void foldPrefix(ASTNode* ast)
{
    if (ast->prefix.operator.type == TOKEN_NOT) {
        foldLogicalNot(ast);

        return;
    }

    if (ast->prefix.operator.type != TOKEN_MINUS) {
        return;
    }

    TokenType type = getTypeId(ast->prefix.expr);
    bool integer = isSignedIntegerTypeToken(type);

    if (integer) {
        foldIntegerNegation(ast, type);

        return;
    }

    foldFloatNegation(ast, type);
}

static void optimizeNodes(Vector* nodes)
{
    size_t count = countVector(nodes);

    for (size_t i = 0; i < count; i++) {
        optimizeNode(getVectorAt(nodes, i));
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

    TokenType type = getTypeId(result);

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

static void optimizeMatch(ASTNode* ast)
{
    if (ast->match.subject) {
        optimizeNode(ast->match.subject);
    }

    size_t count = countVector(&ast->match.arms);

    for (size_t i = 0; i < count; i++) {
        MatchArm* arm = getVectorAt(&ast->match.arms, i);
        if (arm->pattern) {
            optimizeMatchPattern(arm->pattern);
        }

        optimizeConditionalBranch(arm->branch);
    }

    optimizeConditionalBranch(ast->match.defaultBranch);
}

static void optimizeNode(ASTNode* ast)
{
    switch (ast->type) {
        case AST_ASSIGNMENT:
            optimizeNode(ast->assignment.expr);
            break;
        case AST_BINARY:
            optimizeNode(ast->binary.leftExpr);
            optimizeNode(ast->binary.rightExpr);
            foldBinary(ast);
            break;
        case AST_BUILTIN_CALL:
            optimizeNodes(&ast->builtinCall.args);
            break;
        case AST_CONDITIONAL:
            optimizeConditional(ast, true);
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
            optimizeNode(ast->prefix.expr);
            foldPrefix(ast);
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

void optimize(ASTNode* ast, size_t start)
{
    size_t count = countVector(&ast->compound.statements);

    for (size_t i = start; i < count; i++) {
        optimizeNode(getVectorAt(&ast->compound.statements, i));
    }
}
