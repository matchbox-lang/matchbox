#include "ast.h"
#include "builtin.h"
#include "scope.h"
#include "string_object.h"
#include "token.h"
#include "util.h"
#include "vector.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

ASTNode* createASTNode(ASTNodeType type)
{
    ASTNode* ast = calloc(1, sizeof(ASTNode));
    if (!ast) {
        outOfMemoryError();
    }

    ast->type = type;

    switch (type) {
        case AST_BUILTIN_CALL:
            initVector(&ast->builtinCall.args);
            break;
        case AST_COMPOUND:
            initVector(&ast->compound.statements);
            break;
        case AST_CONDITIONAL:
            initVector(&ast->conditional.referenceOrigins);
            break;
        case AST_FUNCTION_CALL:
            initVector(&ast->functionCall.args);
            initVector(&ast->functionCall.referenceOrigins);
            break;
        case AST_FUNCTION_DEFINITION:
            initVector(&ast->functionDefinition.params);
            break;
        case AST_MATCH:
            initVector(&ast->match.arms);
            initVector(&ast->match.referenceOrigins);
            break;
        case AST_PARAMETER:
            initVector(&ast->parameter.referenceOrigins);
            break;
        case AST_PREFIX:
            initVector(&ast->prefix.referenceOrigins);
            break;
        case AST_VARIABLE_DEFINITION:
            initVector(&ast->variableDefinition.referenceOrigins);
            break;
        default:
            break;
    }
    
    return ast;
}

static void freeASTNodeVector(Vector* nodes)
{
    size_t count = countVector(nodes);

    for (size_t i = 0; i < count; i++) {
        freeASTNode(nodes->data[i]);
    }

    freeVector(nodes);
}

static void freeMatchArms(Vector* arms)
{
    size_t count = countVector(arms);

    for (size_t i = 0; i < count; i++) {
        MatchArm* arm = getVectorAt(arms, i);

        freeASTNode(arm->pattern);
        freeASTNode(arm->binding);
        freeASTNode(arm->branch);
        free(arm);
    }

    freeVector(arms);
}

void freeASTNode(ASTNode* ast)
{
    if (!ast) {
        return;
    }

    switch (ast->type) {
        case AST_ASSIGNMENT:
            freeASTNode(ast->assignment.expr);
            break;
        case AST_BINARY:
            freeASTNode(ast->binary.leftExpr);
            freeASTNode(ast->binary.rightExpr);
            break;
        case AST_BUILTIN_CALL:
            freeASTNodeVector(&ast->builtinCall.args);
            break;
        case AST_COMPOUND:
            freeScope(ast->compound.scope);
            freeASTNodeVector(&ast->compound.statements);
            break;
        case AST_CONDITIONAL:
            freeASTNode(ast->conditional.condition);
            freeASTNode(ast->conditional.thenBranch);
            freeASTNode(ast->conditional.elseBranch);
            freeVector(&ast->conditional.referenceOrigins);
            break;
        case AST_FUNCTION_CALL:
            freeStringObject(ast->functionCall.id);
            freeASTNodeVector(&ast->functionCall.args);
            freeVector(&ast->functionCall.referenceOrigins);
            break;
        case AST_FUNCTION_DEFINITION:
            freeStringObject(ast->functionDefinition.id);
            freeASTNodeVector(&ast->functionDefinition.params);
            freeASTNode(ast->functionDefinition.body);
            break;
        case AST_MATCH:
            freeASTNode(ast->match.subject);
            freeMatchArms(&ast->match.arms);
            freeASTNode(ast->match.defaultBranch);
            freeVector(&ast->match.referenceOrigins);
            break;
        case AST_PARAMETER:
            freeStringObject(ast->parameter.id);
            freeVector(&ast->parameter.referenceOrigins);
            break;
        case AST_PREFIX:
            freeASTNode(ast->prefix.expr);
            freeVector(&ast->prefix.referenceOrigins);
            break;
        case AST_RETURN:
            freeASTNode(ast->returnStatement.expr);
            break;
        case AST_VARIABLE_DEFINITION:
            freeStringObject(ast->variableDefinition.id);
            freeASTNode(ast->variableDefinition.expr);
            freeVector(&ast->variableDefinition.referenceOrigins);
            break;
        case AST_VARIABLE:
            freeStringObject(ast->variable.id);
            break;
        default:
            break;
    }

    free(ast);
}

Scope* getScope(const ASTNode* ast)
{
    if (!ast) {
        return NULL;
    }

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
}

TokenType getTypeId(const ASTNode* ast)
{
    if (!ast) {
        return TOKEN_VOID;
    }

    switch (ast->type) {
        case AST_BINARY:
            return ast->binary.typeId;
        case AST_CONDITIONAL:
            return ast->conditional.typeId;
        case AST_BOOLEAN:
            return TOKEN_BOOL;
        case AST_BUILTIN_CALL:
            return ast->builtinCall.builtin->returnTypeId;
        case AST_CHARACTER:
            return TOKEN_CHAR;
        case AST_FLOAT:
            return ast->floatLiteral.typeId;
        case AST_FUNCTION_CALL:
            return getTypeId(ast->functionCall.symbol);
        case AST_FUNCTION_DEFINITION:
            return ast->functionDefinition.returnTypeId;
        case AST_PARAMETER:
            return ast->parameter.typeId;
        case AST_VARIABLE:
            return getTypeId(ast->variable.symbol);
        case AST_VARIABLE_DEFINITION:
            return ast->variableDefinition.typeId;
        case AST_PREFIX:
            return getTypeId(ast->prefix.expr);
        case AST_INTEGER:
            return ast->integerLiteral.typeId;
        case AST_MATCH:
            return ast->match.typeId;
        case AST_STRING:
            return TOKEN_STRING;
        default:
            return TOKEN_VOID;
    }
}

Vector* getReferenceOrigins(ASTNode* ast)
{
    if (!ast) {
        return NULL;
    }

    switch (ast->type) {
        case AST_CONDITIONAL:
            return &ast->conditional.referenceOrigins;
        case AST_FUNCTION_CALL:
            return &ast->functionCall.referenceOrigins;
        case AST_MATCH:
            return &ast->match.referenceOrigins;
        case AST_PARAMETER:
            return &ast->parameter.referenceOrigins;
        case AST_PREFIX:
            return &ast->prefix.referenceOrigins;
        case AST_VARIABLE:
            return getReferenceOrigins(ast->variable.symbol);
        case AST_VARIABLE_DEFINITION:
            return &ast->variableDefinition.referenceOrigins;
        default:
            return NULL;
    }
}

ASTNode* getReferenceOrigin(const ASTNode* ast)
{
    if (!ast) {
        return NULL;
    }

    Vector* origins = getReferenceOrigins((ASTNode*)ast);
    if (origins && origins->count) {
        return origins->data[0];
    }

    if (ast->type == AST_VARIABLE) {
        return getReferenceOrigin(ast->variable.symbol);
    }

    return NULL;
}

static ReferenceType getPrefixReferenceType(TokenType operatorType)
{
    if (operatorType == TOKEN_AMPERSAND) {
        return REFERENCE_SHARED;
    }

    if (operatorType == TOKEN_CIRCUMFLEX) {
        return REFERENCE_EXCLUSIVE;
    }

    return REFERENCE_NONE;
}

ReferenceType getReferenceType(const ASTNode* ast)
{
    if (!ast) {
        return REFERENCE_NONE;
    }

    switch (ast->type) {
        case AST_CONDITIONAL:
            return ast->conditional.referenceType;
        case AST_FUNCTION_CALL:
            return ast->functionCall.symbol->functionDefinition.returnReferenceType;
        case AST_FUNCTION_DEFINITION:
            return ast->functionDefinition.returnReferenceType;
        case AST_MATCH:
            return ast->match.referenceType;
        case AST_PARAMETER:
            return ast->parameter.referenceType;
        case AST_PREFIX:
            return getPrefixReferenceType(ast->prefix.operator.type);
        case AST_VARIABLE:
            return getReferenceType(ast->variable.symbol);
        case AST_VARIABLE_DEFINITION:
            return ast->variableDefinition.referenceType;
        default:
            return REFERENCE_NONE;
    }
}

bool isExpressionStatement(const ASTNode* ast)
{
    if (!ast) {
        return false;
    }

    switch (ast->type) {
        case AST_BINARY:
        case AST_BOOLEAN:
        case AST_BUILTIN_CALL:
        case AST_CHARACTER:
        case AST_CONDITIONAL:
        case AST_FLOAT:
        case AST_FUNCTION_CALL:
        case AST_INTEGER:
        case AST_MATCH:
        case AST_PREFIX:
        case AST_STRING:
        case AST_VARIABLE:
            return true;
        default:
            return false;
    }
}

bool isFunctionCall(const ASTNode* ast)
{
    return ast && ast->type == AST_FUNCTION_CALL;
}

bool isFunctionDefinition(const ASTNode* ast)
{
    return ast && ast->type == AST_FUNCTION_DEFINITION;
}

bool isParameter(const ASTNode* ast)
{
    return ast && ast->type == AST_PARAMETER;
}

bool isPrefix(const ASTNode* ast)
{
    return ast && ast->type == AST_PREFIX;
}

bool isPrefixOperand(const ASTNode* ast)
{
    if (!ast) {
        return false;
    }

    switch (ast->type) {
        case AST_ASSIGNMENT:
        case AST_BINARY:
        case AST_BOOLEAN:
        case AST_BUILTIN_CALL:
        case AST_CHARACTER:
        case AST_CONDITIONAL:
        case AST_FLOAT:
        case AST_FUNCTION_CALL:
        case AST_INTEGER:
        case AST_MATCH:
        case AST_STRING:
        case AST_VARIABLE:
            return true;
        default:
            return false;
    }
}

bool isVariable(const ASTNode* ast)
{
    return ast && ast->type == AST_VARIABLE;
}

bool isVariableDefinition(const ASTNode* ast)
{
    return ast && ast->type == AST_VARIABLE_DEFINITION;
}

bool isVariableType(const ASTNode* ast)
{
    return ast && (ast->type == AST_PARAMETER || ast->type == AST_VARIABLE_DEFINITION);
}

bool isNone(const ASTNode* ast)
{
    return ast && ast->type == AST_NONE;
}

bool isInitialized(const ASTNode* ast)
{
    if (!ast) {
        return false;
    }

    if (ast->type == AST_VARIABLE_DEFINITION) {
        return ast->variableDefinition.initialized;
    }

    return ast->type == AST_PARAMETER;
}

void initializeVariable(ASTNode* ast)
{
    if (isVariableDefinition(ast)) {
        ast->variableDefinition.initialized = true;
    }
}
