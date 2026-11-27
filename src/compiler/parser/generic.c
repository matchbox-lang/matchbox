#include "compiler/parser/generic.h"
#include "common/memory.h"
#include "compiler/ast/generic.h"
#include "compiler/ast/type_alias.h"
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
        return (Type){.kind = TypeUnknown};
    }

    size_t index = find_generic_name(&bindings->names, token);

    if (index < bindings->names.count) {
        return get_generic_argument_type(bindings, index);
    }

    return find_named_type(bindings->parent, token);
}

static bool consume_type_suffix(Parser* parser)
{
    size_t depth = 0;

    while (!is_end_of_file(parser) && (depth || parser->current_token.type == TokenLsquare)) {
        depth += parser->current_token.type == TokenLsquare;
        depth -= parser->current_token.type == TokenRsquare;

        advance(parser);
    }

    return depth == 0;
}

static GenericBindings* find_named_type_bindings(GenericBindings* bindings)
{
    while (bindings && !bindings->names.count) {
        bindings = bindings->parent;
    }

    return bindings;
}

static Type parse_alias_type(Parser* parser, Token token, ReferenceType reference_type, bool* complete)
{
    NamedType* named = allocate_zeroed_memory(1, sizeof(NamedType));
    named->token = token;
    named->reference = reference_type != RefNone;
    named->bindings = find_named_type_bindings(parser->generic_bindings);

    Type type = {.kind = TypeNamed, .named = named};

    if (parser->current_token.type != TokenLsquare) {
        return type;
    }

    named->suffix = parser->current_token;
    *complete = consume_type_suffix(parser);

    return type;
}

Type parse_named_type(Parser* parser, Token token, ReferenceType reference_type, bool* complete)
{
    Type type = find_named_type(parser->generic_bindings, token);

    consume(parser, TokenIdentifier);

    if (type.kind != TypeUnknown) {
        return type;
    }

    return parse_alias_type(parser, token, reference_type, complete);
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

bool parse_generic_type_names(Parser* parser, Vector* names)
{
    if (parser->current_token.type != TokenLsquare) {
        return true;
    }

    consume(parser, TokenLsquare);

    if (parser->current_token.type == TokenRsquare) {
        expected_token_error(TokenIdentifier, parser->current_token);
    }

    while (!is_end_of_file(parser) && parser->current_token.type != TokenRsquare) {
        parse_generic_name(parser, names);
    }

    if (is_end_of_file(parser)) {
        return false;
    }

    consume(parser, TokenRsquare);

    return true;
}


bool parse_generic_names(Parser* parser, ASTNode* ast)
{
    return parse_generic_type_names(parser, &ast->function_definition.generics->bindings.names);
}
