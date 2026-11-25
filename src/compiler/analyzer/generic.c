#include "compiler/analyzer/generic.h"
#include "common/array_type.h"
#include "common/memory.h"
#include "compiler/analyzer/array.h"
#include "compiler/analyzer/function.h"
#include "compiler/ast/generic.h"
#include "compiler/ast/query.h"
#include "compiler/parser/function.h"
#include "compiler/diagnostics.h"
#include "compiler/scope.h"
#include "runtime/string_object.h"

static bool has_named_generic_parameter(ASTNode* ast)
{
    Vector* names = &ast->function_definition.generics->bindings.names;
    bool named = false;

    for (size_t i = 0; i < names->count; i++) {
        Token* name = names->data[i];
        named |= name->length != 0;
    }

    return named;
}

static void analyze_generic_default(Analyzer* analyzer, ASTNode* parameter, bool defer_defaults)
{
    if (defer_defaults || !parameter->parameter.default_value) {
        return;
    }

    analyze_node(analyzer, parameter->parameter.default_value);
}

static void validate_generic_parameter(ASTNode* parameter, Scope* scope, bool* has_default)
{
    StringView id = get_string_view(parameter->parameter.id);

    if (get_local_symbol(scope, id)) {
        redefinition_error("Parameter", parameter->parameter.token);
    }

    if (*has_default && !parameter->parameter.default_value && !parameter->parameter.variadic) {
        semantic_token_error("Required parameter cannot follow a default parameter: ", parameter->parameter.token);
    }

    *has_default |= parameter->parameter.default_value != NULL;

    set_local_symbol(scope, id, parameter);
}

void register_generic_function(Analyzer* analyzer, ASTNode* ast)
{
    Scope* parameters = create_scope(NULL);
    Vector* params = &ast->function_definition.params;
    bool has_default = false;
    bool defer_defaults = has_named_generic_parameter(ast);

    for (size_t i = 0; i < params->count; i++) {
        ASTNode* parameter = params->data[i];

        validate_generic_parameter(parameter, parameters, &has_default);
        analyze_generic_default(analyzer, parameter, defer_defaults);
    }

    free_scope(parameters);

    ast->function_definition.generics->declaration_scope = analyzer->current_scope;

    register_function_overload(analyzer->current_scope, ast);
}

static bool bind_type_argument(Type actual, Type* type)
{
    if (type->kind == TypeUnknown) {
        *type = actual;
    }

    return types_equal(*type, actual);
}

static bool can_infer_element_type(Type pattern, Type actual)
{
    return is_contiguous_type(pattern.kind) && is_contiguous_type(actual.kind) && pattern.array && actual.array;
}

static bool infer_type_argument(Type pattern, Type actual, Vector* types, size_t explicit_count)
{
    if (pattern.kind == TypeGeneric && pattern.generic_index < explicit_count) {
        return true;
    }

    if (pattern.kind == TypeGeneric) {
        return bind_type_argument(actual, types->data[pattern.generic_index]);
    }

    if (!can_infer_element_type(pattern, actual)) {
        return true;
    }

    return infer_type_argument(pattern.array->element_type, actual.array->element_type, types, explicit_count);
}

static void free_type_arguments(Vector* types)
{
    for (size_t i = 0; i < types->count; i++) {
        free_memory(types->data[i]);
    }

    free_vector(types);
}

static Type* copy_call_type_argument(Vector* explicit, size_t index)
{
    Type* type = allocate_zeroed_memory(1, sizeof(Type));

    if (index < explicit->count) {
        *type = *(Type*)explicit->data[index];
    }

    return type;
}

static void create_type_arguments(ASTNode* caller, ASTNode* candidate, Vector* types)
{
    Vector* explicit = &caller->function_call.type_args;
    size_t count = candidate->function_definition.generics->bindings.names.count;

    init_vector(types);

    for (size_t i = 0; i < count; i++) {
        Type* type = copy_call_type_argument(explicit, i);

        push_vector_item(types, type);
    }
}

static bool infer_argument(ASTNode* caller, ASTNode* candidate, size_t index, Vector* types)
{
    ASTNode* parameter = get_call_parameter(&candidate->function_definition.params, index);

    if (!parameter) {
        return false;
    }

    Type pattern = get_parameter_argument_type(parameter);
    Type actual = get_node_data_type(caller->function_call.args.data[index]);

    return infer_type_argument(pattern, actual, types, caller->function_call.type_args.count);
}

static bool has_unbound_type_argument(Type pattern, Vector* types)
{
    if (pattern.kind == TypeGeneric) {
        Type* type = types->data[pattern.generic_index];

        return type->kind == TypeUnknown;
    }

    if (is_contiguous_type(pattern.kind) && pattern.array) {
        return has_unbound_type_argument(pattern.array->element_type, types);
    }

    return false;
}

static Type analyze_default_type(Analyzer* analyzer, ASTNode* candidate, ASTNode* value)
{
    Scope* previous = analyzer->current_scope;
    analyzer->current_scope = candidate->function_definition.generics->declaration_scope;

    analyze_node(analyzer, value);

    analyzer->current_scope = previous;

    return get_node_data_type(value);
}

static bool infer_default_argument(Analyzer* analyzer, ASTNode* candidate, size_t index, Vector* types)
{
    ASTNode* parameter = candidate->function_definition.params.data[index];
    ASTNode* value = parameter->parameter.default_value;

    if (!value) {
        return true;
    }

    Type pattern = get_parameter_argument_type(parameter);

    if (!has_unbound_type_argument(pattern, types)) {
        return true;
    }

    Type actual = analyze_default_type(analyzer, candidate, value);

    return infer_type_argument(pattern, actual, types, 0);
}

static bool is_concrete_type_argument(Type* type)
{
    return type->kind != TypeUnknown && type->kind != TypeVoid && type->kind != TypeGeneric;
}

static bool validate_inferred_type_argument(ASTNode* candidate, Vector* types, size_t index, Token** uninferred)
{
    Type* type = types->data[index];

    if (type->kind == TypeUnknown && !*uninferred) {
        *uninferred = candidate->function_definition.generics->bindings.names.data[index];
    }

    return is_concrete_type_argument(type);
}

static bool infer_type_arguments(Analyzer* analyzer, ASTNode* caller, ASTNode* candidate, Vector* types,
    Token** uninferred)
{
    size_t index = 0;
    size_t count = caller->function_call.args.count;
    bool valid = true;

    while (index < count && valid) {
        valid = infer_argument(caller, candidate, index, types);
        index++;
    }

    count = candidate->function_definition.params.count;

    while (index < count && valid) {
        valid = infer_default_argument(analyzer, candidate, index, types);
        index++;
    }

    index = 0;

    while (index < types->count && valid) {
        valid = validate_inferred_type_argument(candidate, types, index, uninferred);
        index++;
    }

    return valid;
}

static bool type_arguments_equal(Type* left, Type* right)
{
    return types_equal(*left, *right);
}

static bool specialization_matches(ASTNode* specialization, Vector* types)
{
    Vector* previous = &specialization->function_definition.generics->bindings.arguments;
    size_t index = 0;

    while (index < types->count && type_arguments_equal(previous->data[index], types->data[index])) {
        index++;
    }

    return index == types->count;
}

static ASTNode* find_specialization(ASTNode* candidate, Vector* types)
{
    Vector* specializations = &candidate->function_definition.generics->specializations->compound.statements;
    size_t index = 0;

    while (index < specializations->count && !specialization_matches(specializations->data[index], types)) {
        index++;
    }

    return get_vector_at(specializations, index);
}

ASTNode* prepare_generic_candidate(Analyzer* analyzer, ASTNode* caller, ASTNode* candidate, Token** uninferred)
{
    if (!is_generic_function(candidate)) {
        return caller->function_call.type_args.count ? NULL : candidate;
    }

    if (caller->function_call.type_args.count > candidate->function_definition.generics->bindings.names.count) {
        return NULL;
    }

    if (!function_accepts_argument_count(candidate, caller->function_call.args.count)) {
        return NULL;
    }

    Vector types;

    create_type_arguments(caller, candidate, &types);

    if (!infer_type_arguments(analyzer, caller, candidate, &types, uninferred)) {
        free_type_arguments(&types);

        return NULL;
    }

    ASTNode* specialization = find_specialization(candidate, &types);

    if (specialization) {
        free_type_arguments(&types);

        return specialization;
    }

    specialization = parse_function_specialization(candidate, &types);

    resolve_function_signature(analyzer, specialization);

    return specialization;
}

void discard_generic_candidate(ASTNode* ast)
{
    if (!ast || !is_function_specialization(ast) || ast->function_definition.generics->cached) {
        return;
    }

    free_ast_node(ast);
}
