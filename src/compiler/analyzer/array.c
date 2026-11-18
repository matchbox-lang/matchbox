#include "compiler/analyzer/array.h"
#include "common/array_type.h"
#include "common/errors.h"
#include "common/memory.h"
#include "common/vector.h"
#include "compiler/analyzer/conversion.h"
#include "compiler/analyzer/ownership.h"
#include "compiler/analyzer/reference.h"
#include "compiler/analyzer/variable.h"
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

void resolve_dimensions(Analyzer* analyzer, Type* type)
{
    if (type->kind != TypeArray || !type->array->dimensions) {
        return;
    }

    const ArrayType* array_type = type->array;
    Vector* dimensions = array_type->dimensions;
    Type resolved_type = array_type->element_type;
    Vector counts;

    init_vector(&counts);

    for (size_t i = 0; i < dimensions->count; i++) {
        size_t count = resolve_array_count(analyzer, dimensions->data[i]);

        push_vector_item(&counts, (void*)(uintptr_t)count);
    }

    if (resolved_type.kind == TypeVoid || is_array_type(resolved_type.kind)) {
        fatal_error("Invalid array element type");
    }

    for (size_t i = counts.count; i > 0; i--) {
        size_t count = (size_t)(uintptr_t)counts.data[i - 1];
        resolved_type = get_array_type(&analyzer->array_types, resolved_type, count);
    }

    free_vector(&counts);

    for (size_t i = 0; i < dimensions->count; i++) {
        free_ast_node(dimensions->data[i]);
    }

    free_vector(dimensions);
    free_memory(dimensions);
    free_memory((void*)array_type);

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

    if (reference != RefNone || (expected.kind != TypeUnknown && !compatible) || actual.kind == TypeVoid) {
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

static Type get_array_initializer_type(Analyzer* analyzer, ASTNode* ast, size_t* count)
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
    size_t size = get_array_storage_size(declared);
    size_t leaf_size = get_array_storage_size(type);
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
    if (ast->array.fill) {
        analyze_array_fill(analyzer, ast);

        return;
    }

    size_t count = ast->array.elements.count;
    size_t expected_count = count;
    Type expected = get_array_initializer_type(analyzer, ast, &expected_count);

    if (count != expected_count || (!count && expected.kind == TypeUnknown)) {
        semantic_error("Array initializer count does not match its type", ast->array.token);
    }

    expected = analyze_array_elements(analyzer, ast, expected);

    if (ast->array.type_id.kind == TypeUnknown) {
        ast->array.type_id = get_array_type(&analyzer->array_types, expected, count);
    }
}

static ASTNode* get_index_owner(ASTNode* ast)
{
    if (ast->type == ASTIndex) {
        return get_index_owner(ast->index.array);
    }

    if (ast->type == ASTVariable) {
        return ast->variable.symbol;
    }

    return NULL;
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

void analyze_index(Analyzer* analyzer, ASTNode* ast)
{
    Type literal_type = get_index_literal_type(ast->index.index);

    analyze_node(analyzer, ast->index.array);
    apply_literal_type(ast->index.index, literal_type);
    analyze_node(analyzer, ast->index.index);

    Type array_type = get_node_data_type(ast->index.array);
    Type index_type = get_node_data_type(ast->index.index);
    uint64_t value = 0;
    ast->index.constant = evaluate_integer_constant(ast->index.index, &value);
    ASTNode* constant_expression = get_index_constant_expression(ast->index.index);
    bool compatible = ast->index.constant || can_implicitly_widen_integer(index_type.kind, TypeUsize)
        || (is_integer_type(index_type.kind) && constant_expression);
    bool negative = is_signed_integer_type(index_type.kind) && (int64_t)value < 0;

    if (!is_array_type(array_type.kind)) {
        semantic_error("Indexing requires an array", ast->index.token);
    }

    if (!compatible || get_reference_type(ast->index.index) != RefNone) {
        semantic_error("Array index requires usize", ast->index.token);
    }

    const ArrayType* info = array_type.array;
    ast->index.type_id = info->element_type;

    if (!ast->index.constant) {
        return;
    }

    if (negative || value > SIZE_MAX || value >= info->count) {
        semantic_error("Array index is out of bounds", ast->index.token);
    }

    ast->index.position = (size_t)value;
}

void analyze_index_assignment(Analyzer* analyzer, ASTNode* ast)
{
    analyze_index(analyzer, ast);

    ASTNode* owner = get_index_owner(ast->index.array);
    ReferenceType reference = get_reference_type(owner);
    bool constant = owner && is_variable_definition(owner) && owner->variable_definition.constant;
    bool borrowed = owner && reference == RefNone && *get_shared_access_count(owner);

    if (!owner || reference == RefShared || constant || borrowed) {
        semantic_error("Cannot mutate this array", ast->index.token);
    }

    if (ast->index.operator.type != TokenEqual) {
        semantic_error("Invalid operator for array element assignment", ast->index.operator);
    }

    analyze_array_element(analyzer, ast->index.value, ast->index.type_id, ast->index.token);
}
