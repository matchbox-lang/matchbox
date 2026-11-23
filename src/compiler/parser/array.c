#include "compiler/parser/array.h"
#include "common/vector.h"
#include "compiler/ast/ast.h"
#include "compiler/parser/expression.h"
#include "compiler/parser/function.h"
#include "compiler/parser/slice.h"
#include "compiler/parser/type.h"

ASTNode* parse_array_initializer(Parser* parser, Token token, TypeKind kind)
{
    ASTNode* ast = create_ast_node(ASTArray);
    ast->array.token = token;
    ast->array.type_id = (Type){.kind = kind};
    ast->array.fill = true;

    bool complete = parse_dimensions(parser, &ast->array.type_id)
        && !is_end_of_file(parser) && parse_argument_list(parser, &ast->array.elements);

    if (!complete) {
        free_ast_node(ast);

        return NULL;
    }

    return ast;
}

static ASTNode* parse_array_element(Parser* parser, ASTNode* ast)
{
    ASTNode* element = parse_expression(parser);

    if (!element) {
        return NULL;
    }

    push_vector_item(&ast->array.elements, element);

    if (parser->current_token.type == TokenComma) {
        consume(parser, TokenComma);

        return ast;
    }

    if (parser->current_token.type != TokenRsquare && !is_end_of_file(parser)) {
        expected_tokens_error(TokenComma, TokenRsquare, parser->current_token);
    }

    return ast;
}

static bool parse_array_elements(Parser* parser, ASTNode* ast)
{
    ASTNode* result = ast;

    while (result && parser->current_token.type != TokenRsquare && !is_end_of_file(parser)) {
        result = parse_array_element(parser, ast);
    }

    return result && !is_end_of_file(parser);
}

ASTNode* parse_array(Parser* parser)
{
    ASTNode* ast = create_ast_node(ASTArray);
    ast->array.token = parser->current_token;

    consume(parser, TokenLsquare);

    bool complete = parse_array_elements(parser, ast);

    if (!complete) {
        free_ast_node(ast);

        return NULL;
    }

    consume(parser, TokenRsquare);

    return ast;
}

ASTNode* parse_index(Parser* parser, ASTNode* array)
{
    ASTNode* ast = create_ast_node(ASTIndex);
    ast->index.array = array;
    ast->index.token = parser->current_token;

    consume(parser, TokenLsquare);

    ast->index.index = parse_expression(parser);

    if (parser->current_token.type == TokenRange) {
        return parse_slice(parser, ast);
    }

    if (!ast->index.index || is_end_of_file(parser)) {
        free_ast_node(ast);

        return NULL;
    }

    consume(parser, TokenRsquare);

    return ast;
}
