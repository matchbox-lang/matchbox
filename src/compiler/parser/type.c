#include "compiler/parser/type.h"
#include "common/array_type.h"
#include "common/memory.h"
#include "common/vector.h"
#include "compiler/builtin_constant.h"
#include "compiler/diagnostics.h"
#include "compiler/parser/array.h"
#include "compiler/parser/expression.h"
#include "compiler/parser/function.h"

static _Noreturn void expected_type_error(Token token)
{
    fatal_token_error(token, "", "Expected type but found ");
}

void consume_type(Parser* parser)
{
    if (!is_type_token(parser->current_token.type)) {
        expected_type_error(parser->current_token);
    }

    consume(parser, parser->current_token.type);
}

ReferenceType parse_reference_type(Parser* parser)
{
    if (parser->current_token.type == TokenAmpersand) {
        consume(parser, TokenAmpersand);

        return RefShared;
    }

    if (parser->current_token.type == TokenCircumflex) {
        consume(parser, TokenCircumflex);

        return RefExclusive;
    }

    return RefNone;
}

ASTNode* parse_type_expression(Parser* parser)
{
    Token type_token = parser->current_token;
    TypeKind kind = get_token_data_type(type_token.type);

    consume(parser, type_token.type);

    if (parser->current_token.type == TokenLsquare) {
        return parse_array_initializer(parser, type_token, kind);
    }

    if (parser->current_token.type != TokenDot) {
        return parse_function_call(parser);
    }

    consume(parser, TokenDot);

    if (is_end_of_file(parser)) {
        return NULL;
    }

    Token name = parser->current_token;

    consume(parser, TokenIdentifier);

    ASTNode* ast = resolve_builtin_constant(kind, name);
    if (!ast) {
        fatal_source_error(name.line, name.column, "Unknown built-in constant %.*s.%.*s",
            (int)type_token.length, type_token.chars, (int)name.length, name.chars);
    }

    return ast;
}

static void parse_dimension(Parser* parser, Vector* dimensions)
{
    consume(parser, TokenLsquare);

    if (parser->current_token.type == TokenRsquare) {
        push_vector_item(dimensions, NULL);
        consume(parser, TokenRsquare);

        return;
    }

    ASTNode* count = parse_expression(parser);

    if (!count) {
        expected_expression_error(parser->current_token);
    }

    push_vector_item(dimensions, count);
    consume(parser, TokenRsquare);
}

void parse_dimensions(Parser* parser, Type* type)
{
    if (parser->current_token.type != TokenLsquare) {
        return;
    }

    ArrayType* array_type = allocate_zeroed_memory(1, sizeof(ArrayType) + 1);
    Vector* dimensions = allocate_memory(sizeof(Vector));
    array_type->element_type = *type;
    array_type->dimensions = dimensions;

    init_vector(dimensions);

    *type = (Type){.kind = TypeArray, .array = array_type};

    while (parser->current_token.type == TokenLsquare) {
        parse_dimension(parser, dimensions);
    }
}

void parse_declared_type(Parser* parser, Type* type, ReferenceType* reference_type)
{
    *reference_type = parse_reference_type(parser);
    TypeKind kind = get_token_data_type(parser->current_token.type);
    *type = (Type){.kind = kind};

    consume_type(parser);
    parse_dimensions(parser, type);
}

void parse_function_return_type(Parser* parser, ASTNode* ast)
{
    if (parser->current_token.type != TokenArrow) {
        return;
    }

    consume(parser, TokenArrow);

    parse_declared_type(parser, &ast->function_definition.return_type_id,
        &ast->function_definition.return_reference_type);

    ast->function_definition.has_explicit_return_type = true;
}

void parse_variable_type(Parser* parser, ASTNode* ast)
{
    if (parser->current_token.type == TokenColon) {
        consume(parser, TokenColon);

        parse_declared_type(parser, &ast->variable_definition.type_id, &ast->variable_definition.reference_type);

        return;
    }

    if (parser->current_token.type != TokenEqual) {
        expected_token_error(TokenEqual, parser->current_token);
    }
}
