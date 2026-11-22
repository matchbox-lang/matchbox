#include "compiler/analyzer/slice.h"
#include "common/array_type.h"
#include "common/slice.h"
#include "compiler/analyzer/conversion.h"
#include "compiler/analyzer/global_access.h"
#include "compiler/analyzer/reference.h"
#include "compiler/analyzer/variable.h"
#include "compiler/diagnostics.h"

static void analyze_slice_bound(Analyzer* analyzer, ASTNode* bound, Token token)
{
    if (!bound) {
        return;
    }

    apply_literal_type(bound, (Type){.kind = TypeUsize});
    analyze_node(analyzer, bound);

    Type type = get_node_data_type(bound);
    bool compatible = can_implicitly_widen_integer(type.kind, TypeUsize);

    if (!compatible || get_reference_type(bound) != RefNone) {
        semantic_error("Slice bound requires usize", token);
    }
}

static ASTNode* get_slice_binding(ASTNode* expression)
{
    if (expression->type == ASTIndex) {
        return get_slice_binding(expression->index.array);
    }

    if (is_variable(expression)) {
        return expression->variable.symbol;
    }

    return NULL;
}

static bool has_static_string_storage(ASTNode* expression)
{
    if (expression->type == ASTString) {
        return true;
    }

    Type type = get_node_data_type(expression);
    Vector* origins = get_reference_origins_from_expression(expression);

    return type.kind == TypeStringSlice && !origins->count;
}

static ASTNode* get_slice_origin(ASTNode* expression, ASTNode* binding, Token token)
{
    ASTNode* origin = get_reference_origin_from_expression(expression);

    if (!origin && has_static_string_storage(expression)) {
        return NULL;
    }

    if (!origin && binding) {
        origin = get_reference_origin_or_self(binding);
    }

    if (!origin) {
        semantic_error("Slice requires underlying storage with a binding", token);
    }

    return origin;
}

static void validate_exclusive_slice_access(ASTNode* ast, ASTNode* binding, ASTNode* origin, ReferenceType existing)
{
    if (ast->slice.reference_type != RefExclusive) {
        return;
    }

    if (!origin) {
        semantic_error("Exclusive slice requires underlying storage with a binding", ast->slice.token);
    }

    if (binding && *get_shared_access_count(binding)) {
        validate_reference_binding_access(binding, RefExclusive, ast->slice.token);
    }

    if (existing == RefShared) {
        semantic_error("Cannot create an exclusive slice through shared access", ast->slice.token);
    }

    if (existing == RefNone) {
        validate_reference_binding_access(origin, RefExclusive, ast->slice.token);
    }
}

static void validate_slice_storage_access(Analyzer* analyzer, ASTNode* ast, ASTNode* binding,
    ASTNode* origin, ReferenceType existing)
{
    ReferenceType requested = ast->slice.reference_type;

    validate_constant_mutation(origin, requested, ast->slice.token);
    validate_constant_reference(analyzer, origin, ast->slice.token);
    validate_exclusive_slice_access(ast, binding, origin, existing);

    if (existing == RefNone) {
        validate_global_binding_access(analyzer, origin, requested, ast->slice.token);
    }
}

static void set_slice_reference_origins(ASTNode* ast, ASTNode* binding, ASTNode* origin, ReferenceType existing)
{
    Vector* origins = get_reference_origins(ast);
    Vector* source = get_reference_origins_from_expression(ast->slice.expr);

    merge_reference_origins(origins, source);
    add_reference_origin(origins, origin);

    if (ast->slice.reference_type == RefShared && existing == RefExclusive && binding) {
        add_reference_origin(origins, binding);
    }
}

static void validate_slice_access(Analyzer* analyzer, ASTNode* ast)
{
    ASTNode* expression = ast->slice.expr;
    ASTNode* binding = get_slice_binding(expression);
    ReferenceType existing = get_reference_type(binding ? binding : expression);
    ASTNode* origin = get_slice_origin(expression, binding, ast->slice.token);

    validate_slice_storage_access(analyzer, ast, binding, origin, existing);
    set_slice_reference_origins(ast, binding, origin, existing);
}

static bool remove_incompatible_string_slice(ASTNode* ast, Type source)
{
    if (ast->slice.start || ast->slice.type_id.kind != TypeStringSlice) {
        return false;
    }

    bool string_source = source.kind == TypeString || source.kind == TypeStringSlice;
    ReferenceType reference = get_reference_type(ast->slice.expr);
    bool shared_to_exclusive = reference == RefShared && ast->slice.reference_type == RefExclusive;

    if (string_source && !shared_to_exclusive) {
        return false;
    }

    ASTNode* expression = ast->slice.expr;

    free_vector(&ast->slice.reference_origins);
    replace_ast_node(ast, expression);

    return true;
}

static Type get_slice_source_type(Analyzer* analyzer, Type source)
{
    if (source.kind == TypeString || source.kind == TypeStringSlice) {
        return get_string_slice_type(&analyzer->array_types);
    }

    return get_slice_type(&analyzer->array_types, source.array->element_type);
}

static bool resolve_slice_type(Analyzer* analyzer, ASTNode* ast)
{
    analyze_node(analyzer, ast->slice.expr);

    Type source = get_node_data_type(ast->slice.expr);

    if (remove_incompatible_string_slice(ast, source)) {
        return false;
    }

    if (!is_contiguous_type(source.kind) && source.kind != TypeString) {
        semantic_error("Slice requires an array, string, or slice", ast->slice.token);
    }

    Type type = get_slice_source_type(analyzer, source);

    if (ast->slice.type_id.kind != TypeUnknown && !types_equal(ast->slice.type_id, type)) {
        semantic_error("Slice element type does not match", ast->slice.token);
    }

    ast->slice.type_id = type;

    return true;
}

void analyze_slice(Analyzer* analyzer, ASTNode* ast)
{
    if (!resolve_slice_type(analyzer, ast)) {
        return;
    }

    validate_slice_access(analyzer, ast);
    analyze_slice_bound(analyzer, ast->slice.start, ast->slice.token);
    analyze_slice_bound(analyzer, ast->slice.end, ast->slice.token);
}
