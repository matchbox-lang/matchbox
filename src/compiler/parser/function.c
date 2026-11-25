#include "compiler/parser/function.h"
#include "common/vector.h"
#include "compiler/ast/ast.h"
#include "compiler/ast/generic.h"
#include "compiler/parser/expression.h"
#include "compiler/parser/generic.h"
#include "compiler/parser/type.h"
#include "compiler/diagnostics.h"
#include "runtime/string_object.h"

typedef bool (*ParseListItem)(Parser* parser, Vector* items, ASTNode* function);

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

static void validate_missing_argument_expression(Token token)
{
    if (token.type == TokenComma || token.type == TokenRparen) {
        expected_expression_error(token);
    }
}

static bool parse_argument_list_item(Parser* parser, Vector* args, ASTNode* function)
{
    (void)function;

    ASTNode* expr = parse_expression(parser);

    if (!expr) {
        validate_missing_argument_expression(parser->current_token);

        return false;
    }

    push_vector_item(args, expr);

    if (parser->current_token.type != TokenComma && parser->current_token.type != TokenRparen && !is_end_of_file(parser)) {
        expected_tokens_error(TokenComma, TokenRparen, parser->current_token);
    }

    if (parser->current_token.type == TokenComma) {
        consume(parser, TokenComma);
    }

    return true;
}

static bool parse_parenthesized_list(Parser* parser, Vector* items, ParseListItem parse_item, ASTNode* function)
{
    consume(parser, TokenLparen);

    if (is_end_of_file(parser)) {
        return false;
    }

    bool complete = true;

    while (complete && parser->current_token.type != TokenRparen) {
        complete = parse_item(parser, items, function);
    }

    if (!complete || is_end_of_file(parser)) {
        return false;
    }

    consume(parser, TokenRparen);

    return true;
}

bool parse_argument_list(Parser* parser, Vector* args)
{
    return parse_parenthesized_list(parser, args, parse_argument_list_item, NULL);
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

static bool parse_parameter_type(Parser* parser, ASTNode* ast, ASTNode* function)
{
    if (ast->parameter.variadic && parser->current_token.type != TokenColon) {
        expected_token_error(TokenColon, parser->current_token);
    }

    if (parser->current_token.type != TokenColon) {
        ast->parameter.type_id = add_implicit_generic_parameter(function, ast->parameter.token);

        return true;
    }

    consume(parser, TokenColon);

    return parse_declared_type(parser, &ast->parameter.type_id, &ast->parameter.reference_type);
}

static ASTNode* parse_parameter_binding(Parser* parser)
{
    bool variadic = parser->current_token.type == TokenSpread;

    if (variadic) {
        consume(parser, TokenSpread);
    }

    Token token = parser->current_token;
    StringObject* id = copy_string_object(token.chars, token.length);

    consume(parser, TokenIdentifier);

    ASTNode* ast = create_parameter_node(token, id);
    ast->parameter.variadic = variadic;

    return ast;
}

static ASTNode* parse_parameter(Parser* parser, ASTNode* function)
{
    if (is_end_of_file(parser)) {
        return NULL;
    }

    ASTNode* ast = parse_parameter_binding(parser);

    if (!parse_parameter_type(parser, ast, function) || !parse_parameter_default(parser, ast)) {
        free_ast_node(ast);

        return NULL;
    }

    return ast;
}

static bool parse_parameter_list_item(Parser* parser, Vector* params, ASTNode* function)
{
    ASTNode* param = parse_parameter(parser, function);

    if (!param) {
        return false;
    }

    size_t count = count_vector(params);
    ASTNode* previous = get_vector_at(params, count - 1);

    if (previous && previous->parameter.variadic) {
        semantic_token_error("Variadic parameter must be last: ", previous->parameter.token);
    }

    push_vector_item(params, param);

    if (parser->current_token.type != TokenComma && parser->current_token.type != TokenRparen && !is_end_of_file(parser)) {
        expected_tokens_error(TokenComma, TokenRparen, parser->current_token);
    }

    if (parser->current_token.type == TokenComma) {
        consume(parser, TokenComma);
    }

    return true;
}

static bool parse_parameter_list(Parser* parser, ASTNode* function)
{
    Vector* params = &function->function_definition.params;

    if (!parse_parenthesized_list(parser, params, parse_parameter_list_item, function)) {
        return false;
    }

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

    if (!parse_type_arguments(parser, &ast->function_call.type_args)
        || !parse_argument_list(parser, &ast->function_call.args)) {
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

    return parse_braced_block(parser, &ast->function_definition.body);
}

static ASTNode* parse_function_declaration(Parser* parser, Vector* types)
{
    Token source = parser->current_token;

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

    initialize_function_generics(ast, parser->generic_bindings, types, source);

    parser->generic_bindings = &ast->function_definition.generics->bindings;

    if (!parse_generic_names(parser, ast) || !parse_parameter_list(parser, ast)) {
        free_ast_node(ast);

        return NULL;
    }

    if (!parse_function_return_type(parser, ast)) {
        free_ast_node(ast);

        return NULL;
    }

    GenericBindings* parent = parser->generic_bindings->parent;

    finish_function_generics(ast);

    if (!ast->function_definition.generics) {
        parser->generic_bindings = parent;
    }

    if (!parse_function_body(parser, ast)) {
        free_ast_node(ast);

        return NULL;
    }

    return ast;
}

ASTNode* parse_function_definition(Parser* parser)
{
    GenericBindings* parent = parser->generic_bindings;
    ASTNode* ast = parse_function_declaration(parser, NULL);
    parser->generic_bindings = parent;

    if (!ast || !is_generic_function(ast)) {
        return ast;
    }

    ast->function_definition.generics->specializations = create_compound_node(NULL);

    return ast;
}

ASTNode* parse_function_specialization(ASTNode* template, Vector* types)
{
    GenericFunction* generics = template->function_definition.generics;
    Token source = generics->source;
    Parser parser = {0};

    init_parser(&parser, NULL);
    init_lexer(&parser.lexer, source.chars);

    parser.lexer.current.line = source.line;
    parser.lexer.current.column = source.column;
    parser.generic_bindings = generics->bindings.parent;

    advance(&parser);

    ASTNode* ast = parse_function_declaration(&parser, types);
    ast->function_definition.generics->template = template;

    if (!ast->function_definition.has_explicit_return_type) {
        ast->function_definition.return_type_id = (Type){.kind = TypeUnknown};
    }

    return ast;
}
