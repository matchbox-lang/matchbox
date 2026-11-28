#include "compiler/optimizer/ast_optimizer.h"
#include "builtin/builtin.h"
#include "builtin/string.h"
#include "common/array_type.h"
#include "common/errors.h"
#include "common/vector.h"
#include "compiler/ast/ast.h"
#include "compiler/ast/binding.h"
#include "compiler/ast/literal.h"
#include "compiler/ast/predicate.h"
#include "compiler/ast/query.h"
#include "compiler/ast/reference.h"
#include "compiler/lexer/token.h"
#include "compiler/constant.h"
#include "compiler/scope.h"
#include "runtime/concatenation.h"
#include "runtime/string_object.h"
#include <stdint.h>

#define ARRAY_BINARY_EXPANSION_LIMIT 16

static void remove_discarded_expression_reads(ASTNode* ast);
static void remove_discarded_statement_reads(ASTNode* ast);
static void mark_called_functions(ASTNode* ast);
static void reset_function_call_counts(ASTNode* ast);
static void optimize_node(ASTNode* ast);
static void optimize_conditional(ASTNode* ast, bool fold);

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
    if (ast->binary.type_id.kind != TypeString || ast->binary.operator.type != TokenPlus) {
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

    ASTNode* replacement = create_string_node(token, string);

    replace_ast_node(ast, replacement);

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
    replace_ast_node(ast, replacement);
}

static void propagate_literal_let(ASTNode* ast)
{
    ASTNode* symbol = ast->variable.symbol;

    if (!symbol || symbol->type != ASTVariableDefinition) {
        return;
    }

    if (symbol->variable_definition.binding_type == BindingMutable || symbol->variable_definition.address_taken
        || symbol->variable_definition.reference_type != RefNone) {
        return;
    }
    
    Scope* definition_scope = symbol->variable_definition.scope;
    if (definition_scope->frame_scope != ast->variable.scope->frame_scope) {
        return;
    }

    ASTNode* initializer = symbol->variable_definition.expr;
    Type type = symbol->variable_definition.type_id;

    if (!initializer) {
        return;
    }

    bool literal = is_literal_of_type(initializer, type);

    if (!literal) {
        return;
    }

    ASTNode* replacement = copy_literal_node(initializer);
    Token token = ast->variable.token;

    set_literal_token(replacement, token);
    free_string_object(ast->variable.id);

    symbol->variable_definition.read_count--;
    replace_ast_node(ast, replacement);
}

static void fold_prefix(ASTNode* ast)
{
    ConstantValue value = {0};
    bool constant = evaluate_constant(ast, &value);

    if (!constant) {
        return;
    }

    Type type = get_node_data_type(ast);
    ASTNode* replacement = create_constant_node(value, type, ast->prefix.operator);

    free_ast_node(ast->prefix.expr);
    free_vector(&ast->prefix.reference_origins);
    replace_ast_node(ast, replacement);
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

    Type type = get_node_data_type(result);

    if (!types_equal(type, ast->conditional.type_id)) {
        return;
    }

    ASTNode* replacement = copy_literal_node(result);

    free_ast_node(ast->conditional.condition);
    free_ast_node(ast->conditional.then_branch);
    free_ast_node(ast->conditional.else_branch);
    free_vector(&ast->conditional.reference_origins);

    replace_ast_node(ast, replacement);
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
}

static void optimize_builtin_call(ASTNode* ast)
{
    optimize_nodes(&ast->builtin_call.args);

    const Builtin* builtin = ast->builtin_call.builtin;

    if (builtin->entry != builtin_string || ast->builtin_call.args.count != 1) {
        return;
    }

    ASTNode* expression = ast->builtin_call.args.data[0];

    Type expression_type = get_node_data_type(expression);

    if (expression_type.kind != builtin->return_type_id || get_reference_type(expression) != RefNone) {
        return;
    }

    free_vector(&ast->builtin_call.args);
    replace_ast_node(ast, expression);
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

static bool can_remove_conversion(ASTNode* expression, Type destination)
{
    switch (expression->type) {
        case ASTInteger:
        case ASTFloat:
        case ASTCharacter:
            return true;
        default:
            break;
    }

    Type source = get_node_data_type(expression);

    return types_equal(source, destination);
}

static void optimize_conversion(ASTNode* ast)
{
    optimize_node(ast->conversion.expr);
    fold_explicit_conversion(ast);

    ASTNode* expression = ast->conversion.expr;
    if (!can_remove_conversion(expression, ast->conversion.type_id)) {
        return;
    }

    replace_ast_node(ast, expression);
}

static void append_array_binary_element(ASTNode* result, ASTNode* binary, ASTNode* left, ASTNode* right)
{
    Type result_type = get_array_leaf_type(binary->binary.type_id);
    Type operand_type = binary->binary.operand_type;
    ASTNode* element = create_typed_binary_node(left, right, binary->binary.operator, operand_type, result_type);

    optimize_node(element);
    push_vector_item(&result->array.elements, element);
}

static bool expand_literal_array_binary(ASTNode* ast)
{
    Type type = ast->binary.type_id;

    if (!is_array_type(type.kind)) {
        return false;
    }

    Type leaf = get_array_leaf_type(type);
    size_t stride = get_type_storage_size(leaf);

    if (stride == 0) {
        return false;
    }

    size_t size = get_type_storage_size(type);
    size_t count = size / stride;
    ASTNode* left = ast->binary.left_expr;
    ASTNode* right = ast->binary.right_expr;
    Type left_type = get_node_data_type(left);
    Type right_type = get_node_data_type(right);

    if (!is_array_type(left_type.kind) || !is_array_type(right_type.kind)) {
        return false;
    }

    if (count > ARRAY_BINARY_EXPANSION_LIMIT || !is_literal_array_tree(left) || !is_literal_array_tree(right)) {
        return false;
    }

    Vector left_literals;
    Vector right_literals;

    init_vector(&left_literals);
    init_vector(&right_literals);
    copy_flattened_array_literals(left, &left_literals);
    copy_flattened_array_literals(right, &right_literals);

    ASTNode* result = create_array_node(ast->binary.operator, type, true);

    for (size_t i = 0; i < count; i++) {
        append_array_binary_element(result, ast, left_literals.data[i], right_literals.data[i]);
    }

    free_vector(&left_literals);
    free_vector(&right_literals);
    free_ast_node(left);
    free_ast_node(right);
    replace_ast_node(ast, result);

    return true;
}

static void optimize_binary(ASTNode* ast)
{
    optimize_node(ast->binary.left_expr);
    optimize_node(ast->binary.right_expr);

    if (expand_literal_array_binary(ast)) {
        return;
    }

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

static void optimize_slice(ASTNode* ast)
{
    if (ast->slice.expr->type != ASTVariable) {
        optimize_node(ast->slice.expr);
    }

    optimize_node(ast->slice.start);
    optimize_node(ast->slice.end);
}

static void optimize_index(ASTNode* ast)
{
    optimize_node(ast->index.array);
    optimize_node(ast->index.index);
    optimize_node(ast->index.value);
}

static void optimize_node(ASTNode* ast)
{
    if (!ast) {
        return;
    }

    switch (ast->type) {
        case ASTArray:
            optimize_nodes(&ast->array.elements);
            break;
        case ASTTuple:
            optimize_nodes(&ast->tuple.elements);
            break;
        case ASTSlice:
            optimize_slice(ast);
            break;
        case ASTIndex:
        case ASTIndexAssignment:
            optimize_index(ast);
            break;
        case ASTAssignment:
            optimize_node(ast->assignment.expr);
            break;
        case ASTBinary:
            optimize_binary(ast);
            break;
        case ASTBuiltinCall:
            optimize_builtin_call(ast);
            break;
        case ASTFor:
            optimize_node(ast->for_statement.array);
            optimize_node(ast->for_statement.end);
            optimize_nodes(&ast->for_statement.body->compound.statements);
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
        case ASTTestBlock:
            optimize_node(ast->test_block.function);
            break;
        case ASTFunctionDefinition: {
            ASTNode* body = get_function_analysis_body(ast);

            optimize_nodes(&body->compound.statements);
            break;
        }
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
    if (ast->type == ASTTestBlock) {
        remove_discarded_statement_reads(ast->test_block.function);

        return;
    }

    if (ast->type == ASTFunctionDefinition) {
        ASTNode* body = get_function_analysis_body(ast);

        remove_discarded_statement_vector_reads(&body->compound.statements);

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
        case ASTArray:
            mark_called_functions_in_vector(&ast->array.elements);
            break;
        case ASTTuple:
            mark_called_functions_in_vector(&ast->tuple.elements);
            break;
        case ASTSlice:
            mark_called_functions(ast->slice.expr);
            mark_called_functions(ast->slice.start);
            mark_called_functions(ast->slice.end);
            break;
        case ASTIndex:
        case ASTIndexAssignment:
            mark_called_functions(ast->index.array);
            mark_called_functions(ast->index.index);
            mark_called_functions(ast->index.value);
            break;
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
        case ASTFor:
            mark_called_functions(ast->for_statement.array);
            mark_called_functions(ast->for_statement.end);
            mark_called_functions(ast->for_statement.body);
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
}

static void reset_function_call_counts(ASTNode* ast)
{
    if (!ast) {
        return;
    }

    if (ast->type == ASTTestBlock) {
        reset_function_call_counts(ast->test_block.function);

        return;
    }

    if (ast->type == ASTFunctionDefinition) {
        ast->function_definition.call_count = 0;

        ASTNode* body = get_function_analysis_body(ast);

        reset_function_call_counts(body);

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

    if (ast->type == ASTFor) {
        reset_function_call_counts(ast->for_statement.array);
        reset_function_call_counts(ast->for_statement.body);

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
