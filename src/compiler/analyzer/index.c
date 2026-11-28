#include "compiler/analyzer/index.h"
#include "common/array_type.h"
#include "compiler/analyzer/array.h"
#include "compiler/analyzer/conversion.h"
#include "compiler/analyzer/expression.h"
#include "compiler/analyzer/tuple.h"
#include "compiler/analyzer/variable.h"
#include "compiler/ast/binding.h"
#include "compiler/ast/literal.h"
#include "compiler/ast/predicate.h"
#include "compiler/ast/query.h"
#include "compiler/ast/reference.h"
#include "compiler/constant.h"
#include "compiler/diagnostics.h"
#include <stdint.h>

static Type get_index_literal_type(ASTNode* expression)
{
    if (expression->type == ASTPrefix && expression->prefix.operator.type == TokenKeywordConst) {
        return get_index_literal_type(expression->prefix.expr);
    }

    bool negative;

    get_integer_literal(expression, &negative);

    if (negative) {
        return (Type){.kind = TypeI64};
    }

    return (Type){.kind = TypeUsize};
}

static void analyze_index_source(Analyzer* analyzer, ASTNode* ast)
{
    bool previous = analyzer->tuple_projection;

    analyzer->tuple_projection = ast->type == ASTVariable || ast->type == ASTIndex;
    analyze_node(analyzer, ast);
    analyzer->tuple_projection = previous;
}

static bool is_compatible_index(ASTNode* ast, Type type, ASTNode* constant_expression)
{
    if (ast->index.constant || can_implicitly_widen_integer(type.kind, TypeUsize)) {
        return true;
    }

    return is_integer_type(type.kind) && constant_expression;
}

void analyze_index(Analyzer* analyzer, ASTNode* ast)
{
    Type literal_type = get_index_literal_type(ast->index.index);
    bool previous = analyzer->tuple_projection;
    analyzer->tuple_projection = false;

    analyze_index_source(analyzer, ast->index.array);
    apply_literal_type(ast->index.index, literal_type);
    analyze_node(analyzer, ast->index.index);

    analyzer->tuple_projection = previous;

    Type array_type = get_node_data_type(ast->index.array);
    Type index_type = get_node_data_type(ast->index.index);
    uint64_t value = 0;
    ast->index.constant = evaluate_integer_constant(ast->index.index, &value);
    ASTNode* constant_expression = get_index_constant_expression(ast->index.index);
    bool compatible = is_compatible_index(ast, index_type, constant_expression);
    bool negative = is_signed_integer_type(index_type.kind) && (int64_t)value < 0;

    if (array_type.kind == TypeTuple) {
        analyze_tuple_index(analyzer, ast, array_type, value);

        return;
    }

    if (!is_contiguous_type(array_type.kind)) {
        semantic_error("Indexing requires an array", ast->index.token);
    }

    if (!compatible || get_reference_type(ast->index.index) != RefNone) {
        semantic_error("Array index requires usize", ast->index.token);
    }

    const ArrayType* info = array_type.array;
    ast->index.type_id = info->element_type;

    if (is_slice_type(array_type.kind)) {
        ast->index.constant = false;

        return;
    }

    if (!ast->index.constant) {
        return;
    }

    if (negative || value > SIZE_MAX || value >= info->count) {
        semantic_error("Array index is out of bounds", ast->index.token);
    }

    ast->index.position = (size_t)value;
}

static void validate_index_mutation(ASTNode* ast)
{
    Type source = get_node_data_type(ast->index.array);

    if (source.kind == TypeStringSlice) {
        semantic_error("Cannot mutate string slice contents", ast->index.token);
    }

    ASTNode* owner = get_root_binding(ast->index.array);
    ReferenceType reference = get_reference_type(owner);
    bool constant = is_variable_definition(owner) && owner->variable_definition.binding_type == BindingConstant;
    bool has_shared_access = owner && *get_shared_access_count(owner);
    ReferenceType source_reference = get_reference_type(ast->index.array);

    if (!owner || reference == RefShared || source_reference == RefShared || constant || has_shared_access) {
        semantic_error("Cannot mutate this array", ast->index.token);
    }

    Type owner_type = get_node_data_type(owner);

    if (owner_type.kind == TypeTuple) {
        mark_address_taken(owner);
    }
}

static void validate_index_assignment_operator(ASTNode* ast)
{
    if (ast->index.operator.type == TokenEqual) {
        return;
    }

    Token operator = ast->index.operator;
    operator.type = get_assignment_binary_token(operator.type);
    Type right = get_node_data_type(ast->index.value);
    Type operand_type;
    Type result = analyze_binary_operand_types(operator, ast->index.type_id, right, &operand_type);

    if (!types_equal(result, ast->index.type_id)) {
        semantic_error("Compound element assignment cannot change element type", operator);
    }
}

void analyze_index_assignment(Analyzer* analyzer, ASTNode* ast)
{
    analyze_index(analyzer, ast);
    validate_index_mutation(ast);
    Type source = get_node_data_type(ast->index.array);

    if (source.kind == TypeTuple) {
        analyze_tuple_index_value(analyzer, ast);
    } else {
        analyze_array_element(analyzer, ast->index.value, ast->index.type_id, ast->index.token);
    }

    validate_index_assignment_operator(ast);
}
