#include "compiler/parser/test_block.h"
#include "compiler/ast/ast.h"
#include "compiler/parser/function.h"
#include "compiler/parser/string.h"
#include "compiler/diagnostics.h"
#include "runtime/string_object.h"
#include <stdio.h>
#include <string.h>

static void parse_test_skip(Parser* parser, ASTNode* ast)
{
    Token token = parser->current_token;

    if (token.type != TokenIdentifier || token.length != 4) {
        return;
    }

    if (memcmp(token.chars, "skip", 4) != 0) {
        return;
    }

    ast->function_definition.is_test_skipped = true;

    consume(parser, TokenIdentifier);
}

static void parse_test_name(Parser* parser, ASTNode* ast)
{
    if (parser->current_token.type != TokenStringLiteral) {
        return;
    }

    ast->function_definition.test_name = parser->current_token;
    ASTNode* name = parse_string_literal(parser);

    if (name->type != ASTString) {
        Token token = ast->function_definition.test_name;

        fatal_token_error(token, "", "Test name must be a string literal: ");
    }

    free_ast_node(name);
}

static StringObject* create_test_identifier(const Parser* parser, Token token)
{
    char name[64];
    size_t statement_count = parser->top_level->compound.statements.count;

    snprintf(name, sizeof(name), "$test_%lu_%d_%d", (unsigned long)statement_count, token.line, token.column);

    size_t length = strlen(name);

    return copy_string_object(name, length);
}

static ASTNode* create_test_block_node(const Parser* parser, Token token)
{
    StringObject* id = create_test_identifier(parser, token);
    ASTNode* ast = create_function_definition_node(token, id);

    ast->function_definition.is_test = true;
    ast->function_definition.has_explicit_return_type = true;
    ast->function_definition.return_type_id = (Type){.kind = TypeVoid};

    return ast;
}

static bool parse_test_body(Parser* parser, ASTNode* ast)
{
    if (is_end_of_file(parser)) {
        return false;
    }

    if (parser->current_token.type != TokenLbrace) {
        expected_token_error(TokenLbrace, parser->current_token);
    }

    return parse_function_body(parser, ast);
}

ASTNode* parse_test_block(Parser* parser)
{
    Token token = parser->current_token;

    consume(parser, TokenKeywordTest);

    ASTNode* ast = create_test_block_node(parser, token);

    parse_test_skip(parser, ast);
    parse_test_name(parser, ast);

    if (!parse_test_body(parser, ast)) {
        free_ast_node(ast);

        return NULL;
    }

    return ast;
}
