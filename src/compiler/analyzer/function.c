#include "compiler/analyzer/function.h"
#include "common/array_type.h"
#include "common/vector.h"
#include "compiler/analyzer/array.h"
#include "compiler/analyzer/builtin.h"
#include "compiler/analyzer/control_flow.h"
#include "compiler/analyzer/conversion.h"
#include "compiler/analyzer/global_access.h"
#include "compiler/analyzer/ownership.h"
#include "compiler/analyzer/reference.h"
#include "compiler/analyzer/variable.h"
#include "compiler/diagnostics.h"
#include "compiler/scope.h"
#include "runtime/string_object.h"

static ASTNode* find_node_return(ASTNode* ast);

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

void analyze_parameter(Analyzer* analyzer, ASTNode* ast)
{
    resolve_dimensions(analyzer, &ast->parameter.type_id);

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

static bool has_required_omitted_parameter(ASTNode* callee, size_t count, size_t param_count)
{
    if (count >= param_count) {
        return false;
    }

    ASTNode* first_omitted = get_vector_at(&callee->function_definition.params, count);

    return !first_omitted->parameter.default_value;
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
    size_t param_count = count_vector(&callee->function_definition.params);

    if (count > param_count || has_required_omitted_parameter(callee, count, param_count)) {
        semantic_token_error("Invalid arguments to function ", token);
    }

    for (size_t i = 0; i < count; i++) {
        ASTNode* arg = get_vector_at(&caller->function_call.args, i);
        ASTNode* param = get_vector_at(&callee->function_definition.params, i);
        apply_argument_literal_type(arg, param->parameter.type_id);

        ReferenceType argument_type = get_reference_type(arg);
        ReferenceType parameter_type = get_reference_type(param);
        bool value_parameter = parameter_type == RefNone;
        bool implicit_access = argument_type == RefNone;
        bool compatible_reference = value_parameter || implicit_access || argument_type == parameter_type
            || (argument_type == RefExclusive && parameter_type == RefShared);
        Type argument_value_type = get_node_data_type(arg);
        Type parameter_value_type = param->parameter.type_id;
        bool compatible_value = types_equal(argument_value_type, parameter_value_type)
            || (argument_type == RefNone && parameter_type == RefNone
                && can_implicitly_widen_type(argument_value_type.kind, parameter_value_type.kind));

        if (!compatible_value || !compatible_reference) {
            semantic_token_error("Invalid arguments to function ", token);
        }
    }
}

static void analyze_call_argument(Analyzer* analyzer, ASTNode* argument, ASTNode* parameter)
{
    if (!parameter) {
        analyze_node(analyzer, argument);

        return;
    }

    Type parameter_type = parameter->parameter.type_id;

    apply_declared_type(argument, parameter_type, RefNone);
    analyze_node(analyzer, argument);

    ReferenceType argument_reference = get_reference_type(argument);
    bool owned_array = is_array_type(parameter_type.kind) && parameter->parameter.reference_type == RefNone;

    if (owned_array && argument_reference != RefNone) {
        semantic_error("Cannot transfer array ownership through a reference", parameter->parameter.token);
    }

    if (parameter->parameter.reference_type == RefNone) {
        consume_array(argument);
    }
}

static void analyze_call_arguments(Analyzer* analyzer, ASTNode* ast)
{
    StringView id = get_string_view(ast->function_call.id);
    ASTNode* symbol = find_symbol(analyzer, id);

    if (!symbol || symbol->type != ASTFunctionDefinition) {
        analyze_expression_nodes(analyzer, &ast->function_call.args);

        return;
    }

    for (size_t i = 0; i < ast->function_call.args.count; i++) {
        ASTNode* parameter = get_vector_at(&symbol->function_definition.params, i);

        analyze_call_argument(analyzer, ast->function_call.args.data[i], parameter);
    }
}

void analyze_function_call(Analyzer* analyzer, ASTNode* ast)
{
    infer_conversion_literal(ast);
    analyze_call_arguments(analyzer, ast);

    TypeKind called_type = get_token_data_type(ast->function_call.token.type);

    if (is_integer_type(called_type) || is_float_type(called_type) || called_type == TypeChar) {
        analyze_type_conversion(ast);

        return;
    }

    StringView id = get_string_view(ast->function_call.id);
    ASTNode* symbol = find_symbol(analyzer, id);
    
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

static ASTNode* find_function_return(Vector* statements)
{
    size_t count = count_vector(statements);

    for (size_t i = 0; i < count; i++) {
        ASTNode* statement = get_vector_at(statements, i);
        ASTNode* result = find_node_return(statement);

        if (result) {
            return result;
        }
    }

    return NULL;
}

static ASTNode* find_conditional_return(ASTNode* ast)
{
    ASTNode* result = find_node_return(ast->conditional.then_branch);

    if (result) {
        return result;
    }

    return find_node_return(ast->conditional.else_branch);
}

static ASTNode* find_match_return(ASTNode* ast)
{
    size_t count = count_vector(&ast->match.arms);

    for (size_t i = 0; i < count; i++) {
        MatchArm* arm = get_vector_at(&ast->match.arms, i);
        ASTNode* result = find_node_return(arm->branch);

        if (result) {
            return result;
        }
    }

    return NULL;
}

static ASTNode* find_node_return(ASTNode* ast)
{
    if (!ast) {
        return NULL;
    }

    switch (ast->type) {
        case ASTReturn:
            return ast;
        case ASTCompound:
            return find_function_return(&ast->compound.statements);
        case ASTFor:
            return find_node_return(ast->for_statement.body);
        case ASTWhile:
            return find_node_return(ast->while_statement.body);
        case ASTConditional:
            return find_conditional_return(ast);
        case ASTMatch:
            return find_match_return(ast);
        default:
            return NULL;
    }
}

static Type get_function_return_value_type(Vector* statements)
{
    ASTNode* result = find_function_return(statements);

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
        position += get_reference_type(param) == RefNone ? get_type_slot_count(get_node_data_type(param).kind) : 1;
    }
}

static void resolve_function_return_value_type(ASTNode* ast)
{
    Type type = get_function_return_value_type(&ast->function_definition.body->compound.statements);

    if (ast->function_definition.has_explicit_return_type) {
        validate_function_return_value_type(ast, type);
        return;
    }

    ast->function_definition.return_type_id = type;
}

static void validate_parameter_order(ASTNode* parameter, bool* has_default)
{
    if (parameter->parameter.default_value) {
        *has_default = true;

        return;
    }

    if (*has_default) {
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

void analyze_function(Analyzer* analyzer, ASTNode* ast)
{
    resolve_dimensions(analyzer, &ast->function_definition.return_type_id);

    if (ast->function_definition.is_test && analyzer->current_scope != analyzer->top_level->compound.scope) {
        semantic_error("Test blocks are only allowed at file scope", ast->function_definition.token);
    }

    StringView id = get_string_view(ast->function_definition.id);

    if (get_local_symbol(analyzer->current_scope, id)) {
        redefinition_error("Function", ast->function_definition.token);
    }

    Vector array_states;

    capture_ownership_states(&array_states, analyzer->current_scope);

    Scope* parent = analyzer->current_scope;
    ASTNode* previous_function = analyzer->function;
    LoopPath* previous_loop = analyzer->loop_path;
    Scope* scope = create_scope(parent);
    scope->frame_scope = scope;
    scope->local_offset = 0;
    scope->max_local_count = 0;
    ast->function_definition.scope = scope;
    ast->function_definition.body->compound.scope = scope;
    analyzer->current_scope = scope;
    analyzer->function = ast;
    analyzer->loop_path = NULL;

    set_local_symbol(scope, id, ast);
    analyze_parameters(analyzer, &ast->function_definition.params);
    set_parameter_positions(ast);
    set_local_symbol(parent, id, ast);

    FunctionPath path = {ast, analyzer->function_path};
    analyzer->function_path = &path;

    analyze_expression_nodes(analyzer, &ast->function_definition.body->compound.statements);

    analyzer->function_path = path.previous;
    analyzer->current_scope = parent;
    analyzer->function = previous_function;
    analyzer->loop_path = previous_loop;

    resolve_function_return_value_type(ast);
    record_function_array_moves(ast, &array_states);
}

static bool can_return_reference_origin(ASTNode* origin)
{
    if (!origin) {
        return false;
    }

    if (is_top_level_scope(get_scope(origin))) {
        return true;
    }

    return is_parameter(origin) && get_reference_type(origin) != RefNone;
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

    if (!count) {
        ASTNode* origin = get_reference_origin_from_expression(ast->return_statement.expr);
        track_return_origin(analyzer, ast, origin);

        return;
    }

    for (size_t i = 0; i < count; i++) {
        track_return_origin(analyzer, ast, get_vector_at(origins, i));
    }
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
    consume_array(ast->return_statement.expr);
    record_array_return_initializations(analyzer);

    if (validate_return_reference_type(analyzer, ast) == RefNone) {
        return;
    }

    track_return_reference_origin(analyzer, ast);
}
