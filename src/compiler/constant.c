#include "compiler/constant.h"
#include "common/unicode.h"
#include "compiler/diagnostics.h"
#include "compiler/scope.h"
#include <limits.h>
#include <math.h>

static bool evaluateBranch(ASTNode* branch, ConstantValue* value);

static _Noreturn void invalidConstantConversionError(Token token, DataType destination)
{
    const char* name = getDataTypeName(destination);

    fatalSourceError(token.line, token.column, "Constant is outside the supported domain of conversion to %s", name);
}

static uint64_t normalizeIntegerConstant(uint64_t value, DataType type)
{
    size_t bits = getIntegerTypeSize(type) * CHAR_BIT;

    if (bits == 64 || !bits) {
        return value;
    }

    uint64_t mask = (UINT64_C(1) << bits) - 1;
    uint64_t result = value & mask;
    bool negative = isSignedIntegerType(type) && (result & (UINT64_C(1) << (bits - 1)));

    if (negative) {
        result |= ~mask;
    }

    return result;
}

static double roundedFloatConstant(double value, DataType type)
{
    if (type == TYPE_F32) {
        return (float)value;
    }

    return value;
}

static uint64_t floatConstantToInteger(double value, DataType destination, Token token)
{
    bool signedType = isSignedIntegerType(destination);
    size_t bits = getIntegerTypeSize(destination) * CHAR_BIT;
    double upper = ldexp(1.0, bits - signedType);
    double lower = 0.0;
    double truncated = trunc(value);

    if (signedType) {
        lower = -upper;
    }

    if (!isfinite(truncated) || truncated < lower || truncated >= upper) {
        invalidConstantConversionError(token, destination);
    }

    if (signedType) {
        return (uint64_t)(int64_t)truncated;
    }

    return (uint64_t)truncated;
}

static double integerConstantToFloat(uint64_t value, DataType source, DataType destination)
{
    if (isSignedIntegerType(source) && destination == TYPE_F32) {
        return (float)(int64_t)value;
    }

    if (isSignedIntegerType(source)) {
        return (double)(int64_t)value;
    }

    if (destination == TYPE_F32) {
        return (float)value;
    }

    return (double)value;
}

static ConstantValue convertConstantValue(ConstantValue value, DataType source, DataType destination, Token token)
{
    if (isFloatType(source) && isIntegerType(destination)) {
        value.integer = floatConstantToInteger(value.floating, destination, token);

        return value;
    }

    if (isIntegerType(destination)) {
        value.integer = normalizeIntegerConstant(value.integer, destination);

        return value;
    }

    if (isIntegerType(source)) {
        value.floating = integerConstantToFloat(value.integer, source, destination);

        return value;
    }

    value.floating = roundedFloatConstant(value.floating, destination);

    return value;
}

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

    int64_t signedLeft = signedI32Value(left);
    int64_t signedRight = signedI32Value(right);

    *value = (uint32_t)(signedLeft / signedRight);

    return true;
}

static bool foldRemainder(uint32_t left, uint32_t right, uint64_t* value)
{
    if (!right) {
        return false;
    }

    int64_t signedLeft = signedI32Value(left);
    int64_t signedRight = signedI32Value(right);

    *value = (uint32_t)(signedLeft % signedRight);

    return true;
}

static bool foldBinaryValue(TokenType operator, uint32_t left, uint32_t right, uint64_t* value)
{
    switch (operator) {
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
        default:
            return false;
    }
}

static bool getConstantComparisonOperand(DataType type, uint64_t integer, int64_t* value)
{
    if (type == TYPE_BOOL) {
        *value = integer != 0;

        return true;
    }

    switch (type) {
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
        default:
            return false;
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
        default:
            return false;
    }

    return true;
}

ASTNode* createConstantNode(ConstantValue value, DataType type, Token token)
{
    if (type == TYPE_BOOL) {
        return createBooleanNode(token, value.integer != 0);
    }

    if (type == TYPE_CHAR) {
        return createCharacterNode(token, value.integer);
    }

    if (isFloatType(type)) {
        return createFloatNode(token, value.floating, type);
    }

    return createIntegerNode(token, value.integer, type);
}

static bool evaluateConversion(ASTNode* ast, ConstantValue* value)
{
    ASTNode* expression = ast->conversion.expr;
    bool constant = evaluateConstant(expression, value);

    if (!constant) {
        return false;
    }

    DataType source = getNodeDataType(expression);
    DataType destination = ast->conversion.typeId;
    Token token = ast->conversion.token;
    bool scalar = value->integer <= UINT32_MAX && isUnicodeScalar((uint32_t)value->integer);

    if (destination == TYPE_CHAR && !scalar) {
        invalidConstantConversionError(token, destination);
    }

    if (destination != TYPE_CHAR) {
        *value = convertConstantValue(*value, source, destination, token);
    }

    return true;
}

static bool evaluatePrefix(ASTNode* ast, ConstantValue* value)
{
    bool constant = evaluateConstant(ast->prefix.expr, value);

    if (!constant) {
        return false;
    }

    if (ast->prefix.operator.type == TOKEN_NOT) {
        value->integer = !value->integer;

        return true;
    }

    if (ast->prefix.operator.type != TOKEN_MINUS) {
        return false;
    }

    DataType type = getNodeDataType(ast);
    uint64_t negated = 0 - value->integer;

    value->integer = normalizeIntegerConstant(negated, type);
    value->floating = -value->floating;

    return true;
}

static bool evaluateBooleanBinary(ASTNode* ast, ConstantValue leftValue, ConstantValue rightValue, ConstantValue* value)
{
    int64_t left;
    int64_t right;
    DataType leftType = getNodeDataType(ast->binary.leftExpr);
    DataType rightType = getNodeDataType(ast->binary.rightExpr);
    bool comparableLeft = getConstantComparisonOperand(leftType, leftValue.integer, &left);
    bool comparableRight = getConstantComparisonOperand(rightType, rightValue.integer, &right);
    bool result = false;

    if (!comparableLeft || !comparableRight) {
        return false;
    }

    bool folded = foldBooleanValue(ast->binary.operator.type, left, right, &result);

    value->integer = result;

    return folded;
}

static bool evaluateBinary(ASTNode* ast, ConstantValue* value)
{
    ConstantValue left = {0};
    ConstantValue right = {0};
    bool constantLeft = evaluateConstant(ast->binary.leftExpr, &left);
    bool constantRight = evaluateConstant(ast->binary.rightExpr, &right);

    if (!constantLeft || !constantRight) {
        return false;
    }

    DataType leftType = getNodeDataType(ast->binary.leftExpr);
    DataType rightType = getNodeDataType(ast->binary.rightExpr);

    if (ast->binary.typeId == TYPE_BOOL) {
        return evaluateBooleanBinary(ast, left, right, value);
    }

    if (leftType != TYPE_I32 || rightType != TYPE_I32) {
        return false;
    }

    bool folded = foldBinaryValue(ast->binary.operator.type, left.integer, right.integer, &value->integer);

    value->integer = normalizeIntegerConstant(value->integer, TYPE_I32);
    value->floating = integerConstantToFloat(value->integer, TYPE_I32, TYPE_F64);

    return folded;
}

static bool evaluateVariable(ASTNode* ast, ConstantValue* value)
{
    ASTNode* symbol = ast->variable.symbol;

    if (!symbol || symbol->type != AST_VARIABLE_DEFINITION) {
        return false;
    }

    if (!symbol->variableDefinition.fixed || symbol->variableDefinition.addressTaken
        || symbol->variableDefinition.referenceType != REFERENCE_NONE) {
        return false;
    }

    Scope* definitionScope = symbol->variableDefinition.scope;

    if (definitionScope->frameScope != ast->variable.scope->frameScope) {
        return false;
    }

    ASTNode* initializer = symbol->variableDefinition.expr;
    DataType type = symbol->variableDefinition.typeId;

    if (!initializer || type == TYPE_CHAR) {
        return false;
    }

    return evaluateConstant(initializer, value);
}

static bool evaluateSelectedBranch(ASTNode* ast, ConstantValue* value)
{
    ConstantValue condition = {0};
    bool constant = evaluateConstant(ast->conditional.condition, &condition);

    if (!constant) {
        return false;
    }

    ASTNode* branch = condition.integer ? ast->conditional.thenBranch : ast->conditional.elseBranch;

    return evaluateBranch(branch, value);
}

static bool evaluateBranch(ASTNode* branch, ConstantValue* value)
{
    if (!branch) {
        return false;
    }

    if (branch->type == AST_CONDITIONAL) {
        return evaluateSelectedBranch(branch, value);
    }

    if (branch->type != AST_COMPOUND || branch->compound.statements.count != 1) {
        return false;
    }

    ASTNode* result = branch->compound.statements.data[0];
    DataType type = getNodeDataType(result);

    if (type == TYPE_CHAR || type == TYPE_STRING) {
        return false;
    }

    return evaluateConstant(result, value);
}

static bool evaluateConditional(ASTNode* ast, ConstantValue* value)
{
    if (!ast->conditional.expression || ast->conditional.referenceType != REFERENCE_NONE) {
        return false;
    }

    return evaluateSelectedBranch(ast, value);
}

bool evaluateConstant(ASTNode* ast, ConstantValue* value)
{
    if (!ast) {
        return false;
    }

    DataType type = getNodeDataType(ast);

    switch (ast->type) {
        case AST_INTEGER:
            value->integer = normalizeIntegerConstant(ast->integerLiteral.value, type);
            value->floating = integerConstantToFloat(value->integer, type, TYPE_F64);
            return true;
        case AST_FLOAT:
            value->floating = roundedFloatConstant(ast->floatLiteral.value, type);
            return true;
        case AST_BOOLEAN:
            value->integer = ast->booleanLiteral.value;
            return true;
        case AST_CHARACTER:
            value->integer = ast->characterLiteral.value;
            return true;
        case AST_PREFIX:
            return evaluatePrefix(ast, value);
        case AST_BINARY:
            return evaluateBinary(ast, value);
        case AST_CONVERSION:
            return evaluateConversion(ast, value);
        case AST_VARIABLE:
            return evaluateVariable(ast, value);
        case AST_CONDITIONAL:
            return evaluateConditional(ast, value);
        default:
            return false;
    }
}
