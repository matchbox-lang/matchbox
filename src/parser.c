#include "parser.h"
#include "ast.h"
#include "conversion.h"
#include "lexer.h"
#include "string_object.h"
#include "token.h"
#include "vector.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

static AST* expression(Parser* parser);
static AST* identifier(Parser* parser);
static AST* prefix(Parser* parser);
static bool blocklevelStatements(Parser* parser, Vector* nodes);

static void expectedExpressionError(Token token)
{
    fprintf(stderr, "Error: Expected expression but found ");
    printTokenValue(token);
    fprintf(stderr, " on line %d:%d\n", token.line, token.column);
    exit(1);
}

static void expectedOperandError(Token token)
{
    fprintf(stderr, "Error: Expected operand but found ");
    printTokenValue(token);
    fprintf(stderr, " on line %d:%d\n", token.line, token.column);
    exit(1);
}

static void expectedTypeError(Token token)
{
    fprintf(stderr, "Error: Expected type but found ");
    printTokenValue(token);
    fprintf(stderr, " on line %d:%d\n", token.line, token.column);
    exit(1);
}

static void expectedTokenError(TokenType type, Token token)
{
    fprintf(stderr, "Error: Expected %s but found ", tokenTypeName(type));
    printTokenValue(token);
    fprintf(stderr, " on line %d:%d\n", token.line, token.column);
    exit(1);
}

static void advance(Parser* parser)
{
    parser->prevToken = parser->currentToken;
    parser->currentToken = scanToken(&parser->lexer);
}

static void consume(Parser* parser, TokenType type)
{
    if (parser->currentToken.type != type) {
        expectedTokenError(type, parser->currentToken);
    }
    
    advance(parser);
}

static void consumeType(Parser* parser)
{
    if (!isTypeToken(parser->currentToken.type)) {
        expectedTypeError(parser->currentToken);
    }

    consume(parser, parser->currentToken.type);
}

static bool isEof(Parser* parser)
{
    return parser->currentToken.type == TOKEN_EOF;
}

static AST* integerLiteral(Parser* parser, Token token)
{
    AST* ast = createAST(AST_INTEGER);
    ast->intValue = integerLiteralToValue(token.chars, token.length);
    
    consume(parser, token.type);

    return ast;
}

static AST* binaryLiteral(Parser* parser, Token token)
{
    AST* ast = createAST(AST_INTEGER);
    ast->intValue = binaryLiteralToValue(token.chars, token.length);

    consume(parser, token.type);

    return ast;
}

static AST* hexadecimalLiteral(Parser* parser, Token token)
{
    AST* ast = createAST(AST_INTEGER);
    ast->intValue = hexadecimalLiteralToValue(token.chars, token.length);

    consume(parser, token.type);

    return ast;
}

static AST* octalLiteral(Parser* parser, Token token)
{
    AST* ast = createAST(AST_INTEGER);
    ast->intValue = octalLiteralToValue(token.chars, token.length);

    consume(parser, token.type);

    return ast;
}

static AST* groupExpression(Parser* parser)
{
    consume(parser, TOKEN_LPAREN);

    AST* ast = expression(parser);

    if (!ast && parser->currentToken.type == TOKEN_RPAREN) {
        expectedExpressionError(parser->currentToken);
    }
    
    if (!ast || isEof(parser)) {
        freeAST(ast);
        
        return NULL;
    }

    consume(parser, TOKEN_RPAREN);

    return ast;
}

static AST* binary(AST* leftExpr, AST* rightExpr, Token token)
{
    if (!rightExpr) {
        return NULL;
    }

    AST* ast = createAST(AST_BINARY);
    ast->binary.leftExpr = leftExpr;
    ast->binary.operator = token;
    ast->binary.rightExpr = rightExpr;
    ast->binary.typeId = TOKEN_NONE;

    return ast;
}

static AST* createVariable(Token token)
{
    AST* ast = createAST(AST_VARIABLE);
    ast->variable.scope = NULL;
    ast->variable.id = copyStringObject(token.chars, token.length);
    ast->variable.token = token;
    ast->variable.symbol = NULL;

    return ast;
}

static AST* createParameter(Token token, StringObject* id)
{
    AST* ast = createAST(AST_PARAMETER);
    ast->parameter.scope = NULL;
    ast->parameter.id = id;
    ast->parameter.token = token;
    ast->parameter.typeId = TOKEN_INT;

    return ast;
}

static AST* parameter(Parser* parser)
{
    if (isEof(parser)) {
        return NULL;
    }
    
    Token token = parser->currentToken;
    StringObject* id = copyStringObject(token.chars, token.length);
    
    consume(parser, TOKEN_IDENTIFIER);

    AST* ast = createParameter(token, id);

    if (parser->currentToken.type == TOKEN_COLON) {
        consume(parser, TOKEN_COLON);
        ast->parameter.typeId = parser->currentToken.type;
        consumeType(parser);
    }

    return ast;
}

static AST* primary(Parser* parser)
{
    switch (parser->currentToken.type) {
        case TOKEN_INTEGER_LITERAL:
            return integerLiteral(parser, parser->currentToken);
        case TOKEN_BINARY_LITERAL:
            return binaryLiteral(parser, parser->currentToken);
        case TOKEN_HEXADECIMAL_LITERAL:
            return hexadecimalLiteral(parser, parser->currentToken);
        case TOKEN_OCTAL_LITERAL:
            return octalLiteral(parser, parser->currentToken);
        case TOKEN_LPAREN:
            return groupExpression(parser);
        case TOKEN_IDENTIFIER:
            return identifier(parser);
        case TOKEN_EOF:
            return NULL;
        default:
            expectedExpressionError(parser->currentToken);
            return NULL;
    }
}

static AST* prefixOperand(Parser* parser)
{
    if (isPrefixToken(parser->currentToken.type)) {
        return prefix(parser);
    }

    Token token = parser->currentToken;
    AST* expr = primary(parser);
    
    if (!expr) {
        return NULL;
    }

    if (!isPrefix(expr) && !isPrefixOperand(expr)) {
        expectedOperandError(token);
    }
    
    return expr;
}

static AST* prefix(Parser* parser)
{
    if (!isPrefixToken(parser->currentToken.type)) {
        return primary(parser);
    }

    Token token = parser->currentToken;
    consume(parser, token.type);
    AST* expr = prefixOperand(parser);
    
    if (!expr) {
        return NULL;
    }

    AST* ast = createAST(AST_PREFIX);
    ast->prefix.expr = expr;
    ast->prefix.operator = token;

    return ast;
}

static AST* exponent(Parser* parser)
{
    AST* expr = prefix(parser);
    Token token = parser->currentToken;

    if (token.type == TOKEN_POWER) {
        consume(parser, token.type);
        expr = binary(expr, exponent(parser), token);
    }

    return expr;
}

static AST* factor(Parser* parser)
{
    AST* expr = exponent(parser);
    Token token = parser->currentToken;

    while (isFactorToken(token.type)) {
        consume(parser, token.type);
        expr = binary(expr, exponent(parser), token);
        token = parser->currentToken;
    }

    return expr;
}

static AST* term(Parser* parser)
{
    AST* expr = factor(parser);
    Token token = parser->currentToken;

    while (isTermToken(token.type)) {
        consume(parser, token.type);
        expr = binary(expr, factor(parser), token);
        token = parser->currentToken;
    }

    return expr;
}

static AST* shift(Parser* parser)
{
    AST* expr = term(parser);
    Token token = parser->currentToken;

    while (isShiftToken(token.type)) {
        consume(parser, token.type);
        expr = binary(expr, term(parser), token);
        token = parser->currentToken;
    }

    return expr;
}

static AST* comparison(Parser* parser)
{
    AST* expr = shift(parser);
    Token token = parser->currentToken;

    while (isComparisonToken(token.type)) {
        consume(parser, token.type);
        expr = binary(expr, shift(parser), token);
        token = parser->currentToken;
    }

    return expr;
}

static AST* equality(Parser* parser)
{
    AST* expr = comparison(parser);
    Token token = parser->currentToken;

    while (isEqualityToken(token.type)) {
        consume(parser, token.type);
        expr = binary(expr, comparison(parser), token);
        token = parser->currentToken;
    }

    return expr;
}

static AST* bitwiseAND(Parser* parser)
{
    AST* expr = equality(parser);
    Token token = parser->currentToken;

    while (token.type == TOKEN_AMPERSAND) {
        consume(parser, token.type);
        expr = binary(expr, equality(parser), token);
        token = parser->currentToken;
    }

    return expr;
}

static AST* bitwiseXOR(Parser* parser)
{
    AST* expr = bitwiseAND(parser);
    Token token = parser->currentToken;

    while (token.type == TOKEN_CIRCUMFLEX) {
        consume(parser, token.type);
        expr = binary(expr, bitwiseAND(parser), token);
        token = parser->currentToken;
    }

    return expr;
}

static AST* bitwiseOR(Parser* parser)
{
    AST* expr = bitwiseXOR(parser);
    Token token = parser->currentToken;

    while (token.type == TOKEN_PIPE) {
        consume(parser, token.type);
        expr = binary(expr, bitwiseXOR(parser), token);
        token = parser->currentToken;
    }

    return expr;
}

static AST* booleanAND(Parser* parser)
{
    AST* expr = bitwiseOR(parser);
    Token token = parser->currentToken;

    while (token.type == TOKEN_BOOLEAN_AND) {
        consume(parser, token.type);
        expr = binary(expr, bitwiseOR(parser), token);
        token = parser->currentToken;
    }

    return expr;
}

static AST* booleanOR(Parser* parser)
{
    AST* expr = booleanAND(parser);
    Token token = parser->currentToken;

    while (token.type == TOKEN_BOOLEAN_OR) {
        consume(parser, token.type);
        expr = binary(expr, booleanAND(parser), token);
        token = parser->currentToken;
    }

    return expr;
}

static AST* expression(Parser* parser)
{
    return booleanOR(parser);
}

static AST* returnStatement(Parser* parser)
{
    Token token = parser->currentToken;
    consume(parser, TOKEN_RETURN);

    AST* expr = expression(parser);
    if (!expr) {
        return NULL;
    }

    AST* ast = createAST(AST_RETURN);
    ast->returnStatement.token = token;
    ast->returnStatement.expr = expr;

    return ast;
}

static bool arguments(Parser* parser, Vector* args)
{
    consume(parser, TOKEN_LPAREN);

    if (isEof(parser)) {
        return false;
    }

    while (parser->currentToken.type != TOKEN_RPAREN) {
        AST* expr = expression(parser);

        if (!expr && (parser->currentToken.type == TOKEN_COMMA || parser->currentToken.type == TOKEN_RPAREN)) {
            expectedExpressionError(parser->currentToken);
        }

        if (!expr) {
            return false;
        }

        pushVectorItem(args, expr);

        if (parser->currentToken.type == TOKEN_COMMA) {
            consume(parser, TOKEN_COMMA);
        }
    }

    if (isEof(parser)) {
        return false;
    }

    consume(parser, TOKEN_RPAREN);

    return true;
}

static bool parameters(Parser* parser, Vector* params)
{
    consume(parser, TOKEN_LPAREN);

    if (isEof(parser)) {
        return false;
    }

    while (parser->currentToken.type != TOKEN_RPAREN) {
        AST* expr = parameter(parser);

        if (!expr && (parser->currentToken.type == TOKEN_COMMA || parser->currentToken.type == TOKEN_RPAREN)) {
            expectedExpressionError(parser->currentToken);
        }

        if (!expr) {
            return false;
        }

        pushVectorItem(params, expr);

        if (parser->currentToken.type == TOKEN_COMMA) {
            consume(parser, TOKEN_COMMA);
        }
    }

    if (isEof(parser)) {
        return false;
    }
    
    consume(parser, TOKEN_RPAREN);

    size_t count = countVector(params);

    for (size_t i = 0; i < count; i++) {
        AST* param = getVectorAt(params, i);
        param->parameter.position = count - i - 1;
    }

    return true;
}

static AST* createFunctionCall(Token token)
{
    AST* ast = createAST(AST_FUNCTION_CALL);
    ast->functionCall.scope = NULL;
    ast->functionCall.id = copyStringObject(token.chars, token.length);
    ast->functionCall.token = token;
    ast->functionCall.symbol = NULL;

    return ast;
}

static AST* functionCall(Parser* parser)
{
    AST* ast = createFunctionCall(parser->prevToken);

    if (!arguments(parser, &ast->functionCall.args)) {
        freeAST(ast);
        
        return NULL;
    }

    return ast;
}

static AST* createFunctionDefinition(Token token, StringObject* id)
{
    AST* ast = createAST(AST_FUNCTION_DEFINITION);
    ast->functionDefinition.scope = NULL;
    ast->functionDefinition.id = id;
    ast->functionDefinition.token = token;
    ast->functionDefinition.typeId = TOKEN_INT;
    ast->functionDefinition.hasExplicitReturnType = false;
    ast->functionDefinition.body = NULL;

    return ast;
}

static void parseFunctionReturnType(Parser* parser, AST* ast)
{
    if (parser->currentToken.type != TOKEN_ARROW) {
        return;
    }

    consume(parser, TOKEN_ARROW);
    ast->functionDefinition.typeId = parser->currentToken.type;
    consumeType(parser);
    ast->functionDefinition.hasExplicitReturnType = true;
}

static bool parseFunctionBody(Parser* parser, AST* ast)
{
    if (isEof(parser)) {
        return false;
    }

    consume(parser, TOKEN_LBRACE);

    AST* body = createAST(AST_COMPOUND);
    body->compound.scope = NULL;
    ast->functionDefinition.body = body;

    if (!blocklevelStatements(parser, &body->compound.statements)) {
        return false;
    }

    consume(parser, TOKEN_RBRACE);

    return true;
}

static AST* functionDefinition(Parser* parser)
{
    consume(parser, TOKEN_FUNC);

    if (isEof(parser)) {
        return NULL;
    }

    Token token = parser->currentToken;
    StringObject* id = copyStringObject(token.chars, token.length);

    if (parser->currentToken.type != TOKEN_IDENTIFIER) {
        expectedTokenError(TOKEN_IDENTIFIER, parser->currentToken);
    }

    consume(parser, TOKEN_IDENTIFIER);

    if (isEof(parser)) {
        return NULL;
    }

    AST* ast = createFunctionDefinition(token, id);

    if (!parameters(parser, &ast->functionDefinition.params)) {
        freeAST(ast);

        return NULL;
    }

    parseFunctionReturnType(parser, ast);

    if (!parseFunctionBody(parser, ast)) {
        freeAST(ast);

        return NULL;
    }

    return ast;
}

static AST* createAssignment(Token operator, Token token, AST* expr)
{
    AST* ast = createAST(AST_ASSIGNMENT);
    ast->assignment.scope = NULL;
    ast->assignment.operator = operator;
    ast->assignment.token = token;
    ast->assignment.symbol = NULL;
    ast->assignment.expr = expr;

    return ast;
}

static AST* assignment(Parser* parser)
{
    Token operator = parser->currentToken;
    Token token = parser->prevToken;
    consume(parser, operator.type);

    AST* expr = expression(parser);
    if (!expr) {
        return NULL;
    }

    AST* ast = createAssignment(operator, token, expr);

    return ast;
}

static AST* createVariableDefinition(Token token, StringObject* id)
{
    AST* ast = createAST(AST_VARIABLE_DEFINITION);
    ast->variableDefinition.scope = NULL;
    ast->variableDefinition.id = id;
    ast->variableDefinition.token = token;
    ast->variableDefinition.position = 0;
    ast->variableDefinition.expr = NULL;

    return ast;
}

static void parseVariableType(Parser* parser, AST* ast)
{
    if (parser->currentToken.type == TOKEN_COLON) {
        consume(parser, TOKEN_COLON);
        ast->variableDefinition.typeId = parser->currentToken.type;
        consumeType(parser);
        
        return;
    }

    if (parser->currentToken.type != TOKEN_EQUAL) {
        expectedTokenError(TOKEN_EQUAL, parser->currentToken);
    }
}

static bool parseVariableInitializer(Parser* parser, AST* ast)
{
    if (parser->currentToken.type != TOKEN_EQUAL) {
        ast->variableDefinition.expr = createAST(AST_NONE);
        ast->variableDefinition.typeId = TOKEN_INT;

        return true;
    }

    consume(parser, TOKEN_EQUAL);
    ast->variableDefinition.expr = expression(parser);
    ast->variableDefinition.typeId = TOKEN_NONE;

    return ast->variableDefinition.expr != NULL;
}

static AST* variableDefinition(Parser* parser)
{
    consume(parser, TOKEN_VAR);

    if (isEof(parser)) {
        return NULL;
    }

    Token token = parser->currentToken;
    StringObject* id = copyStringObject(token.chars, token.length);

    consume(parser, TOKEN_IDENTIFIER);

    if (isEof(parser)) {
        return NULL;
    }

    AST* ast = createVariableDefinition(token, id);

    parseVariableType(parser, ast);

    if (!parseVariableInitializer(parser, ast)) {
        freeAST(ast);
        return NULL;
    }

    return ast;
}

static AST* identifier(Parser* parser)
{
    consume(parser, TOKEN_IDENTIFIER);
    
    if (isAssignmentToken(parser->currentToken.type)) {
        return assignment(parser);
    }

    if (parser->currentToken.type == TOKEN_LPAREN) {
        return functionCall(parser);
    }

    return createVariable(parser->prevToken);
}

static AST* statement(Parser* parser)
{
    switch (parser->currentToken.type) {
        case TOKEN_FUNC:
            return functionDefinition(parser);
        case TOKEN_VAR:
            return variableDefinition(parser);
        case TOKEN_RETURN:
            return returnStatement(parser);
        default:
            return expression(parser);
    }
}

static bool statements(Parser* parser, Vector* nodes, TokenType type)
{
    Token token = parser->currentToken;

    while (token.type != type) {
        AST* stmt = statement(parser);
        if (!stmt) {
            return false;
        }
        
        bool sameLineStatement = !isEof(parser) &&
            parser->currentToken.line == token.line &&
            parser->currentToken.type != type &&
            parser->prevToken.type != TOKEN_RBRACE;

        if (sameLineStatement || parser->currentToken.type == TOKEN_SEMICOLON) {
            consume(parser, TOKEN_SEMICOLON);
        }

        pushVectorItem(nodes, stmt);
        token = parser->currentToken;
    }

    return true;
}

static bool blocklevelStatements(Parser* parser, Vector* nodes)
{
    return statements(parser, nodes, TOKEN_RBRACE);
}

static bool toplevelStatements(Parser* parser)
{
    return statements(parser, &parser->topLevel->compound.statements, TOKEN_EOF);
}

void initParser(Parser* parser, AST* ast)
{
    parser->topLevel = ast;
}

bool parse(Parser* parser, char* source)
{
    initLexer(&parser->lexer, source);
    advance(parser);

    return toplevelStatements(parser);
}
