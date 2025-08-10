#include "ast.h"
#include "scope.h"
#include "service.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

AST* createAST(ASTType type)
{
    AST* ast = malloc(sizeof(AST));
    ast->type = type;

    switch (type) {
        case AST_COMPOUND:
            initVector(&ast->compound.statements);
            break;
        case AST_FUNCTION_CALL:
            initVector(&ast->functionCall.args);
            break;
        case AST_FUNCTION_DEFINITION:
            initVector(&ast->functionDefinition.params);
            break;
        case AST_SERVICE_REQUEST:
            initVector(&ast->serviceRequest.args);
            break;
        default:
            return ast;
    }
    
    return ast;
}

static void freeASTVector(Vector* nodes)
{
    size_t count = countVector(nodes);

    for (size_t i = 0; i < count; i++) {
        freeAST(nodes->data[i]);
    }

    freeVector(nodes);
}

void freeAST(AST* ast)
{
    if (!ast) {
        return;
    }

    switch (ast->type) {
        case AST_ASSIGNMENT:
            freeAST(ast->assignment.expr);
            break;
        case AST_BINARY:
            freeAST(ast->binary.leftExpr);
            freeAST(ast->binary.rightExpr);
            break;
        case AST_COMPOUND:
            freeScope(ast->compound.scope);
            freeASTVector(&ast->compound.statements);
            break;
        case AST_FUNCTION_CALL:
            freeASTVector(&ast->functionCall.args);
            break;
        case AST_FUNCTION_DEFINITION:
            freeStringObject(ast->functionDefinition.id);
            freeASTVector(&ast->functionDefinition.params);
            freeAST(ast->functionDefinition.body);
            break;
        case AST_PARAMETER:
            freeStringObject(ast->parameter.id);
            break;
        case AST_PREFIX:
            freeAST(ast->prefix.expr);
            break;
        case AST_RETURN:
            freeAST(ast->expression);
            break;
        case AST_VARIABLE_DEFINITION:
            freeStringObject(ast->variableDefinition.id);
            freeAST(ast->variableDefinition.expr);
            break;
        default:
            return;
    }

    free(ast);
}

Scope* getScope(AST* ast)
{
    switch (ast->type) {
        case AST_ASSIGNMENT:
            return ast->assignment.scope;
        case AST_COMPOUND:
            return ast->compound.scope;
        case AST_FUNCTION_CALL:
            return ast->functionCall.scope;
        case AST_FUNCTION_DEFINITION:
            return ast->functionDefinition.scope;
        case AST_PARAMETER:
            return ast->parameter.scope;
        case AST_VARIABLE:
            return ast->variable.scope;
        case AST_VARIABLE_DEFINITION:
            return ast->variableDefinition.scope;
        default:
            return NULL;
    }

    return NULL;
}

int getTypeId(AST* ast)
{
    if (!ast) {
        return T_NONE;
    }

    switch (ast->type) {
        case AST_BINARY:
            return ast->binary.typeId;
        case AST_FUNCTION_CALL:
            return getTypeId(ast->functionCall.symbol);
        case AST_FUNCTION_DEFINITION:
            return ast->functionDefinition.typeId;
        case AST_PARAMETER:
            return ast->parameter.typeId;
        case AST_SERVICE_REQUEST:
            return ast->serviceRequest.service->typeId;
        case AST_VARIABLE:
            return getTypeId(ast->variable.symbol);
        case AST_VARIABLE_DEFINITION:
            return ast->variableDefinition.typeId;
        case AST_PREFIX:
            return getTypeId(ast->prefix.expr);
        case AST_INTEGER:
            return T_INT;
        default:
            return T_NONE;
    }
}

bool isFunctionCall(AST* ast)
{
    return ast->type == AST_FUNCTION_CALL;
}

bool isFunctionDefinition(AST* ast)
{
    return ast->type == AST_FUNCTION_DEFINITION;
}

bool isParameter(AST* ast)
{
    return ast->type == AST_PARAMETER;
}

bool isPrefix(AST* ast)
{
    return ast->type == AST_PREFIX;
}

bool isPrefixOperand(AST* ast)
{
    switch (ast->type) {
        case AST_ASSIGNMENT:
        case AST_BINARY:
        case AST_FUNCTION_CALL:
        case AST_INTEGER:
        case AST_SERVICE_REQUEST:
        case AST_VARIABLE:
            return true;
        default:
            return false;
    }
}

bool isVariable(AST* ast)
{
    return ast->type == AST_VARIABLE;
}

bool isVariableDefinition(AST* ast)
{
    return ast->type == AST_VARIABLE_DEFINITION;
}

bool isVariableType(AST* ast)
{
    return ast->type == AST_PARAMETER || ast->type == AST_VARIABLE_DEFINITION;
}

bool isNone(AST* ast)
{
    return ast->type == AST_NONE;
}

bool isInitialized(AST* ast)
{
    if (ast->type == AST_VARIABLE_DEFINITION) {
        return ast->variableDefinition.initialized;
    }

    return ast->type == AST_PARAMETER;
}

void initialize(AST* ast)
{
    if (isVariableDefinition(ast)) {
        ast->variableDefinition.initialized = true;
    }
}
