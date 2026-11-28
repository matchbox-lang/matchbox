#include "compiler/analyzer/expression.h"
#include "common/array_type.h"
#include "compiler/analyzer/array.h"
#include "compiler/analyzer/conversion.h"
#include "compiler/analyzer/function.h"
#include "compiler/analyzer/reference.h"
#include "compiler/analyzer/tuple.h"
#include "compiler/ast/literal.h"
#include "compiler/ast/query.h"
#include "compiler/ast/reference.h"
#include "compiler/lexer/token.h"
#include "compiler/diagnostics.h"

static _Noreturn void binary_type_error(Token token, Type left, Type right)
{
    const char* left_name = get_data_type_name(left);
    const char* right_name = get_data_type_name(right);

    fatal_source_error(token.line, token.column, "No lossless common numeric type for %s and %s", left_name, right_name);
}

static void apply_binary_float_literal_types(ASTNode* left, ASTNode* right)
{
    Type left_type = get_node_data_type(left);
    Type right_type = get_node_data_type(right);

    if (!is_float_type(left_type.kind) || !is_float_type(right_type.kind)) {
        return;
    }

    TypeKind kind = get_common_numeric_type(left_type.kind, right_type.kind);
    Type type = {.kind = kind};

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

static Type analyze_text_binary_types(TokenType operator_type, Type left_type, Type right_type, Type* operand_type)
{
    bool left_string = left_type.kind == TypeString || left_type.kind == TypeStringSlice;
    bool right_string = right_type.kind == TypeString || right_type.kind == TypeStringSlice;
    bool left_text = left_type.kind == TypeChar || left_string;
    bool right_text = right_type.kind == TypeChar || right_string;

    if (operator_type == TokenPlus && left_text && right_text) {
        return (Type){.kind = TypeString};
    }

    if (left_string && right_string
        && (operator_type == TokenEqualEqual || operator_type == TokenNotEqual)) {
        *operand_type = left_type.kind == TypeStringSlice ? left_type : right_type;

        return (Type){.kind = TypeBool};
    }

    return (Type){.kind = TypeUnknown};
}

static Type resolve_binary_operand_type(Token operator, Type left_type, Type right_type)
{
    Type common_type = {.kind = get_common_numeric_type(left_type.kind, right_type.kind)};

    if (!types_equal(left_type, right_type) && common_type.kind == TypeUnknown) {
        binary_type_error(operator, left_type, right_type);
    }

    if (common_type.kind == TypeUnknown) {
        common_type = left_type;
    }

    return common_type;
}

static void validate_binary_operator(Token operator, Type left_type, Type right_type, bool float_operands)
{
    TokenType operator_type = operator.type;
    bool left_string = left_type.kind == TypeString || left_type.kind == TypeStringSlice;
    bool right_string = right_type.kind == TypeString || right_type.kind == TypeStringSlice;

    if (left_string || right_string) {
        semantic_error("Invalid operator for string operands", operator);
    }

    if (left_type.kind == TypeBool && !is_logical_token(operator_type) && !is_equality_token(operator_type)) {
        semantic_error("Invalid operator for bool operands", operator);
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
}

static Type resolve_binary_result_type(TokenType operator_type, Type operand_type)
{
    if (operator_type == TokenSpaceship) {
        return (Type){.kind = TypeI32};
    }

    if (is_bool_operator_token(operator_type)) {
        return (Type){.kind = TypeBool};
    }

    return operand_type;
}

Type analyze_binary_operand_types(Token operator, Type left_type, Type right_type, Type* operand_type)
{
    if (left_type.kind == TypeTuple || right_type.kind == TypeTuple) {
        semantic_error("Invalid operator for tuple operands", operator);
    }

    Type text_type = analyze_text_binary_types(operator.type, left_type, right_type, operand_type);

    if (text_type.kind != TypeUnknown) {
        return text_type;
    }

    *operand_type = resolve_binary_operand_type(operator, left_type, right_type);
    bool float_operands = is_float_type(operand_type->kind);

    validate_binary_operator(operator, left_type, right_type, float_operands);

    return resolve_binary_result_type(operator.type, *operand_type);
}

static void analyze_binary_types(ASTNode* ast, Type left_type, Type right_type)
{
    ast->binary.type_id = analyze_binary_operand_types(
        ast->binary.operator, left_type, right_type, &ast->binary.operand_type);
}

void analyze_binary(Analyzer* analyzer, ASTNode* ast)
{
    analyze_node(analyzer, ast->binary.left_expr);
    analyze_node(analyzer, ast->binary.right_expr);

    Type left_type = get_node_data_type(ast->binary.left_expr);
    Type right_type = get_node_data_type(ast->binary.right_expr);

    if (is_array_type(left_type.kind) || is_array_type(right_type.kind)) {
        ast->binary.type_id = analyze_array_binary_types(
            analyzer, ast->binary.operator, left_type, right_type, &ast->binary.operand_type);

        return;
    }

    if (left_type.kind == TypeTuple || right_type.kind == TypeTuple) {
        ast->binary.type_id = analyze_tuple_binary(
            analyzer, ast->binary.operator, left_type, right_type, &ast->binary.operand_type);

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

    Type type = get_node_data_type(ast->prefix.expr);

    validate_prefix_operand_type(ast->prefix.operator, type);
}

void validate_prefix_operand_type(Token operator, Type type)
{
    if (is_array_type(type.kind)) {
        type = get_array_leaf_type(type);
    }

    if (type.kind == TypeTuple) {
        validate_tuple_prefix(operator, type);

        return;
    }

    if (operator.type == TokenKeywordNot && type.kind != TypeBool) {
        semantic_error("Logical not requires a bool operand", operator);
    }

    if (operator.type == TokenTilde && !is_integer_type(type.kind)) {
        semantic_error("Bitwise complement requires an integer operand", operator);
    }

    if (type.kind == TypeString) {
        semantic_error("Invalid prefix operator for string operands", operator);
    }

    if (type.kind == TypeChar) {
        semantic_error("Invalid prefix operator for char operands", operator);
    }

    bool numeric = is_signed_integer_type(type.kind) || is_float_type(type.kind);

    if (operator.type == TokenMinus && !numeric) {
        semantic_error("Negation requires a signed numeric operand", operator);
    }
}
