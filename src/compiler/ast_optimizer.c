#include "compiler/ast_optimizer.h"
#include "builtin/builtin.h"
#include "builtin/string.h"
#include "common/errors.h"
#include "common/memory.h"
#include "common/vector.h"
#include "compiler/conversion.h"
#include "compiler/diagnostics.h"
#include "compiler/scope.h"
#include "compiler/token.h"
#include "runtime/concatenation.h"
#include "runtime/string_object.h"
#include <math.h>
#include <stdint.h>

static void optimizeNode(ASTNode* ast, bool checkZero);
static void optimizeMatchPattern(ASTNode* pattern, bool checkZero);
static void optimizeConditional(ASTNode* ast, bool fold, bool checkZero);

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
        case TYPE_I8:
            *value = (int8_t)integer;
            break;
        case TYPE_I16:
            *value = (int16_t)integer;
            break;
        case TYPE_I32:
            *value = signedI32Value(integer);
            break;
        case TYPE_U8:
            *value = (uint8_t)integer;
            break;
        case TYPE_U16:
            *value = (uint16_t)integer;
            break;
        case TYPE_U32:
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

    if (ast->binary.typeId != TYPE_BOOL
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

    if (foldBooleanBinary(ast)) {
        return;
    }

    if (ast->binary.leftExpr->type != AST_INTEGER) {
        return;
    }

    if (ast->binary.rightExpr->type != AST_INTEGER) {
        return;
    }

    if (getTypeId(ast->binary.leftExpr) != TYPE_I32
        || getTypeId(ast->binary.rightExpr) != TYPE_I32) {
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
    ast->integerLiteral.typeId = TYPE_I32;
}

static bool isLiteralOfType(ASTNode* ast, DataType type)
{
    if (ast->type != AST_INTEGER && ast->type != AST_BOOLEAN && ast->type != AST_FLOAT) {
        return false;
    }

    DataType literalType = getTypeId(ast);

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
        default:
            break;
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

static uint64_t negateIntegerValue(uint64_t value, DataType type)
{
    uint64_t result = UINT64_C(0) - value;

    switch (type) {
        case TYPE_I8:
            return (uint64_t)(int64_t)(int8_t)result;
        case TYPE_I16:
            return (uint64_t)(int64_t)(int16_t)result;
        case TYPE_I32:
            return (uint64_t)(int64_t)(int32_t)result;
        default:
            return result;
    }
}

static void foldIntegerNegation(ASTNode* ast, DataType type)
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

static void foldFloatNegation(ASTNode* ast, DataType type)
{
    ASTNode* operand = ast->prefix.expr;

    if (type != TYPE_F32 && type != TYPE_F64) {
        return;
    }

    if (operand->type != AST_FLOAT && operand->type != AST_INTEGER) {
        return;
    }

    double value = operand->type == AST_FLOAT ? operand->floatLiteral.value : operand->integerLiteral.value;
    ASTNode replacement = {.type = AST_FLOAT};

    if (type == TYPE_F32) {
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

    DataType type = getTypeId(ast->prefix.expr);
    bool integer = isSignedIntegerType(type);

    if (integer) {
        foldIntegerNegation(ast, type);

        return;
    }

    foldFloatNegation(ast, type);
}

static void optimizeNodes(Vector* nodes, bool checkZero)
{
    size_t count = countVector(nodes);

    for (size_t i = 0; i < count; i++) {
        ASTNode* node = getVectorAt(nodes, i);

        optimizeNode(node, checkZero);
    }
}

static void optimizeConditionalBranch(ASTNode* branch, bool checkZero)
{
    if (!branch) {
        return;
    }

    if (branch->type == AST_CONDITIONAL) {
        optimizeConditional(branch, false, checkZero);

        return;
    }

    optimizeNodes(&branch->compound.statements, checkZero);
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

    DataType type = getTypeId(result);

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

static void optimizeConditional(ASTNode* ast, bool fold, bool checkZero)
{
    optimizeNode(ast->conditional.condition, checkZero);
    optimizeConditionalBranch(ast->conditional.thenBranch, checkZero);
    optimizeConditionalBranch(ast->conditional.elseBranch, checkZero);

    if (!fold) {
        return;
    }

    foldConditional(ast);
}

static void optimizeMatchPattern(ASTNode* pattern, bool checkZero)
{
    if (pattern->type != AST_BINARY || pattern->binary.operator.type != TOKEN_PIPE) {
        optimizeNode(pattern, checkZero);

        return;
    }

    optimizeMatchPattern(pattern->binary.leftExpr, checkZero);
    optimizeMatchPattern(pattern->binary.rightExpr, checkZero);
}

static void optimizeMatchArm(MatchArm* arm, bool checkZero)
{
    if (arm->pattern) {
        optimizeMatchPattern(arm->pattern, checkZero);
    }

    if (arm->guard) {
        optimizeNode(arm->guard, checkZero);
    }

    optimizeConditionalBranch(arm->branch, checkZero);
}

static void optimizeMatch(ASTNode* ast, bool checkZero)
{
    if (ast->match.subject) {
        optimizeNode(ast->match.subject, checkZero);
    }

    size_t count = countVector(&ast->match.arms);

    for (size_t i = 0; i < count; i++) {
        MatchArm* arm = getVectorAt(&ast->match.arms, i);

        optimizeMatchArm(arm, checkZero);
    }

    optimizeConditionalBranch(ast->match.defaultBranch, checkZero);
}

static void replaceWithExpression(ASTNode* ast, ASTNode* expression)
{
    ASTNode replacement = *expression;

    freeMemory(expression);

    *ast = replacement;
}

static void optimizeBuiltinCall(ASTNode* ast, bool checkZero)
{
    optimizeNodes(&ast->builtinCall.args, checkZero);

    Builtin* builtin = ast->builtinCall.builtin;

    if (builtin->entry != builtinString || ast->builtinCall.args.count != 1) {
        return;
    }

    ASTNode* expression = ast->builtinCall.args.data[0];

    if (getTypeId(expression) != builtin->returnTypeId || getReferenceType(expression) != REFERENCE_NONE) {
        return;
    }

    freeVector(&ast->builtinCall.args);
    replaceWithExpression(ast, expression);
}

static void optimizeConversion(ASTNode* ast, bool checkZero)
{
    optimizeNode(ast->conversion.expr, checkZero);
    foldExplicitConversion(ast);

    ASTNode* expression = ast->conversion.expr;
    bool literal = expression->type == AST_INTEGER || expression->type == AST_FLOAT
        || expression->type == AST_CHARACTER;
    bool identity = getTypeId(expression) == ast->conversion.typeId;

    if (!literal && !identity) {
        return;
    }

    replaceWithExpression(ast, expression);
}

static void validateZeroDivisor(ASTNode* divisor, Token operator, DataType type, bool checkZero)
{
    if (!checkZero || divisor->type != AST_INTEGER || divisor->integerLiteral.value != 0) {
        return;
    }

    bool integer = isIntegerType(type);
    bool division = operator.type == TOKEN_SLASH || operator.type == TOKEN_SLASH_EQUAL;
    bool remainder = operator.type == TOKEN_PERCENT || operator.type == TOKEN_PERCENT_EQUAL;

    if (!integer || (!division && !remainder)) {
        return;
    }

    fatalSourceError(operator.line, operator.column, "Division by zero");
}

static bool shouldCheckRightDivisor(ASTNode* ast, bool checkZero)
{
    if (!checkZero) {
        return false;
    }

    bool logical = isLogicalToken(ast->binary.operator.type);

    if (!logical) {
        return true;
    }

    ASTNode* left = ast->binary.leftExpr;

    if (left->type != AST_BOOLEAN) {
        return false;
    }

    return left->booleanLiteral.value == (ast->binary.operator.type == TOKEN_AND);
}

static void optimizeBinary(ASTNode* ast, bool checkZero)
{
    optimizeNode(ast->binary.leftExpr, checkZero);

    bool checkRight = shouldCheckRightDivisor(ast, checkZero);

    optimizeNode(ast->binary.rightExpr, checkRight);
    validateZeroDivisor(ast->binary.rightExpr, ast->binary.operator, ast->binary.operandType, checkZero);
    foldBinary(ast);
}

static void optimizeAssignment(ASTNode* ast, bool checkZero)
{
    optimizeNode(ast->assignment.expr, checkZero);

    DataType type = getTypeId(ast->assignment.symbol);

    validateZeroDivisor(ast->assignment.expr, ast->assignment.operator, type, checkZero);
}

static void optimizeNode(ASTNode* ast, bool checkZero)
{
    switch (ast->type) {
        case AST_ASSIGNMENT:
            optimizeAssignment(ast, checkZero);
            break;
        case AST_BINARY:
            optimizeBinary(ast, checkZero);
            break;
        case AST_BUILTIN_CALL:
            optimizeBuiltinCall(ast, checkZero);
            break;
        case AST_WHILE:
            optimizeNode(ast->whileStatement.condition, checkZero);
            optimizeNodes(&ast->whileStatement.body->compound.statements, checkZero);
            break;
        case AST_CONDITIONAL:
            optimizeConditional(ast, true, checkZero);
            break;
        case AST_CONVERSION:
            optimizeConversion(ast, checkZero);
            break;
        case AST_FUNCTION_CALL:
            optimizeNodes(&ast->functionCall.args, checkZero);
            break;
        case AST_FUNCTION_DEFINITION:
            optimizeNodes(&ast->functionDefinition.body->compound.statements, checkZero);
            break;
        case AST_MATCH:
            optimizeMatch(ast, checkZero);
            break;
        case AST_PREFIX:
            optimizeNode(ast->prefix.expr, checkZero);
            foldPrefix(ast);
            break;
        case AST_RETURN:
            optimizeNode(ast->returnStatement.expr, checkZero);
            break;
        case AST_VARIABLE:
            propagateLiteralLet(ast);
            break;
        case AST_VARIABLE_DEFINITION:
            optimizeNode(ast->variableDefinition.expr, checkZero);
            break;
        default:
            break;
    }
}

void optimize(ASTNode* ast, size_t start)
{
    size_t count = countVector(&ast->compound.statements);

    for (size_t i = start; i < count; i++) {
        ASTNode* node = getVectorAt(&ast->compound.statements, i);

        optimizeNode(node, true);
    }
}
