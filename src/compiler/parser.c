#include "compiler/parser.h"
#include "builtin/builtin.h"
#include "builtin/string.h"
#include "common/memory.h"
#include "common/util.h"
#include "common/vector.h"
#include "compiler/ast.h"
#include "compiler/diagnostics.h"
#include "compiler/lexer.h"
#include "compiler/literal.h"
#include "compiler/source_position.h"
#include "compiler/token.h"
#include "runtime/string_object.h"
#include <float.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct StringReader
{
    Token token;
    SourcePosition current;
    const char* end;
    size_t width;
    bool raw;
    bool interpolated;
} StringReader;

static ASTNode* parse_expression(Parser* parser);
static ASTNode* parse_identifier(Parser* parser);
static ASTNode* parse_function_call(Parser* parser);
static ASTNode* parse_prefix(Parser* parser);
static bool parse_blocklevel_statements(Parser* parser, Vector* nodes);
static ASTNode* parse_conditional(Parser* parser, bool allow_single_line_body);
static ASTNode* parse_match(Parser* parser);
static ASTNode* parse_statement(Parser* parser);

static _Noreturn void expected_expression_error(Token token)
{
    fatal_token_error(token, "", "Expected expression but found ");
}

static _Noreturn void expected_operand_error(Token token)
{
    fatal_token_error(token, "", "Expected operand but found ");
}

static _Noreturn void expected_token_error(TokenType type, Token token)
{
    const char* type_name = get_token_type_name(type);

    fatal_token_error(token, "", "Expected %s but found ", type_name);
}

static _Noreturn void expected_type_error(Token token)
{
    fatal_token_error(token, "", "Expected type but found ");
}

static _Noreturn void invalid_string_error(Token token)
{
    fatal_source_error(token.line, token.column, "Invalid UTF-8 or escape in string literal");
}

static void advance(Parser* parser)
{
    parser->previous_token = parser->current_token;
    parser->current_token = scan_token(&parser->lexer);
}

static void consume(Parser* parser, TokenType type)
{
    if (parser->current_token.type != type) {
        expected_token_error(type, parser->current_token);
    }

    advance(parser);
}

static void consume_type(Parser* parser)
{
    if (!is_type_token(parser->current_token.type)) {
        expected_type_error(parser->current_token);
    }

    consume(parser, parser->current_token.type);
}

static ReferenceType parse_reference_type(Parser* parser)
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

static bool is_end_of_file(Parser* parser)
{
    return parser->current_token.type == TokenIncompleteInput || parser->current_token.type == TokenEof;
}

static DataType infer_integer_literal_type(uint64_t value, bool negative)
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

static DataType infer_float_literal_type(double value)
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

static ASTNode* parse_boolean_literal_node(Parser* parser, Token token)
{
    bool value = token.type == TokenKeywordTrue;
    ASTNode* ast = create_boolean_node(token, value);

    consume(parser, token.type);

    return ast;
}

static ASTNode* finish_integer_literal_node(Parser* parser, Token token, uint64_t value)
{
    DataType type_id = infer_integer_literal_type(value, false);
    ASTNode* ast = create_integer_node(token, value, type_id);

    consume(parser, token.type);

    return ast;
}

static ASTNode* parse_integer_literal_node(Parser* parser, Token token)
{
    uint64_t value = parse_decimal_literal(token);

    return finish_integer_literal_node(parser, token, value);
}

static ASTNode* parse_float_literal_node(Parser* parser, Token token)
{
    double value = parse_float_literal(token);
    DataType type_id = infer_float_literal_type(value);
    ASTNode* ast = create_float_node(token, value, type_id);

    consume(parser, token.type);

    return ast;
}

static ASTNode* parse_binary_literal_node(Parser* parser, Token token)
{
    uint64_t value = parse_binary_literal(token);

    return finish_integer_literal_node(parser, token, value);
}

static ASTNode* parse_hexadecimal_literal_node(Parser* parser, Token token)
{
    uint64_t value = parse_hexadecimal_literal(token);

    return finish_integer_literal_node(parser, token, value);
}

static ASTNode* parse_octal_literal_node(Parser* parser, Token token)
{
    uint64_t value = parse_octal_literal(token);

    return finish_integer_literal_node(parser, token, value);
}

static ASTNode* parse_group_expression(Parser* parser)
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

static ASTNode* parse_character_literal(Parser* parser)
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
            invalid_string_error(reader->token);
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

static ASTNode* parse_string_literal(Parser* parser)
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

static ASTNode* parse_primary(Parser* parser)
{
    switch (parser->current_token.type) {
        case TokenKeywordTrue:
        case TokenKeywordFalse:
            return parse_boolean_literal_node(parser, parser->current_token);
        case TokenKeywordIf:
            return parse_conditional(parser, false);
        case TokenKeywordMatch:
            return parse_match(parser);
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
            consume(parser, parser->current_token.type);
            return parse_function_call(parser);
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

static ASTNode* parse_conditional_branch(Parser* parser)
{
    consume(parser, TokenLbrace);

    ASTNode* branch = create_compound_node(NULL);

    if (!parse_blocklevel_statements(parser, &branch->compound.statements)) {
        free_ast_node(branch);

        return NULL;
    }

    consume(parser, TokenRbrace);

    return branch;
}

static ASTNode* parse_conditional_else(Parser* parser, bool allow_single_line_body)
{
    if (parser->current_token.type != TokenKeywordElse) {
        return NULL;
    }

    consume(parser, TokenKeywordElse);
    if (parser->current_token.type == TokenKeywordIf) {
        return parse_conditional(parser, allow_single_line_body);
    }

    return parse_conditional_branch(parser);
}

static bool is_parenthesized_condition(Parser start, Token end)
{
    if (start.current_token.type != TokenLparen) {
        return false;
    }

    Token token = start.current_token;
    int depth = 1;

    while (depth > 0 && token.type != TokenEof && token.type != TokenIncompleteInput) {
        token = scan_token(&start.lexer);
        depth += token.type == TokenLparen;
        depth -= token.type == TokenRparen;
    }

    return depth == 0 && token.chars == end.chars;
}

static void validate_single_line_if_branch_end(Parser* parser, Token token)
{
    Token end = parser->previous_token;
    bool multiline = end.line != token.line || memchr(end.chars, '\n', end.length)
        || memchr(end.chars, '\r', end.length);

    if (multiline) {
        fatal_source_error(token.line, token.column, "Expected statement to end on the same physical line as if");
    }

    if (parser->current_token.type == TokenSemicolon) {
        return;
    }

    if (parser->current_token.type == TokenKeywordElse) {
        fatal_token_error(parser->current_token, "", "Unexpected ");
    }

    bool same_line_statement = !is_end_of_file(parser) && parser->current_token.line == token.line
        && parser->current_token.type != TokenRbrace;

    if (same_line_statement) {
        fatal_token_error(parser->current_token, "", "Expected end of line but found ");
    }
}

static ASTNode* parse_single_line_if_branch(Parser* parser, Token token)
{
    if (is_end_of_file(parser)) {
        return NULL;
    }

    if (parser->current_token.line != token.line) {
        fatal_source_error(token.line, token.column, "Expected statement on the same physical line as if");
    }

    if (parser->current_token.type == TokenKeywordIf) {
        expected_token_error(TokenLbrace, parser->current_token);
    }

    ASTNode* statement = parse_statement(parser);

    if (!statement) {
        return NULL;
    }

    validate_single_line_if_branch_end(parser, token);

    ASTNode* branch = create_compound_node(NULL);

    push_vector_item(&branch->compound.statements, statement);

    return branch;
}

static ASTNode* parse_if_branch(Parser* parser, Parser start, Token token, bool allow_single_line_body)
{
    if (parser->current_token.type == TokenLbrace) {
        return parse_conditional_branch(parser);
    }

    if (!allow_single_line_body) {
        expected_token_error(TokenLbrace, parser->current_token);
    }

    bool parenthesized = is_parenthesized_condition(start, parser->previous_token);

    if (!parenthesized) {
        expected_token_error(TokenLbrace, parser->current_token);
    }

    return parse_single_line_if_branch(parser, token);
}

static ASTNode* parse_conditional(Parser* parser, bool allow_single_line_body)
{
    Token token = parser->current_token;

    consume(parser, TokenKeywordIf);

    Parser condition_start = *parser;
    ASTNode* condition = parse_expression(parser);
    if (!condition) {
        return NULL;
    }

    bool single_line_body = parser->current_token.type != TokenLbrace;
    ASTNode* then_branch = parse_if_branch(parser, condition_start, token, allow_single_line_body);

    if (!then_branch) {
        free_ast_node(condition);

        return NULL;
    }

    if (single_line_body) {
        return create_conditional_node(token, condition, then_branch, NULL);
    }

    bool has_else = parser->current_token.type == TokenKeywordElse;
    ASTNode* else_branch = parse_conditional_else(parser, allow_single_line_body);

    if (has_else && !else_branch) {
        free_ast_node(condition);
        free_ast_node(then_branch);

        return NULL;
    }

    return create_conditional_node(token, condition, then_branch, else_branch);
}

static void mark_conditional_statement(ASTNode* ast)
{
    ast->conditional.expression = false;

    ASTNode* else_branch = ast->conditional.else_branch;
    if (else_branch && else_branch->type == ASTConditional) {
        mark_conditional_statement(else_branch);
    }
}

static bool is_match_branch_end(Parser* parser)
{
    return is_match_arm_end_token(parser->current_token.type) || is_end_of_file(parser);
}

static void consume_match_statement_separator(Parser* parser, Token token)
{
    bool same_line_statement = parser->current_token.line == token.line
        && !is_match_arm_end_token(parser->current_token.type)
        && parser->previous_token.type != TokenRbrace;

    if (same_line_statement || parser->current_token.type == TokenSemicolon) {
        consume(parser, TokenSemicolon);
    }
}

static bool parse_match_branch_statement(Parser* parser, ASTNode* branch)
{
    Token token = parser->current_token;
    ASTNode* statement = parse_statement(parser);

    if (!statement) {
        return false;
    }

    consume_match_statement_separator(parser, token);
    push_vector_item(&branch->compound.statements, statement);

    return true;
}

static ASTNode* parse_match_branch(Parser* parser)
{
    ASTNode* branch = create_compound_node(NULL);

    if (is_end_of_file(parser)) {
        free_ast_node(branch);

        return NULL;
    }

    if (is_match_arm_end_token(parser->current_token.type)) {
        free_ast_node(branch);
        expected_expression_error(parser->current_token);
    }

    while (!is_match_branch_end(parser)) {
        if (!parse_match_branch_statement(parser, branch)) {
            free_ast_node(branch);

            return NULL;
        }
    }

    return branch;
}

static ASTNode* parse_match_binding(Parser* parser)
{
    Token token = parser->current_token;
    StringObject* id = copy_string_object(token.chars, token.length);
    ASTNode* binding = create_variable_definition_node(token, id, true);
    binding->variable_definition.initialized = true;

    consume(parser, TokenIdentifier);

    return binding;
}

static bool parse_match_guard(Parser* parser, MatchArm* arm)
{
    if (parser->current_token.type != TokenKeywordIf) {
        return true;
    }

    arm->guard_token = parser->current_token;

    consume(parser, TokenKeywordIf);

    arm->guard = parse_expression(parser);

    return arm->guard != NULL;
}

static bool parse_match_arm(Parser* parser, ASTNode* ast)
{
    consume(parser, TokenKeywordCase);

    MatchArm* arm = allocate_zeroed_memory(1, sizeof(MatchArm));

    push_vector_item(&ast->match.arms, arm);

    if (is_wildcard_token(parser->current_token)) {
        consume(parser, TokenIdentifier);
    } else if (ast->match.subject && parser->current_token.type == TokenIdentifier) {
        arm->binding = parse_match_binding(parser);
    } else {
        arm->pattern = parse_expression(parser);
    }

    if (is_end_of_file(parser)) {
        return false;
    }

    if (!parse_match_guard(parser, arm)) {
        return false;
    }

    consume(parser, TokenColon);
    arm->branch = parse_match_branch(parser);

    return arm->branch != NULL;
}

static bool parse_match_default(Parser* parser, ASTNode* ast)
{
    if (parser->current_token.type != TokenKeywordDefault) {
        return true;
    }

    consume(parser, TokenKeywordDefault);

    if (is_end_of_file(parser)) {
        return false;
    }

    consume(parser, TokenColon);
    ast->match.default_branch = parse_match_branch(parser);

    return ast->match.default_branch != NULL;
}

static ASTNode* parse_match(Parser* parser)
{
    Token token = parser->current_token;
    consume(parser, TokenKeywordMatch);

    ASTNode* ast = create_match_node(token);

    if (parser->current_token.type != TokenLbrace) {
        ast->match.subject = parse_expression(parser);
    }

    if (is_end_of_file(parser)) {
        free_ast_node(ast);

        return NULL;
    }

    consume(parser, TokenLbrace);
    while (parser->current_token.type == TokenKeywordCase) {
        if (!parse_match_arm(parser, ast)) {
            free_ast_node(ast);

            return NULL;
        }
    }

    if (!parse_match_default(parser, ast)) {
        free_ast_node(ast);

        return NULL;
    }

    if (is_end_of_file(parser)) {
        free_ast_node(ast);

        return NULL;
    }

    consume(parser, TokenRbrace);

    return ast;
}

static ASTNode* parse_prefix_operand(Parser* parser)
{
    if (is_prefix_token(parser->current_token.type)) {
        return parse_prefix(parser);
    }

    Token token = parser->current_token;
    ASTNode* expr = parse_primary(parser);

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
        return parse_primary(parser);
    }

    Token token = parser->current_token;
    consume(parser, token.type);
    ASTNode* expr = parse_prefix_operand(parser);

    if (!expr) {
        return NULL;
    }

    if (token.type == TokenMinus && expr->type == ASTInteger) {
        expr->integer_literal.type_id = infer_integer_literal_type(expr->integer_literal.value, true);
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

static ASTNode* parse_ternary_branch(Parser* parser)
{
    ASTNode* expression = parse_expression(parser);

    if (!expression) {
        return NULL;
    }

    ASTNode* branch = create_compound_node(NULL);

    push_vector_item(&branch->compound.statements, expression);

    return branch;
}

static ASTNode* parse_ternary(Parser* parser, ASTNode* condition)
{
    Token token = parser->current_token;

    consume(parser, TokenQuestion);

    ASTNode* then_branch = parse_ternary_branch(parser);

    if (!then_branch || is_end_of_file(parser)) {
        free_ast_node(condition);
        free_ast_node(then_branch);

        return NULL;
    }

    consume(parser, TokenColon);

    ASTNode* else_branch = parse_ternary_branch(parser);

    if (!else_branch) {
        free_ast_node(condition);
        free_ast_node(then_branch);

        return NULL;
    }

    return create_conditional_node(token, condition, then_branch, else_branch);
}

static ASTNode* parse_expression(Parser* parser)
{
    ASTNode* condition = parse_boolean_or(parser);

    if (!condition || parser->current_token.type != TokenQuestion) {
        return condition;
    }

    return parse_ternary(parser, condition);
}

static ASTNode* parse_return_statement(Parser* parser)
{
    Token token = parser->current_token;
    consume(parser, TokenKeywordReturn);

    ASTNode* expr = parse_expression(parser);
    if (!expr) {
        return NULL;
    }

    return create_return_node(token, expr);
}

static ASTNode* parse_argument(Parser* parser)
{
    return parse_expression(parser);
}

static bool parse_argument_list_item(Parser* parser, Vector* args)
{
    ASTNode* expr = parse_argument(parser);

    if (!expr && (parser->current_token.type == TokenComma || parser->current_token.type == TokenRparen)) {
        expected_expression_error(parser->current_token);
    }

    if (!expr) {
        return false;
    }

    push_vector_item(args, expr);

    if (parser->current_token.type == TokenComma) {
        consume(parser, TokenComma);
    }

    return true;
}

static bool parse_argument_list(Parser* parser, Vector* args)
{
    consume(parser, TokenLparen);

    if (is_end_of_file(parser)) {
        return false;
    }

    while (parser->current_token.type != TokenRparen) {
        if (!parse_argument_list_item(parser, args)) {
            return false;
        }
    }

    if (is_end_of_file(parser)) {
        return false;
    }

    consume(parser, TokenRparen);

    return true;
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

static ASTNode* parse_parameter(Parser* parser)
{
    if (is_end_of_file(parser)) {
        return NULL;
    }

    Token token = parser->current_token;
    StringObject* id = copy_string_object(token.chars, token.length);

    consume(parser, TokenIdentifier);

    ASTNode* ast = create_parameter_node(token, id);

    if (parser->current_token.type == TokenColon) {
        consume(parser, TokenColon);

        ast->parameter.reference_type = parse_reference_type(parser);
        ast->parameter.type_id = get_token_data_type(parser->current_token.type);

        consume_type(parser);
    }

    if (!parse_parameter_default(parser, ast)) {
        free_ast_node(ast);

        return NULL;
    }

    return ast;
}

static bool parse_parameter_list_item(Parser* parser, Vector* params)
{
    ASTNode* param = parse_parameter(parser);

    if (!param) {
        return false;
    }

    push_vector_item(params, param);

    if (parser->current_token.type == TokenComma) {
        consume(parser, TokenComma);
    }

    return true;
}

static bool parse_parameter_list(Parser* parser, Vector* params)
{
    consume(parser, TokenLparen);

    if (is_end_of_file(parser)) {
        return false;
    }

    while (parser->current_token.type != TokenRparen) {
        if (!parse_parameter_list_item(parser, params)) {
            return false;
        }
    }

    if (is_end_of_file(parser)) {
        return false;
    }

    consume(parser, TokenRparen);

    size_t count = count_vector(params);

    for (size_t i = 0; i < count; i++) {
        ASTNode* param = get_vector_at(params, i);
        param->parameter.position = count - i - 1;
    }

    return true;
}

static ASTNode* parse_function_call(Parser* parser)
{
    ASTNode* ast = create_function_call_node(parser->previous_token);

    if (!parse_argument_list(parser, &ast->function_call.args)) {
        free_ast_node(ast);

        return NULL;
    }

    return ast;
}

static void parse_function_return_type(Parser* parser, ASTNode* ast)
{
    if (parser->current_token.type != TokenArrow) {
        return;
    }

    consume(parser, TokenArrow);

    ast->function_definition.return_reference_type = parse_reference_type(parser);
    ast->function_definition.return_type_id = get_token_data_type(parser->current_token.type);

    consume_type(parser);

    ast->function_definition.has_explicit_return_type = true;
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

static bool parse_function_body(Parser* parser, ASTNode* ast)
{
    if (is_end_of_file(parser)) {
        return false;
    }

    if (parser->current_token.type == TokenEqual) {
        return parse_function_expression_body(parser, ast);
    }

    consume(parser, TokenLbrace);

    ASTNode* body = create_compound_node(NULL);
    ast->function_definition.body = body;

    if (!parse_blocklevel_statements(parser, &body->compound.statements)) {
        return false;
    }

    consume(parser, TokenRbrace);

    return true;
}

static ASTNode* parse_function_definition(Parser* parser)
{
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

    if (!parse_parameter_list(parser, &ast->function_definition.params)) {
        free_ast_node(ast);

        return NULL;
    }

    parse_function_return_type(parser, ast);

    if (!parse_function_body(parser, ast)) {
        free_ast_node(ast);

        return NULL;
    }

    return ast;
}

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

static void parse_variable_type(Parser* parser, ASTNode* ast)
{
    if (parser->current_token.type == TokenColon) {
        consume(parser, TokenColon);

        ast->variable_definition.reference_type = parse_reference_type(parser);
        ast->variable_definition.type_id = get_token_data_type(parser->current_token.type);

        consume_type(parser);

        return;
    }

    if (parser->current_token.type != TokenEqual) {
        expected_token_error(TokenEqual, parser->current_token);
    }
}

static bool parse_variable_initializer(Parser* parser, ASTNode* ast)
{
    if (parser->current_token.type == TokenEqual) {
        consume(parser, TokenEqual);
        ast->variable_definition.expr = parse_expression(parser);

        return ast->variable_definition.expr != NULL;
    }

    ast->variable_definition.expr = create_ast_node(ASTNone);

    if (ast->variable_definition.type_id == TypeUnknown) {
        ast->variable_definition.type_id = TypeI32;
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

static ASTNode* parse_variable_value(Parser* parser, ASTNode* ast, Token keyword)
{
    parse_variable_type(parser, ast);

    if (ast->variable_definition.fixed && parser->current_token.type != TokenEqual) {
        expected_token_error(TokenEqual, parser->current_token);
    }

    if (!parse_variable_initializer(parser, ast)) {
        free_ast_node(ast);
        return NULL;
    }

    if (ast->variable_definition.constant) {
        ast->variable_definition.expr = create_prefix_node(keyword, ast->variable_definition.expr);
    }

    return ast;
}

static ASTNode* parse_variable_definition(Parser* parser)
{
    Token keyword = parser->current_token;
    ASTNode* ast = parse_variable_binding(parser, keyword);

    if (!ast) {
        return NULL;
    }

    return parse_variable_value(parser, ast, keyword);
}

static ASTNode* parse_identifier(Parser* parser)
{
    consume(parser, TokenIdentifier);

    if (is_assignment_token(parser->current_token.type)) {
        return parse_assignment(parser);
    }

    if (parser->current_token.type == TokenLparen) {
        return parse_function_call(parser);
    }

    return create_variable_node(parser->previous_token);
}

static ASTNode* parse_while_statement(Parser* parser)
{
    Token token = parser->current_token;
    consume(parser, TokenKeywordWhile);
    ASTNode* condition = parse_expression(parser);

    if (!condition) {
        return NULL;
    }

    ASTNode* body = parse_conditional_branch(parser);

    if (!body) {
        free_ast_node(condition);

        return NULL;
    }

    return create_while_node(token, condition, body);
}

static ASTNode* parse_if_statement(Parser* parser)
{
    ASTNode* ast = parse_conditional(parser, true);
    if (!ast) {
        return NULL;
    }

    mark_conditional_statement(ast);

    return ast;
}

static ASTNode* parse_match_statement(Parser* parser)
{
    ASTNode* ast = parse_match(parser);
    if (!ast) {
        return NULL;
    }

    ast->match.expression = false;

    return ast;
}

static ASTNode* parse_constant_statement(Parser* parser)
{
    Lexer lexer = parser->lexer;
    Token name = scan_token(&lexer);

    if (name.type != TokenIdentifier) {
        return parse_expression(parser);
    }

    Token next = scan_token(&lexer);

    if (next.type == TokenEqual || next.type == TokenColon) {
        return parse_variable_definition(parser);
    }

    return parse_expression(parser);
}

static ASTNode* parse_statement(Parser* parser)
{
    switch (parser->current_token.type) {
        case TokenKeywordFunc:
            return parse_function_definition(parser);
        case TokenKeywordConst:
            return parse_constant_statement(parser);
        case TokenKeywordLet:
        case TokenKeywordVar:
            return parse_variable_definition(parser);
        case TokenKeywordReturn:
            return parse_return_statement(parser);
        case TokenKeywordWhile:
            return parse_while_statement(parser);
        case TokenKeywordIf:
            return parse_if_statement(parser);
        case TokenKeywordMatch:
            return parse_match_statement(parser);
        default:
            return parse_expression(parser);
    }
}

static bool parse_statements(Parser* parser, Vector* nodes, TokenType type)
{
    Token token = parser->current_token;

    while (token.type != type && token.type != TokenIncompleteInput) {
        ASTNode* stmt = parse_statement(parser);
        if (!stmt) {
            return false;
        }

        bool same_line_statement = !is_end_of_file(parser) &&
            parser->current_token.line == token.line &&
            parser->current_token.type != type &&
            parser->previous_token.type != TokenRbrace;

        if (same_line_statement || parser->current_token.type == TokenSemicolon) {
            consume(parser, TokenSemicolon);
        }

        push_vector_item(nodes, stmt);
        token = parser->current_token;
    }

    return true;
}

static bool parse_blocklevel_statements(Parser* parser, Vector* nodes)
{
    return parse_statements(parser, nodes, TokenRbrace);
}

static bool parse_top_level_statements(Parser* parser)
{
    return parse_statements(parser, &parser->top_level->compound.statements, TokenEof);
}

void init_parser(Parser* parser, ASTNode* ast)
{
    parser->top_level = ast;
}

bool parse(Parser* parser, const char* source)
{
    init_lexer(&parser->lexer, source);
    advance(parser);

    return parse_top_level_statements(parser) && parser->current_token.type != TokenIncompleteInput;
}
