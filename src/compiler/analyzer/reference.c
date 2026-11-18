#include "compiler/analyzer/reference.h"
#include "common/array_type.h"
#include "common/vector.h"
#include "compiler/analyzer/global_access.h"
#include "compiler/analyzer/ownership.h"
#include "compiler/analyzer/variable.h"
#include "compiler/diagnostics.h"
#include "runtime/string_object.h"

size_t* get_shared_access_count(ASTNode* symbol)
{
    if (is_parameter(symbol)) {
        return &symbol->parameter.shared_access_count;
    }

    return &symbol->variable_definition.shared_access_count;
}

bool has_exclusive_access(ASTNode* symbol)
{
    if (is_parameter(symbol)) {
        return symbol->parameter.exclusive_access_active;
    }

    return symbol->variable_definition.exclusive_access_active;
}

void set_exclusive_access(ASTNode* symbol, bool active)
{
    if (is_parameter(symbol)) {
        symbol->parameter.exclusive_access_active = active;

        return;
    }

    symbol->variable_definition.exclusive_access_active = active;
}

ASTNode* get_reference_origin_or_self(ASTNode* symbol)
{
    ASTNode* origin = get_reference_origin(symbol);

    if (origin) {
        return origin;
    }

    return symbol;
}

ASTNode* reference_origin_from_expression(ASTNode* ast)
{
    if (ast->type == ASTPrefix && is_variable(ast->prefix.expr)) {
        ASTNode* symbol = ast->prefix.expr->variable.symbol;

        return get_reference_origin_or_self(symbol);
    }

    return get_reference_origin(ast);
}

Vector* reference_origins_from_expression(ASTNode* ast)
{
    Vector* origins = get_reference_origins(ast);

    if (origins) {
        return origins;
    }

    static Vector empty;

    return &empty;
}

bool contains_reference_origin(Vector* origins, ASTNode* origin)
{
    size_t count = count_vector(origins);

    for (size_t i = 0; i < count; i++) {
        if (get_vector_at(origins, i) == origin) {
            return true;
        }
    }

    return false;
}

void add_reference_origin(Vector* origins, ASTNode* origin)
{
    if (!origin || contains_reference_origin(origins, origin)) {
        return;
    }

    push_vector_item(origins, origin);
}

void copy_reference_origins(Vector* destination, Vector* source)
{
    if (destination == source) {
        return;
    }

    destination->count = 0;

    merge_reference_origins(destination, source);
}

void merge_reference_origins(Vector* destination, Vector* source)
{
    size_t count = count_vector(source);

    for (size_t i = 0; i < count; i++) {
        add_reference_origin(destination, get_vector_at(source, i));
    }
}

void set_reference_origins_from_expression(ASTNode* destination, ASTNode* expression)
{
    Vector* destination_origins = get_reference_origins(destination);
    Vector* origins = reference_origins_from_expression(expression);
    copy_reference_origins(destination_origins, origins);
}

void release_reference_access(ASTNode* reference)
{
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

    if (is_moved(symbol)) {
        symbol_error(" is moved", variable->variable.token);
    }

    return symbol;
}

static void validate_reference_access(ASTNode* ast, ASTNode* variable, ASTNode* symbol)
{
    ReferenceType requested = get_reference_type(ast);
    ReferenceType existing = get_reference_type(symbol);

    validate_constant_mutation(symbol, requested, variable->variable.token);

    if (existing != RefNone) {
        semantic_error("Cannot create a reference to another reference", ast->prefix.operator);
    }

    if (requested == RefShared && has_exclusive_access(symbol)) {
        binding_access_error("Cannot share binding ", variable->variable.token, " while exclusive access is active");
    }

    if (requested == RefExclusive
        && (has_exclusive_access(symbol) || *get_shared_access_count(symbol))) {
        binding_access_error("Cannot take exclusive access to binding ", variable->variable.token, " while another access is active");
    }
}

static void mark_address_taken(ASTNode* symbol)
{
    if (!symbol || !is_variable_definition(symbol)) {
        return;
    }

    symbol->variable_definition.address_taken = true;
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
}

static bool reference_argument_requires_binding(ReferenceType type)
{
    return type == RefExclusive;
}

static ASTNode* get_argument_reference_origin(ASTNode* argument)
{
    ASTNode* origin = reference_origin_from_expression(argument);

    if (origin || !is_variable(argument)) {
        return origin;
    }

    return get_reference_origin_or_self(argument->variable.symbol);
}

void validate_call_argument_access(Analyzer* analyzer, ASTNode* caller, ASTNode* callee, Token token)
{
    size_t count = count_vector(&caller->function_call.args);

    for (size_t i = 0; i < count; i++) {
        ASTNode* param = get_vector_at(&callee->function_definition.params, i);
        ReferenceType type = get_reference_type(param);

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

    ReferenceType parameter_type = get_reference_type(parameter);
    Type value_type = get_node_data_type(argument);

    if (parameter_type == RefNone && is_array_type(value_type.kind)) {
        return RefExclusive;
    }

    return parameter_type;
}

static bool reference_access_conflicts(ASTNode* left_origin, ReferenceType left_type,
    ASTNode* right, ASTNode* right_parameter)
{
    ReferenceType right_type = get_argument_reference_type(right, right_parameter);

    if (left_type != RefExclusive && right_type != RefExclusive) {
        return false;
    }

    Vector* right_origins = reference_origins_from_expression(right);
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
        ASTNode* right_parameter = get_vector_at(parameters, i);

        if (!reference_access_conflicts(left_origin, left_type, right, right_parameter)) {
            continue;
        }

        semantic_token_error("Conflicting reference access in call ", token);
    }
}

static void validate_argument_access(Vector* arguments, Vector* parameters, size_t position, Token token)
{
    ASTNode* left = get_vector_at(arguments, position);
    ASTNode* left_parameter = get_vector_at(parameters, position);
    ReferenceType left_type = get_argument_reference_type(left, left_parameter);

    if (left_type == RefNone) {
        return;
    }

    Vector* origins = reference_origins_from_expression(left);
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

void set_call_reference_origins(ASTNode* caller, ASTNode* callee)
{
    ASTNode* returned = callee->function_definition.return_reference_origin;

    if (!returned || !is_parameter(returned)) {
        Vector* origins = get_reference_origins(caller);
        add_reference_origin(origins, returned);

        return;
    }

    size_t count = count_vector(&callee->function_definition.params);

    for (size_t i = 0; i < count; i++) {
        if (get_vector_at(&callee->function_definition.params, i) != returned) {
            continue;
        }

        ASTNode* argument = get_vector_at(&caller->function_call.args, i);
        set_reference_origins_from_expression(caller, argument);

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
    bool transfers_exclusive = get_reference_type(binding) == RefExclusive && is_variable(expression);

    if (transfers_exclusive) {
        transfer_exclusive_reference(expression);
    } else {
        activate_reference_access(binding);
    }

    binding->variable_definition.reference_access_active = true;
}

void update_reference_binding(ASTNode* ast, ASTNode* symbol, bool initializes_binding)
{
    if (ast->assignment.operator.type != TokenEqual || get_reference_type(symbol) == RefNone) {
        return;
    }

    set_reference_origins_from_expression(symbol, ast->assignment.expr);

    establish_reference_access(symbol, ast->assignment.expr);

    ast->assignment.initializes_binding = initializes_binding;
}
