#include "compiler/constant.h"
#include "common/unicode.h"
#include "compiler/diagnostics.h"
#include "compiler/scope.h"
#include <limits.h>
#include <math.h>

static bool evaluate_branch(const ASTNode* branch, ConstantValue* value);

static _Noreturn void invalid_constant_conversion_error(Token token, DataType destination)
{
    const char* name = get_data_type_name(destination);

    fatal_source_error(token.line, token.column, "Constant is outside the supported domain of conversion to %s", name);
}

static uint64_t normalize_integer_constant(uint64_t value, DataType type)
{
    size_t bits = get_integer_type_size(type) * CHAR_BIT;

    if (bits == 64 || !bits) {
        return value;
    }

    uint64_t mask = (UINT64_C(1) << bits) - 1;
    uint64_t result = value & mask;
    bool negative = is_signed_integer_type(type) && (result & (UINT64_C(1) << (bits - 1)));

    if (negative) {
        result |= ~mask;
    }

    return result;
}

static double rounded_float_constant(double value, DataType type)
{
    if (type == TypeF32) {
        return (float)value;
    }

    return value;
}

static uint64_t float_constant_to_integer(double value, DataType destination, Token token)
{
    bool signed_type = is_signed_integer_type(destination);
    size_t bits = get_integer_type_size(destination) * CHAR_BIT;
    double upper = ldexp(1.0, bits - signed_type);
    double lower = 0.0;
    double truncated = trunc(value);

    if (signed_type) {
        lower = -upper;
    }

    if (!isfinite(truncated) || truncated < lower || truncated >= upper) {
        invalid_constant_conversion_error(token, destination);
    }

    if (signed_type) {
        return (uint64_t)(int64_t)truncated;
    }

    return (uint64_t)truncated;
}

static double integer_constant_to_float(uint64_t value, DataType source, DataType destination)
{
    if (is_signed_integer_type(source) && destination == TypeF32) {
        return (float)(int64_t)value;
    }

    if (is_signed_integer_type(source)) {
        return (double)(int64_t)value;
    }

    if (destination == TypeF32) {
        return (float)value;
    }

    return (double)value;
}

static ConstantValue convert_constant_value(ConstantValue value, DataType source, DataType destination, Token token)
{
    if (is_float_type(source) && is_integer_type(destination)) {
        value.integer = float_constant_to_integer(value.floating, destination, token);

        return value;
    }

    if (is_integer_type(destination)) {
        value.integer = normalize_integer_constant(value.integer, destination);

        return value;
    }

    if (is_integer_type(source)) {
        value.floating = integer_constant_to_float(value.integer, source, destination);

        return value;
    }

    value.floating = rounded_float_constant(value.floating, destination);

    return value;
}

static int64_t signed_i32_value(uint32_t value)
{
    if (value <= INT32_MAX) {
        return value;
    }

    return (int64_t)value - (UINT64_C(1) << 32);
}

static bool fold_division(uint32_t left, uint32_t right, uint64_t* value)
{
    if (!right) {
        return false;
    }

    int64_t signed_left = signed_i32_value(left);
    int64_t signed_right = signed_i32_value(right);

    *value = (uint32_t)(signed_left / signed_right);

    return true;
}

static bool fold_remainder(uint32_t left, uint32_t right, uint64_t* value)
{
    if (!right) {
        return false;
    }

    int64_t signed_left = signed_i32_value(left);
    int64_t signed_right = signed_i32_value(right);

    *value = (uint32_t)(signed_left % signed_right);

    return true;
}

static bool fold_binary_value(TokenType operator, uint32_t left, uint32_t right, uint64_t* value)
{
    switch (operator) {
        case TokenPlus:
            *value = left + right;
            return true;
        case TokenMinus:
            *value = left - right;
            return true;
        case TokenStar:
            *value = left * right;
            return true;
        case TokenSlash:
            return fold_division(left, right, value);
        case TokenPercent:
            return fold_remainder(left, right, value);
        case TokenPower:
            *value = (int)pow(left, right);
            return true;
        case TokenAmpersand:
            *value = left & right;
            return true;
        case TokenPipe:
            *value = left | right;
            return true;
        case TokenCircumflex:
            *value = left ^ right;
            return true;
        default:
            return false;
    }
}

static bool get_constant_comparison_operand(DataType type, uint64_t integer, int64_t* value)
{
    if (type == TypeBool) {
        *value = integer != 0;

        return true;
    }

    switch (type) {
        case TypeI8:
            *value = (int8_t)integer;
            break;
        case TypeI16:
            *value = (int16_t)integer;
            break;
        case TypeI32:
            *value = signed_i32_value(integer);
            break;
        case TypeU8:
            *value = (uint8_t)integer;
            break;
        case TypeU16:
            *value = (uint16_t)integer;
            break;
        case TypeU32:
            *value = (uint32_t)integer;
            break;
        default:
            return false;
    }

    return true;
}

static bool fold_boolean_value(TokenType operator, int64_t left, int64_t right, bool* value)
{
    switch (operator) {
        case TokenEqualEqual:
            *value = left == right;
            break;
        case TokenNotEqual:
            *value = left != right;
            break;
        case TokenLess:
            *value = left < right;
            break;
        case TokenLessEqual:
            *value = left <= right;
            break;
        case TokenGreater:
            *value = left > right;
            break;
        case TokenGreaterEqual:
            *value = left >= right;
            break;
        case TokenKeywordAnd:
            *value = left && right;
            break;
        case TokenKeywordOr:
            *value = left || right;
            break;
        default:
            return false;
    }

    return true;
}

ASTNode* create_constant_node(ConstantValue value, DataType type, Token token)
{
    if (type == TypeBool) {
        return create_boolean_node(token, value.integer != 0);
    }

    if (type == TypeChar) {
        return create_character_node(token, value.integer);
    }

    if (is_float_type(type)) {
        return create_float_node(token, value.floating, type);
    }

    return create_integer_node(token, value.integer, type);
}

static bool evaluate_conversion(const ASTNode* ast, ConstantValue* value)
{
    const ASTNode* expression = ast->conversion.expr;
    bool constant = evaluate_constant(expression, value);

    if (!constant) {
        return false;
    }

    DataType source = get_node_data_type(expression);
    DataType destination = ast->conversion.type_id;
    Token token = ast->conversion.token;
    bool scalar = value->integer <= UINT32_MAX && is_unicode_scalar((uint32_t)value->integer);

    if (destination == TypeChar && !scalar) {
        invalid_constant_conversion_error(token, destination);
    }

    if (destination != TypeChar) {
        *value = convert_constant_value(*value, source, destination, token);
    }

    return true;
}

static bool evaluate_prefix(const ASTNode* ast, ConstantValue* value)
{
    bool constant = evaluate_constant(ast->prefix.expr, value);

    if (!constant) {
        return false;
    }

    if (ast->prefix.operator.type == TokenKeywordNot) {
        value->integer = !value->integer;

        return true;
    }

    if (ast->prefix.operator.type != TokenMinus) {
        return false;
    }

    DataType type = get_node_data_type(ast);
    uint64_t negated = 0 - value->integer;

    value->integer = normalize_integer_constant(negated, type);
    value->floating = -value->floating;

    return true;
}

static bool evaluate_boolean_binary(
    const ASTNode* ast, ConstantValue left_value, ConstantValue right_value, ConstantValue* value)
{
    int64_t left;
    int64_t right;
    DataType left_type = get_node_data_type(ast->binary.left_expr);
    DataType right_type = get_node_data_type(ast->binary.right_expr);
    bool comparable_left = get_constant_comparison_operand(left_type, left_value.integer, &left);
    bool comparable_right = get_constant_comparison_operand(right_type, right_value.integer, &right);
    bool result = false;

    if (!comparable_left || !comparable_right) {
        return false;
    }

    bool folded = fold_boolean_value(ast->binary.operator.type, left, right, &result);

    value->integer = result;

    return folded;
}

static bool evaluate_binary(const ASTNode* ast, ConstantValue* value)
{
    ConstantValue left = {0};
    ConstantValue right = {0};
    bool constant_left = evaluate_constant(ast->binary.left_expr, &left);
    bool constant_right = evaluate_constant(ast->binary.right_expr, &right);

    if (!constant_left || !constant_right) {
        return false;
    }

    DataType left_type = get_node_data_type(ast->binary.left_expr);
    DataType right_type = get_node_data_type(ast->binary.right_expr);

    if (ast->binary.type_id == TypeBool) {
        return evaluate_boolean_binary(ast, left, right, value);
    }

    if (left_type != TypeI32 || right_type != TypeI32) {
        return false;
    }

    bool folded = fold_binary_value(ast->binary.operator.type, left.integer, right.integer, &value->integer);

    value->integer = normalize_integer_constant(value->integer, TypeI32);
    value->floating = integer_constant_to_float(value->integer, TypeI32, TypeF64);

    return folded;
}

static bool evaluate_variable(const ASTNode* ast, ConstantValue* value)
{
    const ASTNode* symbol = ast->variable.symbol;

    if (!symbol || symbol->type != ASTVariableDefinition) {
        return false;
    }

    if (!symbol->variable_definition.fixed || symbol->variable_definition.address_taken
        || symbol->variable_definition.reference_type != RefNone) {
        return false;
    }

    const Scope* definition_scope = symbol->variable_definition.scope;

    if (definition_scope->frame_scope != ast->variable.scope->frame_scope) {
        return false;
    }

    const ASTNode* initializer = symbol->variable_definition.expr;
    DataType type = symbol->variable_definition.type_id;

    if (!initializer || type == TypeChar) {
        return false;
    }

    return evaluate_constant(initializer, value);
}

static bool evaluate_selected_branch(const ASTNode* ast, ConstantValue* value)
{
    ConstantValue condition = {0};
    bool constant = evaluate_constant(ast->conditional.condition, &condition);

    if (!constant) {
        return false;
    }

    const ASTNode* branch = condition.integer ? ast->conditional.then_branch : ast->conditional.else_branch;

    return evaluate_branch(branch, value);
}

static bool evaluate_branch(const ASTNode* branch, ConstantValue* value)
{
    if (!branch) {
        return false;
    }

    if (branch->type == ASTConditional) {
        return evaluate_selected_branch(branch, value);
    }

    if (branch->type != ASTCompound || branch->compound.statements.count != 1) {
        return false;
    }

    const ASTNode* result = branch->compound.statements.data[0];
    DataType type = get_node_data_type(result);

    if (type == TypeChar || type == TypeString) {
        return false;
    }

    return evaluate_constant(result, value);
}

static bool evaluate_conditional(const ASTNode* ast, ConstantValue* value)
{
    if (!ast->conditional.expression || ast->conditional.reference_type != RefNone) {
        return false;
    }

    return evaluate_selected_branch(ast, value);
}

bool evaluate_constant(const ASTNode* ast, ConstantValue* value)
{
    if (!ast) {
        return false;
    }

    DataType type = get_node_data_type(ast);

    switch (ast->type) {
        case ASTInteger:
            value->integer = normalize_integer_constant(ast->integer_literal.value, type);
            value->floating = integer_constant_to_float(value->integer, type, TypeF64);
            return true;
        case ASTFloat:
            value->floating = rounded_float_constant(ast->float_literal.value, type);
            return true;
        case ASTBoolean:
            value->integer = ast->boolean_literal.value;
            return true;
        case ASTCharacter:
            value->integer = ast->character_literal.value;
            return true;
        case ASTPrefix:
            return evaluate_prefix(ast, value);
        case ASTBinary:
            return evaluate_binary(ast, value);
        case ASTConversion:
            return evaluate_conversion(ast, value);
        case ASTVariable:
            return evaluate_variable(ast, value);
        case ASTConditional:
            return evaluate_conditional(ast, value);
        default:
            return false;
    }
}
