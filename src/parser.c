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

static AST* parseExpression(Parser* parser);
static AST* parseIdentifier(Parser* parser);
static AST* parsePrefix(Parser* parser);
static bool parseBlocklevelStatements(Parser* parser, Vector* nodes);

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
    fprintf(stderr, "Error: Expected %s but found ", getTokenTypeName(type));
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

static bool isEndOfFile(Parser* parser)
{
    return parser->currentToken.type == TOKEN_EOF;
}

static AST* parseIntegerLiteral(Parser* parser, Token token)
{
    AST* ast = createAST(AST_INTEGER);
    ast->integerLiteral.value = integerLiteralToValue(token.chars, token.length);
    ast->integerLiteral.token = token;
    
    consume(parser, token.type);

    return ast;
}

static AST* parseBinaryLiteral(Parser* parser, Token token)
{
    AST* ast = createAST(AST_INTEGER);
    ast->integerLiteral.value = binaryLiteralToValue(token.chars, token.length);
    ast->integerLiteral.token = token;

    consume(parser, token.type);

    return ast;
}

static AST* parseHexadecimalLiteral(Parser* parser, Token token)
{
    AST* ast = createAST(AST_INTEGER);
    ast->integerLiteral.value = hexadecimalLiteralToValue(token.chars, token.length);
    ast->integerLiteral.token = token;

    consume(parser, token.type);

    return ast;
}

static AST* parseOctalLiteral(Parser* parser, Token token)
{
    AST* ast = createAST(AST_INTEGER);
    ast->integerLiteral.value = octalLiteralToValue(token.chars, token.length);
    ast->integerLiteral.token = token;

    consume(parser, token.type);

    return ast;
}

static AST* parseGroupExpression(Parser* parser)
{
    consume(parser, TOKEN_LPAREN);

    AST* ast = parseExpression(parser);

    if (!ast && parser->currentToken.type == TOKEN_RPAREN) {
        expectedExpressionError(parser->currentToken);
    }
    
    if (!ast || isEndOfFile(parser)) {
        freeAST(ast);
        
        return NULL;
    }

    consume(parser, TOKEN_RPAREN);

    return ast;
}

static AST* createBinary(AST* leftExpr, AST* rightExpr, Token token)
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

static AST* parsePrimary(Parser* parser)
{
    switch (parser->currentToken.type) {
        case TOKEN_INTEGER_LITERAL:
            return parseIntegerLiteral(parser, parser->currentToken);
        case TOKEN_BINARY_LITERAL:
            return parseBinaryLiteral(parser, parser->currentToken);
        case TOKEN_HEXADECIMAL_LITERAL:
            return parseHexadecimalLiteral(parser, parser->currentToken);
        case TOKEN_OCTAL_LITERAL:
            return parseOctalLiteral(parser, parser->currentToken);
        case TOKEN_LPAREN:
            return parseGroupExpression(parser);
        case TOKEN_IDENTIFIER:
            return parseIdentifier(parser);
        case TOKEN_EOF:
            return NULL;
        default:
            expectedExpressionError(parser->currentToken);
            return NULL;
    }
}

static AST* parsePrefixOperand(Parser* parser)
{
    if (isPrefixToken(parser->currentToken.type)) {
        return parsePrefix(parser);
    }

    Token token = parser->currentToken;
    AST* expr = parsePrimary(parser);
    
    if (!expr) {
        return NULL;
    }

    if (!isPrefix(expr) && !isPrefixOperand(expr)) {
        expectedOperandError(token);
    }
    
    return expr;
}

static AST* parsePrefix(Parser* parser)
{
    if (!isPrefixToken(parser->currentToken.type)) {
        return parsePrimary(parser);
    }

    Token token = parser->currentToken;
    consume(parser, token.type);
    AST* expr = parsePrefixOperand(parser);
    
    if (!expr) {
        return NULL;
    }

    AST* ast = createAST(AST_PREFIX);
    ast->prefix.expr = expr;
    ast->prefix.operator = token;

    return ast;
}

static AST* parseExponent(Parser* parser)
{
    AST* expr = parsePrefix(parser);
    Token token = parser->currentToken;

    if (token.type == TOKEN_POWER) {
        consume(parser, token.type);
        expr = createBinary(expr, parseExponent(parser), token);
    }

    return expr;
}

static AST* parseFactor(Parser* parser)
{
    AST* expr = parseExponent(parser);
    Token token = parser->currentToken;

    while (isFactorToken(token.type)) {
        consume(parser, token.type);
        expr = createBinary(expr, parseExponent(parser), token);
        token = parser->currentToken;
    }

    return expr;
}

static AST* parseTerm(Parser* parser)
{
    AST* expr = parseFactor(parser);
    Token token = parser->currentToken;

    while (isTermToken(token.type)) {
        consume(parser, token.type);
        expr = createBinary(expr, parseFactor(parser), token);
        token = parser->currentToken;
    }

    return expr;
}

static AST* parseShift(Parser* parser)
{
    AST* expr = parseTerm(parser);
    Token token = parser->currentToken;

    while (isShiftToken(token.type)) {
        consume(parser, token.type);
        expr = createBinary(expr, parseTerm(parser), token);
        token = parser->currentToken;
    }

    return expr;
}

static AST* parseComparison(Parser* parser)
{
    AST* expr = parseShift(parser);
    Token token = parser->currentToken;

    while (isComparisonToken(token.type)) {
        consume(parser, token.type);
        expr = createBinary(expr, parseShift(parser), token);
        token = parser->currentToken;
    }

    return expr;
}

static AST* parseEquality(Parser* parser)
{
    AST* expr = parseComparison(parser);
    Token token = parser->currentToken;

    while (isEqualityToken(token.type)) {
        consume(parser, token.type);
        expr = createBinary(expr, parseComparison(parser), token);
        token = parser->currentToken;
    }

    return expr;
}

static AST* parseBitwiseAND(Parser* parser)
{
    AST* expr = parseEquality(parser);
    Token token = parser->currentToken;

    while (token.type == TOKEN_AMPERSAND) {
        consume(parser, token.type);
        expr = createBinary(expr, parseEquality(parser), token);
        token = parser->currentToken;
    }

    return expr;
}

static AST* parseBitwiseXOR(Parser* parser)
{
    AST* expr = parseBitwiseAND(parser);
    Token token = parser->currentToken;

    while (token.type == TOKEN_CIRCUMFLEX) {
        consume(parser, token.type);
        expr = createBinary(expr, parseBitwiseAND(parser), token);
        token = parser->currentToken;
    }

    return expr;
}

static AST* parseBitwiseOR(Parser* parser)
{
    AST* expr = parseBitwiseXOR(parser);
    Token token = parser->currentToken;

    while (token.type == TOKEN_PIPE) {
        consume(parser, token.type);
        expr = createBinary(expr, parseBitwiseXOR(parser), token);
        token = parser->currentToken;
    }

    return expr;
}

static AST* parseBooleanAND(Parser* parser)
{
    AST* expr = parseBitwiseOR(parser);
    Token token = parser->currentToken;

    while (token.type == TOKEN_BOOLEAN_AND) {
        consume(parser, token.type);
        expr = createBinary(expr, parseBitwiseOR(parser), token);
        token = parser->currentToken;
    }

    return expr;
}

static AST* parseBooleanOR(Parser* parser)
{
    AST* expr = parseBooleanAND(parser);
    Token token = parser->currentToken;

    while (token.type == TOKEN_BOOLEAN_OR) {
        consume(parser, token.type);
        expr = createBinary(expr, parseBooleanAND(parser), token);
        token = parser->currentToken;
    }

    return expr;
}

static AST* parseExpression(Parser* parser)
{
    return parseBooleanOR(parser);
}

static AST* parseReturnStatement(Parser* parser)
{
    Token token = parser->currentToken;
    consume(parser, TOKEN_RETURN);

    AST* expr = parseExpression(parser);
    if (!expr) {
        return NULL;
    }

    AST* ast = createAST(AST_RETURN);
    ast->returnStatement.token = token;
    ast->returnStatement.expr = expr;

    return ast;
}

static AST* parseArgument(Parser* parser)
{
    return parseExpression(parser);
}

static bool parseArgumentListItem(Parser* parser, Vector* args)
{
    AST* expr = parseArgument(parser);

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

    return true;
}

static bool parseArgumentList(Parser* parser, Vector* args)
{
    consume(parser, TOKEN_LPAREN);

    if (isEndOfFile(parser)) {
        return false;
    }

    while (parser->currentToken.type != TOKEN_RPAREN) {
        if (!parseArgumentListItem(parser, args)) {
            return false;
        }
    }

    if (isEndOfFile(parser)) {
        return false;
    }

    consume(parser, TOKEN_RPAREN);

    return true;
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

static AST* parseParameter(Parser* parser)
{
    if (isEndOfFile(parser)) {
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

static bool parseParameterListItem(Parser* parser, Vector* params)
{
    AST* param = parseParameter(parser);

    if (!param) {
        return false;
    }

    pushVectorItem(params, param);

    if (parser->currentToken.type == TOKEN_COMMA) {
        consume(parser, TOKEN_COMMA);
    }

    return true;
}

static bool parseParameterList(Parser* parser, Vector* params)
{
    consume(parser, TOKEN_LPAREN);

    if (isEndOfFile(parser)) {
        return false;
    }

    while (parser->currentToken.type != TOKEN_RPAREN) {
        if (!parseParameterListItem(parser, params)) {
            return false;
        }
    }

    if (isEndOfFile(parser)) {
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

static AST* parseFunctionCall(Parser* parser)
{
    AST* ast = createFunctionCall(parser->prevToken);

    if (!parseArgumentList(parser, &ast->functionCall.args)) {
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
    if (isEndOfFile(parser)) {
        return false;
    }

    consume(parser, TOKEN_LBRACE);

    AST* body = createAST(AST_COMPOUND);
    body->compound.scope = NULL;
    ast->functionDefinition.body = body;

    if (!parseBlocklevelStatements(parser, &body->compound.statements)) {
        return false;
    }

    consume(parser, TOKEN_RBRACE);

    return true;
}

static AST* parseFunctionDefinition(Parser* parser)
{
    consume(parser, TOKEN_FUNC);

    if (isEndOfFile(parser)) {
        return NULL;
    }

    Token token = parser->currentToken;
    StringObject* id = copyStringObject(token.chars, token.length);

    if (parser->currentToken.type != TOKEN_IDENTIFIER) {
        expectedTokenError(TOKEN_IDENTIFIER, parser->currentToken);
    }

    consume(parser, TOKEN_IDENTIFIER);

    if (isEndOfFile(parser)) {
        return NULL;
    }

    AST* ast = createFunctionDefinition(token, id);

    if (!parseParameterList(parser, &ast->functionDefinition.params)) {
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

static AST* parseAssignment(Parser* parser)
{
    Token operator = parser->currentToken;
    Token token = parser->prevToken;
    consume(parser, operator.type);

    AST* expr = parseExpression(parser);
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
    ast->variableDefinition.expr = parseExpression(parser);
    ast->variableDefinition.typeId = TOKEN_NONE;

    return ast->variableDefinition.expr != NULL;
}

static AST* parseVariableDefinition(Parser* parser)
{
    consume(parser, TOKEN_VAR);

    if (isEndOfFile(parser)) {
        return NULL;
    }

    Token token = parser->currentToken;
    StringObject* id = copyStringObject(token.chars, token.length);

    consume(parser, TOKEN_IDENTIFIER);

    if (isEndOfFile(parser)) {
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

static AST* parseIdentifier(Parser* parser)
{
    consume(parser, TOKEN_IDENTIFIER);
    
    if (isAssignmentToken(parser->currentToken.type)) {
        return parseAssignment(parser);
    }

    if (parser->currentToken.type == TOKEN_LPAREN) {
        return parseFunctionCall(parser);
    }

    return createVariable(parser->prevToken);
}

static AST* parseStatement(Parser* parser)
{
    switch (parser->currentToken.type) {
        case TOKEN_FUNC:
            return parseFunctionDefinition(parser);
        case TOKEN_VAR:
            return parseVariableDefinition(parser);
        case TOKEN_RETURN:
            return parseReturnStatement(parser);
        default:
            return parseExpression(parser);
    }
}

static bool parseStatements(Parser* parser, Vector* nodes, TokenType type)
{
    Token token = parser->currentToken;

    while (token.type != type) {
        AST* stmt = parseStatement(parser);
        if (!stmt) {
            return false;
        }
        
        bool sameLineStatement = !isEndOfFile(parser) &&
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

static bool parseBlocklevelStatements(Parser* parser, Vector* nodes)
{
    return parseStatements(parser, nodes, TOKEN_RBRACE);
}

static bool parseToplevelStatements(Parser* parser)
{
    return parseStatements(parser, &parser->topLevel->compound.statements, TOKEN_EOF);
}

void initParser(Parser* parser, AST* ast)
{
    parser->topLevel = ast;
}

bool parse(Parser* parser, char* source)
{
    initLexer(&parser->lexer, source);
    advance(parser);

    return parseToplevelStatements(parser);
}
