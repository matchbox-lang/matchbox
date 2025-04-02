#include "parser.h"
#include "ast.h"
#include "conversion.h"
#include "lexer.h"
#include "scope.h"
#include "table.h"
#include "service.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

static AST* expression();
static AST* identifier();
static AST* statements();
static AST* variable();

static Token currentToken;
static Token prevToken;
static Scope* currentScope;
static Scope* topLevel;

static const char* invalidArgsError = "Error: Invalid arguments to function %.*s";
static const char* invalidOperandsError = "Error: Invalid operands to binary %.*s";
static const char* invalidTypeError = "Error: Invalid type for variable %.*s";
static const char* redefinitionError = "Error: Redefinition of %.*s";
static const char* undefinedError = "Error: %.*s is undefined";
static const char* unexpectedEndError = "Error: Unexpected end of input";
static const char* unexpectedTokenError = "Error: Unexpected %.*s";
static const char* uninitializedError = "Error: %.*s is uninitialized";

static void advance()
{
    prevToken = currentToken;
    currentToken = scanToken();
}

static void error(const char* message, Token token)
{
    fprintf(stderr, message, token.length, token.chars);
    fprintf(stderr, " on line %d:%d\n", token.line, token.column);
    exit(1);
}

static void consume(TokenType type)
{
    if (currentToken.type != type) {
        error(unexpectedTokenError, currentToken);
    }
    
    advance();
}

static void consumeType()
{
    if (!isTypeToken(currentToken.type)) {
        error(unexpectedTokenError, currentToken);
    }

    consume(currentToken.type);
}

static bool isEof()
{
    return currentToken.type == T_EOF;
}

static AST* booleanLiteral(Token token)
{
    AST* ast = createAST(AST_BOOLEAN);
    ast->boolValue = token.type == T_TRUE;
    consume(token.type);

    return ast;
}

static AST* integerLiteral(Token token)
{
    AST* ast = createAST(AST_INTEGER);
    ast->intValue = integerLiteralToValue(token.chars, token.length);
    consume(token.type);

    return ast;
}

static AST* binaryLiteral(Token token)
{
    AST* ast = createAST(AST_INTEGER);
    ast->intValue = binaryLiteralToValue(token.chars, token.length);
    consume(token.type);

    return ast;
}

static AST* hexadecimalLiteral(Token token)
{
    AST* ast = createAST(AST_INTEGER);
    ast->intValue = hexadecimalLiteralToValue(token.chars, token.length);
    consume(token.type);

    return ast;
}

static AST* octalLiteral(Token token)
{
    AST* ast = createAST(AST_INTEGER);
    ast->intValue = octalLiteralToValue(token.chars, token.length);
    consume(token.type);

    return ast;
}

static AST* floatLiteral(Token token)
{
    AST* ast = createAST(AST_FLOAT);
    ast->floatValue = floatLiteralToValue(token.chars, token.length);
    consume(token.type);

    return ast;
}

static AST* characterLiteral(Token token)
{
    AST* ast = createAST(AST_CHARACTER);
    ast->character = token;
    consume(token.type);

    return ast;
}

static AST* stringLiteral(Token token)
{
    AST* ast = createAST(AST_STRING);
    ast->string = token;
    consume(token.type);

    return ast;
}

static AST* groupExpression()
{
    consume(T_LPAREN);
    AST* ast = expression();

    if (!ast && currentToken.type == T_RPAREN) {
        error(unexpectedTokenError, currentToken);
    }
    
    if (!ast || isEof()) {
        freeAST(ast);
        return NULL;
    }

    consume(T_RPAREN);

    return ast;
}

static AST* primary()
{
    switch (currentToken.type) {
        case T_TRUE:
        case T_FALSE:
            return booleanLiteral(currentToken);
        case T_INTEGER_LITERAL:
            return integerLiteral(currentToken);
        case T_BINARY_LITERAL:
            return binaryLiteral(currentToken);
        case T_HEXADECIMAL_LITERAL:
            return hexadecimalLiteral(currentToken);
        case T_OCTAL_LITERAL:
            return octalLiteral(currentToken);
        case T_FLOAT_LITERAL:
            return floatLiteral(currentToken);
        case T_CHARACTER_LITERAL:
            return characterLiteral(currentToken);
        case T_STRING_LITERAL:
            return stringLiteral(currentToken);
        case T_LPAREN:
            return groupExpression();
        case T_IDENTIFIER:
            return identifier();
        case T_EOF:
            return NULL;
    }

    error(unexpectedTokenError, currentToken);
}

static AST* postfix()
{
    if (!isPostfixToken(currentToken.type)) {
        return primary();
    }

    Token token = currentToken;

    AST* expr = variable();
    if (!expr) {
        return NULL;
    }

    AST* ast = createAST(AST_POSTFIX);
    ast->postfix.expr = expr;
    ast->postfix.operator = token;
    consume(token.type);

    return ast;
}

static AST* prefix()
{
    if (!isPrefixToken(currentToken.type)) {
        return postfix();
    }

    Token token = currentToken;
    consume(token.type);

    if (isEof()) {
        return NULL;
    }

    AST* ast = createAST(AST_PREFIX);
    ast->prefix.operator = token;

    if (currentToken.type == T_IDENTIFIER) {
        consume(T_IDENTIFIER);
        ast->prefix.expr = variable();
    } else if (!isPostfixToken(token.type)) {
        ast->prefix.expr = prefix();
    } else {
        error(unexpectedTokenError, currentToken);
    }

    if (!ast->prefix.expr) {
        freeAST(ast);
        return NULL;
    }

    return ast;
}

static AST* binary(AST* leftExpr, AST* rightExpr, Token token)
{
    if (!rightExpr) {
        return NULL;
    }

    int a = getTypeId(leftExpr);
    int b = getTypeId(rightExpr);

    if (a != b) {
        error(invalidOperandsError, token);
    }

    int typeId = isBoolOperatorToken(token.type) ? T_BOOL : a;
    
    AST* ast = createAST(AST_BINARY);
    ast->binary.leftExpr = leftExpr;
    ast->binary.operator = token;
    ast->binary.rightExpr = rightExpr;
    ast->binary.typeId = typeId;

    return ast;
}

static AST* exponent()
{
    AST* expr = prefix();
    Token token = currentToken;

    while (token.type == T_POWER) {
        consume(token.type);
        expr = binary(expr, prefix(), token);
        token = currentToken;
    }

    return expr;
}

static AST* factor()
{
    AST* expr = exponent();
    Token token = currentToken;

    while (isFactorToken(token.type)) {
        consume(token.type);
        expr = binary(expr, exponent(), token);
        token = currentToken;
    }

    return expr;
}

static AST* term()
{
    AST* expr = factor();
    Token token = currentToken;

    while (isTermToken(token.type)) {
        consume(token.type);
        expr = binary(expr, factor(), token);
        token = currentToken;
    }

    return expr;
}

static AST* shift()
{
    AST* expr = term();
    Token token = currentToken;

    while (isShiftToken(token.type)) {
        consume(token.type);
        expr = binary(expr, term(), token);
        token = currentToken;
    }

    return expr;
}

static AST* comparison()
{
    AST* expr = shift();
    Token token = currentToken;

    while (isComparisonToken(token.type)) {
        consume(token.type);
        expr = binary(expr, shift(), token);
        token = currentToken;
    }

    return expr;
}

static AST* equality()
{
    AST* expr = comparison();
    Token token = currentToken;

    while (isEqualityToken(token.type)) {
        consume(token.type);
        expr = binary(expr, comparison(), token);
        token = currentToken;
    }

    return expr;
}

static AST* bitwiseAND()
{
    AST* expr = equality();
    Token token = currentToken;

    while (token.type == T_AMPERSAND) {
        consume(token.type);
        expr = binary(expr, equality(), token);
        token = currentToken;
    }

    return expr;
}

static AST* bitwiseXOR()
{
    AST* expr = bitwiseAND();
    Token token = currentToken;

    while (token.type == T_CIRCUMFLEX) {
        consume(token.type);
        expr = binary(expr, bitwiseAND(), token);
        token = currentToken;
    }

    return expr;
}

static AST* bitwiseOR()
{
    AST* expr = bitwiseXOR();
    Token token = currentToken;

    while (token.type == T_PIPE) {
        consume(token.type);
        expr = binary(expr, bitwiseXOR(), token);
        token = currentToken;
    }

    return expr;
}

static AST* booleanAND()
{
    AST* expr = bitwiseOR();
    Token token = currentToken;

    while (token.type == T_BOOLEAN_AND) {
        consume(token.type);
        expr = binary(expr, bitwiseOR(), token);
        token = currentToken;
    }

    return expr;
}

static AST* booleanOR()
{
    AST* expr = booleanAND();
    Token token = currentToken;

    while (token.type == T_BOOLEAN_OR) {
        consume(token.type);
        expr = binary(expr, booleanAND(), token);
        token = currentToken;
    }

    return expr;
}

static AST* expression()
{
    return booleanOR();
}

static AST* returnStatement()
{
    if (currentScope->level < 2) {
        error(unexpectedTokenError, currentToken);
    }

    consume(T_RETURN);
    
    AST* expr = expression();
    if (!expr) {
        return NULL;
    }

    AST* ast = createAST(AST_RETURN);
    ast->expr = expr;

    return ast;
}

static AST* parameter()
{
    if (isEof()) {
        return NULL;
    }
    
    Token token = currentToken;
    StringObject* id = copyStringObject(token.chars, token.length);
    AST* symbol = getLocalSymbol(currentScope, id);

    if (symbol) {
        error(redefinitionError, token);
    }
    
    consume(T_IDENTIFIER);

    AST* ast = createAST(AST_PARAMETER);
    ast->param.scope = currentScope;
    ast->param.id = id;
    ast->param.typeId = T_INT;
    ast->param.position = getLocalCount(currentScope);

    if (isTypeToken(currentToken.type)) {
        ast->param.typeId = currentToken.type;
        consumeType();
    }

    setLocalVariableSymbol(currentScope, id, ast);

    return ast;
}

static AST* variable()
{
    Token token = prevToken;
    StringObject* id = copyStringObject(token.chars, token.length);
    AST* symbol = getLocalSymbol(currentScope, id);

    if (!symbol) {
        symbol = getLocalSymbol(topLevel, id);
    }

    freeStringObject(id);

    if (!symbol || !isVariableType(symbol)) {
        error(undefinedError, token);
    }
    
    if (!isInitialized(symbol)) {
        error(uninitializedError, token);
    }

    AST* ast = createAST(AST_VARIABLE);
    ast->var.scope = currentScope;
    ast->var.symbol = symbol;

    return ast;
}

static void compareFunctionSignature(AST* caller, AST* callee, Token token)
{
    size_t argCount = countVector(&caller->funcCall.args);
    size_t paramCount = countVector(&callee->funcDef.params);

    if (argCount != paramCount) {
        error(invalidArgsError, token);
    }

    for (int i = 0; i < argCount; i++) {
        AST* a = getVectorAt(&caller->funcCall.args, i);
        AST* b = getVectorAt(&callee->funcDef.params, i);
        int typeId = getTypeId(a);

        if (typeId != b->param.typeId) {
            error(invalidArgsError, token);
        }
    }
}

static void compareServiceSignature(AST* caller, Service* service, Token token)
{
    size_t argCount = countVector(&caller->syscall.args);

    if (argCount != service->paramCount) {
        error(invalidArgsError, token);
    }

    for (int i = 0; i < argCount; i++) {
        AST* expr = getVectorAt(&caller->syscall.args, i);
        int typeId = getTypeId(expr);

        if (typeId != service->params[i]) {
            error(invalidArgsError, token);
        }
    }
}

static bool arguments(Vector* args)
{
    consume(T_LPAREN);

    if (isEof()) {
        return false;
    }

    while (currentToken.type != T_RPAREN) {
        AST* expr = expression();

        if (!expr && (currentToken.type == T_COMMA || currentToken.type == T_RPAREN)) {
            error(unexpectedTokenError, currentToken);
        }

        if (!expr) {
            return false;
        }

        pushVectorItem(args, expr);

        if (currentToken.type == T_COMMA) {
            consume(T_COMMA);
        }
    }

    if (isEof()) {
        return false;
    }

    consume(T_RPAREN);

    return true;
}

static bool parameters(Vector* params)
{
    consume(T_LPAREN);

    if (isEof()) {
        return false;
    }

    while (currentToken.type != T_RPAREN) {
        AST* expr = parameter();

        if (!expr && (currentToken.type == T_COMMA || currentToken.type == T_RPAREN)) {
            error(unexpectedTokenError, currentToken);
        }

        if (!expr) {
            return false;
        }

        pushVectorItem(params, expr);

        if (currentToken.type == T_COMMA) {
            consume(T_COMMA);
        }
    }

    if (isEof()) {
        return false;
    }
    
    consume(T_RPAREN);

    return true;
}

static AST* systemCall(Token token)
{
    StringObject* id = copyStringObject(token.chars, token.length);
    Service* service = getServiceByName(id->chars);

    if (!service) {
        error(undefinedError, token);
    }

    freeStringObject(id);

    AST* ast = createAST(AST_SYSCALL);
    ast->syscall.opcode = service->opcode;
    ast->syscall.service = service;

    if (!arguments(&ast->syscall.args)) {
        freeAST(ast);
        return NULL;
    }

    compareServiceSignature(ast, service, token);

    return ast;
}

static AST* functionCall()
{
    Token token = prevToken;
    StringObject* id = copyStringObject(token.chars, token.length);
    AST* symbol = getSymbol(currentScope, id);
    
    freeStringObject(id);
    
    if (!symbol) {
        return systemCall(token);
    }

    if (!symbol || symbol->type != AST_FUNCTION_DEFINITION) {
        error(undefinedError, token);
    }

    AST* ast = createAST(AST_FUNCTION_CALL);
    ast->funcCall.scope = currentScope;
    ast->funcCall.symbol = symbol;

    if (!arguments(&ast->funcCall.args)) {
        freeAST(ast);
        return NULL;
    }

    compareFunctionSignature(ast, symbol, token);

    return ast;
}

static AST* functionDefinition()
{
    consume(T_FUNC);

    if (isEof()) {
        return NULL;
    }

    Token token = currentToken;
    StringObject* id = copyStringObject(token.chars, token.length);
    AST* symbol = getLocalSymbol(currentScope, id);

    if (symbol) {
        error(redefinitionError, token);
    }

    if (currentToken.type != T_IDENTIFIER) {
        error(unexpectedTokenError, currentToken);
    }

    consume(T_IDENTIFIER);

    if (isEof()) {
        return NULL;
    }

    AST* ast = createAST(AST_FUNCTION_DEFINITION);
    ast->funcDef.scope = currentScope;
    ast->funcDef.id = id;
    ast->funcDef.typeId = T_INT;
    ast->funcDef.body = NULL;

    currentScope = createScope(currentScope);
    
    if (!parameters(&ast->funcDef.params)) {
        freeAST(ast);
        return NULL;
    }

    if (isTypeToken(currentToken.type)) {
        ast->funcDef.typeId = currentToken.type;
        consumeType();
    }

    if (isEof()) {
        freeAST(ast);
        return NULL;
    }

    consume(T_LBRACE);

    AST* body = statements(T_RBRACE);
    if (!body) {
        freeAST(ast);
        return NULL;
    }

    ast->funcDef.body = body;
    consume(T_RBRACE);
    currentScope = currentScope->parent;
    setLocalSymbol(currentScope, id, ast);

    return ast;
}

static AST* assignment()
{
    Token operator = currentToken;
    Token token = prevToken;
    StringObject* id = copyStringObject(token.chars, token.length);
    AST* symbol = getSymbol(currentScope, id);

    freeStringObject(id);

    if (!symbol) {
        error(undefinedError, token);
    }

    if (currentScope != getScope(symbol) && !isInitialized(symbol)) {
        error(uninitializedError, token);
    }
    
    consume(operator.type);
    
    AST* expr = expression();
    if (!expr) {
        return NULL;
    }

    AST* ast = createAST(AST_ASSIGNMENT);
    ast->assignment.scope = currentScope;
    ast->assignment.operator = operator;
    ast->assignment.symbol = symbol;
    ast->assignment.expr = expr;

    initialize(symbol);

    return ast;
}

static AST* variableDefinition()
{
    consume(T_VAR);

    if (isEof()) {
        return NULL;
    }

    Token token = currentToken;
    StringObject* id = copyStringObject(token.chars, token.length);
    AST* symbol = getLocalSymbol(currentScope, id);

    if (symbol) {
        error(redefinitionError, token);
    }

    if (currentToken.type != T_IDENTIFIER) {
        error(unexpectedTokenError, currentToken);
    }

    consume(T_IDENTIFIER);

    if (isEof()) {
        return NULL;
    }

    AST* ast = createAST(AST_VARIABLE_DEFINITION);
    ast->varDef.scope = currentScope;
    ast->varDef.id = id;
    ast->varDef.position = getLocalCount(currentScope);
    ast->varDef.expr = NULL;

    if (isTypeToken(currentToken.type)) {
        ast->varDef.typeId = currentToken.type;
        consumeType();
    }
    
    if (currentToken.type != T_EQUAL) {
        ast->varDef.expr = createAST(AST_NONE);
        ast->varDef.typeId = T_INT;
        setLocalVariableSymbol(currentScope, id, ast);

        return ast;
    }
    
    consume(T_EQUAL);

    AST* expr = expression();
    if (!expr) {
        freeAST(ast);
        return NULL;
    }

    ast->varDef.typeId = getTypeId(expr);
    ast->varDef.expr = expr;

    if (ast->varDef.typeId == T_NONE) {
        error(invalidTypeError, token);
    }

    setLocalVariableSymbol(currentScope, id, ast);
    initialize(ast);

    return ast;
}

static AST* identifier()
{
    consume(T_IDENTIFIER);
    
    if (isAssignmentToken(currentToken.type)) {
        return assignment();
    } else if (isPostfixToken(currentToken.type)) {
        return postfix();
    } else if (currentToken.type == T_LPAREN) {
        return functionCall();
    }

    return variable();
}

static AST* statement()
{
    switch (currentToken.type) {
        case T_FUNC:
            return functionDefinition();
        case T_VAR:
            return variableDefinition();
        case T_RETURN:
            return returnStatement();
    }

    return expression();
}

static AST* statements(TokenType type)
{
    Token token = currentToken;
    AST* ast = createAST(AST_COMPOUND);

    ast->compound.scope = currentScope;

    while (token.type != type) {
        AST* stmt = statement();
        if (!stmt) {
            freeAST(ast);
            return NULL;
        }
        
        if (!isEof() && 
            currentToken.line == token.line &&
            currentToken.type != type &&
            prevToken.type != T_RBRACE) {
            consume(T_SEMICOLON);
        }

        pushVectorItem(&ast->compound.statements, stmt);
        token = currentToken;
    }

    return ast;
}

AST* parse(char* source)
{
    initLexer(source);
    currentScope = createScope(NULL);
    topLevel = currentScope;
    advance();

    AST* ast = statements(T_EOF);
    if (!ast) {
        error(unexpectedEndError, currentToken);
    }
    
    return ast;
}
