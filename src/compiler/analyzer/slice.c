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

static ASTNode* get_slice_origin(ASTNode* expression, ASTNode* binding, Token token)
{
    ASTNode* origin = get_reference_origin_from_expression(expression);

    if (!origin && binding) {
        origin = get_reference_origin_or_self(binding);
    }

    if (!origin) {
        semantic_error("Slice requires underlying storage with a binding", token);
    }

    return origin;
}

static void validate_slice_storage_access(Analyzer* analyzer, ASTNode* ast, ASTNode* origin, ReferenceType existing)
{
    ReferenceType requested = ast->slice.reference_type;

    validate_constant_mutation(origin, requested, ast->slice.token);
    validate_constant_reference(analyzer, origin, ast->slice.token);

    if (existing == RefNone) {
        validate_global_binding_access(analyzer, origin, requested, ast->slice.token);
    }
}

static void validate_exclusive_slice_access(ASTNode* ast, ASTNode* binding, ASTNode* origin, ReferenceType existing)
{
    if (ast->slice.reference_type != RefExclusive) {
        return;
    }

    if (binding && *get_shared_access_count(binding)) {
        semantic_error("Cannot create an exclusive slice while shared access is active", ast->slice.token);
    }

    if (existing == RefShared) {
        semantic_error("Cannot create an exclusive slice through shared access", ast->slice.token);
    }

    if (existing == RefNone && (*get_shared_access_count(origin) || has_exclusive_access(origin))) {
        semantic_error("Cannot create an exclusive slice while another access is active", ast->slice.token);
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

    validate_slice_storage_access(analyzer, ast, origin, existing);
    validate_exclusive_slice_access(ast, binding, origin, existing);
    set_slice_reference_origins(ast, binding, origin, existing);
}

static Type analyze_slice_source(Analyzer* analyzer, ASTNode* ast)
{
    analyze_node(analyzer, ast->slice.expr);

    Type source = get_node_data_type(ast->slice.expr);

    if (!is_contiguous_type(source.kind)) {
        semantic_error("Slice requires an array or slice", ast->slice.token);
    }

    return source;
}

static void resolve_slice_type(Analyzer* analyzer, ASTNode* ast)
{
    Type source = analyze_slice_source(analyzer, ast);
    Type type = get_slice_type(&analyzer->array_types, source.array->element_type);

    if (ast->slice.type_id.kind != TypeUnknown && !types_equal(ast->slice.type_id, type)) {
        semantic_error("Slice element type does not match", ast->slice.token);
    }

    ast->slice.type_id = type;
}

void analyze_slice(Analyzer* analyzer, ASTNode* ast)
{
    resolve_slice_type(analyzer, ast);
    validate_slice_access(analyzer, ast);
    analyze_slice_bound(analyzer, ast->slice.start, ast->slice.token);
    analyze_slice_bound(analyzer, ast->slice.end, ast->slice.token);
}
