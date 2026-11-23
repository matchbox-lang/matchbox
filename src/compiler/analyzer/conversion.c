#include "compiler/analyzer/conversion.h"
#include "common/array_type.h"
#include "common/vector.h"
#include "compiler/ast/literal.h"
#include "compiler/ast/query.h"
#include "compiler/ast/reference.h"
#include "compiler/diagnostics.h"
#include "runtime/string_object.h"
#include <float.h>

static _Noreturn void float_literal_out_of_range_error(Token token, Type type)
{
    const char* type_name = get_data_type_name(type);

    fatal_source_error(token.line, token.column, "Float literal is outside the range of %s", type_name);
}

static _Noreturn void integer_literal_not_representable_error(Token token, Type type)
{
    const char* type_name = get_data_type_name(type);

    fatal_source_error(token.line, token.column, "Integer literal cannot be represented as %s", type_name);
}

static _Noreturn void integer_literal_out_of_range_error(Token token, Type type)
{
    const char* type_name = get_data_type_name(type);

    fatal_source_error(token.line, token.column, "Integer literal is outside the range of %s", type_name);
}

static _Noreturn void negative_unsigned_integer_error(Token token)
{
    fatal_source_error(token.line, token.column, "Negative integer literal cannot have an unsigned type");
}

static _Noreturn void unsupported_explicit_conversion_error(Token token, Type source, Type destination)
{
    const char* source_name = get_data_type_name(source);
    const char* destination_name = get_data_type_name(destination);

    fatal_source_error(token.line, token.column, "Unsupported explicit conversion from %s to %s",
        source_name, destination_name);
}

bool apply_integer_literal_type(ASTNode* expression, Type type)
{
    bool negative;
    ASTNode* literal = get_integer_literal(expression, &negative);

    if (!literal) {
        return false;
    }

    if (negative && is_integer_type(type.kind) && !is_signed_integer_type(type.kind)) {
        negative_unsigned_integer_error(literal->integer_literal.token);
    }

    bool fits_integer = integer_literal_fits_type(literal->integer_literal.value, negative, type);
    bool fits_float = integer_literal_fits_float(literal->integer_literal.value, type);

    if (is_float_type(type.kind) && !fits_float) {
        integer_literal_not_representable_error(literal->integer_literal.token, type);
    }

    if (is_integer_type(type.kind) && !fits_integer) {
        integer_literal_out_of_range_error(literal->integer_literal.token, type);
    }

    literal->integer_literal.type_id = type;

    return true;
}

static void apply_array_type(ASTNode* ast, Type type)
{
    if (ast->array.fill) {
        return;
    }

    ast->array.type_id = type;
}

void apply_literal_type(ASTNode* expression, Type type)
{
    if (expression->type == ASTArray && is_array_type(type.kind)) {
        apply_array_type(expression, type);

        return;
    }

    if (expression->type == ASTPrefix && expression->prefix.operator.type == TokenKeywordConst) {
        apply_literal_type(expression->prefix.expr, type);

        return;
    }

    if (is_integer_type(type.kind) || is_float_type(type.kind)) {
        apply_integer_literal_type(expression, type);
    }

    expression = get_float_literal(expression);

    if (!expression || !is_float_type(type.kind)) {
        return;
    }

    if (type.kind == TypeF32 && expression->float_literal.value > FLT_MAX) {
        float_literal_out_of_range_error(expression->float_literal.token, type);
    }

    expression->float_literal.type_id = type;
}

static void apply_slice_branch_type(ASTNode* branch, Type type, ReferenceType reference_type)
{
    if (!branch || !is_slice_type(type.kind)) {
        return;
    }

    if (branch->type == ASTConditional) {
        apply_declared_type(branch, type, reference_type);

        return;
    }

    Vector* statements = &branch->compound.statements;

    if (statements->count) {
        apply_declared_type(statements->data[statements->count - 1], type, reference_type);
    }
}

static void apply_match_slice_type(ASTNode* expression, Type type, ReferenceType reference_type)
{
    for (size_t i = 0; i < expression->match.arms.count; i++) {
        MatchArm* arm = expression->match.arms.data[i];

        apply_slice_branch_type(arm->branch, type, reference_type);
    }
}

static void apply_conditional_declared_type(ASTNode* expression, Type type, ReferenceType reference_type)
{
    apply_literal_type(expression, type);

    expression->conditional.type_id = type;
    expression->conditional.reference_type = reference_type;

    apply_slice_branch_type(expression->conditional.then_branch, type, reference_type);
    apply_slice_branch_type(expression->conditional.else_branch, type, reference_type);
}

static void apply_match_declared_type(ASTNode* expression, Type type, ReferenceType reference_type)
{
    apply_literal_type(expression, type);

    expression->match.type_id = type;
    expression->match.reference_type = reference_type;

    apply_match_slice_type(expression, type, reference_type);
}

static void apply_value_declared_type(ASTNode* expression, Type type, ReferenceType reference_type)
{
    if (is_slice_type(type.kind)) {
        apply_slice_type(expression, type, reference_type);

        return;
    }

    apply_literal_type(expression, type);
}

void apply_declared_type(ASTNode* expression, Type type, ReferenceType reference_type)
{
    if (expression->type == ASTPrefix && expression->prefix.operator.type == TokenKeywordConst) {
        apply_declared_type(expression->prefix.expr, type, reference_type);

        return;
    }

    if (type.kind == TypeUnknown) {
        return;
    }

    switch (expression->type) {
        case ASTConditional:
            apply_conditional_declared_type(expression, type, reference_type);
            break;
        case ASTMatch:
            apply_match_declared_type(expression, type, reference_type);
            break;
        default:
            apply_value_declared_type(expression, type, reference_type);
            break;
    }
}

void infer_conversion_literal(ASTNode* ast)
{
    TypeKind kind = get_token_data_type(ast->function_call.token.type);
    size_t count = count_vector(&ast->function_call.args);

    if ((!is_integer_type(kind) && !is_float_type(kind) && kind != TypeChar) || count != 1) {
        return;
    }

    ASTNode* expression = ast->function_call.args.data[0];
    Type source = get_node_data_type(expression);

    if (source.kind == TypeUnknown && expression->type == ASTInteger) {
        apply_integer_literal_type(expression, (Type){.kind = TypeU64});
    }
}

void analyze_type_conversion(ASTNode* ast)
{
    Token token = ast->function_call.token;
    Type destination = {.kind = get_token_data_type(token.type)};
    size_t count = count_vector(&ast->function_call.args);

    if (count != 1) {
        semantic_error("Explicit type conversion requires exactly one argument", token);
    }

    ASTNode* expression = ast->function_call.args.data[0];
    Type source = get_node_data_type(expression);
    bool supported = can_explicitly_convert_type(source.kind, destination.kind);
    ReferenceType reference = get_reference_type(expression);

    if (!supported) {
        unsupported_explicit_conversion_error(token, source, destination);
    }

    if (reference != RefNone) {
        semantic_error("Explicit type conversions require a value", token);
    }

    free_string_object(ast->function_call.id);
    free_vector(&ast->function_call.args);
    free_vector(&ast->function_call.reference_origins);

    ast->type = ASTConversion;
    ast->conversion.expr = expression;
    ast->conversion.token = token;
    ast->conversion.type_id = destination;
}
