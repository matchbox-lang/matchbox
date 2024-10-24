#include "parser.h"
#include "ast.h"
#include "lexer.h"
#include "scope.h"
#include "table.h"
#include "service.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct Parser
{
    Token current;
    Token previous;
    Scope* scope;
} Parser;

Parser parser;

static AST* expression();
static AST* identifier();
static AST* statements();
static AST* variable();

const char* assignmentError = "Error: Invalid assignment for variable %.*s on line %d:%d\n";
const char* invalidArgumentsError = "Error: Invalid arguments to function %.*s on line %d:%d\n";
const char* invalidOperandError = "Error: Invalid operand to unary %.*s on line %d:%d\n";
const char* invalidOperandsError = "Error: Invalid operands to binary %.*s on line %d:%d\n";
const char* invalidTypeError = "Error: Invalid type for variable %.*s on line %d:%d\n";
const char* identifierError = "Error: Expected identifier on line %d:%d\n";
const char* redefinitionError = "Error: Redefinition of %.*s on line %d:%d\n";
const char* undefinedError = "Error: %.*s is undefined on line %d:%d\n";
const char* unexpectedTokenError = "Error: Unexpected %.*s on line %d:%d\n";
const char* unexpectedLastTokenError = "Error: Unexpected end of input on line %d:%d\n";
const char* uninitializedError = "Error: %.*s is uninitialized on line %d:%d\n";

static void advance()
{
    parser.previous = parser.current;
    parser.current = scanToken();
}

static Token peek()
{
    return parser.current;
}

static Token prev()
{
    return parser.previous;
}

static void error(const char* message, Token token)
{
    fprintf(stderr, message, token.length, token.chars, token.line, token.column);
    exit(1);
}

static void lineError(const char* message, Token token)
{
    int column = token.column + token.length;

    fprintf(stderr, message, token.line, column);
    exit(1);
}

static void tokenError()
{
    Token token = peek();

    if (token.type == T_EOF) {
        lineError(unexpectedLastTokenError, prev());
    }

    error(unexpectedTokenError, token);
}

static bool isBool(TokenType type)
{
    switch (type) {
        case T_TRUE:
        case T_FALSE:
            return true;
    }

    return false;
}

static bool isAssignment(TokenType type)
{
    switch (type) {
        case T_EQUAL:
        case T_PLUS_EQUAL:
        case T_MINUS_EQUAL:
        case T_STAR_EQUAL:
        case T_SLASH_EQUAL:
        case T_FLOOR_EQUAL:
        case T_PERCENT_EQUAL:
        case T_POWER_EQUAL:
            return true;
    }

    return false;
}

static bool isType(TokenType type)
{
    return type == T_INT;
}

static bool isComparison(TokenType type)
{
    switch (type) {
        case T_GREATER:
        case T_GREATER_EQUAL:
        case T_LESS:
        case T_LESS_EQUAL:
        case T_SPACESHIP:
            return true;
    }

    return false;
}

static bool isEquality(TokenType type)
{
    switch (type) {
        case T_EQUAL_EQUAL:
        case T_NOT_EQUAL:
            return true;
    }

    return false;
}

static bool isBoolOperator(TokenType type)
{
    return isComparison(type) || isEquality(type);
}

static bool isShift(TokenType type)
{
    switch (type) {
        case T_LSHIFT:
        case T_RSHIFT:
            return true;
    }

    return false;
}

static bool isTerm(TokenType type)
{
    switch (type) {
        case T_PLUS:
        case T_MINUS:
            return true;
    }

    return false;
}

static bool isFactor(TokenType type)
{
    switch (type) {
        case T_STAR:
        case T_SLASH:
        case T_FLOOR:
        case T_PERCENT:
            return true;
    }

    return false;
}

static bool isPrefix(TokenType type)
{
    switch (type) {
        case T_INCREMENT:
        case T_DECREMENT:
        case T_PLUS:
        case T_MINUS:
        case T_EXCLAMATION:
        case T_TILDE:
            return true;
    }

    return false;
}

static bool isPostfix(TokenType type)
{
    switch (type) {
        case T_INCREMENT:
        case T_DECREMENT:
            return true;
    }

    return false;
}

static void consume(TokenType type)
{
    Token token = peek();

    if (token.type != type) {
        tokenError();
    }
    
    advance();
}

static void consumeType()
{
    Token token = peek();

    if (!isType(token.type)) {
        tokenError();
    }

    consume(token.type);
}

static AST* primary()
{
    Token token = peek();

    if (isBool(token.type)) {
        AST* ast = createAST(AST_BOOLEAN);
        ast->boolVal = token.type == T_TRUE;
        consume(token.type);

        return ast;
    }

    if (token.type == T_DECIMAL_LITERAL) {
        AST* ast = createAST(AST_INTEGER);
        ast->intVal = strtol(token.chars, NULL, 10);
        consume(token.type);

        return ast;
    }

    if (token.type == T_FLOAT_LITERAL) {
        AST* ast = createAST(AST_FLOAT);
        ast->floatVal = strtod(token.chars, NULL);
        consume(token.type);

        return ast;
    }

    if (token.type == T_CHARACTER_LITERAL) {
        AST* ast = createAST(AST_CHARACTER);
        ast->character = token;
        consume(token.type);

        return ast;
    }

    if (token.type == T_STRING_LITERAL) {
        AST* ast = createAST(AST_STRING);
        ast->string = token;
        consume(token.type);

        return ast;
    }

    if (token.type == T_LPAREN) {
        consume(T_LPAREN);
        AST* ast = expression();
        
        if (ast->type == AST_NONE) {
            tokenError();
        }

        consume(T_RPAREN);

        return ast;
    }

    if (token.type == T_IDENTIFIER) {
        return identifier();
    }
    
    return createAST(AST_NONE);
}

static AST* postfix()
{
    if (peek().type != T_IDENTIFIER) {
        return primary();
    }

    AST* expr = identifier();
    Token token = peek();

    if (!isPostfix(token.type)) {
        return expr;
    }

    AST* ast = createAST(AST_POSTFIX);
    ast->postfix.operator = token;
    consume(token.type);
    ast->postfix.expr = expr;

    return ast;
}

static AST* prefix()
{
    if (!isPrefix(peek().type)) {
        return postfix();
    }

    Token token = peek();
    AST* ast = createAST(AST_PREFIX);
    ast->prefix.operator = token;
    consume(token.type);

    if (peek().type == T_IDENTIFIER) {
        consume(T_IDENTIFIER);
        ast->prefix.expr = variable();
    } else {
        ast->prefix.expr = prefix();
    }

    return ast;

}

static AST* binary(AST* leftExpr, AST* rightExpr, Token token)
{
    if (rightExpr->type == AST_NONE) {
        tokenError();
    }

    int a = getTypeId(leftExpr);
    int b = getTypeId(rightExpr);

    if (a != b) {
        error(invalidOperandsError, token);
    }

    int typeId = isBoolOperator(token.type) ? T_BOOL : a;
    
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
    Token token = peek();

    while (token.type == T_POWER) {
        consume(token.type);
        expr = binary(expr, prefix(), token);
        token = peek();
    }

    return expr;
}

static AST* factor()
{
    AST* expr = exponent();
    Token token = peek();

    while (isFactor(token.type)) {
        consume(token.type);
        expr = binary(expr, exponent(), token);
        token = peek();
    }

    return expr;
}

static AST* term()
{
    AST* expr = factor();
    Token token = peek();

    while (isTerm(token.type)) {
        consume(token.type);
        expr = binary(expr, factor(), token);
        token = peek();
    }

    return expr;
}

static AST* shift()
{
    AST* expr = term();
    Token token = peek();

    while (isShift(token.type)) {
        consume(token.type);
        expr = binary(expr, term(), token);
        token = peek();
    }

    return expr;
}

static AST* comparison()
{
    AST* expr = shift();
    Token token = peek();

    while (isComparison(token.type)) {
        consume(token.type);
        expr = binary(expr, shift(), token);
        token = peek();
    }

    return expr;
}

static AST* equality()
{
    AST* expr = comparison();
    Token token = peek();

    while (isEquality(token.type)) {
        consume(token.type);
        expr = binary(expr, comparison(), token);
        token = peek();
    }

    return expr;
}

static AST* bitwiseAND()
{
    AST* expr = equality();
    Token token = peek();

    while (token.type == T_AMPERSAND) {
        consume(token.type);
        expr = binary(expr, equality(), token);
        token = peek();
    }

    return expr;
}

static AST* bitwiseXOR()
{
    AST* expr = bitwiseAND();
    Token token = peek();

    while (token.type == T_CIRCUMFLEX) {
        consume(token.type);
        expr = binary(expr, bitwiseAND(), token);
        token = peek();
    }

    return expr;
}

static AST* bitwiseOR()
{
    AST* expr = bitwiseXOR();
    Token token = peek();

    while (token.type == T_PIPE) {
        consume(token.type);
        expr = binary(expr, bitwiseXOR(), token);
        token = peek();
    }

    return expr;
}

static AST* booleanAND()
{
    AST* expr = bitwiseOR();
    Token token = peek();

    while (token.type == T_BOOLEAN_AND) {
        consume(token.type);
        expr = binary(expr, bitwiseOR(), token);
        token = peek();
    }

    return expr;
}

static AST* booleanOR()
{
    AST* expr = booleanAND();
    Token token = peek();

    while (token.type == T_BOOLEAN_OR) {
        consume(token.type);
        expr = binary(expr, booleanAND(), token);
        token = peek();
    }

    return expr;
}

static AST* expression()
{
    return booleanOR();
}

static AST* returnStmt()
{
    if (parser.scope->level < 2) {
        tokenError();
    }

    consume(T_RETURN);
    
    AST* ast = createAST(AST_RETURN);
    ast->expr = expression();

    return ast;
}

static AST* argument()
{
    AST* expr = expression();

    if (expr->type == AST_NONE) {
        tokenError();
    }

    return expr;
}

static AST* parameter()
{
    Token token = peek();
    StringObject* id = copyString(token.chars, token.length);
    AST* symbol = getLocalSymbol(parser.scope, id);

    if (symbol) {
        error(redefinitionError, token);
    }
    
    consume(T_IDENTIFIER);

    AST* ast = createAST(AST_PARAMETER);
    ast->param.scope = parser.scope;
    ast->param.id = id;
    ast->param.typeId = T_INT;

    if (isType(peek().type)) {
        ast->param.typeId = peek().type;
        consumeType();
    }

    setLocalSymbol(parser.scope, id, ast, false);

    return ast;
}

static AST* assignment()
{
    Token operator = peek();
    Token token = prev();
    StringObject* id = copyString(token.chars, token.length);
    AST* symbol = getSymbol(parser.scope, id);

    if (!symbol) {
        error(undefinedError, token);
    }

    consume(operator.type);

    AST* ast = createAST(AST_ASSIGNMENT);
    ast->assignment.scope = parser.scope;
    ast->assignment.id = id;
    ast->assignment.operator = operator;

    AST* expr = expression();
    if (expr->type == AST_NONE) {
        tokenError();
    }

    if (symbol->type == AST_VARIABLE_DEFINITION) {
        symbol->varDef.initialized = true;
    }

    ast->assignment.expr = expr;

    return ast;
}

static AST* variable()
{
    Token token = prev();
    StringObject* id = copyString(token.chars, token.length);
    AST* symbol = getLocalSymbol(parser.scope, id);

    if (!symbol) {
        error(undefinedError, token);
    }
    
    if (symbol->type == AST_VARIABLE_DEFINITION && !symbol->varDef.initialized) {
        error(uninitializedError, token);
    }

    AST* ast = createAST(AST_VARIABLE);
    ast->var.scope = parser.scope;
    ast->var.id = id;
    ast->var.symbol = symbol;

    return ast;
}

static void variableExpression(AST* ast, Token token)
{
    AST* expr = expression();

    if (expr->type == AST_NONE) {
        tokenError();
    }

    int typeId = getTypeId(expr);
    if (typeId < 0) {
        error(assignmentError, token);
    }

    ast->varDef.initialized = true;
    ast->varDef.expr = expr;
    ast->varDef.typeId = typeId;
}

static AST* variableDefinition()
{
    consume(T_VAR);

    Token token = peek();
    StringObject* id = copyString(token.chars, token.length);
    AST* symbol = getLocalSymbol(parser.scope, id);

    if (symbol) {
        error(redefinitionError, token);
    }

    if (peek().type != T_IDENTIFIER) {
        lineError(identifierError, prev());
    }

    consume(T_IDENTIFIER);

    AST* ast = createAST(AST_VARIABLE_DEFINITION);
    ast->varDef.scope = parser.scope;
    ast->varDef.id = id;
    ast->varDef.typeId = T_NONE;
    ast->varDef.position = getLocalCount(parser.scope);

    if (isType(peek().type)) {
        ast->varDef.typeId = peek().type;
        consumeType();
    }
    
    if (peek().type != T_EQUAL) {
        ast->varDef.expr = createAST(AST_NONE);
        ast->varDef.initialized = false;
    } else {
        consume(T_EQUAL);
        variableExpression(ast, token);
    }

    if (ast->varDef.typeId == T_NONE) {
        error(invalidTypeError, token);
    }

    setLocalSymbol(parser.scope, id, ast, true);

    return ast;
}

static void compareFunctionSignature(AST* caller, AST* callee, Token token)
{
    size_t argCount = countVector(&caller->funcCall.args);
    size_t paramCount = countVector(&callee->funcDef.params);

    if (argCount != paramCount) {
        error(invalidArgumentsError, token);
    }

    for (int i = 0; i < argCount; i++) {
        AST* a = getVectorAt(&caller->funcCall.args, i);
        AST* b = getVectorAt(&callee->funcDef.params, i);
        int typeId = getTypeId(a);

        if (typeId != b->param.typeId) {
            error(invalidArgumentsError, token);
        }
    }
}

static void compareServiceSignature(AST* caller, Service* service, Token token)
{
    size_t argCount = countVector(&caller->syscall.args);

    if (argCount != service->paramCount) {
        error(invalidArgumentsError, token);
    }

    for (int i = 0; i < argCount; i++) {
        AST* expr = getVectorAt(&caller->syscall.args, i);
        int typeId = getTypeId(expr);

        if (typeId != service->params[i]) {
            error(invalidArgumentsError, token);
        }
    }
}

static void arguments(Vector* args)
{
    consume(T_LPAREN);

    while (peek().type != T_RPAREN) {
        AST* expr = argument();
        pushVector(args, expr);

        if (peek().type != T_COMMA) {
            break;
        }
        
        consume(T_COMMA);
    }

    consume(T_RPAREN);
}

static void parameters(Vector* params)
{
    consume(T_LPAREN);

    int paramCount = 0;

    while (peek().type != T_RPAREN) {
        AST* expr = parameter();
        expr->param.position = paramCount++;
        pushVector(params, expr);

        if (peek().type != T_COMMA) {
            break;
        }
        
        consume(T_COMMA);
    }
    
    consume(T_RPAREN);
}

static AST* systemCall(Service* service, Token token)
{
    AST* ast = createAST(AST_SYSCALL);
    ast->syscall.opcode = service->opcode;
    ast->syscall.service = service;

    arguments(&ast->syscall.args);
    compareServiceSignature(ast, service, token);

    return ast;
}

static AST* functionCall()
{
    Token token = prev();
    StringObject* id = copyString(token.chars, token.length);
    Service* service = getServiceByName(id->chars);
    
    if (service) {
        return systemCall(service, token);
    }

    AST* symbol = getSymbol(parser.scope, id);
    if (!symbol) {
        error(undefinedError, token);
    }

    AST* ast = createAST(AST_FUNCTION_CALL);
    ast->funcCall.scope = parser.scope;
    ast->funcCall.id = id;
    ast->funcCall.symbol = symbol;
    
    arguments(&ast->funcCall.args);
    compareFunctionSignature(ast, symbol, token);

    return ast;
}

static AST* functionDefinition()
{
    consume(T_FUNC);

    Token token = peek();
    StringObject* id = copyString(token.chars, token.length);
    AST* symbol = getLocalSymbol(parser.scope, id);

    if (symbol) {
        error(redefinitionError, token);
    }

    if (peek().type != T_IDENTIFIER) {
        lineError(identifierError, prev());
    }

    AST* ast = createAST(AST_FUNCTION_DEFINITION);
    ast->funcDef.scope = parser.scope;
    ast->funcDef.id = id;
    ast->funcDef.typeId = T_NONE;

    consume(T_IDENTIFIER);
    parser.scope = createScope(parser.scope);
    parameters(&ast->funcDef.params);

    if (isType(peek().type)) {
        ast->funcDef.typeId = peek().type;
        consumeType();
    }

    consume(T_LBRACE);
    ast->funcDef.body = statements(T_RBRACE);
    consume(T_RBRACE);
    parser.scope = parser.scope->parent;
    setLocalSymbol(parser.scope, id, ast, false);

    return ast;
}

static AST* identifier()
{
    consume(T_IDENTIFIER);

    if (isAssignment(peek().type)) {
        return assignment();
    }

    if (peek().type == T_LPAREN) {
        return functionCall();
    }

    return variable();
}

static AST* statement()
{
    Token token = peek();
    
    switch (token.type) {
        case T_UNKNOWN:
            tokenError();
        case T_VAR:
            return variableDefinition();
        case T_FUNC:
            return functionDefinition();
        case T_RETURN:
            return returnStmt();
        default:
            return expression();
    }
}

static AST* statements(TokenType type)
{
    Token token = peek();
    AST* ast = createAST(AST_COMPOUND);

    ast->compound.scope = parser.scope;

    while (token.type != type) {
        AST* stmt = statement();
        
        if (peek().line == token.line &&
            peek().type != type &&
            prev().type != T_RBRACE) {
            consume(T_SEMICOLON);
        }

        pushVector(&ast->compound.statements, stmt);
        token = peek();
    }

    return ast;
}

AST* parse(char* source)
{
    initLexer(source);
    parser.scope = createScope(NULL);
    advance();
    
    return statements(T_EOF);
}
