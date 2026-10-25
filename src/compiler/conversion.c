#include "compiler/conversion.h"
#include "common/memory.h"
#include "common/unicode.h"
#include "common/util.h"
#include "compiler/diagnostics.h"
#include <errno.h>
#include <float.h>
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

static _Noreturn void floatOutsideSupportedRangeError(Token token)
{
    fatalSourceError(token.line, token.column, "Float is outside the supported range");
}

static _Noreturn void integerExceedsMaximumSizeError(Token token)
{
    fatalSourceError(token.line, token.column, "Integer exceeds the maximum supported size");
}

static _Noreturn void invalidConstantConversionError(Token token, TokenType destination)
{
    const char* name = getTokenTypeName(destination);

    fatalSourceError(token.line, token.column, "Constant is outside the supported domain of conversion to %s", name);
}

static uint64_t integerLiteralToValueInBase(Token token, size_t offset, int base)
{
    size_t length = token.length - offset;
    char* tmp = strndup(token.chars + offset, length);
    stripUnderscores(tmp, &length);
    errno = 0;
    uint64_t value = strtoull(tmp, NULL, base);

    if (errno == ERANGE) {
        integerExceedsMaximumSizeError(token);
    }

    freeMemory(tmp);

    return value;
}

uint64_t integerLiteralToValue(Token token)
{
    return integerLiteralToValueInBase(token, 0, 10);
}

uint64_t binaryLiteralToValue(Token token)
{
    return integerLiteralToValueInBase(token, 2, 2);
}

uint64_t hexadecimalLiteralToValue(Token token)
{
    return integerLiteralToValueInBase(token, 2, 16);
}

uint64_t octalLiteralToValue(Token token)
{
    return integerLiteralToValueInBase(token, 2, 8);
}

double floatLiteralToValue(Token token)
{
    size_t length = token.length;
    char* tmp = strndup(token.chars, length);
    stripUnderscores(tmp, &length);
    errno = 0;
    double value = strtod(tmp, NULL);

    if (errno == ERANGE) {
        floatOutsideSupportedRangeError(token);
    }

    freeMemory(tmp);

    return value;
}

bool canImplicitlyWidenInteger(TokenType source, TokenType destination)
{
    if (!isIntegerTypeToken(source) || !isIntegerTypeToken(destination)) {
        return false;
    }

    if (isSignedIntegerTypeToken(source) && !isSignedIntegerTypeToken(destination)) {
        return false;
    }

    return getIntegerTypeSize(source) < getIntegerTypeSize(destination);
}

bool canImplicitlyWidenFloat(TokenType source, TokenType destination)
{
    return source == TOKEN_F32 && destination == TOKEN_F64;
}

bool canImplicitlyConvertIntegerToFloat(TokenType source, TokenType destination)
{
    if (!isIntegerTypeToken(source)) {
        return false;
    }

    size_t bits = getIntegerTypeSize(source) * CHAR_BIT;
    bool signedType = isSignedIntegerTypeToken(source);
    size_t precision = 0;

    if (destination == TOKEN_F32) {
        precision = FLT_MANT_DIG;
    }

    if (destination == TOKEN_F64) {
        precision = DBL_MANT_DIG;
    }

    return bits - signedType <= precision;
}

bool canImplicitlyWidenType(TokenType source, TokenType destination)
{
    if (canImplicitlyWidenInteger(source, destination)) {
        return true;
    }

    if (canImplicitlyConvertIntegerToFloat(source, destination)) {
        return true;
    }

    return canImplicitlyWidenFloat(source, destination);
}

bool canExplicitlyConvertType(TokenType source, TokenType destination)
{
    bool sourceNumeric = isIntegerTypeToken(source) || isFloatTypeToken(source);
    bool destinationNumeric = isIntegerTypeToken(destination) || isFloatTypeToken(destination);

    if (source == TOKEN_BOOL && isIntegerTypeToken(destination)) {
        return true;
    }

    if (sourceNumeric && destinationNumeric) {
        return true;
    }

    if (source == TOKEN_CHAR) {
        return destination == TOKEN_CHAR || isIntegerTypeToken(destination);
    }

    return isIntegerTypeToken(source) && destination == TOKEN_CHAR;
}

static bool acceptsNumericType(TokenType source, TokenType destination)
{
    return source == destination || canImplicitlyWidenType(source, destination);
}

static bool acceptsBothNumericTypes(TokenType left, TokenType right, TokenType destination)
{
    bool acceptsLeft = acceptsNumericType(left, destination);
    bool acceptsRight = acceptsNumericType(right, destination);

    return acceptsLeft && acceptsRight;
}

TokenType getCommonNumericType(TokenType left, TokenType right)
{
    bool leftNumeric = isIntegerTypeToken(left) || isFloatTypeToken(left);
    bool rightNumeric = isIntegerTypeToken(right) || isFloatTypeToken(right);

    if (!leftNumeric || !rightNumeric) {
        return TOKEN_UNKNOWN;
    }

    if (acceptsNumericType(left, right)) {
        return right;
    }

    if (acceptsNumericType(right, left)) {
        return left;
    }

    TokenType candidate = TOKEN_I8;

    while (candidate <= TOKEN_F64 && !acceptsBothNumericTypes(left, right, candidate)) {
        candidate++;
    }

    if (candidate > TOKEN_F64) {
        return TOKEN_UNKNOWN;
    }

    return candidate;
}

static uint64_t normalizeIntegerConstant(uint64_t value, TokenType type)
{
    size_t bits = getIntegerTypeSize(type) * CHAR_BIT;

    if (bits == 64 || !bits) {
        return value;
    }

    uint64_t mask = (UINT64_C(1) << bits) - 1;
    uint64_t result = value & mask;
    bool negative = isSignedIntegerTypeToken(type) && (result & (UINT64_C(1) << (bits - 1)));

    if (negative) {
        result |= ~mask;
    }

    return result;
}

static double roundedFloatConstant(double value, TokenType type)
{
    if (type == TOKEN_F32) {
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

    TokenType type = getTypeId(ast);
    uint64_t negated = 0 - value->integer;

    value->integer = normalizeIntegerConstant(negated, type);
    value->floating = -value->floating;

    return true;
}

static bool getNumericConstant(ASTNode* ast, NumericConstant* value)
{
    TokenType type = getTypeId(ast);

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

static uint64_t floatConstantToInteger(double value, TokenType destination, Token token)
{
    bool signedType = isSignedIntegerTypeToken(destination);
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

static double integerConstantToFloat(uint64_t value, TokenType source, TokenType destination)
{
    if (isSignedIntegerTypeToken(source) && destination == TOKEN_F32) {
        return (float)(int64_t)value;
    }

    if (isSignedIntegerTypeToken(source)) {
        return (double)(int64_t)value;
    }

    if (destination == TOKEN_F32) {
        return (float)value;
    }

    return (double)value;
}

static NumericConstant convertNumericConstant(NumericConstant value, TokenType source,
    TokenType destination, Token token)
{
    if (isFloatTypeToken(source) && isIntegerTypeToken(destination)) {
        value.integer = floatConstantToInteger(value.floating, destination, token);

        return value;
    }

    if (isIntegerTypeToken(destination)) {
        value.integer = normalizeIntegerConstant(value.integer, destination);

        return value;
    }

    if (isIntegerTypeToken(source)) {
        value.floating = integerConstantToFloat(value.integer, source, destination);

        return value;
    }

    value.floating = roundedFloatConstant(value.floating, destination);

    return value;
}

static ASTNode* createConvertedConstant(NumericConstant value, TokenType type, Token token)
{
    if (type == TOKEN_CHAR) {
        ASTNode* result = createASTNode(AST_CHARACTER);

        result->characterLiteral.value = value.integer;
        result->characterLiteral.token = token;

        return result;
    }

    if (isFloatTypeToken(type)) {
        ASTNode* result = createASTNode(AST_FLOAT);

        result->floatLiteral.value = value.floating;
        result->floatLiteral.token = token;
        result->floatLiteral.typeId = type;

        return result;
    }

    ASTNode* result = createASTNode(AST_INTEGER);

    result->integerLiteral.value = value.integer;
    result->integerLiteral.token = token;
    result->integerLiteral.typeId = type;

    return result;
}

void foldExplicitConversion(ASTNode* ast)
{
    ASTNode* expression = ast->conversion.expr;
    TokenType source = getTypeId(expression);
    TokenType destination = ast->conversion.typeId;
    Token token = ast->conversion.token;
    NumericConstant value = {0};
    bool constant = getNumericConstant(expression, &value);

    if (!constant) {
        return;
    }

    if (destination == TOKEN_CHAR && (value.integer > UINT32_MAX || !isUnicodeScalar((uint32_t)value.integer))) {
        invalidConstantConversionError(token, destination);
    }

    if (destination != TOKEN_CHAR) {
        value = convertNumericConstant(value, source, destination, token);
    }

    ASTNode* replacement = createConvertedConstant(value, destination, token);

    freeASTNode(expression);

    ast->conversion.expr = replacement;
}
