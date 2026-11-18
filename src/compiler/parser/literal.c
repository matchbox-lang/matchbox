#include "compiler/parser/literal.h"
#include "compiler/literal.h"
#include <float.h>
#include <stdint.h>

TypeKind infer_integer_literal_type(uint64_t value, bool negative)
{
    uint64_t limit32 = (uint64_t)INT32_MAX + negative;
    uint64_t limit64 = (uint64_t)INT64_MAX + negative;

    if (value <= limit32) {
        return TypeI32;
    }

    if (value <= limit64) {
        return TypeI64;
    }

    return TypeUnknown;
}

static TypeKind infer_float_literal_type(double value)
{
    if (value > FLT_MAX) {
        return TypeF64;
    }

    float narrowed = (float)value;

    if ((double)narrowed == value) {
        return TypeF32;
    }

    return TypeF64;
}

ASTNode* parse_boolean_literal_node(Parser* parser, Token token)
{
    bool value = token.type == TokenKeywordTrue;
    ASTNode* ast = create_boolean_node(token, value);

    consume(parser, token.type);

    return ast;
}

static ASTNode* finish_integer_literal_node(Parser* parser, Token token, uint64_t value)
{
    Type type_id = {.kind = infer_integer_literal_type(value, false)};
    ASTNode* ast = create_integer_node(token, value, type_id);

    consume(parser, token.type);

    return ast;
}

ASTNode* parse_integer_literal_node(Parser* parser, Token token)
{
    uint64_t value = parse_decimal_literal(token);

    return finish_integer_literal_node(parser, token, value);
}

ASTNode* parse_float_literal_node(Parser* parser, Token token)
{
    double value = parse_float_literal(token);
    Type type_id = {.kind = infer_float_literal_type(value)};
    ASTNode* ast = create_float_node(token, value, type_id);

    consume(parser, token.type);

    return ast;
}

ASTNode* parse_binary_literal_node(Parser* parser, Token token)
{
    uint64_t value = parse_binary_literal(token);

    return finish_integer_literal_node(parser, token, value);
}

ASTNode* parse_hexadecimal_literal_node(Parser* parser, Token token)
{
    uint64_t value = parse_hexadecimal_literal(token);

    return finish_integer_literal_node(parser, token, value);
}

ASTNode* parse_octal_literal_node(Parser* parser, Token token)
{
    uint64_t value = parse_octal_literal(token);

    return finish_integer_literal_node(parser, token, value);
}

ASTNode* parse_character_literal(Parser* parser)
{
    Token token = parser->current_token;
    DecodedScalar scalar;
    bool valid = decode_literal_scalar(token.chars + 1, token.length - 2, &scalar);

    if (!valid) {
        expected_expression_error(token);
    }

    ASTNode* ast = create_character_node(token, scalar.value);

    consume(parser, TokenCharacterLiteral);

    return ast;
}
