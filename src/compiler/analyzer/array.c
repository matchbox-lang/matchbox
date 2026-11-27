#include "compiler/analyzer/array.h"
#include "common/array_type.h"
#include "common/errors.h"
#include "common/memory.h"
#include "common/slice.h"
#include "common/vector.h"
#include "compiler/analyzer/conversion.h"
#include "compiler/analyzer/expression.h"
#include "compiler/analyzer/ownership.h"
#include "compiler/analyzer/reference.h"
#include "compiler/analyzer/tuple.h"
#include "compiler/analyzer/type_alias.h"
#include "compiler/analyzer/variable.h"
#include "compiler/ast/ast.h"
#include "compiler/ast/binding.h"
#include "compiler/ast/literal.h"
#include "compiler/ast/predicate.h"
#include "compiler/ast/query.h"
#include "compiler/ast/reference.h"
#include "compiler/constant.h"
#include "compiler/diagnostics.h"
#include "runtime/string_object.h"
#include <stdint.h>

static void set_inferred_array_count(Vector* dimensions, size_t index, size_t count, Token token)
{
    if (dimensions->data[index]) {
        return;
    }

    Type count_type = {.kind = TypeUsize};

    dimensions->data[index] = create_integer_node(token, count, count_type);
}

static bool infer_literal_array_counts(ASTNode* initializer, Vector* dimensions, size_t index, size_t depth, Token token)
{
    if (index == depth) {
        return true;
    }

    if (initializer->type != ASTArray || initializer->array.fill) {
        return false;
    }

    size_t count = initializer->array.elements.count;

    if (!count) {
        semantic_error("Cannot infer array length from empty array initializer", token);
    }

    set_inferred_array_count(dimensions, index, count, token);

    ASTNode* first = initializer->array.elements.data[0];

    return infer_literal_array_counts(first, dimensions, index + 1, depth, token);
}

static void infer_typed_array_counts(Type type, Vector* dimensions, size_t index, size_t depth, Token token)
{
    if (index == depth) {
        return;
    }

    if (type.kind != TypeArray) {
        semantic_error("Cannot infer array length from a non-array initializer", token);
    }

    set_inferred_array_count(dimensions, index, type.array->count, token);
    infer_typed_array_counts(type.array->element_type, dimensions, index + 1, depth, token);
}

bool infer_variable_array_count(Analyzer* analyzer, ASTNode* ast)
{
    Type type = ast->variable_definition.type_id;

    if (type.kind != TypeArray || !type.array->dimensions) {
        return false;
    }

    Vector* dimensions = type.array->dimensions;
    size_t depth = dimensions->count;

    while (depth && dimensions->data[depth - 1]) {
        depth--;
    }

    if (!depth) {
        return false;
    }

    ASTNode* initializer = ast->variable_definition.expr;
    Token token = ast->variable_definition.token;

    if (!initializer || is_none(initializer)) {
        semantic_error("Cannot infer array length without an initializer", token);
    }

    bool inferred = infer_literal_array_counts(initializer, dimensions, 0, depth, token);

    if (inferred) {
        return false;
    }

    analyze_node(analyzer, initializer);

    type = get_node_data_type(initializer);

    infer_typed_array_counts(type, dimensions, 0, depth, token);

    return true;
}

static size_t resolve_array_count(Analyzer* analyzer, ASTNode* expression)
{
    if (!expression) {
        fatal_error("Array count must be explicit outside an array declaration");
    }

    analyze_node(analyzer, expression);

    Type type = get_node_data_type(expression);
    uint64_t value = 0;
    bool constant = evaluate_integer_constant(expression, &value);

    if (!constant || !is_integer_type(type.kind) || value > SIZE_MAX || (int64_t)value < 0) {
        fatal_error("Array count must be a nonnegative compile-time integer");
    }

    return (size_t)value;
}

static void resolve_array_dimension(Analyzer* analyzer, Type* type, size_t count, bool slice)
{
    Type element_type = *type;

    if (slice) {
        *type = get_slice_type(&analyzer->array_types, element_type);

        return;
    }

    *type = find_array_type(&analyzer->array_types, element_type, count);

    if (type->kind == TypeUnknown) {
        *type = add_array_type(&analyzer->array_types, element_type, count);
    }
}

static size_t resolve_declared_array_count(Analyzer* analyzer, Type type, size_t index)
{
    if (!index && type.kind == TypeArraySlice) {
        return SIZE_MAX;
    }

    return resolve_array_count(analyzer, type.array->dimensions->data[index]);
}

static void resolve_dimension_counts(Analyzer* analyzer, Type type, Vector* counts)
{
    Vector* dimensions = type.array->dimensions;

    init_vector(counts);

    for (size_t i = 0; i < dimensions->count; i++) {
        size_t count = resolve_declared_array_count(analyzer, type, i);

        push_vector_item(counts, (void*)(uintptr_t)count);
    }
}

static Type resolve_dimension_types(Analyzer* analyzer, Type type, Vector* counts)
{
    Type resolved_type = type.array->element_type;

    resolve_dimensions(analyzer, &resolved_type);

    if (resolved_type.kind == TypeVoid) {
        fatal_error("Invalid array element type");
    }

    for (size_t i = counts->count; i > 0; i--) {
        size_t count = (size_t)(uintptr_t)counts->data[i - 1];
        bool slice = type.kind == TypeArraySlice && i == 1;

        resolve_array_dimension(analyzer, &resolved_type, count, slice);
    }

    return resolved_type;
}

static void free_array_dimensions(const ArrayType* array_type)
{
    Vector* dimensions = array_type->dimensions;

    for (size_t i = 0; i < dimensions->count; i++) {
        free_ast_node(dimensions->data[i]);
    }

    free_vector(dimensions);
    free_memory(dimensions);
    free_memory((void*)array_type);
}

void resolve_dimensions(Analyzer* analyzer, Type* type)
{
    resolve_named_type(analyzer, type, NULL);
    resolve_tuple_type(analyzer, type);

    if (type->kind == TypeStringSlice) {
        *type = get_string_slice_type(&analyzer->array_types);

        return;
    }

    if (!is_contiguous_type(type->kind) || !type->array->dimensions) {
        return;
    }

    Vector counts;

    resolve_dimension_counts(analyzer, *type, &counts);

    Type resolved_type = resolve_dimension_types(analyzer, *type, &counts);

    free_vector(&counts);
    free_array_dimensions(type->array);

    *type = resolved_type;
}

static void analyze_array_element(Analyzer* analyzer, ASTNode* element, Type expected, Token token)
{
    apply_literal_type(element, expected);
    analyze_node(analyzer, element);

    Type actual = get_node_data_type(element);
    ReferenceType reference = get_reference_type(element);
    bool compatible = types_equal(actual, expected) || can_implicitly_widen_type(actual.kind, expected.kind);

    if (actual.kind == TypeUnknown) {
        semantic_error("Array element requires an explicit type", token);
    }

    Vector* references = get_tuple_references(element);

    if (reference != RefNone || references->count || (expected.kind != TypeUnknown && !compatible)
        || actual.kind == TypeVoid) {
        semantic_error("Array element type does not match", token);
    }

    consume_array(element);
}

static bool array_has_nested_elements(Analyzer* analyzer, ASTNode* ast)
{
    if (!ast->array.elements.count) {
        return false;
    }

    ASTNode* element = ast->array.elements.data[0];

    if (element->type == ASTArray) {
        return true;
    }

    if (element->type == ASTFunctionCall) {
        StringView id = get_string_view(element->function_call.id);
        ASTNode* symbol = find_symbol(analyzer, id);
        Type type = get_node_data_type(symbol);

        return is_array_type(type.kind);
    }

    if (element->type != ASTVariable) {
        return false;
    }

    StringView id = get_string_view(element->variable.id);
    ASTNode* symbol = find_symbol(analyzer, id);
    Type type = get_node_data_type(symbol);

    return is_array_type(type.kind);
}

static Type resolve_array_initializer_type(Analyzer* analyzer, ASTNode* ast, size_t* count)
{
    Type declared = ast->array.type_id;

    if (!is_array_type(declared.kind)) {
        return (Type){.kind = TypeUnknown};
    }

    const ArrayType* info = declared.array;
    Type type = info->element_type;
    bool nested = array_has_nested_elements(analyzer, ast);
    ast->array.flat = is_array_type(type.kind) && !nested;
    *count = info->count;

    if (!ast->array.flat) {
        return type;
    }

    type = get_array_leaf_type(declared);
    size_t size = get_type_storage_size(declared);
    size_t leaf_size = get_type_storage_size(type);
    *count = size / leaf_size;

    return type;
}

static Type analyze_array_elements(Analyzer* analyzer, ASTNode* ast, Type expected)
{
    size_t count = ast->array.elements.count;
    size_t start = 0;

    if (count && expected.kind == TypeUnknown) {
        ASTNode* element = ast->array.elements.data[0];

        analyze_array_element(analyzer, element, expected, ast->array.token);

        expected = get_node_data_type(element);
        start = 1;
    }

    for (size_t i = start; i < count; i++) {
        analyze_array_element(analyzer, ast->array.elements.data[i], expected, ast->array.token);
    }

    return expected;
}

static void analyze_array_fill(Analyzer* analyzer, ASTNode* ast)
{
    Token token = ast->array.token;

    resolve_dimensions(analyzer, &ast->array.type_id);

    if (ast->array.elements.count != 1) {
        semantic_error("Array initializer requires exactly one argument", token);
    }

    Type leaf = get_array_leaf_type(ast->array.type_id);

    analyze_array_element(analyzer, ast->array.elements.data[0], leaf, token);
}

void analyze_array(Analyzer* analyzer, ASTNode* ast)
{
    if (ast->array.tuple) {
        analyze_tuple(analyzer, ast);

        return;
    }

    if (ast->array.fill) {
        analyze_array_fill(analyzer, ast);

        return;
    }

    size_t count = ast->array.elements.count;
    size_t expected_count = count;
    Type expected = resolve_array_initializer_type(analyzer, ast, &expected_count);

    if (count != expected_count || (!count && expected.kind == TypeUnknown)) {
        semantic_error("Array initializer count does not match its type", ast->array.token);
    }

    expected = analyze_array_elements(analyzer, ast, expected);

    if (ast->array.type_id.kind != TypeUnknown) {
        return;
    }

    ast->array.type_id = find_array_type(&analyzer->array_types, expected, count);

    if (ast->array.type_id.kind == TypeUnknown) {
        ast->array.type_id = add_array_type(&analyzer->array_types, expected, count);
    }
}

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
    bool compatible = ast->index.constant || can_implicitly_widen_integer(index_type.kind, TypeUsize)
        || (is_integer_type(index_type.kind) && constant_expression);
    bool negative = is_signed_integer_type(index_type.kind) && (int64_t)value < 0;

    if (array_type.kind == TypeTuple) {
        analyze_tuple_index(ast, array_type, value);

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
    bool constant = owner && is_variable_definition(owner) && owner->variable_definition.constant;
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
