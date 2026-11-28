#include "compiler/parser/control_flow.h"
#include "common/vector.h"
#include "compiler/ast/ast.h"
#include "compiler/ast/binding.h"
#include "compiler/parser/expression.h"
#include "compiler/parser/type.h"
#include "runtime/string_object.h"

static ASTNode* parse_conditional_branch(Parser* parser)
{
    ASTNode* branch;

    if (!parse_braced_block(parser, &branch)) {
        free_ast_node(branch);

        return NULL;
    }

    return branch;
}

static ASTNode* parse_if_condition(Parser* parser)
{
    Parser start = *parser;
    ASTNode* condition = parse_expression(parser);

    if (!condition || parser->current_token.type == TokenLbrace || is_end_of_file(parser)) {
        return condition;
    }

    free_ast_node(condition);

    *parser = start;

    return parse_group_expression(parser);
}

bool parse_branch_statement(Parser* parser, ASTNode* branch, bool expression)
{
    ASTNode* statement = NULL;

    if (expression) {
        statement = parse_expression(parser);
    } else {
        statement = parse_statement(parser);
    }

    if (!statement) {
        return false;
    }

    append_statement(&branch->compound.statements, statement);

    return true;
}

static ASTNode* parse_if_branch(Parser* parser, bool expression)
{
    bool braced = parser->current_token.type == TokenLbrace;

    if (braced && !expression) {
        return parse_conditional_branch(parser);
    }

    if (braced) {
        consume(parser, TokenLbrace);
    }

    ASTNode* branch = create_compound_node(NULL);

    if (!parse_branch_statement(parser, branch, expression)) {
        free_ast_node(branch);

        return NULL;
    }

    if (!braced) {
        return branch;
    }

    if (parser->current_token.type == TokenSemicolon) {
        consume(parser, TokenSemicolon);
    }

    if (is_end_of_file(parser)) {
        free_ast_node(branch);

        return NULL;
    }

    consume(parser, TokenRbrace);

    return branch;
}

ASTNode* parse_conditional(Parser* parser, bool expression)
{
    Token token = parser->current_token;

    consume(parser, TokenKeywordIf);

    if (is_end_of_file(parser)) {
        return NULL;
    }

    ASTNode* condition = parse_if_condition(parser);

    if (!condition) {
        return NULL;
    }

    ASTNode* then_branch = parse_if_branch(parser, expression);

    if (!then_branch) {
        free_ast_node(condition);

        return NULL;
    }

    ASTNode* ast = create_conditional_node(token, condition, then_branch, NULL);
    ast->conditional.expression = expression;

    if (!expression && parser->current_token.type != TokenKeywordElse) {
        return ast;
    }

    if (is_end_of_file(parser)) {
        free_ast_node(ast);

        return NULL;
    }

    consume(parser, TokenKeywordElse);

    if (parser->current_token.type == TokenKeywordIf) {
        ast->conditional.else_branch = parse_conditional(parser, expression);
    } else {
        ast->conditional.else_branch = parse_if_branch(parser, expression);
    }

    if (!ast->conditional.else_branch) {
        free_ast_node(ast);

        return NULL;
    }

    return ast;
}

ASTNode* parse_while_statement(Parser* parser)
{
    Token token = parser->current_token;
    consume(parser, TokenKeywordWhile);
    ASTNode* condition = parse_expression(parser);

    if (!condition) {
        return NULL;
    }

    ASTNode* body = parse_conditional_branch(parser);

    if (!body) {
        free_ast_node(condition);

        return NULL;
    }

    return create_while_node(token, condition, body);
}

static ASTNode* parse_for_binding(Parser* parser)
{
    if (is_end_of_file(parser)) {
        return NULL;
    }

    Token token = parser->current_token;

    consume(parser, TokenIdentifier);

    StringObject* id = copy_string_object(token.chars, token.length);
    ASTNode* binding = create_variable_definition_node(token, id, BindingFixed);
    binding->variable_definition.expr = create_ast_node(ASTNone);

    if (parser->current_token.type == TokenColon && !parse_variable_type(parser, binding)) {
        free_ast_node(binding);

        return NULL;
    }

    return binding;
}

static bool parse_for_bindings(Parser* parser, ASTNode* ast)
{
    ast->for_statement.value = parse_for_binding(parser);

    if (parser->current_token.type == TokenComma) {
        consume(parser, TokenComma);

        ast->for_statement.index = ast->for_statement.value;
        ast->for_statement.value = parse_for_binding(parser);
    }

    return ast->for_statement.value && !is_end_of_file(parser);
}

static bool parse_for_header(Parser* parser, ASTNode* ast)
{
    bool parenthesized = parser->current_token.type == TokenLparen;

    if (parenthesized) {
        consume(parser, TokenLparen);
    }

    if (is_end_of_file(parser) || !parse_for_bindings(parser, ast)) {
        return false;
    }

    consume(parser, TokenKeywordIn);

    ast->for_statement.array = parse_expression(parser);

    if (!ast->for_statement.array || is_end_of_file(parser)) {
        return false;
    }

    if (parser->current_token.type == TokenRange) {
        consume(parser, TokenRange);

        ast->for_statement.end = parse_expression(parser);
    }

    if (parenthesized) {
        consume(parser, TokenRparen);
    }

    return !is_end_of_file(parser);
}

ASTNode* parse_for_statement(Parser* parser)
{
    Token token = parser->current_token;

    consume(parser, TokenKeywordFor);

    ASTNode* ast = create_for_node(token);

    if (!parse_for_header(parser, ast)) {
        free_ast_node(ast);

        return NULL;
    }

    ast->for_statement.body = parse_conditional_branch(parser);

    if (!ast->for_statement.body) {
        free_ast_node(ast);

        return NULL;
    }

    return ast;
}

ASTNode* parse_loop_control(Parser* parser)
{
    Token token = parser->current_token;

    advance(parser);

    return create_loop_control_node(token);
}
