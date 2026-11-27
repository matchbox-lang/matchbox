#include "compiler/constant.h"
#include "builtin/builtin.h"
#include "common/array_type.h"
#include "common/math.h"
#include "common/string_view.h"
#include "common/tuple_type.h"
#include "common/unicode.h"
#include "compiler/ast/ast.h"
#include "compiler/ast/literal.h"
#include "compiler/ast/query.h"
#include "compiler/ast/reference.h"
#include "compiler/diagnostics.h"
#include "compiler/scope.h"
#include "runtime/string_object.h"
#include <limits.h>
#include <math.h>

static bool evaluate_branch(const ASTNode* branch, ConstantValue* value);
static bool evaluate_string_constant(const ASTNode* ast, StringView* value);
static bool can_discard_length_argument(const ASTNode* ast);
static const ASTNode* get_constant_tuple_initializer(const ASTNode* ast);

static _Noreturn void invalid_conversion_error(Token token, Type destination)
{
    const char* name = get_data_type_name(destination);

    fatal_source_error(token.line, token.column, "Cannot convert value to %s", name);
}

static uint64_t normalize_integer_constant(uint64_t value, Type type)
{
    size_t bits = get_integer_type_size(type.kind) * CHAR_BIT;

    if (bits == 64 || !bits) {
        return value;
    }

    uint64_t mask = (UINT64_C(1) << bits) - 1;
    uint64_t result = value & mask;
    bool negative = is_signed_integer_type(type.kind) && (result & (UINT64_C(1) << (bits - 1)));

    if (negative) {
        result |= ~mask;
    }

    return result;
}

static double rounded_float_constant(double value, Type type)
{
    if (type.kind == TypeF32) {
        return (float)value;
    }

    return value;
}

static uint64_t float_constant_to_integer(double value, Type destination, Token token)
{
    bool signed_type = is_signed_integer_type(destination.kind);
    size_t bits = get_integer_type_size(destination.kind) * CHAR_BIT;
    double upper = ldexp(1.0, bits - signed_type);
    double lower = 0.0;
    double truncated = trunc(value);

    if (signed_type) {
        lower = -upper;
    }

    if (!isfinite(truncated) || truncated < lower || truncated >= upper) {
        invalid_conversion_error(token, destination);
    }

    if (signed_type) {
        return (uint64_t)(int64_t)truncated;
    }

    return (uint64_t)truncated;
}

static double integer_constant_to_float(uint64_t value, Type source, Type destination)
{
    if (is_signed_integer_type(source.kind) && destination.kind == TypeF32) {
        return (float)(int64_t)value;
    }

    if (is_signed_integer_type(source.kind)) {
        return (double)(int64_t)value;
    }

    if (destination.kind == TypeF32) {
        return (float)value;
    }

    return (double)value;
}

static ConstantValue convert_constant_value(ConstantValue value, Type source, Type destination, Token token)
{
    if (is_float_type(source.kind) && is_integer_type(destination.kind)) {
        value.integer = float_constant_to_integer(value.floating, destination, token);

        return value;
    }

    if (is_integer_type(destination.kind)) {
        value.integer = normalize_integer_constant(value.integer, destination);

        return value;
    }

    if (is_integer_type(source.kind)) {
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

static bool fold_power(uint32_t left, uint32_t right, uint64_t* value)
{
    int64_t base = signed_i32_value(left);
    int64_t exponent = signed_i32_value(right);

    if (base == 0 && exponent < 0) {
        return false;
    }

    int64_t result = ipow(base, exponent);
    *value = (uint64_t)result;

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
            return fold_power(left, right, value);
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

static bool get_constant_comparison_operand(Type type, uint64_t integer, int64_t* value)
{
    if (type.kind == TypeBool) {
        *value = integer != 0;

        return true;
    }

    switch (type.kind) {
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

ASTNode* create_constant_node(ConstantValue value, Type type, Token token)
{
    if (type.kind == TypeBool) {
        return create_boolean_node(token, value.integer != 0);
    }

    if (type.kind == TypeChar) {
        return create_character_node(token, value.integer);
    }

    if (is_float_type(type.kind)) {
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

    Type source = get_node_data_type(expression);
    Type destination = ast->conversion.type_id;
    Token token = ast->conversion.token;
    bool scalar = value->integer <= UINT32_MAX && is_unicode_scalar((uint32_t)value->integer);

    if (destination.kind == TypeChar && !scalar) {
        invalid_conversion_error(token, destination);
    }

    if (destination.kind != TypeChar) {
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

    Type type = get_node_data_type(ast);
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
    Type left_type = get_node_data_type(ast->binary.left_expr);
    Type right_type = get_node_data_type(ast->binary.right_expr);
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

static bool evaluate_string_binary(const ASTNode* ast, ConstantValue* value)
{
    const ASTNode* left = ast->binary.left_expr;
    const ASTNode* right = ast->binary.right_expr;

    if (left->type != ASTString || right->type != ASTString) {
        return false;
    }

    bool equal = string_objects_equal(left->string_literal.value, right->string_literal.value);
    value->integer = equal == (ast->binary.operator.type == TokenEqualEqual);

    return true;
}

static bool evaluate_binary(const ASTNode* ast, ConstantValue* value)
{
    if (ast->binary.operand_type.kind == TypeString && ast->binary.type_id.kind == TypeBool) {
        return evaluate_string_binary(ast, value);
    }

    ConstantValue left = {0};
    ConstantValue right = {0};
    bool constant_left = evaluate_constant(ast->binary.left_expr, &left);
    bool constant_right = evaluate_constant(ast->binary.right_expr, &right);

    if (!constant_left || !constant_right) {
        return false;
    }

    Type left_type = get_node_data_type(ast->binary.left_expr);
    Type right_type = get_node_data_type(ast->binary.right_expr);

    if (ast->binary.type_id.kind == TypeBool) {
        return evaluate_boolean_binary(ast, left, right, value);
    }

    if (left_type.kind != TypeI32 || right_type.kind != TypeI32) {
        return false;
    }

    bool folded = fold_binary_value(ast->binary.operator.type, left.integer, right.integer, &value->integer);

    value->integer = normalize_integer_constant(value->integer, (Type){.kind = TypeI32});
    value->floating = integer_constant_to_float(value->integer, (Type){.kind = TypeI32}, (Type){.kind = TypeF64});

    return folded;
}

static const ASTNode* get_constant_variable_initializer(const ASTNode* ast)
{
    const ASTNode* symbol = ast->variable.symbol;

    if (!symbol || symbol->type != ASTVariableDefinition) {
        return NULL;
    }

    if (!symbol->variable_definition.fixed || symbol->variable_definition.address_taken
        || symbol->variable_definition.reference_type != RefNone) {
        return NULL;
    }

    const Scope* definition_scope = symbol->variable_definition.scope;

    if (definition_scope->frame_scope != ast->variable.scope->frame_scope) {
        return NULL;
    }

    return symbol->variable_definition.expr;
}

static bool evaluate_variable(const ASTNode* ast, ConstantValue* value)
{
    const ASTNode* initializer = get_constant_variable_initializer(ast);
    Type type = get_node_data_type(ast);

    if (!initializer || type.kind == TypeChar) {
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
    Type type = get_node_data_type(result);

    if (type.kind == TypeChar || type.kind == TypeString) {
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

static bool can_discard_length_prefix(const ASTNode* ast)
{
    bool reference = ast->prefix.operator.type == TokenAmpersand || ast->prefix.operator.type == TokenCircumflex;

    return reference && can_discard_length_argument(ast->prefix.expr);
}

static bool can_discard_length_argument(const ASTNode* ast)
{
    if (ast->type == ASTVariable || is_literal(ast)) {
        return true;
    }

    if (ast->type == ASTPrefix) {
        return can_discard_length_prefix(ast);
    }

    if (ast->type != ASTArray) {
        return false;
    }

    bool discard = true;

    for (size_t i = 0; i < ast->array.elements.count && discard; i++) {
        discard = can_discard_length_argument(ast->array.elements.data[i]);
    }

    return discard;
}

static bool has_local_constant_variables(const ASTNode* ast)
{
    if (ast->type == ASTVariable) {
        const ASTNode* initializer = get_constant_variable_initializer(ast);

        return initializer && has_local_constant_variables(initializer);
    }

    if (ast->type == ASTBinary) {
        return has_local_constant_variables(ast->binary.left_expr)
            && has_local_constant_variables(ast->binary.right_expr);
    }

    if (ast->type == ASTPrefix) {
        return has_local_constant_variables(ast->prefix.expr);
    }

    if (ast->type == ASTConversion) {
        return has_local_constant_variables(ast->conversion.expr);
    }

    return true;
}

static bool get_string_slice_constants(const ASTNode* ast, StringView* source, uint64_t* start, uint64_t* end)
{
    if (ast->type != ASTSlice || ast->slice.type_id.kind != TypeStringSlice || !ast->slice.start) {
        return false;
    }

    bool constant_source = evaluate_string_constant(ast->slice.expr, source);

    if (!constant_source || !has_local_constant_variables(ast->slice.start)
        || !has_local_constant_variables(ast->slice.end)) {
        return false;
    }

    bool constant_start = evaluate_integer_constant(ast->slice.start, start);
    bool constant_end = evaluate_integer_constant(ast->slice.end, end);

    return constant_start && constant_end;
}

static bool evaluate_string_slice_constant(const ASTNode* ast, StringView* value)
{
    uint64_t start = 0;
    uint64_t end = 0;
    bool constant = get_string_slice_constants(ast, value, &start, &end);

    if (!constant || start > end || end > value->length) {
        return false;
    }

    value->chars += (size_t)start;
    value->length = (size_t)(end - start);
    value->hash = 0;

    return true;
}

static bool evaluate_string_constant(const ASTNode* ast, StringView* value)
{
    if (!ast) {
        return false;
    }

    if (ast->type == ASTString) {
        *value = get_string_view(ast->string_literal.value);

        return true;
    }

    if (ast->type == ASTVariable) {
        const ASTNode* initializer = get_constant_variable_initializer(ast);

        return evaluate_string_constant(initializer, value);
    }

    if (ast->type == ASTPrefix && ast->prefix.operator.type == TokenKeywordConst) {
        return evaluate_string_constant(ast->prefix.expr, value);
    }

    return evaluate_string_slice_constant(ast, value);
}

bool validate_string_slice_constant(const ASTNode* ast)
{
    StringView source = {0};
    uint64_t start = 0;
    uint64_t end = 0;
    bool constant = get_string_slice_constants(ast, &source, &start, &end);

    if (!constant) {
        return false;
    }

    if (start > end || end > source.length) {
        semantic_error("String slice is out of bounds", ast->slice.token);
    }

    bool valid_start = is_utf8_boundary(source.chars, (size_t)start, source.length);
    bool valid_end = is_utf8_boundary(source.chars, (size_t)end, source.length);

    if (!valid_start || !valid_end) {
        semantic_error("String slice bound splits a UTF-8 character", ast->slice.token);
    }

    return true;
}

static const ASTNode* get_constant_tuple_element(const ASTNode* ast)
{
    if (!ast->index.constant) {
        return NULL;
    }

    const ASTNode* tuple = get_constant_tuple_initializer(ast->index.array);

    if (!tuple || ast->index.position >= tuple->array.elements.count) {
        return NULL;
    }

    return tuple->array.elements.data[ast->index.position];
}

static const ASTNode* get_constant_tuple_variable(const ASTNode* ast)
{
    const ASTNode* symbol = ast->variable.symbol;

    if (symbol && symbol->type == ASTVariableDefinition && symbol->variable_definition.constant) {
        return symbol->variable_definition.expr;
    }

    return get_constant_variable_initializer(ast);
}

static const ASTNode* get_constant_tuple_initializer(const ASTNode* ast)
{
    if (!ast) {
        return NULL;
    }

    if (ast->type == ASTVariable) {
        const ASTNode* initializer = get_constant_tuple_variable(ast);

        return get_constant_tuple_initializer(initializer);
    }

    if (ast->type == ASTPrefix && ast->prefix.operator.type == TokenKeywordConst) {
        return get_constant_tuple_initializer(ast->prefix.expr);
    }

    if (ast->type == ASTIndex) {
        const ASTNode* element = get_constant_tuple_element(ast);

        return get_constant_tuple_initializer(element);
    }

    if (ast->type != ASTArray || !ast->array.tuple || !can_discard_length_argument(ast)) {
        return NULL;
    }

    return ast;
}

static bool evaluate_tuple_index(const ASTNode* ast, ConstantValue* value)
{
    Type type = get_node_data_type(ast->index.array);

    if (type.kind != TypeTuple || get_reference_type(ast) != RefNone) {
        return false;
    }

    const ASTNode* element = get_constant_tuple_element(ast);

    return evaluate_constant(element, value);
}

static bool evaluate_builtin_call(const ASTNode* ast, ConstantValue* value)
{
    if (ast->builtin_call.builtin->entry) {
        return false;
    }

    const ASTNode* argument = ast->builtin_call.args.data[0];
    Type type = get_node_data_type(argument);

    if (type.kind == TypeString || type.kind == TypeStringSlice) {
        StringView string = {0};
        bool constant = evaluate_string_constant(argument, &string);
        value->integer = string.length;

        return constant;
    }

    bool discard = can_discard_length_argument(argument);

    if (!discard) {
        return false;
    }

    if (is_slice_type(type.kind)) {
        return false;
    }

    if (type.kind == TypeTuple) {
        value->integer = type.tuple->count;

        return true;
    }

    value->integer = type.array->count;

    return true;
}

bool evaluate_constant(const ASTNode* ast, ConstantValue* value)
{
    if (!ast) {
        return false;
    }

    Type type = get_node_data_type(ast);

    switch (ast->type) {
        case ASTIndex:
            return evaluate_tuple_index(ast, value);
        case ASTBuiltinCall:
            return evaluate_builtin_call(ast, value);
        case ASTInteger:
            value->integer = normalize_integer_constant(ast->integer_literal.value, type);
            value->floating = integer_constant_to_float(value->integer, type, (Type){.kind = TypeF64});
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

static bool evaluate_count_variable(const ASTNode* ast, uint64_t* value)
{
    const ASTNode* symbol = ast->variable.symbol;

    if (!symbol || symbol->type != ASTVariableDefinition) {
        return false;
    }

    if (!symbol->variable_definition.fixed || symbol->variable_definition.address_taken
        || symbol->variable_definition.reference_type != RefNone) {
        return false;
    }

    return evaluate_integer_constant(symbol->variable_definition.expr, value);
}

static bool divide_count_values(uint64_t left, uint64_t right, Type type, bool remainder, uint64_t* value)
{
    if (!right) {
        return false;
    }

    if (!is_signed_integer_type(type.kind)) {
        *value = remainder ? left % right : left / right;

        return true;
    }

    int64_t signed_left = (int64_t)left;
    int64_t signed_right = (int64_t)right;

    if (signed_left == INT64_MIN && signed_right == -1) {
        *value = remainder ? 0 : left;

        return true;
    }

    *value = remainder ? signed_left % signed_right : signed_left / signed_right;

    return true;
}

static uint64_t shift_count_right(uint64_t value, uint64_t shift, Type type)
{
    if (is_signed_integer_type(type.kind)) {
        return (uint64_t)((int64_t)value >> shift);
    }

    return value >> shift;
}

static bool fold_count_binary_value(TokenType operator_type, Type type, uint64_t left, uint64_t right, uint64_t* value)
{
    size_t bits = get_integer_type_size(type.kind) * CHAR_BIT;

    switch (operator_type) {
        case TokenPlus:
            *value = left + right;
            break;
        case TokenMinus:
            *value = left - right;
            break;
        case TokenStar:
            *value = left * right;
            break;
        case TokenSlash:
            return divide_count_values(left, right, type, false, value);
        case TokenPercent:
            return divide_count_values(left, right, type, true, value);
        case TokenAmpersand:
            *value = left & right;
            break;
        case TokenPipe:
            *value = left | right;
            break;
        case TokenCircumflex:
            *value = left ^ right;
            break;
        case TokenLshift:
            *value = left << (right % bits);
            break;
        case TokenRshift:
            *value = shift_count_right(left, right % bits, type);
            break;
        default:
            return false;
    }

    return true;
}

static bool evaluate_count_binary(const ASTNode* ast, uint64_t* value)
{
    uint64_t left = 0;
    uint64_t right = 0;
    bool left_constant = evaluate_integer_constant(ast->binary.left_expr, &left);
    bool right_constant = evaluate_integer_constant(ast->binary.right_expr, &right);

    if (!left_constant || !right_constant) {
        return false;
    }

    Type type = ast->binary.operand_type;

    return fold_count_binary_value(ast->binary.operator.type, type, left, right, value);
}

static bool fold_count_prefix_value(TokenType operator_type, uint64_t* value)
{
    switch (operator_type) {
        case TokenMinus:
            *value = 0 - *value;
            return true;
        case TokenTilde:
            *value = ~*value;
            return true;
        case TokenKeywordConst:
            return true;
        default:
            return false;
    }
}

static bool evaluate_count_prefix(const ASTNode* ast, uint64_t* value)
{
    bool evaluated = evaluate_integer_constant(ast->prefix.expr, value);

    if (!evaluated) {
        return false;
    }

    return fold_count_prefix_value(ast->prefix.operator.type, value);
}

static bool evaluate_count_conversion(const ASTNode* ast, uint64_t* value)
{
    ConstantValue constant = {0};
    bool evaluated = evaluate_constant(ast, &constant);

    if (evaluated) {
        *value = constant.integer;

        return true;
    }

    return evaluate_integer_constant(ast->conversion.expr, value);
}

static bool evaluate_count_expression(const ASTNode* ast, uint64_t* value)
{
    if (ast->type == ASTVariable) {
        return evaluate_count_variable(ast, value);
    }

    if (ast->type == ASTBinary) {
        return evaluate_count_binary(ast, value);
    }

    if (ast->type == ASTConversion) {
        return evaluate_count_conversion(ast, value);
    }

    if (ast->type == ASTPrefix) {
        return evaluate_count_prefix(ast, value);
    }

    ConstantValue constant = {0};
    bool evaluated = evaluate_constant(ast, &constant);
    *value = constant.integer;

    return evaluated;
}

bool evaluate_integer_constant(const ASTNode* ast, uint64_t* value)
{
    Type type = get_node_data_type(ast);
    ReferenceType reference = get_reference_type(ast);

    if (!is_integer_type(type.kind) || reference != RefNone) {
        return false;
    }

    bool evaluated = evaluate_count_expression(ast, value);
    *value = normalize_integer_constant(*value, type);

    return evaluated;
}
