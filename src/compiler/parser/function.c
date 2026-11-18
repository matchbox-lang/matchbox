#include "compiler/parser/function.h"
#include "common/vector.h"
#include "compiler/parser/expression.h"
#include "compiler/parser/type.h"
#include "runtime/string_object.h"

ASTNode* parse_return_statement(Parser* parser)
{
    Token token = parser->current_token;
    consume(parser, TokenKeywordReturn);

    ASTNode* expr = parse_expression(parser);
    if (!expr) {
        return NULL;
    }

    return create_return_node(token, expr);
}

static ASTNode* parse_argument(Parser* parser)
{
    return parse_expression(parser);
}

static bool parse_argument_list_item(Parser* parser, Vector* args)
{
    ASTNode* expr = parse_argument(parser);

    if (!expr && (parser->current_token.type == TokenComma || parser->current_token.type == TokenRparen)) {
        expected_expression_error(parser->current_token);
    }

    if (!expr) {
        return false;
    }

    push_vector_item(args, expr);

    if (parser->current_token.type == TokenComma) {
        consume(parser, TokenComma);
    }

    return true;
}

bool parse_argument_list(Parser* parser, Vector* args)
{
    consume(parser, TokenLparen);

    if (is_end_of_file(parser)) {
        return false;
    }

    while (parser->current_token.type != TokenRparen) {
        if (!parse_argument_list_item(parser, args)) {
            return false;
        }
    }

    if (is_end_of_file(parser)) {
        return false;
    }

    consume(parser, TokenRparen);

    return true;
}

static bool parse_parameter_default(Parser* parser, ASTNode* ast)
{
    if (parser->current_token.type != TokenEqual) {
        return true;
    }

    Token token = parser->current_token;
    token.type = TokenKeywordConst;
    consume(parser, TokenEqual);

    ASTNode* expression = parse_expression(parser);
    if (!expression) {
        return false;
    }

    ast->parameter.default_value = create_prefix_node(token, expression);

    return true;
}

static ASTNode* parse_parameter(Parser* parser)
{
    if (is_end_of_file(parser)) {
        return NULL;
    }

    Token token = parser->current_token;
    StringObject* id = copy_string_object(token.chars, token.length);

    consume(parser, TokenIdentifier);

    ASTNode* ast = create_parameter_node(token, id);

    if (parser->current_token.type == TokenColon) {
        consume(parser, TokenColon);

        ast->parameter.reference_type = parse_reference_type(parser);
        ast->parameter.type_id = (Type){.kind = get_token_data_type(parser->current_token.type)};

        consume_type(parser);
        parse_dimensions(parser, &ast->parameter.type_id);
    }

    if (!parse_parameter_default(parser, ast)) {
        free_ast_node(ast);

        return NULL;
    }

    return ast;
}

static bool parse_parameter_list_item(Parser* parser, Vector* params)
{
    ASTNode* param = parse_parameter(parser);

    if (!param) {
        return false;
    }

    push_vector_item(params, param);

    if (parser->current_token.type == TokenComma) {
        consume(parser, TokenComma);
    }

    return true;
}

static bool parse_parameter_list(Parser* parser, Vector* params)
{
    consume(parser, TokenLparen);

    if (is_end_of_file(parser)) {
        return false;
    }

    while (parser->current_token.type != TokenRparen) {
        if (!parse_parameter_list_item(parser, params)) {
            return false;
        }
    }

    if (is_end_of_file(parser)) {
        return false;
    }

    consume(parser, TokenRparen);

    size_t count = count_vector(params);

    for (size_t i = 0; i < count; i++) {
        ASTNode* param = get_vector_at(params, i);
        param->parameter.position = count - i - 1;
    }

    return true;
}

ASTNode* parse_function_call(Parser* parser)
{
    ASTNode* ast = create_function_call_node(parser->previous_token);

    if (!parse_argument_list(parser, &ast->function_call.args)) {
        free_ast_node(ast);

        return NULL;
    }

    return ast;
}

static bool parse_function_expression_body(Parser* parser, ASTNode* ast)
{
    Token token = parser->current_token;

    consume(parser, TokenEqual);

    ASTNode* expression = parse_expression(parser);

    if (!expression) {
        return false;
    }

    ASTNode* body = create_compound_node(NULL);
    ASTNode* statement = create_return_node(token, expression);
    ast->function_definition.body = body;

    push_vector_item(&body->compound.statements, statement);

    return true;
}

bool parse_function_body(Parser* parser, ASTNode* ast)
{
    if (is_end_of_file(parser)) {
        return false;
    }

    if (parser->current_token.type == TokenEqual) {
        return parse_function_expression_body(parser, ast);
    }

    consume(parser, TokenLbrace);

    ASTNode* body = create_compound_node(NULL);
    ast->function_definition.body = body;

    if (!parse_blocklevel_statements(parser, &body->compound.statements)) {
        return false;
    }

    consume(parser, TokenRbrace);

    return true;
}

ASTNode* parse_function_definition(Parser* parser)
{
    consume(parser, TokenKeywordFunc);

    if (is_end_of_file(parser)) {
        return NULL;
    }

    Token token = parser->current_token;
    StringObject* id = copy_string_object(token.chars, token.length);

    if (parser->current_token.type != TokenIdentifier) {
        expected_token_error(TokenIdentifier, parser->current_token);
    }

    consume(parser, TokenIdentifier);

    if (is_end_of_file(parser)) {
        return NULL;
    }

    ASTNode* ast = create_function_definition_node(token, id);

    if (!parse_parameter_list(parser, &ast->function_definition.params)) {
        free_ast_node(ast);

        return NULL;
    }

    parse_function_return_type(parser, ast);

    if (!parse_function_body(parser, ast)) {
        free_ast_node(ast);

        return NULL;
    }

    return ast;
}
