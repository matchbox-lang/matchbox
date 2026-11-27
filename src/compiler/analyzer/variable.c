#include "compiler/analyzer/variable.h"
#include "common/array_type.h"
#include "common/tuple_type.h"
#include "compiler/analyzer/array.h"
#include "compiler/analyzer/conversion.h"
#include "compiler/analyzer/expression.h"
#include "compiler/analyzer/ownership.h"
#include "compiler/analyzer/reference.h"
#include "compiler/analyzer/type_alias.h"
#include "compiler/ast/binding.h"
#include "compiler/ast/predicate.h"
#include "compiler/ast/query.h"
#include "compiler/ast/reference.h"
#include "compiler/lexer/token.h"
#include "compiler/diagnostics.h"
#include "compiler/scope.h"
#include "runtime/string_object.h"

static _Noreturn void float_literal_requires_type_error(Token token)
{
    fatal_source_error(token.line, token.column, "Float literal requires an explicit type");
}

static _Noreturn void implicit_conversion_error(Token token, Type source, Type destination)
{
    const char* source_name = get_data_type_name(source);
    const char* destination_name = get_data_type_name(destination);

    fatal_token_error(token, "", "Cannot implicitly convert %s to %s for binding ", source_name, destination_name);
}

static _Noreturn void integer_literal_requires_type_error(Token token)
{
    fatal_source_error(token.line, token.column, "Integer literal requires an explicit type");
}

static ASTNode* find_frame_symbol(Scope* scope, StringView id)
{
    Scope* frame = scope->frame_scope;
    ASTNode* symbol = NULL;

    for (Scope* current = scope;
        current && current->frame_scope == frame && !symbol;
        current = current->parent) {
        symbol = get_local_symbol(current, id);
    }

    return symbol;
}

ASTNode* find_symbol(Analyzer* analyzer, StringView id)
{
    ASTNode* symbol = find_frame_symbol(analyzer->current_scope, id);
    if (symbol) {
        return symbol;
    }

    return get_local_symbol(analyzer->top_level->compound.scope, id);
}

void validate_constant_binding(Analyzer* analyzer, ASTNode* symbol, Token token)
{
    if (!analyzer->constant_scope || (is_variable_definition(symbol) && symbol->variable_definition.constant)) {
        return;
    }

    Scope* scope = get_scope(symbol);

    if (scope->level > analyzer->constant_scope->level) {
        return;
    }

    fatal_token_error(token, " in constant expression", "Cannot access runtime binding ");
}

void validate_constant_reference(Analyzer* analyzer, ASTNode* symbol, Token token)
{
    if (!analyzer->constant_scope || !symbol) {
        return;
    }

    Scope* scope = get_scope(symbol);
    bool called_function = analyzer->function_path != analyzer->constant_functions;

    if (scope->level > analyzer->constant_scope->level || (called_function && !is_top_level_scope(scope))) {
        return;
    }

    Token binding = token;

    if (symbol->type == ASTParameter) {
        binding = symbol->parameter.token;
    }

    if (is_variable_definition(symbol)) {
        binding = symbol->variable_definition.token;
    }

    binding.line = token.line;
    binding.column = token.column;

    fatal_token_error(binding, " in constant expression", "Cannot reference runtime binding ");
}

void validate_constant_mutation(ASTNode* symbol, ReferenceType type, Token token)
{
    if (type == RefExclusive && is_variable_definition(symbol) && symbol->variable_definition.constant) {
        semantic_token_error("Cannot mutate constant ", token);
    }
}

void analyze_variable(Analyzer* analyzer, ASTNode* ast)
{
    StringView id = get_string_view(ast->variable.id);
    ASTNode* symbol = find_symbol(analyzer, id);

    if (!symbol || !is_variable_type(symbol)) {
        symbol_error(" is undefined", ast->variable.token);
    }

    validate_constant_binding(analyzer, symbol, ast->variable.token);

    if (!is_initialized(symbol)) {
        symbol_error(" is uninitialized", ast->variable.token);
    }

    Vector* moves = get_tuple_moves(symbol);

    if (is_moved(symbol) || (moves->count && !analyzer->tuple_projection)) {
        symbol_error(" is moved", ast->variable.token);
    }

    if (get_reference_type(symbol) == RefNone
        && has_exclusive_access(symbol)) {
        binding_access_error("Cannot read binding ", ast->variable.token, " while exclusive access is active");
    }

    ast->variable.scope = analyzer->current_scope;
    ast->variable.symbol = symbol;
    track_variable_read(symbol);
}

static ASTNode* find_assignment_symbol(Analyzer* analyzer, ASTNode* ast)
{
    Token token = ast->assignment.token;
    StringView id = create_string_view(token.chars, token.length);
    ASTNode* symbol = find_symbol(analyzer, id);

    if (!symbol || !is_variable_type(symbol)) {
        symbol_error(" is undefined", ast->assignment.token);
    }

    return symbol;
}

static void validate_compound_assignment_target(ASTNode* ast, ASTNode* symbol)
{
    if (ast->assignment.operator.type == TokenEqual) {
        return;
    }

    if (!is_initialized(symbol)) {
        symbol_error(" is uninitialized", ast->assignment.token);
    }

    if (get_reference_type(symbol) == RefShared) {
        semantic_token_error("Cannot mutate through shared reference ", ast->assignment.token);
    }
}

static void validate_assignment_target(ASTNode* ast, ASTNode* symbol)
{
    Token token = ast->assignment.token;

    if (is_variable_definition(symbol) && symbol->variable_definition.constant) {
        semantic_token_error("Cannot reassign constant ", token);
    }

    if (is_moved(symbol)) {
        symbol_error(" is moved", token);
    }

    if (get_reference_type(symbol) == RefNone && has_exclusive_access(symbol)) {
        binding_access_error("Cannot mutate binding ", token, " while exclusive access is active");
    }

    if (*get_shared_access_count(symbol)) {
        binding_access_error("Cannot mutate binding ", token, " while shared access is active");
    }

    Type type = get_node_data_type(symbol);
    bool mutates_array = is_array_type(type.kind) && ast->assignment.operator.type != TokenEqual;

    if (!mutates_array && get_reference_type(symbol) == RefNone && is_variable_definition(symbol)
        && symbol->variable_definition.fixed && symbol->variable_definition.possibly_initialized) {
        semantic_token_error("Cannot reassign fixed binding ", token);
    }
}

static void validate_assignment_type(ASTNode* ast, ASTNode* symbol)
{
    ASTNode* expression = ast->assignment.expr;
    Type expression_type = get_node_data_type(expression);
    Type binding_type = get_node_data_type(symbol);
    ReferenceType binding_reference_type = get_reference_type(symbol);
    ReferenceType expression_reference_type = get_reference_type(expression);

    if (ast->assignment.operator.type != TokenEqual && binding_reference_type == RefExclusive) {
        binding_reference_type = RefNone;
    }

    bool compatible_value = types_equal(binding_type, expression_type) || (binding_reference_type == RefNone
        && expression_reference_type == RefNone && can_implicitly_widen_type(expression_type.kind, binding_type.kind));

    if (compatible_value && binding_reference_type == expression_reference_type) {
        return;
    }

    if (binding_reference_type == RefNone && expression_reference_type == RefNone) {
        implicit_conversion_error(ast->assignment.token, expression_type, binding_type);
    }

    semantic_token_error("Assignment type does not match binding ", ast->assignment.token);
}

static void analyze_array_compound_assignment(ASTNode* ast, ASTNode* symbol)
{
    Type left = get_node_data_type(symbol);
    Type right = get_node_data_type(ast->assignment.expr);
    Token operator = ast->assignment.operator;
    operator.type = get_assignment_binary_token(operator.type);

    if (!is_array_type(right.kind) || !array_shapes_equal(left, right)) {
        semantic_error("Array compound assignment requires matching array shapes", ast->assignment.operator);
    }

    Type left_leaf = get_array_leaf_type(left);
    Type right_leaf = get_array_leaf_type(right);
    bool numeric = is_integer_type(left_leaf.kind) || is_float_type(left_leaf.kind);
    bool compatible = types_equal(left_leaf, right_leaf)
        || can_implicitly_widen_type(right_leaf.kind, left_leaf.kind);

    if (!numeric || !compatible) {
        semantic_error("Array compound assignment requires compatible numeric elements", ast->assignment.operator);
    }

    Type operand_type;
    Type result_type = analyze_binary_operand_types(operator, left_leaf, right_leaf, &operand_type);

    if (!types_equal(result_type, left_leaf)) {
        semantic_error("Array compound assignment cannot change element type", ast->assignment.operator);
    }

    validate_assignment_target(ast, symbol);
    validate_compound_assignment_target(ast, symbol);
}

static Type analyze_assignment_expression(Analyzer* analyzer, ASTNode* ast, ASTNode* symbol)
{
    if (ast->assignment.operator.type == TokenEqual && is_variable_definition(symbol)
        && is_initialized(symbol) && symbol->variable_definition.reference_access_active) {
        release_reference_access(symbol);
    }

    Type destination_type = get_node_data_type(symbol);
    ReferenceType reference = get_reference_type(symbol);

    apply_declared_type(ast->assignment.expr, destination_type, reference);
    analyze_node(analyzer, ast->assignment.expr);

    return destination_type;
}

static void validate_assignment_operator(ASTNode* ast, ASTNode* symbol)
{
    Type binding_type = get_node_data_type(symbol);
    bool string_type = binding_type.kind == TypeString || binding_type.kind == TypeStringSlice;

    if (binding_type.kind == TypeTuple && ast->assignment.operator.type != TokenEqual) {
        semantic_error("Invalid operator for tuple assignment", ast->assignment.operator);
    }


    if (string_type && ast->assignment.operator.type != TokenEqual) {
        semantic_error("Invalid operator for string assignment", ast->assignment.operator);
    }

    if (binding_type.kind == TypeChar && ast->assignment.operator.type != TokenEqual) {
        semantic_error("Invalid operator for char assignment", ast->assignment.operator);
    }

    if (is_float_type(binding_type.kind)
        && !is_float_assignment_token(ast->assignment.operator.type)) {
        semantic_error("Invalid operands for assignment", ast->assignment.operator);
    }

    if (ast->assignment.operator.type == TokenPowerEqual && binding_type.kind != TypeI32) {
        semantic_error("Invalid operands for assignment", ast->assignment.operator);
    }
}

void analyze_assignment(Analyzer* analyzer, ASTNode* ast)
{
    ASTNode* symbol = find_assignment_symbol(analyzer, ast);
    bool initializes_binding = !is_initialized(symbol);

    validate_constant_binding(analyzer, symbol, ast->assignment.token);
    validate_assignment_target(ast, symbol);
    validate_compound_assignment_target(ast, symbol);
    track_variable_read(symbol);

    Type destination_type = analyze_assignment_expression(analyzer, ast, symbol);

    if (is_array_type(destination_type.kind) && ast->assignment.operator.type != TokenEqual) {
        analyze_array_compound_assignment(ast, symbol);

        ast->assignment.scope = analyzer->current_scope;
        ast->assignment.symbol = symbol;

        return;
    }

    validate_assignment_type(ast, symbol);
    consume_array(ast->assignment.expr);
    validate_assignment_operator(ast, symbol);

    ast->assignment.scope = analyzer->current_scope;
    ast->assignment.symbol = symbol;

    Vector* moves = get_tuple_moves(symbol);
    moves->count = 0;

    update_reference_binding(ast, symbol, initializes_binding);
    initialize_variable(symbol);
}

static void initialize_variable_definition(Analyzer* analyzer, ASTNode* ast)
{
    ast->variable_definition.scope = analyzer->current_scope;
    ast->variable_definition.position = get_next_local_position(analyzer->current_scope);
    ast->variable_definition.initialized = false;
    ast->variable_definition.possibly_initialized = false;
}

static bool variable_types_match(ASTNode* ast, Type type, ReferenceType reference_type)
{
    ReferenceType declared_reference_type = ast->variable_definition.reference_type;

    if (declared_reference_type != reference_type) {
        return false;
    }

    Type declared_type = ast->variable_definition.type_id;
    if (types_equal(declared_type, type)) {
        return true;
    }

    if (reference_type != RefNone) {
        return false;
    }

    return can_implicitly_widen_type(type.kind, declared_type.kind);
}

static void validate_variable_type_match(ASTNode* ast, Type type, ReferenceType reference_type)
{
    if (variable_types_match(ast, type, reference_type)) {
        return;
    }

    Type declared_type = ast->variable_definition.type_id;
    ReferenceType declared_reference_type = ast->variable_definition.reference_type;

    if (declared_reference_type == RefNone && reference_type == RefNone) {
        implicit_conversion_error(ast->variable_definition.token, type, declared_type);
    }

    semantic_token_error("Initializer type does not match variable ", ast->variable_definition.token);
}

static void resolve_variable_type(ASTNode* ast, ASTNode* expression)
{
    Type type = get_node_data_type(expression);
    ReferenceType reference_type = get_reference_type(expression);

    if (ast->variable_definition.type_id.kind != TypeUnknown) {
        validate_variable_type_match(ast, type, reference_type);

        return;
    }

    if (type.kind == TypeUnknown && expression->type == ASTFloat) {
        float_literal_requires_type_error(expression->float_literal.token);
    }

    if (type.kind == TypeUnknown) {
        integer_literal_requires_type_error(ast->variable_definition.token);
    }

    ast->variable_definition.type_id = type;
    ast->variable_definition.reference_type = reference_type;
}

static void analyze_variable_initializer(Analyzer* analyzer, ASTNode* ast, bool analyzed)
{
    ASTNode* expression = ast->variable_definition.expr;

    if (is_none(expression)) {
        return;
    }

    if (!analyzed) {
        apply_declared_type(expression, ast->variable_definition.type_id, ast->variable_definition.reference_type);
        analyze_node(analyzer, expression);
    }

    resolve_variable_type(ast, expression);
    consume_array(expression);

    set_reference_origins_from_expression(ast, expression);
    initialize_variable(ast);

    size_t count = ast->variable_definition.destructure_count;
    Type type = get_node_data_type(ast);

    if (count && (type.kind != TypeTuple || type.tuple->count != count)) {
        semantic_error("Tuple destructuring element count does not match", ast->variable_definition.token);
    }
}

static void validate_variable_type(ASTNode* ast)
{
    if (ast->variable_definition.type_id.kind == TypeVoid) {
        semantic_token_error("Invalid type for variable ", ast->variable_definition.token);
    }
}

static void track_variable_reference(ASTNode* ast)
{
    if (ast->variable_definition.destructure_count) {
        return;
    }

    if ((ast->variable_definition.reference_type == RefNone && !ast->tuple_references.count)
        || !ast->variable_definition.initialized) {
        return;
    }

    establish_reference_access(ast, ast->variable_definition.expr);
}

void analyze_variable_definition(Analyzer* analyzer, ASTNode* ast)
{
    StringView id = get_string_view(ast->variable_definition.id);

    if (get_local_symbol(analyzer->current_scope, id)) {
        redefinition_error("Variable", ast->variable_definition.token);
    }

    initialize_variable_definition(analyzer, ast);

    resolve_named_type(analyzer, &ast->variable_definition.type_id, &ast->variable_definition.reference_type);

    bool analyzed = infer_variable_array_count(analyzer, ast);

    resolve_dimensions(analyzer, &ast->variable_definition.type_id);
    analyze_variable_initializer(analyzer, ast, analyzed);
    validate_variable_type(ast);
    set_local_variable_symbol(analyzer->current_scope, id, ast);
    track_variable_reference(ast);
}
