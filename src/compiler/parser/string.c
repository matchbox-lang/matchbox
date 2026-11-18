#include "compiler/parser/string.h"
#include "builtin/builtin.h"
#include "builtin/string.h"
#include "common/memory.h"
#include "common/vector.h"
#include "compiler/diagnostics.h"
#include "compiler/literal.h"
#include "compiler/parser/expression.h"
#include "compiler/source_position.h"
#include "runtime/string_object.h"

typedef struct StringReader
{
    Token token;
    SourcePosition current;
    const char* end;
    size_t width;
    bool raw;
    bool interpolated;
} StringReader;

static _Noreturn void invalid_string_error(Token token, bool escape)
{
    if (escape) {
        fatal_source_error(token.line, token.column, "Invalid escape sequence in string literal");
    }

    fatal_source_error(token.line, token.column, "Invalid UTF-8 encoding in string literal");
}

static void advance_string_reader(StringReader* reader, size_t length)
{
    for (size_t i = 0; i < length; i++) {
        char c = *reader->current.chars++;
        reader->current.column++;

        if (c == '\n') {
            reader->current.line++;
            reader->current.column = 1;
        }
    }
}

static StringReader create_string_reader(Token token)
{
    StringReader reader = {.token = token, .current = {token.chars, token.line, token.column}, .width = 1};
    reader.interpolated = token.chars[0] == '$';

    if (reader.interpolated) {
        advance_string_reader(&reader, 1);
    }

    char delimiter = *reader.current.chars;
    size_t run = 0;
    reader.raw = delimiter == '`';

    while (reader.current.chars[run] == delimiter) {
        run++;
    }

    if (run >= 3) {
        reader.width = run;
    }

    advance_string_reader(&reader, reader.width);

    reader.end = token.chars + token.length - reader.width;

    return reader;
}

static ASTNode* parse_string_segment(StringReader* reader)
{
    size_t capacity = reader->end - reader->current.chars;
    char* bytes = allocate_memory(capacity + 1);
    size_t count = 0;

    while (reader->current.chars < reader->end) {
        const char* chars = reader->current.chars;
        size_t length = reader->end - chars;
        char c = *chars;
        bool brace_pair = reader->interpolated && length >= 2 && chars[1] == c && (c == '{' || c == '}');

        if (reader->interpolated && c == '{' && !brace_pair) {
            break;
        }

        if (brace_pair) {
            bytes[count++] = c;

            advance_string_reader(reader, 2);
            continue;
        }

        DecodedScalar scalar;
        bool valid;

        if (reader->raw) {
            valid = decode_utf8(chars, length, &scalar);
        } else {
            valid = decode_literal_scalar(chars, length, &scalar);
        }

        if (!valid) {
            free_memory(bytes);
            invalid_string_error(reader->token, !reader->raw && c == '\\');
        }

        size_t encoded_length = encode_utf8(scalar.value, bytes + count);
        count += encoded_length;

        advance_string_reader(reader, scalar.length);
    }

    StringObject* value = copy_string_object(bytes, count);
    ASTNode* ast = create_string_node(reader->token, value);

    free_memory(bytes);

    return ast;
}

static ASTNode* parse_string_interpolation(Parser* parser, StringReader* reader)
{
    advance_string_reader(reader, 1);

    parser->lexer.current = reader->current;

    advance(parser);

    ASTNode* expression = parse_expression(parser);

    if (!expression) {
        expected_expression_error(parser->current_token);
    }

    if (parser->current_token.type != TokenRbrace) {
        expected_token_error(TokenRbrace, parser->current_token);
    }

    reader->current = parser->lexer.current;

    return expression;
}

static void append_string_part(ASTNode* call, ASTNode* part)
{
    if (part->type == ASTString && part->string_literal.value->length == 0) {
        free_ast_node(part);

        return;
    }

    if (part->type == ASTBuiltinCall && part->builtin_call.builtin->entry == builtin_string) {
        Vector* args = &part->builtin_call.args;

        for (size_t i = 0; i < args->count; i++) {
            push_vector_item(&call->builtin_call.args, args->data[i]);
        }

        args->count = 0;

        free_ast_node(part);

        return;
    }

    push_vector_item(&call->builtin_call.args, part);
}

ASTNode* parse_string_literal(Parser* parser)
{
    Token token = parser->current_token;
    Token previous = parser->previous_token;
    Lexer lexer = parser->lexer;
    StringReader reader = create_string_reader(token);
    ASTNode* ast = parse_string_segment(&reader);

    if (reader.current.chars < reader.end) {
        const Builtin* builtin = resolve_builtin("string", NULL, 0);
        ASTNode* call = create_builtin_call_node(token, builtin);

        append_string_part(call, ast);

        ast = call;
    }

    while (reader.current.chars < reader.end) {
        ASTNode* expression = parse_string_interpolation(parser, &reader);

        append_string_part(ast, expression);

        ASTNode* segment = parse_string_segment(&reader);

        append_string_part(ast, segment);
    }

    parser->lexer = lexer;
    parser->current_token = token;
    parser->previous_token = previous;

    consume(parser, TokenStringLiteral);

    return ast;
}
