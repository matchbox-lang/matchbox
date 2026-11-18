#include "compiler/parser/expression.h"
#include "compiler/diagnostics.h"
#include "compiler/parser/array.h"
#include "compiler/parser/control_flow.h"
#include "compiler/parser/literal.h"
#include "compiler/parser/match.h"
#include "compiler/parser/string.h"
#include "compiler/parser/type.h"
#include "compiler/parser/variable.h"
#include "compiler/token.h"

static ASTNode* parse_prefix(Parser* parser);

static _Noreturn void expected_operand_error(Token token)
{
    fatal_token_error(token, "", "Expected operand but found ");
}

ASTNode* parse_group_expression(Parser* parser)
{
    consume(parser, TokenLparen);

    ASTNode* ast = parse_expression(parser);

    if (!ast && parser->current_token.type == TokenRparen) {
        expected_expression_error(parser->current_token);
    }

    if (!ast || is_end_of_file(parser)) {
        free_ast_node(ast);

        return NULL;
    }

    consume(parser, TokenRparen);

    return ast;
}

static ASTNode* parse_primary(Parser* parser)
{
    switch (parser->current_token.type) {
        case TokenLsquare:
            return parse_array(parser);
        case TokenKeywordTrue:
        case TokenKeywordFalse:
            return parse_boolean_literal_node(parser, parser->current_token);
        case TokenKeywordIf:
            return parse_conditional(parser, true);
        case TokenKeywordMatch:
            return parse_match(parser, true);
        case TokenKeywordI8:
        case TokenKeywordI16:
        case TokenKeywordI32:
        case TokenKeywordI64:
        case TokenKeywordU8:
        case TokenKeywordU16:
        case TokenKeywordU32:
        case TokenKeywordU64:
        case TokenKeywordIsize:
        case TokenKeywordUsize:
        case TokenKeywordF32:
        case TokenKeywordF64:
        case TokenKeywordChar:
        case TokenKeywordString:
        case TokenKeywordBool:
        case TokenKeywordVoid:
            return parse_type_expression(parser);
        case TokenStringLiteral:
            return parse_string_literal(parser);
        case TokenCharacterLiteral:
            return parse_character_literal(parser);
        case TokenIntegerLiteral:
            return parse_integer_literal_node(parser, parser->current_token);
        case TokenFloatLiteral:
            return parse_float_literal_node(parser, parser->current_token);
        case TokenBinaryLiteral:
            return parse_binary_literal_node(parser, parser->current_token);
        case TokenHexadecimalLiteral:
            return parse_hexadecimal_literal_node(parser, parser->current_token);
        case TokenOctalLiteral:
            return parse_octal_literal_node(parser, parser->current_token);
        case TokenLparen:
            return parse_group_expression(parser);
        case TokenIdentifier:
            return parse_identifier(parser);
        case TokenIncompleteInput:
        case TokenEof:
            return NULL;
        default:
            expected_expression_error(parser->current_token);
            return NULL;
    }
}

static ASTNode* parse_postfix(Parser* parser)
{
    ASTNode* ast = parse_primary(parser);

    while (ast && parser->current_token.type == TokenLsquare) {
        ast = parse_index(parser, ast);
    }

    if (!ast || ast->type != ASTIndex || !is_assignment_token(parser->current_token.type)) {
        return ast;
    }

    ast->type = ASTIndexAssignment;
    ast->index.operator = parser->current_token;

    consume(parser, parser->current_token.type);

    ast->index.value = parse_expression(parser);

    if (!ast->index.value) {
        free_ast_node(ast);

        return NULL;
    }

    return ast;
}

static ASTNode* parse_prefix_operand(Parser* parser)
{
    if (is_prefix_token(parser->current_token.type)) {
        return parse_prefix(parser);
    }

    Token token = parser->current_token;
    ASTNode* expr = parse_postfix(parser);

    if (!expr) {
        return NULL;
    }

    if (!is_prefix(expr) && !is_prefix_operand(expr)) {
        expected_operand_error(token);
    }

    return expr;
}

static ASTNode* parse_prefix(Parser* parser)
{
    if (!is_prefix_token(parser->current_token.type)) {
        return parse_postfix(parser);
    }

    Token token = parser->current_token;
    consume(parser, token.type);
    ASTNode* expr = parse_prefix_operand(parser);

    if (!expr) {
        return NULL;
    }

    if (token.type == TokenMinus && expr->type == ASTInteger) {
        expr->integer_literal.type_id = (Type){.kind = infer_integer_literal_type(expr->integer_literal.value, true)};
    }

    return create_prefix_node(token, expr);
}

static ASTNode* parse_exponent(Parser* parser)
{
    ASTNode* expr = parse_prefix(parser);
    Token token = parser->current_token;

    if (token.type == TokenPower) {
        consume(parser, token.type);
        expr = create_binary_node(expr, parse_exponent(parser), token);
    }

    return expr;
}

static ASTNode* parse_factor(Parser* parser)
{
    ASTNode* expr = parse_exponent(parser);
    Token token = parser->current_token;

    while (is_factor_token(token.type)) {
        consume(parser, token.type);
        expr = create_binary_node(expr, parse_exponent(parser), token);
        token = parser->current_token;
    }

    return expr;
}

static ASTNode* parse_term(Parser* parser)
{
    ASTNode* expr = parse_factor(parser);
    Token token = parser->current_token;

    while (is_term_token(token.type)) {
        consume(parser, token.type);
        expr = create_binary_node(expr, parse_factor(parser), token);
        token = parser->current_token;
    }

    return expr;
}

static ASTNode* parse_shift(Parser* parser)
{
    ASTNode* expr = parse_term(parser);
    Token token = parser->current_token;

    while (is_shift_token(token.type)) {
        consume(parser, token.type);
        expr = create_binary_node(expr, parse_term(parser), token);
        token = parser->current_token;
    }

    return expr;
}

static ASTNode* parse_comparison(Parser* parser)
{
    ASTNode* expr = parse_shift(parser);
    Token token = parser->current_token;

    while (is_comparison_token(token.type)) {
        consume(parser, token.type);
        expr = create_binary_node(expr, parse_shift(parser), token);
        token = parser->current_token;
    }

    return expr;
}

static ASTNode* parse_equality(Parser* parser)
{
    ASTNode* expr = parse_comparison(parser);
    Token token = parser->current_token;

    while (is_equality_token(token.type)) {
        consume(parser, token.type);
        expr = create_binary_node(expr, parse_comparison(parser), token);
        token = parser->current_token;
    }

    return expr;
}

static ASTNode* parse_bitwise_and(Parser* parser)
{
    ASTNode* expr = parse_equality(parser);
    Token token = parser->current_token;

    while (token.type == TokenAmpersand) {
        consume(parser, token.type);
        expr = create_binary_node(expr, parse_equality(parser), token);
        token = parser->current_token;
    }

    return expr;
}

static ASTNode* parse_bitwise_xor(Parser* parser)
{
    ASTNode* expr = parse_bitwise_and(parser);
    Token token = parser->current_token;

    while (token.type == TokenCircumflex) {
        consume(parser, token.type);
        expr = create_binary_node(expr, parse_bitwise_and(parser), token);
        token = parser->current_token;
    }

    return expr;
}

static ASTNode* parse_bitwise_or(Parser* parser)
{
    ASTNode* expr = parse_bitwise_xor(parser);
    Token token = parser->current_token;

    while (token.type == TokenPipe) {
        consume(parser, token.type);
        expr = create_binary_node(expr, parse_bitwise_xor(parser), token);
        token = parser->current_token;
    }

    return expr;
}

static ASTNode* parse_boolean_and(Parser* parser)
{
    ASTNode* expr = parse_bitwise_or(parser);
    Token token = parser->current_token;

    while (token.type == TokenKeywordAnd) {
        consume(parser, token.type);
        expr = create_binary_node(expr, parse_bitwise_or(parser), token);
        token = parser->current_token;
    }

    return expr;
}

static ASTNode* parse_boolean_or(Parser* parser)
{
    ASTNode* expr = parse_boolean_and(parser);
    Token token = parser->current_token;

    while (token.type == TokenKeywordOr) {
        consume(parser, token.type);
        expr = create_binary_node(expr, parse_boolean_and(parser), token);
        token = parser->current_token;
    }

    return expr;
}

ASTNode* parse_expression(Parser* parser)
{
    return parse_boolean_or(parser);
}
