#include "compiler/analyzer/expression.h"
#include "common/array_type.h"
#include "compiler/analyzer/conversion.h"
#include "compiler/analyzer/function.h"
#include "compiler/analyzer/reference.h"
#include "compiler/lexer/token.h"
#include "compiler/diagnostics.h"

static _Noreturn void binary_type_error(Token token, Type left, Type right)
{
    const char* left_name = get_data_type_name(left);
    const char* right_name = get_data_type_name(right);

    fatal_source_error(token.line, token.column, "No lossless common numeric type for %s and %s", left_name, right_name);
}

static Type get_float_binary_type(Type left_type, Type right_type)
{
    if (left_type.kind == TypeF64 || right_type.kind == TypeF64) {
        return (Type){.kind = TypeF64};
    }

    return (Type){.kind = TypeF32};
}

static void apply_binary_float_literal_types(ASTNode* left, ASTNode* right)
{
    Type left_type = get_node_data_type(left);
    Type right_type = get_node_data_type(right);

    if (!is_float_type(left_type.kind) || !is_float_type(right_type.kind)) {
        return;
    }

    Type type = get_float_binary_type(left_type, right_type);

    if (!get_float_literal(left)) {
        type = left_type;
    }

    if (!get_float_literal(right)) {
        type = right_type;
    }

    apply_literal_type(left, type);
    apply_literal_type(right, type);
}

static void apply_binary_integer_literal_types(ASTNode* left, ASTNode* right)
{
    Type left_type = get_node_data_type(left);
    Type right_type = get_node_data_type(right);
    bool negative;
    ASTNode* left_literal = get_integer_literal(left, &negative);
    ASTNode* right_literal = get_integer_literal(right, &negative);
    bool same_signedness = is_signed_integer_type(left_type.kind) == is_signed_integer_type(right_type.kind);
    bool widen_left = same_signedness && can_implicitly_widen_integer(left_type.kind, right_type.kind);

    if (left_literal && right_literal && widen_left) {
        apply_integer_literal_type(left, right_type);

        left_type = right_type;
    }

    if (is_integer_type(left_type.kind)) {
        apply_integer_literal_type(right, left_type);

        right_type = get_node_data_type(right);
    }

    if (is_integer_type(right_type.kind)) {
        apply_integer_literal_type(left, right_type);
    }
}

static Type array_binary_result_type(Analyzer* analyzer, Type shape, Type leaf)
{
    if (!is_array_type(shape.kind)) {
        return leaf;
    }

    Type element = array_binary_result_type(analyzer, shape.array->element_type, leaf);

    Type result = find_array_type(&analyzer->array_types, element, shape.array->count);

    if (result.kind != TypeUnknown) {
        return result;
    }

    return add_array_type(&analyzer->array_types, element, shape.array->count);
}

Type analyze_binary_operand_types(Token operator, Type left_type, Type right_type, Type* operand_type)
{
    TokenType operator_type = operator.type;

    bool left_text = left_type.kind == TypeChar || left_type.kind == TypeString;
    bool right_text = right_type.kind == TypeChar || right_type.kind == TypeString;

    if (operator_type == TokenPlus && left_text && right_text) {
        return (Type){.kind = TypeString};
    }

    Type common_type = {.kind = get_common_numeric_type(left_type.kind, right_type.kind)};
    bool float_operands = is_float_type(common_type.kind);

    if (!types_equal(left_type, right_type) && common_type.kind == TypeUnknown) {
        binary_type_error(operator, left_type, right_type);
    }

    if (common_type.kind == TypeUnknown) {
        common_type = left_type;
    }

    *operand_type = common_type;

    if (left_type.kind == TypeString) {
        semantic_error("Invalid operator for string operands", operator);
    }

    if (left_type.kind == TypeChar && !is_character_comparison_token(operator_type)) {
        semantic_error("Invalid operator for char operands", operator);
    }

    if (is_logical_token(operator_type) && left_type.kind != TypeBool) {
        semantic_error("Logical operators require bool operands", operator);
    }

    if (is_comparison_token(operator_type) && !is_integer_type(left_type.kind)
        && !float_operands && left_type.kind != TypeChar) {
        semantic_error("Comparison operators require compatible operand types", operator);
    }

    if (operator_type == TokenPower && left_type.kind != TypeI32) {
        semantic_error("Invalid operands for binary operator", operator);
    }

    if (float_operands && !is_term_token(operator_type)
        && !is_factor_token(operator_type) && !is_comparison_token(operator_type)
        && !is_equality_token(operator_type)) {
        semantic_error("Invalid operands for binary operator", operator);
    }

    Type result_type = common_type;

    if (operator_type == TokenSpaceship) {
        result_type = (Type){.kind = TypeI32};
    }

    if (is_bool_operator_token(operator_type)) {
        result_type = (Type){.kind = TypeBool};
    }

    return result_type;
}

static void analyze_binary_types(ASTNode* ast, Type left_type, Type right_type)
{
    ast->binary.type_id = analyze_binary_operand_types(
        ast->binary.operator, left_type, right_type, &ast->binary.operand_type);
}

static void analyze_array_binary(Analyzer* analyzer, ASTNode* ast, Type left, Type right)
{
    TokenType operator_type = ast->binary.operator.type;
    bool equality = is_equality_token(operator_type);
    bool same_shape = array_shapes_equal(left, right);

    if (!is_array_type(left.kind) || !is_array_type(right.kind)) {
        semantic_error("Array binary operators require two arrays", ast->binary.operator);
    }

    if (!same_shape && !equality) {
        semantic_error("Array binary operators require matching shapes", ast->binary.operator);
    }

    if (!same_shape) {
        ast->binary.type_id = (Type){.kind = TypeBool};

        return;
    }

    Type left_leaf = get_array_leaf_type(left);
    Type right_leaf = get_array_leaf_type(right);

    analyze_binary_types(ast, left_leaf, right_leaf);

    Type result = ast->binary.type_id;
    ast->binary.type_id = (Type){.kind = TypeBool};

    if (!equality) {
        ast->binary.type_id = array_binary_result_type(analyzer, left, result);
    }
}

void analyze_binary(Analyzer* analyzer, ASTNode* ast)
{
    analyze_node(analyzer, ast->binary.left_expr);
    analyze_node(analyzer, ast->binary.right_expr);

    Type left_type = get_node_data_type(ast->binary.left_expr);
    Type right_type = get_node_data_type(ast->binary.right_expr);

    if (is_array_type(left_type.kind) || is_array_type(right_type.kind)) {
        analyze_array_binary(analyzer, ast, left_type, right_type);

        return;
    }

    TokenType operator_type = ast->binary.operator.type;
    bool comparison = is_comparison_token(operator_type) || is_equality_token(operator_type);

    if (comparison) {
        apply_binary_float_literal_types(ast->binary.left_expr, ast->binary.right_expr);
    }

    apply_binary_integer_literal_types(ast->binary.left_expr, ast->binary.right_expr);

    left_type = get_node_data_type(ast->binary.left_expr);
    right_type = get_node_data_type(ast->binary.right_expr);

    analyze_binary_types(ast, left_type, right_type);
}

static void infer_negated_integer_literal_type(ASTNode* ast)
{
    bool negative;
    ASTNode* literal = get_integer_literal(ast, &negative);

    if (!literal || !negative) {
        return;
    }

    if (literal->integer_literal.type_id.kind != TypeUnknown) {
        return;
    }

    apply_integer_literal_type(ast, (Type){.kind = TypeI64});
}

static void analyze_constant_expression(Analyzer* analyzer, ASTNode* ast)
{
    Scope* previous = analyzer->constant_scope;
    FunctionPath* previous_functions = analyzer->constant_functions;
    analyzer->constant_scope = analyzer->current_scope;
    analyzer->constant_functions = analyzer->function_path;

    analyze_node(analyzer, ast->prefix.expr);

    analyzer->constant_scope = previous;
    analyzer->constant_functions = previous_functions;

    if (get_reference_type(ast->prefix.expr) != RefNone || get_node_data_type(ast).kind == TypeVoid) {
        semantic_error("Constant expression must produce a value", ast->prefix.operator);
    }
}

void analyze_prefix(Analyzer* analyzer, ASTNode* ast)
{
    if (ast->prefix.operator.type == TokenKeywordConst) {
        analyze_constant_expression(analyzer, ast);

        return;
    }

    if (get_reference_type(ast) != RefNone) {
        analyze_reference(analyzer, ast);

        return;
    }

    infer_negated_integer_literal_type(ast);
    analyze_node(analyzer, ast->prefix.expr);

    if (ast->prefix.operator.type == TokenKeywordNot && get_node_data_type(ast->prefix.expr).kind != TypeBool) {
        semantic_error("Logical not requires a bool operand", ast->prefix.operator);
    }

    Type type = get_node_data_type(ast->prefix.expr);

    if (is_array_type(type.kind)) {
        type = get_array_leaf_type(type);
    }

    if (ast->prefix.operator.type == TokenTilde && !is_integer_type(type.kind)) {
        semantic_error("Bitwise complement requires an integer operand", ast->prefix.operator);
    }

    if (type.kind == TypeString) {
        semantic_error("Invalid prefix operator for string operands", ast->prefix.operator);
    }

    if (type.kind == TypeChar) {
        semantic_error("Invalid prefix operator for char operands", ast->prefix.operator);
    }

    bool numeric = is_signed_integer_type(type.kind) || is_float_type(type.kind);

    if (ast->prefix.operator.type == TokenMinus && !numeric) {
        semantic_error("Negation requires a signed numeric operand", ast->prefix.operator);
    }
}
