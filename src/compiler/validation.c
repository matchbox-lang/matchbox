#include "compiler/validation.h"
#include "compiler/constant.h"
#include "compiler/diagnostics.h"

static void validate_node(const ASTNode* ast, bool check_zero);

static void validate_nodes(const Vector* nodes, size_t start, bool check_zero)
{
    for (size_t i = start; i < nodes->count; i++) {
        const ASTNode* node = nodes->data[i];

        validate_node(node, check_zero);
    }
}

static void validate_zero_divisor(const ASTNode* divisor, Token operator, DataType type, bool check_zero)
{
    bool division = operator.type == TokenSlash || operator.type == TokenSlashEqual;
    bool remainder = operator.type == TokenPercent || operator.type == TokenPercentEqual;

    if (!check_zero || !is_integer_type(type) || (!division && !remainder)) {
        return;
    }

    ConstantValue value = {0};
    bool constant = evaluate_constant(divisor, &value);

    if (!constant || value.integer != 0) {
        return;
    }

    fatal_source_error(operator.line, operator.column, "Division by zero");
}

static bool should_check_right_divisor(const ASTNode* ast, bool check_zero)
{
    if (!check_zero) {
        return false;
    }

    if (!is_logical_token(ast->binary.operator.type)) {
        return true;
    }

    ConstantValue left = {0};
    bool constant = evaluate_constant(ast->binary.left_expr, &left);

    if (!constant) {
        return false;
    }

    return (left.integer != 0) == (ast->binary.operator.type == TokenKeywordAnd);
}

static void validate_binary(const ASTNode* ast, bool check_zero)
{
    validate_node(ast->binary.left_expr, check_zero);

    bool check_right = should_check_right_divisor(ast, check_zero);

    validate_node(ast->binary.right_expr, check_right);
    validate_zero_divisor(ast->binary.right_expr, ast->binary.operator, ast->binary.operand_type, check_zero);
}

static void validate_conversion(const ASTNode* ast, bool check_zero)
{
    validate_node(ast->conversion.expr, check_zero);

    ConstantValue value = {0};
    bool constant = evaluate_constant(ast, &value);
    DataType source = get_node_data_type(ast->conversion.expr);

    if (constant || ast->conversion.type_id != TypeChar || source == TypeU8 || source == TypeChar) {
        return;
    }

    Token token = ast->conversion.token;

    fatal_source_error(token.line, token.column, "Integer-to-char conversion requires a constant or u8 source");
}

static void validate_assignment(const ASTNode* ast, bool check_zero)
{
    validate_node(ast->assignment.expr, check_zero);

    DataType type = get_node_data_type(ast->assignment.symbol);

    validate_zero_divisor(ast->assignment.expr, ast->assignment.operator, type, check_zero);
}

static void validate_conditional(const ASTNode* ast, bool check_zero)
{
    validate_node(ast->conditional.condition, check_zero);
    validate_node(ast->conditional.then_branch, check_zero);
    validate_node(ast->conditional.else_branch, check_zero);
}

static void validate_match_arm(const MatchArm* arm, bool check_zero)
{
    validate_node(arm->pattern, check_zero);
    validate_node(arm->guard, check_zero);
    validate_node(arm->branch, check_zero);
}

static void validate_match(const ASTNode* ast, bool check_zero)
{
    validate_node(ast->match.subject, check_zero);

    for (size_t i = 0; i < ast->match.arms.count; i++) {
        const MatchArm* arm = ast->match.arms.data[i];

        validate_match_arm(arm, check_zero);
    }

    validate_node(ast->match.default_branch, check_zero);
}

static void validate_node(const ASTNode* ast, bool check_zero)
{
    if (!ast) {
        return;
    }

    switch (ast->type) {
        case ASTAssignment:
            validate_assignment(ast, check_zero);
            break;
        case ASTBinary:
            validate_binary(ast, check_zero);
            break;
        case ASTBuiltinCall:
            validate_nodes(&ast->builtin_call.args, 0, check_zero);
            break;
        case ASTCompound:
            validate_nodes(&ast->compound.statements, 0, check_zero);
            break;
        case ASTConditional:
            validate_conditional(ast, check_zero);
            break;
        case ASTConversion:
            validate_conversion(ast, check_zero);
            break;
        case ASTFunctionCall:
            validate_nodes(&ast->function_call.args, 0, check_zero);
            break;
        case ASTFunctionDefinition:
            validate_nodes(&ast->function_definition.params, 0, check_zero);
            validate_node(ast->function_definition.body, check_zero);
            break;
        case ASTMatch:
            validate_match(ast, check_zero);
            break;
        case ASTParameter:
            validate_node(ast->parameter.default_value, check_zero);
            break;
        case ASTPrefix:
            validate_node(ast->prefix.expr, check_zero);
            break;
        case ASTReturn:
            validate_node(ast->return_statement.expr, check_zero);
            break;
        case ASTVariableDefinition:
            validate_node(ast->variable_definition.expr, check_zero);
            break;
        case ASTWhile:
            validate_node(ast->while_statement.condition, check_zero);
            validate_node(ast->while_statement.body, check_zero);
            break;
        default:
            break;
    }
}

void validate_constant_expressions(const ASTNode* ast, size_t start)
{
    validate_nodes(&ast->compound.statements, start, true);
}
