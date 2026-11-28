#include "compiler/parser/variable.h"
#include "common/array_type.h"
#include "compiler/ast/ast.h"
#include "compiler/parser/expression.h"
#include "compiler/parser/function.h"
#include "compiler/parser/generic.h"
#include "compiler/parser/type.h"
#include "runtime/string_object.h"
#include <stdio.h>
#include <string.h>

static ASTNode* parse_assignment(Parser* parser)
{
    Token operator = parser->current_token;
    Token token = parser->previous_token;
    consume(parser, operator.type);

    ASTNode* expr = parse_expression(parser);
    if (!expr) {
        return NULL;
    }

    ASTNode* ast = create_assignment_node(operator, token, expr);

    return ast;
}

static bool parse_variable_initializer(Parser* parser, ASTNode* ast)
{
    if (parser->current_token.type == TokenEqual) {
        consume(parser, TokenEqual);
        ast->variable_definition.expr = parse_expression(parser);

        return ast->variable_definition.expr != NULL;
    }

    ast->variable_definition.expr = create_ast_node(ASTNone);

    if (ast->variable_definition.type_id.kind == TypeUnknown) {
        ast->variable_definition.type_id = (Type){.kind = TypeI32};
    }

    return true;
}

static ASTNode* parse_variable_binding(Parser* parser, Token keyword)
{
    bool constant = keyword.type == TokenKeywordConst;
    bool fixed = constant || keyword.type == TokenKeywordLet;

    consume(parser, parser->current_token.type);

    if (is_end_of_file(parser)) {
        return NULL;
    }

    Token token = parser->current_token;

    consume(parser, TokenIdentifier);

    if (is_end_of_file(parser)) {
        return NULL;
    }

    StringObject* id = copy_string_object(token.chars, token.length);
    ASTNode* ast = create_variable_definition_node(token, id, fixed);
    ast->variable_definition.constant = constant;

    return ast;
}

static bool has_inferred_dimensions(Type type)
{
    if (type.kind != TypeArray || !type.array->dimensions) {
        return false;
    }

    const Vector* dimensions = type.array->dimensions;
    size_t index = 0;

    while (index < dimensions->count && dimensions->data[index]) {
        index++;
    }

    return index < dimensions->count;
}

static ASTNode* parse_variable_value(Parser* parser, ASTNode* ast, Token keyword)
{
    if (!parse_variable_type(parser, ast)) {
        free_ast_node(ast);

        return NULL;
    }

    bool requires_initializer = ast->variable_definition.fixed
        && (ast->variable_definition.constant || ast->variable_definition.type_id.kind != TypeArray);

    if (requires_initializer && parser->current_token.type != TokenEqual && !is_end_of_file(parser)) {
        expected_token_error(TokenEqual, parser->current_token);
    }

    bool incomplete = is_end_of_file(parser)
        && (requires_initializer || has_inferred_dimensions(ast->variable_definition.type_id));

    if (incomplete || !parse_variable_initializer(parser, ast)) {
        free_ast_node(ast);

        return NULL;
    }

    if (ast->variable_definition.constant) {
        ast->variable_definition.expr = create_prefix_node(keyword, ast->variable_definition.expr);
    }

    return ast;
}

static ASTNode* parse_destructuring_name(Parser* parser, ASTNode* group, Token keyword)
{
    consume(parser, TokenComma);

    Token token = parser->current_token;

    if (is_end_of_file(parser)) {
        return NULL;
    }

    consume(parser, TokenIdentifier);

    StringObject* id = copy_string_object(token.chars, token.length);
    bool fixed = keyword.type != TokenKeywordVar;
    ASTNode* binding = create_variable_definition_node(token, id, fixed);

    push_vector_item(&group->compound.statements, binding);

    return binding;
}

static ASTNode* parse_destructuring(Parser* parser, ASTNode* first, Token keyword)
{
    ASTNode* group = create_compound_node(NULL);

    push_vector_item(&group->compound.statements, first);

    ASTNode* binding = first;

    while (binding && parser->current_token.type == TokenComma) {
        binding = parse_destructuring_name(parser, group, keyword);
    }

    if (!binding) {
        free_ast_node(group);

        return NULL;
    }

    char name[80];
    Token token = first->variable_definition.token;

    snprintf(name, sizeof(name), "$tuple_%d_%d", token.line, token.column);

    size_t length = strlen(name);
    StringObject* id = copy_string_object(name, length);
    token.chars = id->chars;
    token.length = id->length;
    ASTNode* source = create_variable_definition_node(token, id, true);
    source->variable_definition.constant = first->variable_definition.constant;
    source = parse_variable_value(parser, source, keyword);

    if (!source) {
        free_ast_node(group);

        return NULL;
    }

    initialize_destructuring(group, source);

    return group;
}

ASTNode* parse_variable_definition(Parser* parser)
{
    Token keyword = parser->current_token;
    ASTNode* ast = parse_variable_binding(parser, keyword);
    if (!ast) {
        return NULL;
    }

    if (parser->current_token.type == TokenComma) {
        return parse_destructuring(parser, ast, keyword);
    }

    return parse_variable_value(parser, ast, keyword);
}

ASTNode* parse_identifier(Parser* parser)
{
    consume(parser, TokenIdentifier);

    if (is_assignment_token(parser->current_token.type)) {
        return parse_assignment(parser);
    }

    if (parser->current_token.type == TokenLparen || is_type_argument_list(parser)) {
        return parse_function_call(parser);
    }

    return create_variable_node(parser->previous_token);
}

ASTNode* parse_constant_statement(Parser* parser)
{
    Lexer lexer = parser->lexer;
    Token name = scan_token(&lexer);

    if (name.type != TokenIdentifier) {
        return parse_expression(parser);
    }

    Token next = scan_token(&lexer);

    if (next.type == TokenEqual || next.type == TokenColon || next.type == TokenComma) {
        return parse_variable_definition(parser);
    }

    return parse_expression(parser);
}
