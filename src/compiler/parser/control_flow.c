#include "compiler/parser/control_flow.h"
#include "common/vector.h"
#include "compiler/parser/expression.h"

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

    if (!condition || parser->current_token.type == TokenLbrace) {
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

    push_vector_item(&branch->compound.statements, statement);

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
