#include "compiler/analyzer/function.h"
#include "common/array_type.h"
#include "common/slice.h"
#include "common/vector.h"
#include "compiler/analyzer/array.h"
#include "compiler/analyzer/builtin.h"
#include "compiler/analyzer/control_flow.h"
#include "compiler/analyzer/conversion.h"
#include "compiler/analyzer/generic.h"
#include "compiler/analyzer/global_access.h"
#include "compiler/analyzer/ownership.h"
#include "compiler/analyzer/reference.h"
#include "compiler/analyzer/slice.h"
#include "compiler/analyzer/variable.h"
#include "compiler/ast/control_flow.h"
#include "compiler/ast/generic.h"
#include "compiler/ast/literal.h"
#include "compiler/ast/predicate.h"
#include "compiler/ast/query.h"
#include "compiler/ast/reference.h"
#include "compiler/diagnostics.h"
#include "compiler/scope.h"
#include "runtime/string_object.h"

static _Noreturn void no_matching_overload_error(Token token, Token* uninferred)
{
    if (!uninferred) {
        semantic_token_error("No compatible overload for function ", token);
    }

    fatal_source_error(token.line, token.column, "Cannot infer type parameter %.*s for function %.*s",
        (int)uninferred->length, uninferred->chars, (int)token.length, token.chars);
}

static _Noreturn void return_outside_function_error(Token token)
{
    fatal_source_error(token.line, token.column, "Return statement is only valid inside a function");
}

static void analyze_parameter_default(Analyzer* analyzer, ASTNode* ast)
{
    ASTNode* value = ast->parameter.default_value;

    if (!value) {
        return;
    }

    if (ast->parameter.reference_type != RefNone) {
        semantic_token_error("Reference parameter cannot have a default: ", ast->parameter.token);
    }

    Scope* previous_scope = analyzer->current_scope;
    analyzer->current_scope = previous_scope->parent;
    apply_declared_type(value, ast->parameter.type_id, RefNone);
    analyze_node(analyzer, value);
    analyzer->current_scope = previous_scope;

    Type source = get_node_data_type(value);
    Type destination = ast->parameter.type_id;
    bool compatible = types_equal(source, destination) || can_implicitly_widen_type(source.kind, destination.kind);

    if (!compatible) {
        semantic_token_error("Default value type does not match parameter ", ast->parameter.token);
    }
}

static void resolve_variadic_parameter(Analyzer* analyzer, ASTNode* ast)
{
    if (!ast->parameter.variadic) {
        return;
    }

    Type element = ast->parameter.type_id;
    ReferenceType reference = ast->parameter.reference_type;

    if (ast->parameter.default_value || element.kind == TypeVoid || reference == RefExclusive
        || (reference != RefNone && !is_slice_type(element.kind))) {
        semantic_token_error("Invalid variadic element type or default: ", ast->parameter.token);
    }

    ast->parameter.type_id = get_slice_type(&analyzer->array_types, element);
    ast->parameter.reference_type = RefShared;
}

void resolve_function_signature(Analyzer* analyzer, ASTNode* ast)
{
    if (ast->function_definition.signature_resolved) {
        return;
    }

    resolve_dimensions(analyzer, &ast->function_definition.return_type_id);

    Vector* params = &ast->function_definition.params;

    for (size_t i = 0; i < params->count; i++) {
        ASTNode* parameter = params->data[i];

        resolve_dimensions(analyzer, &parameter->parameter.type_id);
        resolve_variadic_parameter(analyzer, parameter);
    }

    ast->function_definition.signature_resolved = true;
}

void analyze_parameter(Analyzer* analyzer, ASTNode* ast)
{
    StringView id = get_string_view(ast->parameter.id);

    if (get_local_symbol(analyzer->current_scope, id)) {
        redefinition_error("Parameter", ast->parameter.token);
    }

    ast->parameter.scope = analyzer->current_scope;

    analyze_parameter_default(analyzer, ast);

    if (get_reference_type(ast) != RefNone) {
        Vector* origins = get_reference_origins(ast);
        add_reference_origin(origins, ast);
    }

    set_local_symbol(analyzer->current_scope, id, ast);
}

static void apply_argument_literal_type(ASTNode* argument, Type type)
{
    if (argument->type == ASTArray) {
        return;
    }

    apply_literal_type(argument, type);
}

static void validate_function_call(ASTNode* caller, ASTNode* callee, Token token)
{
    size_t count = count_vector(&caller->function_call.args);

    if (!function_accepts_argument_count(callee, count)) {
        semantic_token_error("Invalid arguments to function ", token);
    }

    for (size_t i = 0; i < count; i++) {
        ASTNode* arg = get_vector_at(&caller->function_call.args, i);
        ASTNode* param = get_call_parameter(&callee->function_definition.params, i);
        Type parameter_value_type = get_parameter_argument_type(param);

        apply_argument_literal_type(arg, parameter_value_type);

        ReferenceType argument_type = get_reference_type(arg);
        ReferenceType parameter_type = get_parameter_argument_reference(param);
        bool value_parameter = parameter_type == RefNone;
        bool implicit_access = argument_type == RefNone;
        bool compatible_reference = value_parameter || implicit_access || argument_type == parameter_type
            || (argument_type == RefExclusive && parameter_type == RefShared);
        Type argument_value_type = get_node_data_type(arg);
        bool compatible_value = types_equal(argument_value_type, parameter_value_type)
            || (argument_type == RefNone && parameter_type == RefNone
                && can_implicitly_widen_type(argument_value_type.kind, parameter_value_type.kind));

        if (!compatible_value || !compatible_reference) {
            semantic_token_error("Invalid arguments to function ", token);
        }
    }
}

static void finish_call_argument(ASTNode* argument, ASTNode* parameter)
{
    Type type = get_parameter_argument_type(parameter);
    ReferenceType reference = get_parameter_argument_reference(parameter);
    ReferenceType argument_reference = get_reference_type(argument);
    bool owned_array = is_array_type(type.kind) && reference == RefNone;

    if (owned_array && argument_reference != RefNone) {
        semantic_error("Cannot transfer array ownership through a reference", parameter->parameter.token);
    }

    if (reference == RefNone && !parameter->parameter.variadic) {
        consume_array(argument);
    }
}

static void analyze_call_argument(Analyzer* analyzer, ASTNode* argument, ASTNode* parameter)
{
    if (!parameter) {
        analyze_node(analyzer, argument);

        return;
    }

    Type parameter_type = get_parameter_argument_type(parameter);
    ReferenceType parameter_reference = get_parameter_argument_reference(parameter);
    ReferenceType reference = is_slice_type(parameter_type.kind) ? parameter_reference : RefNone;

    apply_declared_type(argument, parameter_type, reference);
    analyze_node(analyzer, argument);

    finish_call_argument(argument, parameter);
}

static size_t get_argument_reference_cost(ASTNode* argument, ASTNode* parameter)
{
    ReferenceType argument_reference = get_reference_type(argument);
    ReferenceType parameter_reference = get_parameter_argument_reference(parameter);
    bool compatible = parameter_reference == RefNone || argument_reference == RefNone
        || argument_reference == parameter_reference
        || (argument_reference == RefExclusive && parameter_reference == RefShared);

    if (!compatible) {
        return SIZE_MAX;
    }

    return argument_reference != parameter_reference;
}

static size_t get_overload_value_cost(ASTNode* argument, ASTNode* parameter, size_t reference_cost)
{
    Type source = get_node_data_type(argument);
    Type type = get_parameter_argument_type(parameter);

    if (types_equal(source, type)) {
        return reference_cost;
    }

    if (is_slice_type(type.kind) && can_slice_type(source, type)) {
        return 1 + reference_cost;
    }

    ReferenceType argument_reference = get_reference_type(argument);
    ReferenceType parameter_reference = get_parameter_argument_reference(parameter);
    bool literal = is_numeric_literal(argument);

    if (argument_reference != RefNone || (parameter_reference != RefNone && !literal)) {
        return SIZE_MAX;
    }

    return get_argument_conversion_cost(argument, type);
}

static size_t get_overload_argument_cost(ASTNode* argument, ASTNode* parameter)
{
    size_t reference_cost = get_argument_reference_cost(argument, parameter);

    if (reference_cost == SIZE_MAX) {
        return SIZE_MAX;
    }

    return get_overload_value_cost(argument, parameter, reference_cost);
}

static size_t add_overload_argument_cost(size_t cost, ASTNode* argument, ASTNode* parameter, size_t count)
{
    size_t argument_cost = get_overload_argument_cost(argument, parameter);

    if (argument_cost == SIZE_MAX) {
        return SIZE_MAX;
    }

    if (!argument_cost) {
        return cost;
    }

    return cost + count * MAX_IMPLICIT_CONVERSION_COST + argument_cost;
}

static size_t get_overload_cost(ASTNode* caller, ASTNode* callee)
{
    size_t count = caller->function_call.args.count;
    Vector* params = &callee->function_definition.params;

    if (!function_accepts_argument_count(callee, count)) {
        return SIZE_MAX;
    }

    size_t cost = 0;

    for (size_t i = 0; i < count && cost != SIZE_MAX; i++) {
        ASTNode* parameter = get_call_parameter(params, i);
        cost = add_overload_argument_cost(cost, caller->function_call.args.data[i], parameter, count);
    }

    return cost;
}

static ASTNode* consider_function_overload(Analyzer* analyzer, ASTNode* caller, ASTNode* candidate, ASTNode* best,
    size_t* best_cost, bool* ambiguous, Token** uninferred)
{
    candidate = prepare_generic_candidate(analyzer, caller, candidate, uninferred);

    if (!candidate) {
        return best;
    }

    size_t cost = get_overload_cost(caller, candidate);

    if (cost == SIZE_MAX || cost > *best_cost) {
        discard_generic_candidate(candidate);

        return best;
    }

    bool generic = is_function_specialization(candidate);
    bool best_generic = best && is_function_specialization(best);

    if (cost == *best_cost && generic && !best_generic) {
        discard_generic_candidate(candidate);

        return best;
    }

    *ambiguous = cost == *best_cost && generic == best_generic;
    *best_cost = cost;

    discard_generic_candidate(best);

    return candidate;
}

static ASTNode* resolve_function_overload(Analyzer* analyzer, ASTNode* caller, ASTNode* symbol)
{
    ASTNode* best = NULL;
    size_t best_cost = SIZE_MAX;
    bool ambiguous = false;
    Token* uninferred = NULL;

    for (ASTNode* candidate = symbol; candidate; candidate = candidate->function_definition.next_overload) {
        best = consider_function_overload(analyzer, caller, candidate, best, &best_cost, &ambiguous, &uninferred);
    }

    if (!best) {
        no_matching_overload_error(caller->function_call.token, uninferred);
    }

    if (ambiguous) {
        semantic_token_error("Ambiguous overload for function ", caller->function_call.token);
    }

    return best;
}

static void apply_overload_argument(Analyzer* analyzer, ASTNode* argument, ASTNode* parameter)
{
    Type type = get_parameter_argument_type(parameter);
    ReferenceType reference = get_parameter_argument_reference(parameter);
    Type source = get_node_data_type(argument);

    if (is_slice_type(type.kind) && !types_equal(source, type)) {
        apply_declared_type(argument, type, reference);
        analyze_slice_argument(analyzer, argument);
    }

    apply_analyzed_argument_type(argument, type);
    finish_call_argument(argument, parameter);
}

static void prepare_call_argument(Analyzer* analyzer, ASTNode* argument, ASTNode* parameter, bool overloaded)
{
    if (overloaded) {
        apply_overload_argument(analyzer, argument, parameter);

        return;
    }

    analyze_call_argument(analyzer, argument, parameter);
}

static Type get_function_return_value_type(ASTNode* body)
{
    ASTNode* result = find_node_return(body);

    if (!result) {
        return (Type){.kind = TypeVoid};
    }

    return get_node_data_type(result->return_statement.expr);
}

static void validate_function_return_value_type(ASTNode* ast, Type type)
{
    Type return_type = ast->function_definition.return_type_id;

    bool value_return = ast->function_definition.return_reference_type == RefNone;

    if (!types_equal(return_type, type)
        && (!value_return || !can_implicitly_widen_type(type.kind, return_type.kind))) {
        semantic_token_error("Invalid return type for function ", ast->function_definition.token);
    }
}

static void set_parameter_positions(ASTNode* ast)
{
    size_t count = count_vector(&ast->function_definition.params);
    size_t position = 0;

    for (size_t i = 0; i < count; i++) {
        ASTNode* param = get_vector_at(&ast->function_definition.params, i);
        param->parameter.position = position;
        Type type = get_node_data_type(param);
        bool value = get_reference_type(param) == RefNone || is_slice_type(type.kind);
        position += value ? get_type_slot_count(type.kind) : 1;
    }
}

static void merge_function_return_value_type(ASTNode* function, Type type)
{
    Type previous = function->function_definition.return_type_id;

    if (function->function_definition.has_explicit_return_type) {
        validate_function_return_value_type(function, type);

        return;
    }

    if (previous.kind == TypeUnknown) {
        function->function_definition.return_type_id = type;

        return;
    }

    TypeKind common = get_common_numeric_type(previous.kind, type.kind);

    if (common != TypeUnknown) {
        function->function_definition.return_type_id = (Type){.kind = common};

        return;
    }

    validate_function_return_value_type(function, type);
}

static bool merge_function_return(ASTNode* ast, void* context)
{
    ASTNode* function = context;
    Type type = get_node_data_type(ast->return_statement.expr);

    merge_function_return_value_type(function, type);

    return true;
}

static void prepare_function_return_value_type(ASTNode* ast)
{
    Type type = get_function_return_value_type(ast->function_definition.body);

    if (ast->function_definition.has_explicit_return_type) {
        validate_function_return_value_type(ast, type);

        return;
    }

    if (is_function_specialization(ast) && ast->function_definition.return_type_id.kind != TypeUnknown) {
        return;
    }

    ast->function_definition.return_type_id = type;
}

static void resolve_function_return_value_type(ASTNode* ast)
{
    prepare_function_return_value_type(ast);
    visit_node_returns(ast->function_definition.body, merge_function_return, ast);

    if (ast->function_definition.return_type_id.kind == TypeUnknown) {
        semantic_token_error("Cannot infer recursive return type for function ", ast->function_definition.token);
    }
}

static void validate_parameter_order(ASTNode* parameter, bool* has_default)
{
    if (parameter->parameter.default_value) {
        *has_default = true;

        return;
    }

    if (*has_default && !parameter->parameter.variadic) {
        semantic_token_error("Required parameter cannot follow a default parameter: ", parameter->parameter.token);
    }
}

static void analyze_parameters(Analyzer* analyzer, Vector* parameters)
{
    bool has_default = false;

    for (size_t i = 0; i < parameters->count; i++) {
        ASTNode* parameter = parameters->data[i];

        validate_parameter_order(parameter, &has_default);
        analyze_node(analyzer, parameter);
    }
}

static void validate_overload_signature(ASTNode* ast, ASTNode* previous)
{
    ASTNode* candidate = previous;

    while (candidate && !function_signatures_equal(ast, candidate)) {
        candidate = candidate->function_definition.next_overload;
    }

    if (candidate) {
        redefinition_error("Function signature", ast->function_definition.token);
    }
}

void register_function_overload(Scope* scope, ASTNode* ast)
{
    StringView id = get_string_view(ast->function_definition.id);
    ASTNode* previous = get_local_symbol(scope, id);

    if (previous && previous->type != ASTFunctionDefinition) {
        redefinition_error("Function", ast->function_definition.token);
    }

    validate_overload_signature(ast, previous);

    if (previous) {
        ast->function_definition.next_overload = previous->function_definition.next_overload;
        previous->function_definition.next_overload = ast;
    }

    set_local_symbol(scope, id, ast);
}

static ASTNode* validate_function_declaration(Analyzer* analyzer, ASTNode* ast)
{
    if (ast->function_definition.is_test && analyzer->current_scope != analyzer->top_level->compound.scope) {
        semantic_error("Test blocks are only allowed at file scope", ast->function_definition.token);
    }

    StringView id = get_string_view(ast->function_definition.id);
    ASTNode* previous = get_local_symbol(analyzer->current_scope, id);

    if (previous && previous->type != ASTFunctionDefinition) {
        redefinition_error("Function", ast->function_definition.token);
    }

    if (is_function_specialization(ast)) {
        return NULL;
    }

    return previous;
}

static void enter_function_scope(Analyzer* analyzer, ASTNode* ast)
{
    Scope* scope = create_scope(analyzer->current_scope);
    scope->frame_scope = scope;
    scope->local_offset = 0;
    scope->max_local_count = 0;

    ast->function_definition.scope = scope;
    ast->function_definition.body->compound.scope = scope;

    analyzer->current_scope = scope;
    analyzer->function = ast;
    analyzer->loop_path = NULL;
}

static void prepare_function_parameters(Analyzer* analyzer, ASTNode* ast, ASTNode* previous)
{
    StringView id = get_string_view(ast->function_definition.id);
    ASTNode* symbol = previous ? previous : ast;

    if (is_function_specialization(ast)) {
        symbol = ast->function_definition.generics->template;
    }

    set_local_symbol(analyzer->current_scope, id, symbol);
    analyze_parameters(analyzer, &ast->function_definition.params);
    set_parameter_positions(ast);

    if (is_function_specialization(ast)) {
        return;
    }

    register_function_overload(analyzer->current_scope->parent, ast);
}

static void analyze_function_body(Analyzer* analyzer, ASTNode* ast)
{
    FunctionPath path = {ast, analyzer->function_path};
    analyzer->function_path = &path;

    analyze_expression_nodes(analyzer, &ast->function_definition.body->compound.statements);

    analyzer->function_path = path.previous;
}

static void analyze_scoped_function(Analyzer* analyzer, ASTNode* ast, ASTNode* previous)
{
    Scope* parent = analyzer->current_scope;
    ASTNode* previous_function = analyzer->function;
    LoopPath* previous_loop = analyzer->loop_path;

    enter_function_scope(analyzer, ast);
    prepare_function_parameters(analyzer, ast, previous);
    analyze_function_body(analyzer, ast);

    analyzer->current_scope = parent;
    analyzer->function = previous_function;
    analyzer->loop_path = previous_loop;
}

void analyze_function(Analyzer* analyzer, ASTNode* ast)
{
    resolve_function_signature(analyzer, ast);

    if (is_generic_function(ast)) {
        register_generic_function(analyzer, ast);

        return;
    }

    ASTNode* previous = validate_function_declaration(analyzer, ast);
    Vector array_states;

    capture_ownership_states(&array_states, analyzer->current_scope);
    analyze_scoped_function(analyzer, ast, previous);
    resolve_function_return_value_type(ast);
    record_function_array_moves(ast, &array_states);
}

static void analyze_function_specialization(Analyzer* analyzer, ASTNode* ast)
{
    if (!is_function_specialization(ast) || ast->function_definition.scope) {
        return;
    }

    Scope* previous = analyzer->current_scope;
    Scope* constant_scope = analyzer->constant_scope;
    FunctionPath* constant_functions = analyzer->constant_functions;
    Vector states;

    capture_ownership_states(&states, previous);

    ASTNode* template = ast->function_definition.generics->template;

    if (!ast->function_definition.generics->cached) {
        push_vector_item(&template->function_definition.generics->specializations->compound.statements, ast);

        ast->function_definition.generics->cached = true;
    }

    analyzer->constant_scope = NULL;
    analyzer->constant_functions = NULL;
    analyzer->current_scope = template->function_definition.generics->declaration_scope;

    analyze_function(analyzer, ast);

    ast->function_definition.generics->analyzed = true;

    apply_ownership_states(&states);
    free_ownership_states(&states);

    analyzer->current_scope = previous;
    analyzer->constant_scope = constant_scope;
    analyzer->constant_functions = constant_functions;
}

static ASTNode* analyze_call_arguments(Analyzer* analyzer, ASTNode* ast)
{
    StringView id = get_string_view(ast->function_call.id);
    ASTNode* symbol = find_symbol(analyzer, id);

    if (!symbol || symbol->type != ASTFunctionDefinition) {
        analyze_expression_nodes(analyzer, &ast->function_call.args);

        return symbol;
    }

    bool overloaded = symbol->function_definition.next_overload != NULL
        || is_generic_function(symbol) || ast->function_call.type_args.count != 0;

    if (overloaded) {
        analyze_expression_nodes(analyzer, &ast->function_call.args);
        symbol = resolve_function_overload(analyzer, ast, symbol);
        analyze_function_specialization(analyzer, symbol);
    }

    for (size_t i = 0; i < ast->function_call.args.count; i++) {
        ASTNode* parameter = get_call_parameter(&symbol->function_definition.params, i);
        ASTNode* argument = ast->function_call.args.data[i];

        prepare_call_argument(analyzer, argument, parameter, overloaded);
    }

    return symbol;
}

static void resolve_call_type_arguments(Analyzer* analyzer, ASTNode* ast)
{
    Vector* types = &ast->function_call.type_args;

    for (size_t i = 0; i < types->count; i++) {
        resolve_dimensions(analyzer, types->data[i]);
    }
}

void analyze_function_call(Analyzer* analyzer, ASTNode* ast)
{
    resolve_call_type_arguments(analyzer, ast);
    infer_conversion_literal(ast);

    ASTNode* symbol = analyze_call_arguments(analyzer, ast);
    TypeKind called_type = get_token_data_type(ast->function_call.token.type);

    if (is_integer_type(called_type) || is_float_type(called_type) || called_type == TypeChar) {
        analyze_type_conversion(ast);

        return;
    }

    if (!symbol && ast->function_call.type_args.count) {
        semantic_token_error("Type arguments require a generic function: ", ast->function_call.token);
    }

    if (!symbol) {
        analyze_builtin_call(analyzer, ast);
        return;
    }

    if (!is_function_definition(symbol)) {
        symbol_error(" is undefined", ast->function_call.token);
    }

    ast->function_call.scope = analyzer->current_scope;
    ast->function_call.symbol = symbol;

    validate_function_call(ast, symbol, ast->function_call.token);
    validate_call_argument_access(analyzer, ast, symbol, ast->function_call.token);
    validate_argument_accesses(ast, symbol);
    validate_function_global_access(analyzer, symbol, true);
    set_call_reference_origins(ast, symbol);
}

static bool can_return_reference_origin(ASTNode* origin)
{
    if (!origin) {
        return false;
    }

    if (is_top_level_scope(get_scope(origin))) {
        return true;
    }

    return is_parameter(origin) && !origin->parameter.variadic && get_reference_type(origin) != RefNone;
}

static ReferenceType get_expected_return_reference_type(Analyzer* analyzer)
{
    if (!analyzer->function) {
        return RefNone;
    }

    return analyzer->function->function_definition.return_reference_type;
}

static void validate_return_scope(Analyzer* analyzer, ASTNode* ast)
{
    if (analyzer->current_scope->level < 2) {
        return_outside_function_error(ast->return_statement.token);
    }
}

static ReferenceType validate_return_reference_type(Analyzer* analyzer, ASTNode* ast)
{
    ReferenceType expected = get_expected_return_reference_type(analyzer);

    if (get_reference_type(ast->return_statement.expr) != expected) {
        semantic_token_error("Invalid reference return from function ", ast->return_statement.token);
    }

    return expected;
}

static void track_return_origin(Analyzer* analyzer, ASTNode* ast, ASTNode* origin)
{
    if (origin) {
        origin = get_reference_origin_or_self(origin);
    }

    if (!can_return_reference_origin(origin)) {
        semantic_token_error("Reference to local binding cannot escape function ", ast->return_statement.token);
    }

    ASTNode* function = analyzer->function;
    ASTNode* previous = function->function_definition.return_reference_origin;

    if (previous && previous != origin) {
        semantic_token_error("Reference returns must share one lifetime in function ", ast->return_statement.token);
    }

    function->function_definition.return_reference_origin = origin;
}

static void track_return_reference_origin(Analyzer* analyzer, ASTNode* ast)
{
    Vector* origins = get_reference_origins_from_expression(ast->return_statement.expr);
    size_t count = count_vector(origins);

    Type type = get_node_data_type(ast->return_statement.expr);

    if (!count && type.kind == TypeStringSlice && get_reference_type(ast->return_statement.expr) == RefShared) {
        return;
    }

    if (!count) {
        ASTNode* origin = get_reference_origin_from_expression(ast->return_statement.expr);
        track_return_origin(analyzer, ast, origin);

        return;
    }

    for (size_t i = 0; i < count; i++) {
        track_return_origin(analyzer, ast, get_vector_at(origins, i));
    }
}

static void resolve_specialized_return_type(ASTNode* function, ASTNode* expression)
{
    if (!is_function_specialization(function)) {
        return;
    }

    Type type = get_node_data_type(expression);

    merge_function_return_value_type(function, type);
}

void analyze_return(Analyzer* analyzer, ASTNode* ast)
{
    validate_return_scope(analyzer, ast);

    ASTNode* function = analyzer->function;

    if (function->function_definition.has_explicit_return_type) {
        apply_declared_type(ast->return_statement.expr, function->function_definition.return_type_id,
            function->function_definition.return_reference_type);
    }

    analyze_node(analyzer, ast->return_statement.expr);
    resolve_specialized_return_type(function, ast->return_statement.expr);
    consume_array(ast->return_statement.expr);
    record_array_return_initializations(analyzer);

    if (validate_return_reference_type(analyzer, ast) == RefNone) {
        return;
    }

    track_return_reference_origin(analyzer, ast);
}
