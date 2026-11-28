#include "compiler/parser/match.h"
#include "common/memory.h"
#include "common/vector.h"
#include "compiler/ast/ast.h"
#include "compiler/parser/control_flow.h"
#include "compiler/parser/expression.h"
#include "compiler/diagnostics.h"
#include "runtime/string_object.h"

static bool is_match_branch_end(Parser* parser)
{
    return is_match_arm_end_token(parser->current_token.type) || is_end_of_file(parser);
}

static void consume_match_statement_separator(Parser* parser)
{
    bool same_line_statement = !is_end_of_file(parser)
        && parser->current_token.line == parser->previous_token.line
        && !is_match_arm_end_token(parser->current_token.type)
        && parser->previous_token.type != TokenRbrace;

    if (same_line_statement || parser->current_token.type == TokenSemicolon) {
        consume(parser, TokenSemicolon);
    }
}

static bool parse_match_branch_statement(Parser* parser, ASTNode* branch, bool expression)
{
    if (!parse_branch_statement(parser, branch, expression)) {
        return false;
    }

    consume_match_statement_separator(parser);

    return true;
}

static ASTNode* parse_match_branch(Parser* parser, bool expression)
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
        if (!parse_match_branch_statement(parser, branch, expression)) {
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
    arm->branch = parse_match_branch(parser, ast->match.expression);

    if (!arm->pattern && !arm->guard && parser->current_token.type == TokenKeywordCase) {
        Token token = parser->current_token;

        fatal_source_error(token.line, token.column, "Catch-all match arm must be last");
    }

    return arm->branch != NULL;
}

ASTNode* parse_match(Parser* parser, bool expression)
{
    Token token = parser->current_token;
    consume(parser, TokenKeywordMatch);

    ASTNode* ast = create_match_node(token);
    ast->match.expression = expression;

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

    if (is_end_of_file(parser)) {
        free_ast_node(ast);

        return NULL;
    }

    consume(parser, TokenRbrace);

    return ast;
}
