#include "compiler/ast_optimizer.h"
#include "builtin/builtin.h"
#include "builtin/string.h"
#include "common/errors.h"
#include "common/memory.h"
#include "common/vector.h"
#include "compiler/constant.h"
#include "compiler/scope.h"
#include "compiler/token.h"
#include "runtime/concatenation.h"
#include "runtime/string_object.h"
#include <stdint.h>

static void remove_discarded_expression_reads(ASTNode* ast);
static void remove_discarded_statement_reads(ASTNode* ast);
static void mark_called_functions(ASTNode* ast);
static void reset_function_call_counts(ASTNode* ast);
static void optimize_node(ASTNode* ast);
static void optimize_match_pattern(ASTNode* pattern);
static void optimize_conditional(ASTNode* ast, bool fold);
static void replace_with_expression(ASTNode* ast, ASTNode* expression);

static void write_text_literal(Writer* writer, ASTNode* ast)
{
    if (ast->type == ASTString) {
        format_string(writer, ast->string_literal.value);

        return;
    }

    format_char(writer, ast->character_literal.value);
}

static void write_concatenation_literals(Writer* writer, const void* context)
{
    const ASTNode* ast = context;

    write_text_literal(writer, ast->binary.left_expr);
    write_text_literal(writer, ast->binary.right_expr);
}

static size_t text_literal_capacity(const ASTNode* ast)
{
    if (ast->type == ASTString) {
        return ast->string_literal.value->length;
    }

    return 4;
}

static size_t concatenation_literal_capacity(const ASTNode* ast)
{
    size_t left = text_literal_capacity(ast->binary.left_expr);
    size_t right = text_literal_capacity(ast->binary.right_expr);

    if (right > SIZE_MAX - left) {
        out_of_memory_error();
    }

    return left + right;
}

static bool fold_concatenation(ASTNode* ast)
{
    if (ast->binary.type_id != TypeString || ast->binary.operator.type != TokenPlus) {
        return false;
    }

    if (!is_text_literal(ast->binary.left_expr) || !is_text_literal(ast->binary.right_expr)) {
        return false;
    }

    Token token = ast->binary.operator;
    size_t capacity = concatenation_literal_capacity(ast);
    StringObject* string = concatenate_string_parts(capacity, write_concatenation_literals, ast);

    free_ast_node(ast->binary.left_expr);
    free_ast_node(ast->binary.right_expr);

    ast->type = ASTString;
    ast->string_literal.value = string;
    ast->string_literal.token = token;

    return true;
}

static void fold_binary(ASTNode* ast)
{
    if (fold_concatenation(ast)) {
        return;
    }

    ConstantValue value = {0};
    bool constant = evaluate_constant(ast, &value);

    if (!constant) {
        return;
    }

    ASTNode* replacement = create_constant_node(value, ast->binary.type_id, ast->binary.operator);

    free_ast_node(ast->binary.left_expr);
    free_ast_node(ast->binary.right_expr);
    replace_with_expression(ast, replacement);
}

static bool is_literal_of_type(ASTNode* ast, DataType type)
{
    if (!is_literal(ast)) {
        return false;
    }

    DataType literal_type = get_node_data_type(ast);

    return literal_type == type;
}

static void set_literal_token(ASTNode* ast, Token token)
{
    switch (ast->type) {
        case ASTInteger:
            ast->integer_literal.token = token;
            break;
        case ASTBoolean:
            ast->boolean_literal.token = token;
            break;
        case ASTFloat:
            ast->float_literal.token = token;
            break;
        case ASTString:
            ast->string_literal.token = token;
            break;
        case ASTCharacter:
            ast->character_literal.token = token;
            break;
        default:
            break;
    }
}

static ASTNode copy_literal_initializer(ASTNode* initializer)
{
    ASTNode replacement = *initializer;

    if (initializer->type == ASTString) {
        StringObject* string = initializer->string_literal.value;
        replacement.string_literal.value = copy_string_object(string->chars, string->length);
    }

    return replacement;
}

static void propagate_literal_let(ASTNode* ast)
{
    ASTNode* symbol = ast->variable.symbol;

    if (!symbol || symbol->type != ASTVariableDefinition) {
        return;
    }

    if (!symbol->variable_definition.fixed || symbol->variable_definition.address_taken
        || symbol->variable_definition.reference_type != RefNone) {
        return;
    }
    
    Scope* definition_scope = symbol->variable_definition.scope;
    if (definition_scope->frame_scope != ast->variable.scope->frame_scope) {
        return;
    }

    ASTNode* initializer = symbol->variable_definition.expr;
    DataType type = symbol->variable_definition.type_id;

    if (!initializer) {
        return;
    }

    bool literal = is_literal_of_type(initializer, type);

    if (!literal) {
        return;
    }

    ASTNode replacement = copy_literal_initializer(initializer);
    Token token = ast->variable.token;

    set_literal_token(&replacement, token);
    free_string_object(ast->variable.id);

    symbol->variable_definition.read_count--;
    *ast = replacement;
}

static void fold_prefix(ASTNode* ast)
{
    ConstantValue value = {0};
    bool constant = evaluate_constant(ast, &value);

    if (!constant) {
        return;
    }

    DataType type = get_node_data_type(ast);
    ASTNode* replacement = create_constant_node(value, type, ast->prefix.operator);

    free_ast_node(ast->prefix.expr);
    free_vector(&ast->prefix.reference_origins);
    replace_with_expression(ast, replacement);
}

static void optimize_nodes(Vector* nodes)
{
    size_t count = count_vector(nodes);

    for (size_t i = 0; i < count; i++) {
        ASTNode* node = get_vector_at(nodes, i);

        optimize_node(node);
    }
}

static void optimize_conditional_branch(ASTNode* branch)
{
    if (!branch) {
        return;
    }

    if (branch->type == ASTConditional) {
        optimize_conditional(branch, false);

        return;
    }

    optimize_nodes(&branch->compound.statements);
}

static ASTNode* get_literal_branch_result(ASTNode* branch)
{
    while (branch && branch->type == ASTConditional) {
        ASTNode* condition = branch->conditional.condition;

        if (condition->type != ASTBoolean) {
            return NULL;
        }

        branch = condition->boolean_literal.value ? branch->conditional.then_branch : branch->conditional.else_branch;
    }

    if (!branch || branch->type != ASTCompound) {
        return NULL;
    }

    size_t count = count_vector(&branch->compound.statements);

    if (count != 1) {
        return NULL;
    }

    ASTNode* result = get_vector_at(&branch->compound.statements, 0);

    if (result->type != ASTInteger && result->type != ASTFloat && result->type != ASTBoolean) {
        return NULL;
    }

    return result;
}

static void fold_conditional(ASTNode* ast)
{
    ASTNode* condition = ast->conditional.condition;

    if (!ast->conditional.expression || ast->conditional.reference_type != RefNone
        || condition->type != ASTBoolean) {
        return;
    }

    ASTNode* selected = condition->boolean_literal.value ? ast->conditional.then_branch : ast->conditional.else_branch;
    ASTNode* result = get_literal_branch_result(selected);

    if (!result) {
        return;
    }

    DataType type = get_node_data_type(result);

    if (type != ast->conditional.type_id) {
        return;
    }

    ASTNode replacement = *result;

    free_ast_node(ast->conditional.condition);
    free_ast_node(ast->conditional.then_branch);
    free_ast_node(ast->conditional.else_branch);
    free_vector(&ast->conditional.reference_origins);

    *ast = replacement;
}

static void optimize_conditional(ASTNode* ast, bool fold)
{
    optimize_node(ast->conditional.condition);
    optimize_conditional_branch(ast->conditional.then_branch);
    optimize_conditional_branch(ast->conditional.else_branch);

    if (!fold) {
        return;
    }

    fold_conditional(ast);
}

static void optimize_match_pattern(ASTNode* pattern)
{
    if (pattern->type != ASTBinary || pattern->binary.operator.type != TokenPipe) {
        optimize_node(pattern);

        return;
    }

    optimize_match_pattern(pattern->binary.left_expr);
    optimize_match_pattern(pattern->binary.right_expr);
}

static void optimize_match_arm(MatchArm* arm)
{
    if (arm->pattern) {
        optimize_match_pattern(arm->pattern);
    }

    if (arm->guard) {
        optimize_node(arm->guard);
    }

    optimize_conditional_branch(arm->branch);
}

static void optimize_match(ASTNode* ast)
{
    if (ast->match.subject) {
        optimize_node(ast->match.subject);
    }

    size_t count = count_vector(&ast->match.arms);

    for (size_t i = 0; i < count; i++) {
        MatchArm* arm = get_vector_at(&ast->match.arms, i);

        optimize_match_arm(arm);
    }

    optimize_conditional_branch(ast->match.default_branch);
}

static void replace_with_expression(ASTNode* ast, ASTNode* expression)
{
    ASTNode replacement = *expression;

    free_memory(expression);

    *ast = replacement;
}

static void optimize_builtin_call(ASTNode* ast)
{
    optimize_nodes(&ast->builtin_call.args);

    const Builtin* builtin = ast->builtin_call.builtin;

    if (builtin->entry != builtin_string || ast->builtin_call.args.count != 1) {
        return;
    }

    ASTNode* expression = ast->builtin_call.args.data[0];

    if (get_node_data_type(expression) != builtin->return_type_id || get_reference_type(expression) != RefNone) {
        return;
    }

    free_vector(&ast->builtin_call.args);
    replace_with_expression(ast, expression);
}

static void fold_explicit_conversion(ASTNode* ast)
{
    ConstantValue value = {0};
    bool constant = evaluate_constant(ast, &value);

    if (!constant) {
        return;
    }

    ASTNode* replacement = create_constant_node(value, ast->conversion.type_id, ast->conversion.token);
    ASTNode* expression = ast->conversion.expr;

    free_ast_node(expression);

    ast->conversion.expr = replacement;
}

static void optimize_conversion(ASTNode* ast)
{
    optimize_node(ast->conversion.expr);
    fold_explicit_conversion(ast);

    ASTNode* expression = ast->conversion.expr;
    bool literal = expression->type == ASTInteger || expression->type == ASTFloat
        || expression->type == ASTCharacter;
    bool identity = get_node_data_type(expression) == ast->conversion.type_id;

    if (!literal && !identity) {
        return;
    }

    replace_with_expression(ast, expression);
}

static void optimize_binary(ASTNode* ast)
{
    optimize_node(ast->binary.left_expr);
    optimize_node(ast->binary.right_expr);
    fold_binary(ast);
}

static void optimize_prefix(ASTNode* ast)
{
    if (ast->prefix.operator.type == TokenKeywordConst) {
        return;
    }

    optimize_node(ast->prefix.expr);
    fold_prefix(ast);
}

static void optimize_node(ASTNode* ast)
{
    switch (ast->type) {
        case ASTAssignment:
            optimize_node(ast->assignment.expr);
            break;
        case ASTBinary:
            optimize_binary(ast);
            break;
        case ASTBuiltinCall:
            optimize_builtin_call(ast);
            break;
        case ASTWhile:
            optimize_node(ast->while_statement.condition);
            optimize_nodes(&ast->while_statement.body->compound.statements);
            break;
        case ASTConditional:
            optimize_conditional(ast, true);
            break;
        case ASTConversion:
            optimize_conversion(ast);
            break;
        case ASTFunctionCall:
            optimize_nodes(&ast->function_call.args);
            break;
        case ASTFunctionDefinition:
            optimize_nodes(&ast->function_definition.body->compound.statements);
            break;
        case ASTMatch:
            optimize_match(ast);
            break;
        case ASTPrefix:
            optimize_prefix(ast);
            break;
        case ASTReturn:
            optimize_node(ast->return_statement.expr);
            break;
        case ASTVariable:
            propagate_literal_let(ast);
            break;
        case ASTVariableDefinition:
            optimize_node(ast->variable_definition.expr);
            break;
        default:
            break;
    }
}

void optimize_ast(ASTNode* ast, size_t start)
{
    size_t count = count_vector(&ast->compound.statements);

    for (size_t i = start; i < count; i++) {
        ASTNode* node = get_vector_at(&ast->compound.statements, i);

        optimize_node(node);
    }
}

static bool has_no_value_reads(ASTNode* ast)
{
    return ast->variable_definition.read_count == ast->variable_definition.discarded_read_count;
}

static void remove_unread_initializer_reads(ASTNode* ast)
{
    if (!has_no_value_reads(ast) || ast->variable_definition.discarded_reads_analyzed) {
        return;
    }

    ast->variable_definition.discarded_reads_analyzed = true;

    remove_discarded_expression_reads(ast->variable_definition.expr);
}

static void remove_discarded_variable_read(ASTNode* ast)
{
    ASTNode* symbol = ast->variable.symbol;

    if (!is_variable_definition(symbol)) {
        return;
    }

    symbol->variable_definition.discarded_read_count++;

    remove_unread_initializer_reads(symbol);
}

static void remove_discarded_expression_reads(ASTNode* ast)
{
    switch (ast->type) {
        case ASTVariable:
            remove_discarded_variable_read(ast);
            break;
        case ASTPrefix:
            remove_discarded_expression_reads(ast->prefix.expr);
            break;
        default:
            break;
    }
}

static void remove_discarded_statement_vector_reads(Vector* nodes)
{
    size_t count = count_vector(nodes);

    for (size_t i = 0; i < count; i++) {
        ASTNode* node = get_vector_at(nodes, i);

        remove_discarded_statement_reads(node);
    }
}

static void remove_discarded_statement_reads(ASTNode* ast)
{
    if (ast->type == ASTFunctionDefinition) {
        remove_discarded_statement_vector_reads(&ast->function_definition.body->compound.statements);

        return;
    }

    if (ast->type == ASTVariableDefinition) {
        remove_unread_initializer_reads(ast);

        return;
    }

    remove_discarded_expression_reads(ast);
}

void remove_discarded_reads(Vector* nodes, size_t start)
{
    size_t count = count_vector(nodes);

    for (size_t i = start; i < count; i++) {
        ASTNode* node = get_vector_at(nodes, i);

        remove_discarded_statement_reads(node);
    }
}

static void mark_called_functions_in_vector(Vector* nodes)
{
    size_t count = count_vector(nodes);

    for (size_t i = 0; i < count; i++) {
        ASTNode* node = get_vector_at(nodes, i);

        mark_called_functions(node);
    }
}

static void mark_called_function(ASTNode* ast)
{
    ASTNode* function = ast->function_call.symbol;

    mark_called_functions_in_vector(&ast->function_call.args);

    if (!function) {
        return;
    }

    function->function_definition.call_count++;

    if (function->function_definition.call_count > 1) {
        return;
    }

    mark_called_functions_in_vector(&function->function_definition.params);
    mark_called_functions(function->function_definition.body);
}

static void mark_match_called_functions(ASTNode* ast)
{
    mark_called_functions(ast->match.subject);

    size_t count = count_vector(&ast->match.arms);

    for (size_t i = 0; i < count; i++) {
        MatchArm* arm = get_vector_at(&ast->match.arms, i);

        mark_called_functions(arm->pattern);
        mark_called_functions(arm->guard);
        mark_called_functions(arm->branch);
    }

    mark_called_functions(ast->match.default_branch);
}

static void mark_prefix_called_functions(ASTNode* ast)
{
    if (ast->prefix.operator.type == TokenKeywordConst && ast->prefix.evaluated) {
        return;
    }

    mark_called_functions(ast->prefix.expr);
}

static void mark_called_functions(ASTNode* ast)
{
    if (!ast) {
        return;
    }

    switch (ast->type) {
        case ASTAssignment:
            mark_called_functions(ast->assignment.expr);
            break;
        case ASTBinary:
            mark_called_functions(ast->binary.left_expr);
            mark_called_functions(ast->binary.right_expr);
            break;
        case ASTBuiltinCall:
            mark_called_functions_in_vector(&ast->builtin_call.args);
            break;
        case ASTWhile:
            mark_called_functions(ast->while_statement.condition);
            mark_called_functions(ast->while_statement.body);
            break;
        case ASTCompound:
            mark_called_functions_in_vector(&ast->compound.statements);
            break;
        case ASTConditional:
            mark_called_functions(ast->conditional.condition);
            mark_called_functions(ast->conditional.then_branch);
            mark_called_functions(ast->conditional.else_branch);
            break;
        case ASTConversion:
            mark_called_functions(ast->conversion.expr);
            break;
        case ASTFunctionCall:
            mark_called_function(ast);
            break;
        case ASTMatch:
            mark_match_called_functions(ast);
            break;
        case ASTParameter:
            mark_called_functions(ast->parameter.default_value);
            break;
        case ASTPrefix:
            mark_prefix_called_functions(ast);
            break;
        case ASTReturn:
            mark_called_functions(ast->return_statement.expr);
            break;
        case ASTVariableDefinition:
            mark_called_functions(ast->variable_definition.expr);
            break;
        default:
            break;
    }
}

static void reset_function_call_counts_in_vector(Vector* nodes)
{
    size_t count = count_vector(nodes);

    for (size_t i = 0; i < count; i++) {
        ASTNode* node = get_vector_at(nodes, i);

        reset_function_call_counts(node);
    }
}

static void reset_match_function_call_counts(ASTNode* ast)
{
    size_t count = count_vector(&ast->match.arms);

    for (size_t i = 0; i < count; i++) {
        MatchArm* arm = get_vector_at(&ast->match.arms, i);

        reset_function_call_counts(arm->pattern);
        reset_function_call_counts(arm->guard);
        reset_function_call_counts(arm->branch);
    }

    reset_function_call_counts(ast->match.default_branch);
}

static void reset_function_call_counts(ASTNode* ast)
{
    if (!ast) {
        return;
    }

    if (ast->type == ASTFunctionDefinition) {
        ast->function_definition.call_count = 0;

        reset_function_call_counts(ast->function_definition.body);

        return;
    }

    if (ast->type == ASTCompound) {
        reset_function_call_counts_in_vector(&ast->compound.statements);

        return;
    }

    if (ast->type == ASTConditional) {
        reset_function_call_counts(ast->conditional.then_branch);
        reset_function_call_counts(ast->conditional.else_branch);

        return;
    }

    if (ast->type == ASTWhile) {
        reset_function_call_counts(ast->while_statement.condition);
        reset_function_call_counts(ast->while_statement.body);

        return;
    }

    if (ast->type == ASTMatch) {
        reset_match_function_call_counts(ast);
    }
}

static void mark_root_called_functions(ASTNode* ast)
{
    if (ast->type == ASTFunctionDefinition) {
        return;
    }

    mark_called_functions(ast);
}

void mark_reachable_functions(Vector* nodes)
{
    size_t count = count_vector(nodes);

    reset_function_call_counts_in_vector(nodes);

    for (size_t i = 0; i < count; i++) {
        ASTNode* ast = get_vector_at(nodes, i);

        mark_root_called_functions(ast);
    }
}
