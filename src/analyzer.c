#include "analyzer.h"
#include "builtin.h"
#include "scope.h"
#include "string_object.h"
#include "token.h"
#include "vector.h"
#include <stdio.h>
#include <stdlib.h>

static void analyzeNode(Analyzer* analyzer, AST* ast);

static void semanticError(char* message, Token token)
{
    fprintf(stderr, "Error: %s", message);
    printTokenValue(token);
    fprintf(stderr, " on line %d:%d\n", token.line, token.column);
    exit(1);
}

static void symbolError(char* message, Token token)
{
    fprintf(stderr, "Error: ");
    printTokenValue(token);
    fprintf(stderr, " is %s on line %d:%d\n", message, token.line, token.column);
    exit(1);
}

static AST* findSymbol(Analyzer* analyzer, StringObject* id)
{
    AST* symbol = getLocalSymbol(analyzer->currentScope, id);
    if (symbol) {
        return symbol;
    }

    return getLocalSymbol(analyzer->topLevel->compound.scope, id);
}

static void analyzeNodes(Analyzer* analyzer, Vector* nodes, size_t start)
{
    size_t count = countVector(nodes);

    for (size_t i = start; i < count; i++) {
        analyzeNode(analyzer, getVectorAt(nodes, i));
    }
}

static void analyzeExpressionNodes(Analyzer* analyzer, Vector* nodes)
{
    analyzeNodes(analyzer, nodes, 0);
}

static void analyzeBinary(Analyzer* analyzer, AST* ast)
{
    analyzeNode(analyzer, ast->binary.leftExpr);
    analyzeNode(analyzer, ast->binary.rightExpr);

    int leftType = getTypeId(ast->binary.leftExpr);
    int rightType = getTypeId(ast->binary.rightExpr);

    if (leftType != rightType) {
        semanticError("Invalid operands to binary ", ast->binary.operator);
    }

    ast->binary.typeId = leftType;

    if (isBoolOperatorToken(ast->binary.operator.type)) {
        ast->binary.typeId = TOKEN_BOOL;
    }
}

static void analyzeVariable(Analyzer* analyzer, AST* ast)
{
    AST* symbol = findSymbol(analyzer, ast->variable.id);

    if (!symbol || !isVariableType(symbol)) {
        symbolError("undefined", ast->variable.token);
    }

    if (!isInitialized(symbol)) {
        symbolError("uninitialized", ast->variable.token);
    }

    ast->variable.scope = analyzer->currentScope;
    ast->variable.symbol = symbol;
}

static void analyzeParameter(Analyzer* analyzer, AST* ast)
{
    if (getLocalSymbol(analyzer->currentScope, ast->parameter.id)) {
        semanticError("Redefinition of ", ast->parameter.token);
    }

    ast->parameter.scope = analyzer->currentScope;
    setLocalSymbol(analyzer->currentScope, ast->parameter.id, ast);
}

static void validateBuiltinCall(AST* ast, Builtin* builtin, Token token)
{
    size_t count = countVector(&ast->functionCall.args);

    if (count != (size_t)builtin->paramCount) {
        semanticError("Invalid arguments to function ", token);
    }

    for (size_t i = 0; i < count; i++) {
        AST* arg = getVectorAt(&ast->functionCall.args, i);

        if (getTypeId(arg) != builtin->params[i]) {
            semanticError("Invalid arguments to function ", token);
        }
    }
}

static void convertBuiltinCall(AST* ast, Builtin* builtin)
{
    Vector args = ast->functionCall.args;
    
    freeStringObject(ast->functionCall.id);

    ast->type = AST_BUILTIN_CALL;
    ast->builtinCall.id = builtin->id;
    ast->builtinCall.args = args;
    ast->builtinCall.builtin = builtin;
}

static void analyzeBuiltinCall(AST* ast)
{
    Builtin* builtin = getBuiltinByName(ast->functionCall.id->chars);
    if (!builtin) {
        symbolError("undefined", ast->functionCall.token);
    }

    validateBuiltinCall(ast, builtin, ast->functionCall.token);
    convertBuiltinCall(ast, builtin);
}

static void validateFunctionCall(AST* caller, AST* callee, Token token)
{
    size_t count = countVector(&caller->functionCall.args);
    size_t paramCount = countVector(&callee->functionDefinition.params);

    if (count != paramCount) {
        semanticError("Invalid arguments to function ", token);
    }

    for (size_t i = 0; i < count; i++) {
        AST* arg = getVectorAt(&caller->functionCall.args, i);
        AST* param = getVectorAt(&callee->functionDefinition.params, i);

        if (getTypeId(arg) != param->parameter.typeId) {
            semanticError("Invalid arguments to function ", token);
        }
    }
}

static void analyzeFunctionCall(Analyzer* analyzer, AST* ast)
{
    analyzeExpressionNodes(analyzer, &ast->functionCall.args);

    AST* symbol = findSymbol(analyzer, ast->functionCall.id);
    if (!symbol) {
        analyzeBuiltinCall(ast);
        return;
    }

    if (!isFunctionDefinition(symbol)) {
        symbolError("undefined", ast->functionCall.token);
    }

    ast->functionCall.scope = analyzer->currentScope;
    ast->functionCall.symbol = symbol;
    validateFunctionCall(ast, symbol, ast->functionCall.token);
}

static bool hasValueReturn(Vector* statements)
{
    size_t count = countVector(statements);

    for (size_t i = 0; i < count; i++) {
        AST* statement = getVectorAt(statements, i);

        if (statement->type == AST_RETURN) {
            return true;
        }
    }

    return false;
}

static void analyzeFunction(Analyzer* analyzer, AST* ast)
{
    if (getLocalSymbol(analyzer->currentScope, ast->functionDefinition.id)) {
        semanticError("Redefinition of ", ast->functionDefinition.token);
    }

    Scope* parent = analyzer->currentScope;
    Scope* scope = createScope(parent);
    ast->functionDefinition.scope = scope;
    ast->functionDefinition.body->compound.scope = scope;
    analyzer->currentScope = scope;
    analyzeExpressionNodes(analyzer, &ast->functionDefinition.params);

    size_t count = countVector(&ast->functionDefinition.params);

    for (size_t i = 0; i < count; i++) {
        AST* param = getVectorAt(&ast->functionDefinition.params, i);
        param->parameter.position = count - i - 1;
    }

    analyzeExpressionNodes(analyzer, &ast->functionDefinition.body->compound.statements);
    analyzer->currentScope = parent;

    if (!ast->functionDefinition.hasExplicitReturnType &&
        !hasValueReturn(&ast->functionDefinition.body->compound.statements)) {
        ast->functionDefinition.typeId = TOKEN_NONE;
    }

    setLocalSymbol(parent, ast->functionDefinition.id, ast);
}

static void analyzeAssignment(Analyzer* analyzer, AST* ast)
{
    StringObject* id = copyStringObject(ast->assignment.token.chars, ast->assignment.token.length);
    AST* symbol = findSymbol(analyzer, id);

    freeStringObject(id);

    if (!symbol) {
        symbolError("undefined", ast->assignment.token);
    }

    if (analyzer->currentScope != getScope(symbol) && !isInitialized(symbol)) {
        symbolError("uninitialized", ast->assignment.token);
    }

    analyzeNode(analyzer, ast->assignment.expr);
    ast->assignment.scope = analyzer->currentScope;
    ast->assignment.symbol = symbol;
    initialize(symbol);
}

static void analyzeVariableDefinition(Analyzer* analyzer, AST* ast)
{
    if (getLocalSymbol(analyzer->currentScope, ast->variableDefinition.id)) {
        semanticError("Redefinition of ", ast->variableDefinition.token);
    }

    ast->variableDefinition.scope = analyzer->currentScope;
    ast->variableDefinition.position = getLocalCount(analyzer->currentScope);
    ast->variableDefinition.initialized = false;

    if (!isNone(ast->variableDefinition.expr)) {
        analyzeNode(analyzer, ast->variableDefinition.expr);
        ast->variableDefinition.typeId = getTypeId(ast->variableDefinition.expr);
        initialize(ast);
    }

    if (ast->variableDefinition.typeId == TOKEN_NONE) {
        semanticError("Invalid type for variable ", ast->variableDefinition.token);
    }

    setLocalVariableSymbol(analyzer->currentScope, ast->variableDefinition.id, ast);
}

static void analyzeNode(Analyzer* analyzer, AST* ast)
{
    switch (ast->type) {
        case AST_ASSIGNMENT:
            analyzeAssignment(analyzer, ast); break;
        case AST_BINARY:
            analyzeBinary(analyzer, ast); break;
        case AST_BUILTIN_CALL:
            analyzeExpressionNodes(analyzer, &ast->builtinCall.args); break;
        case AST_FUNCTION_CALL:
            analyzeFunctionCall(analyzer, ast); break;
        case AST_FUNCTION_DEFINITION:
            analyzeFunction(analyzer, ast); break;
        case AST_PARAMETER:
            analyzeParameter(analyzer, ast); break;
        case AST_PREFIX:
            analyzeNode(analyzer, ast->prefix.expr); break;
        case AST_RETURN:
            if (analyzer->currentScope->level < 2) {
                semanticError("Expected return inside a function but found ", ast->returnStatement.token);
            }
            analyzeNode(analyzer, ast->returnStatement.expr);
            break;
        case AST_VARIABLE_DEFINITION:
            analyzeVariableDefinition(analyzer, ast); break;
        case AST_VARIABLE:
            analyzeVariable(analyzer, ast); break;
        default: break;
    }
}

void initAnalyzer(Analyzer* analyzer, AST* ast)
{
    analyzer->topLevel = ast;
    analyzer->currentScope = ast->compound.scope;
}

bool analyze(Analyzer* analyzer, size_t start)
{
    analyzer->currentScope = analyzer->topLevel->compound.scope;
    analyzeNodes(analyzer, &analyzer->topLevel->compound.statements, start);

    return true;
}
