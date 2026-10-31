#include "compiler/conversion.h"
#include "common/memory.h"
#include "common/unicode.h"
#include "compiler/diagnostics.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct NumericConstant
{
    uint64_t integer;
    double floating;
} NumericConstant;

static bool getNumericConstant(ASTNode* ast, NumericConstant* value);

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

static bool getNegatedNumericConstant(ASTNode* ast, NumericConstant* value)
{
    bool constant = getNumericConstant(ast->prefix.expr, value);

    if (!constant) {
        return false;
    }

    DataType type = getNodeDataType(ast);
    uint64_t negated = 0 - value->integer;

    value->integer = normalizeIntegerConstant(negated, type);
    value->floating = -value->floating;

    return true;
}

static bool getNumericConstant(ASTNode* ast, NumericConstant* value)
{
    DataType type = getNodeDataType(ast);

    if (ast->type == AST_CONVERSION) {
        return getNumericConstant(ast->conversion.expr, value);
    }

    if (ast->type == AST_PREFIX && ast->prefix.operator.type == TOKEN_MINUS) {
        return getNegatedNumericConstant(ast, value);
    }

    if (ast->type == AST_FLOAT) {
        value->floating = roundedFloatConstant(ast->floatLiteral.value, type);

        return true;
    }

    if (ast->type == AST_BOOLEAN) {
        value->integer = ast->booleanLiteral.value;

        return true;
    }

    if (ast->type == AST_CHARACTER) {
        value->integer = ast->characterLiteral.value;

        return true;
    }

    if (ast->type != AST_INTEGER) {
        return false;
    }

    uint64_t integer = ast->integerLiteral.value;

    value->integer = normalizeIntegerConstant(integer, type);
    value->floating = roundedFloatConstant((double)integer, type);

    return true;
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

static NumericConstant convertNumericConstant(NumericConstant value, DataType source,
    DataType destination, Token token)
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

static ASTNode* createConvertedConstant(NumericConstant value, DataType type, Token token)
{
    if (type == TYPE_CHAR) {
        return createCharacterNode(token, value.integer);
    }

    if (isFloatType(type)) {
        return createFloatNode(token, value.floating, type);
    }

    return createIntegerNode(token, value.integer, type);
}

void foldExplicitConversion(ASTNode* ast)
{
    ASTNode* expression = ast->conversion.expr;
    DataType source = getNodeDataType(expression);
    DataType destination = ast->conversion.typeId;
    Token token = ast->conversion.token;
    NumericConstant value = {0};
    bool constant = getNumericConstant(expression, &value);

    if (!constant) {
        return;
    }

    if (destination == TYPE_CHAR && (value.integer > UINT32_MAX || !isUnicodeScalar((uint32_t)value.integer))) {
        invalidConstantConversionError(token, destination);
    }

    if (destination != TYPE_CHAR) {
        value = convertNumericConstant(value, source, destination, token);
    }

    ASTNode* replacement = createConvertedConstant(value, destination, token);

    freeASTNode(expression);

    ast->conversion.expr = replacement;
}
