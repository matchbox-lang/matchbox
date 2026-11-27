#include "compiler/analyzer/reference.h"
#include "common/array_type.h"
#include "common/slice.h"
#include "common/tuple_type.h"
#include "common/vector.h"
#include "compiler/analyzer/global_access.h"
#include "compiler/analyzer/ownership.h"
#include "compiler/analyzer/tuple.h"
#include "compiler/analyzer/variable.h"
#include "compiler/ast/binding.h"
#include "compiler/ast/predicate.h"
#include "compiler/ast/query.h"
#include "compiler/ast/reference.h"
#include "compiler/diagnostics.h"
#include "compiler/scope.h"
#include "runtime/string_object.h"

static void validate_slice_lifetime(ASTNode* destination, ASTNode* origin)
{
    Type type = get_node_data_type(destination);
    Scope* scope = get_scope(destination);

    if (!is_slice_type(type.kind) || !scope || !origin) {
        return;
    }

    origin = get_reference_origin_or_self(origin);
    Scope* storage = get_scope(origin);

    if (storage && storage->level > scope->level) {
        semantic_error("Slice cannot outlive its underlying storage", destination->variable_definition.token);
    }
}

void set_reference_origins_from_expression(ASTNode* destination, ASTNode* expression)
{
    Vector* destination_origins = get_reference_origins(destination);
    Vector* origins = get_reference_origins_from_expression(expression);

    copy_reference_origins(destination_origins, origins);
    set_tuple_binding_references(destination, expression);

    for (size_t i = 0; i < origins->count; i++) {
        validate_slice_lifetime(destination, origins->data[i]);
    }
}

void release_reference_access(ASTNode* reference)
{
    if (reference->tuple_references.count && get_reference_type(reference) == RefNone) {
        release_tuple_access(reference);

        return;
    }

    Vector* origins = get_reference_origins(reference);
    size_t count = count_vector(origins);

    for (size_t i = 0; i < count; i++) {
        ASTNode* origin = get_vector_at(origins, i);

        if (reference->variable_definition.reference_type == RefShared) {
            (*get_shared_access_count(origin))--;
        } else {
            set_exclusive_access(origin, false);
        }
    }

    reference->variable_definition.reference_access_active = false;
}

void release_inactive_reference_access(Vector* references)
{
    size_t count = count_vector(references);

    for (size_t i = 0; i < count; i++) {
        ASTNode* ast = get_vector_at(references, i);

        if (ast->variable_definition.reference_access_active) {
            release_reference_access(ast);
        }
    }
}

static ASTNode* get_reference_variable(ASTNode* ast)
{
    if (!is_variable(ast->prefix.expr)) {
        semantic_error("References require a binding", ast->prefix.operator);
    }

    return ast->prefix.expr;
}

static ASTNode* find_reference_symbol(Analyzer* analyzer, ASTNode* variable)
{
    StringView id = get_string_view(variable->variable.id);
    ASTNode* symbol = find_symbol(analyzer, id);

    if (!symbol || !is_variable_type(symbol)) {
        symbol_error(" is undefined", variable->variable.token);
    }

    if (!is_initialized(symbol)) {
        symbol_error(" is uninitialized", variable->variable.token);
    }

    Vector* moves = get_tuple_moves(symbol);

    if (is_moved(symbol) || moves->count) {
        symbol_error(" is moved", variable->variable.token);
    }

    return symbol;
}

void validate_reference_binding_access(ASTNode* symbol, ReferenceType requested, Token token)
{
    Token binding;

    if (is_parameter(symbol)) {
        binding = symbol->parameter.token;
    } else {
        binding = symbol->variable_definition.token;
    }

    binding.line = token.line;
    binding.column = token.column;

    if (requested == RefShared && has_exclusive_access(symbol)) {
        binding_access_error("Cannot share binding ", binding, " while exclusive access is active");
    }

    if (requested == RefExclusive && (has_exclusive_access(symbol) || *get_shared_access_count(symbol))) {
        binding_access_error("Cannot take exclusive access to binding ", binding, " while another access is active");
    }
}

static void validate_reference_access(ASTNode* ast, ASTNode* variable, ASTNode* symbol)
{
    ReferenceType requested = get_reference_type(ast);
    ReferenceType existing = get_reference_type(symbol);

    validate_constant_mutation(symbol, requested, variable->variable.token);

    if (existing != RefNone) {
        semantic_error("Cannot create a reference to another reference", ast->prefix.operator);
    }

    validate_reference_binding_access(symbol, requested, variable->variable.token);
}

static void resolve_string_reference_type(Analyzer* analyzer, ASTNode* ast)
{
    Type type = get_node_data_type(ast);

    if (type.kind != TypeString) {
        return;
    }

    type = get_string_slice_type(&analyzer->array_types);

    convert_string_reference_to_slice(ast, type);
}

void analyze_reference(Analyzer* analyzer, ASTNode* ast)
{
    ASTNode* variable = get_reference_variable(ast);
    ASTNode* symbol = find_reference_symbol(analyzer, variable);

    validate_constant_binding(analyzer, symbol, variable->variable.token);
    validate_constant_reference(analyzer, symbol, variable->variable.token);
    validate_reference_access(ast, variable, symbol);
    mark_address_taken(symbol);

    variable->variable.scope = analyzer->current_scope;
    variable->variable.symbol = symbol;
    Vector* origins = get_reference_origins(ast);
    
    add_reference_origin(origins, symbol);
    track_variable_read(symbol);
    resolve_string_reference_type(analyzer, ast);
}

static bool reference_argument_requires_binding(ReferenceType type)
{
    return type == RefExclusive;
}

void validate_call_argument_access(Analyzer* analyzer, ASTNode* caller, ASTNode* callee, Token token)
{
    size_t count = count_vector(&caller->function_call.args);

    for (size_t i = 0; i < count; i++) {
        ASTNode* param = get_call_parameter(&callee->function_definition.params, i);
        ReferenceType type = get_parameter_argument_reference(param);

        if (type == RefNone) {
            continue;
        }

        ASTNode* arg = get_vector_at(&caller->function_call.args, i);

        if (get_reference_type(arg) != RefNone) {
            continue;
        }

        ASTNode* origin = get_argument_reference_origin(arg);

        if (!origin && reference_argument_requires_binding(type)) {
            semantic_token_error("Reference access requires a binding for ", token);
        }

        validate_global_binding_access(analyzer, origin, type, token);
        validate_constant_reference(analyzer, origin, token);
        mark_address_taken(origin);
    }
}

static ReferenceType get_argument_reference_type(ASTNode* argument, ASTNode* parameter)
{
    ReferenceType type = get_reference_type(argument);

    if (type != RefNone) {
        return type;
    }

    ReferenceType parameter_type = get_parameter_argument_reference(parameter);
    Type value_type = get_node_data_type(argument);

    if (parameter_type == RefNone && is_owned_type(value_type)) {
        return RefExclusive;
    }

    return parameter_type;
}

static bool tuple_reference_conflicts(const TupleReference* access, ASTNode* origin, ReferenceType reference)
{
    bool exclusive = reference == RefExclusive || access->reference == RefExclusive;

    return access->origin == origin && exclusive;
}

static bool tuple_argument_conflicts(ASTNode* origin, ReferenceType reference, ASTNode* argument)
{
    Vector* references = get_tuple_references(argument);
    size_t index = 0;

    while (index < references->count && !tuple_reference_conflicts(references->data[index], origin, reference)) {
        index++;
    }

    return index < references->count;
}

static bool reference_access_conflicts(ASTNode* left_origin, ReferenceType left_type,
    ASTNode* right, ASTNode* right_parameter)
{
    if (tuple_argument_conflicts(left_origin, left_type, right)) {
        return true;
    }

    ReferenceType right_type = get_argument_reference_type(right, right_parameter);

    if (left_type != RefExclusive && right_type != RefExclusive) {
        return false;
    }

    Vector* right_origins = get_reference_origins_from_expression(right);
    if (contains_reference_origin(right_origins, left_origin)) {
        return true;
    }

    ASTNode* right_origin = get_argument_reference_origin(right);

    return left_origin == right_origin;
}

static void validate_argument_origin_access(Vector* arguments, Vector* parameters,
    size_t position, ASTNode* left_origin, ReferenceType left_type, Token token)
{
    size_t count = count_vector(arguments);

    for (size_t i = position + 1; i < count; i++) {
        ASTNode* right = get_vector_at(arguments, i);
        ASTNode* right_parameter = get_call_parameter(parameters, i);

        if (!reference_access_conflicts(left_origin, left_type, right, right_parameter)) {
            continue;
        }

        semantic_token_error("Conflicting reference access in call ", token);
    }
}

static void validate_tuple_argument_access(Vector* arguments, Vector* parameters, size_t position, Token token)
{
    ASTNode* argument = get_vector_at(arguments, position);
    Vector* references = get_tuple_references(argument);

    for (size_t i = 0; i < references->count; i++) {
        TupleReference* access = references->data[i];

        validate_argument_origin_access(arguments, parameters, position, access->origin, access->reference, token);
    }
}

static void validate_argument_access(Vector* arguments, Vector* parameters, size_t position, Token token)
{
    ASTNode* left = get_vector_at(arguments, position);
    ASTNode* left_parameter = get_call_parameter(parameters, position);
    ReferenceType left_type = get_argument_reference_type(left, left_parameter);

    validate_tuple_argument_access(arguments, parameters, position, token);

    if (left_type == RefNone) {
        return;
    }

    Vector* origins = get_reference_origins_from_expression(left);
    size_t count = count_vector(origins);

    for (size_t i = 0; i < count; i++) {
        ASTNode* origin = get_vector_at(origins, i);
        validate_argument_origin_access(arguments, parameters, position, origin, left_type, token);
    }

    if (count) {
        return;
    }

    ASTNode* origin = get_argument_reference_origin(left);
    if (origin) {
        validate_argument_origin_access(arguments, parameters, position, origin, left_type, token);
    }
}

void validate_argument_accesses(ASTNode* caller, ASTNode* callee)
{
    Vector* arguments = &caller->function_call.args;
    Vector* parameters = &callee->function_definition.params;
    size_t count = count_vector(arguments);

    for (size_t i = 0; i < count; i++) {
        validate_argument_access(arguments, parameters, i, caller->function_call.token);
    }
}

static void transfer_returned_slice(ASTNode* caller, ASTNode* argument)
{
    Type type = get_node_data_type(caller);

    if (!is_slice_type(type.kind) || get_reference_type(caller) != RefExclusive) {
        return;
    }

    if (argument->type == ASTSlice) {
        argument = argument->slice.expr;
    }

    if (is_variable(argument) && get_reference_type(argument) == RefExclusive) {
        transfer_exclusive_reference(argument);
    }
}

void set_call_reference_origins(ASTNode* caller, ASTNode* callee)
{
    set_tuple_call_references(caller, callee);

    ASTNode* returned = callee->function_definition.return_reference_origin;

    if (!returned || !is_parameter(returned)) {
        Vector* origins = get_reference_origins(caller);
        add_reference_origin(origins, returned);

        return;
    }

    size_t count = count_vector(&callee->function_definition.params);

    for (size_t i = 0; i < count; i++) {
        if (get_call_parameter(&callee->function_definition.params, i) != returned) {
            continue;
        }

        ASTNode* argument = get_vector_at(&caller->function_call.args, i);
        set_reference_origins_from_expression(caller, argument);
        transfer_returned_slice(caller, argument);

        return;
    }
}

void transfer_exclusive_reference(ASTNode* expression)
{
    ASTNode* source = expression->variable.symbol;
    set_moved_state(source, true);

    if (!is_variable_definition(source)) {
        return;
    }

    source->variable_definition.reference_access_active = false;
}

void activate_reference_access(ASTNode* ast)
{
    Vector* origins = get_reference_origins(ast);
    size_t count = count_vector(origins);

    for (size_t i = 0; i < count; i++) {
        ASTNode* origin = get_vector_at(origins, i);

        if (ast->variable_definition.reference_type == RefShared) {
            (*get_shared_access_count(origin))++;
        } else {
            set_exclusive_access(origin, true);
        }
    }
}

void establish_reference_access(ASTNode* binding, ASTNode* expression)
{
    if (binding->tuple_references.count && get_reference_type(binding) == RefNone) {
        activate_tuple_access(binding);

        return;
    }

    if (expression->type == ASTSlice) {
        expression = expression->slice.expr;
    }

    bool tuple_reference = get_reference_type(binding) == RefExclusive && expression->type == ASTIndex;

    if (tuple_reference) {
        consume_tuple_element(expression);
    }

    bool transfers_exclusive = get_reference_type(binding) == RefExclusive && is_variable(expression)
        && get_reference_type(expression) == RefExclusive;

    if (transfers_exclusive) {
        transfer_exclusive_reference(expression);
    } else {
        activate_reference_access(binding);
    }

    binding->variable_definition.reference_access_active = true;
}

void update_reference_binding(ASTNode* ast, ASTNode* symbol, bool initializes_binding)
{
    Type type = get_node_data_type(symbol);

    if (ast->assignment.operator.type != TokenEqual
        || (get_reference_type(symbol) == RefNone && type.kind != TypeTuple)) {
        return;
    }

    set_reference_origins_from_expression(symbol, ast->assignment.expr);
    establish_reference_access(symbol, ast->assignment.expr);

    ast->assignment.initializes_binding = initializes_binding;
}
