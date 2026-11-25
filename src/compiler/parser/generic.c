#include "compiler/parser/generic.h"
#include "common/memory.h"
#include "compiler/ast/generic.h"
#include "compiler/lexer/token.h"
#include "compiler/parser/type.h"
#include "compiler/diagnostics.h"

static size_t find_generic_name(Vector* names, Token token)
{
    size_t index = 0;

    while (index < names->count && !token_texts_equal(names->data[index], &token)) {
        index++;
    }

    return index;
}

static Type find_named_type(GenericBindings* bindings, Token token)
{
    if (!bindings) {
        semantic_token_error("Unknown generic type ", token);
    }

    size_t index = find_generic_name(&bindings->names, token);

    if (index < bindings->names.count) {
        return get_generic_argument_type(bindings, index);
    }

    return find_named_type(bindings->parent, token);
}

Type parse_named_type(Parser* parser, Token token)
{
    return find_named_type(parser->generic_bindings, token);
}

bool is_type_argument_list(Parser* parser)
{
    if (parser->current_token.type != TokenLsquare) {
        return false;
    }

    Lexer lexer = parser->lexer;
    Token token = scan_token(&lexer);
    size_t depth = 1;

    while (depth && token.type != TokenEof && token.type != TokenIncompleteInput) {
        depth += token.type == TokenLsquare;
        depth -= token.type == TokenRsquare;
        token = scan_token(&lexer);
    }

    return !depth && token.type == TokenLparen;
}

static bool parse_type_argument(Parser* parser, Vector* types)
{
    Type* type = allocate_zeroed_memory(1, sizeof(Type));
    ReferenceType reference;

    push_vector_item(types, type);

    if (!parse_declared_type(parser, type, &reference)) {
        return false;
    }

    if (reference != RefNone) {
        semantic_token_error("Type arguments cannot include a reference qualifier: ", parser->previous_token);
    }

    if (parser->current_token.type == TokenComma) {
        consume(parser, TokenComma);
    } else if (parser->current_token.type != TokenRsquare) {
        expected_tokens_error(TokenComma, TokenRsquare, parser->current_token);
    }

    return true;
}

bool parse_type_arguments(Parser* parser, Vector* types)
{
    if (parser->current_token.type != TokenLsquare) {
        return true;
    }

    consume(parser, TokenLsquare);

    if (parser->current_token.type == TokenRsquare) {
        expected_expression_error(parser->current_token);
    }

    bool complete = true;

    while (complete && !is_end_of_file(parser) && parser->current_token.type != TokenRsquare) {
        complete = parse_type_argument(parser, types);
    }

    if (!complete || is_end_of_file(parser)) {
        return false;
    }

    consume(parser, TokenRsquare);

    return true;
}

static bool parse_generic_name(Parser* parser, Vector* names)
{
    Token token = parser->current_token;

    consume(parser, TokenIdentifier);

    size_t index = find_generic_name(names, token);

    if (index < names->count) {
        semantic_token_error("Duplicate generic type parameter ", token);
    }

    Token* name = allocate_memory(sizeof(Token));
    *name = token;

    push_vector_item(names, name);

    if (parser->current_token.type == TokenComma) {
        consume(parser, TokenComma);
    } else if (parser->current_token.type != TokenRsquare) {
        expected_tokens_error(TokenComma, TokenRsquare, parser->current_token);
    }

    return true;
}

bool parse_generic_names(Parser* parser, ASTNode* ast)
{
    if (parser->current_token.type != TokenLsquare) {
        return true;
    }

    consume(parser, TokenLsquare);

    if (parser->current_token.type == TokenRsquare) {
        expected_token_error(TokenIdentifier, parser->current_token);
    }

    while (!is_end_of_file(parser) && parser->current_token.type != TokenRsquare) {
        parse_generic_name(parser, &ast->function_definition.generics->bindings.names);
    }

    if (is_end_of_file(parser)) {
        return false;
    }

    consume(parser, TokenRsquare);

    return true;
}

