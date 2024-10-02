#include "ast.h"
#include "object.h"
#include "scope.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

AST* createAST(TokenType type)
{
    AST* ast = malloc(sizeof(AST));
    ast->type = type;

    switch (type)
    {
        case AST_FUNCTION_CALL:
            initVector(&ast->funcCall.args);
            break;
        case AST_FUNCTION_DEFINITION:
            initVector(&ast->funcDef.params);
            break;
        case AST_STATEMENTS:
            initVector(&ast->statements);
            break;
    }
    
    return ast;
}

static void freeASTVector(Vector* statements)
{
    for (size_t i = 0; i < countVector(statements); i++) {
        AST* ast = vectorGet(statements, i);
        freeAST(ast);
    }

    freeVector(statements);
}

void freeAST(AST* ast)
{
    if (!ast) {
        return;
    }

    switch (ast->type) {
        case AST_ASSIGNMENT:
            freeString(ast->assignment.id);
            freeAST(ast->assignment.expr);
            break;
        case AST_BINARY:
            freeAST(ast->binary.leftExpr);
            freeAST(ast->binary.rightExpr);
            break;
        case AST_FUNCTION_CALL:
            freeScope(ast->funcCall.scope);
            freeString(ast->funcCall.id);
            freeASTVector(&ast->funcCall.args);
            break;
        case AST_FUNCTION_DEFINITION:
            freeScope(ast->funcDef.scope);
            freeString(ast->funcDef.id);
            freeASTVector(&ast->funcDef.params);
            freeAST(ast->funcDef.body);
            break;
        case AST_POSTFIX:
            freeAST(ast->postfix.expr);
            break;
        case AST_PREFIX:
            freeAST(ast->prefix.expr);
            break;
        case AST_RETURN:
            freeAST(ast->expr);
            break;
        case AST_STATEMENTS:
            freeASTVector(&ast->statements);
            break;
        case AST_VARIABLE:
            freeScope(ast->var.scope);
            freeString(ast->var.id);
            break;
        case AST_VARIABLE_DEFINITION:
        case AST_PARAMETER:
            freeScope(ast->varDef.scope);
            freeString(ast->varDef.id);
            freeAST(ast->varDef.expr);
            break;
    }

    free(ast);
}
