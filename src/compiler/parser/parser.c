#include "compiler/parser/parser.h"
#include "common/vector.h"
#include "compiler/parser/control_flow.h"
#include "compiler/parser/expression.h"
#include "compiler/parser/function.h"
#include "compiler/parser/match.h"
#include "compiler/parser/test_block.h"
#include "compiler/parser/variable.h"
#include "compiler/diagnostics.h"

_Noreturn void expected_expression_error(Token token)
{
    fatal_token_error(token, "", "Expected expression but found ");
}

_Noreturn void expected_token_error(TokenType type, Token token)
{
    const char* type_name = get_token_type_name(type);

    fatal_token_error(token, "", "Expected %s but found ", type_name);
}

_Noreturn void expected_tokens_error(TokenType first_type, TokenType second_type, Token token)
{
    const char* first_name = get_token_type_name(first_type);
    const char* second_name = get_token_type_name(second_type);

    fatal_token_error(token, "", "Expected %s or %s but found ", first_name, second_name);
}

void advance(Parser* parser)
{
    parser->previous_token = parser->current_token;
    parser->current_token = scan_token(&parser->lexer);
}

void consume(Parser* parser, TokenType type)
{
    if (parser->current_token.type != type) {
        expected_token_error(type, parser->current_token);
    }

    advance(parser);
}

bool is_end_of_file(Parser* parser)
{
    return parser->current_token.type == TokenIncompleteInput || parser->current_token.type == TokenEof;
}

ASTNode* parse_statement(Parser* parser)
{
    switch (parser->current_token.type) {
        case TokenKeywordFunc:
            return parse_function_definition(parser);
        case TokenKeywordTest:
            return parse_test_block(parser);
        case TokenKeywordConst:
            return parse_constant_statement(parser);
        case TokenKeywordLet:
        case TokenKeywordVar:
            return parse_variable_definition(parser);
        case TokenKeywordReturn:
            return parse_return_statement(parser);
        case TokenKeywordFor:
            return parse_for_statement(parser);
        case TokenKeywordBreak:
        case TokenKeywordContinue:
            return parse_loop_control(parser);
        case TokenKeywordWhile:
            return parse_while_statement(parser);
        case TokenKeywordIf:
            return parse_conditional(parser, false);
        case TokenKeywordMatch:
            return parse_match(parser, false);
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
            parser->current_token.line == parser->previous_token.line &&
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

bool parse_braced_block(Parser* parser, ASTNode** block)
{
    *block = NULL;

    if (is_end_of_file(parser)) {
        return false;
    }

    consume(parser, TokenLbrace);

    *block = create_compound_node(NULL);
    bool complete = parse_statements(parser, &(*block)->compound.statements, TokenRbrace);

    if (!complete || is_end_of_file(parser)) {
        return false;
    }

    consume(parser, TokenRbrace);

    return true;
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
