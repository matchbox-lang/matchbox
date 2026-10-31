#include "compiler/ast.h"
#include "builtin/builtin.h"
#include "common/memory.h"
#include "common/util.h"
#include "common/vector.h"
#include "compiler/scope.h"
#include "compiler/token.h"
#include "runtime/string_object.h"
#include <stdbool.h>
#include <stdlib.h>

ASTNode* createASTNode(ASTNodeType type)
{
    ASTNode* ast = allocateZeroedMemory(1, sizeof(ASTNode));

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

ASTNode* createAssignmentNode(Token operator, Token token, ASTNode* expr)
{
    ASTNode* ast = createASTNode(AST_ASSIGNMENT);
    ast->assignment.scope = NULL;
    ast->assignment.operator = operator;
    ast->assignment.token = token;
    ast->assignment.symbol = NULL;
    ast->assignment.expr = expr;
    ast->assignment.initializesBinding = false;

    return ast;
}

ASTNode* createBinaryNode(ASTNode* leftExpr, ASTNode* rightExpr, Token token)
{
    if (!rightExpr) {
        return NULL;
    }

    ASTNode* ast = createASTNode(AST_BINARY);
    ast->binary.leftExpr = leftExpr;
    ast->binary.operator = token;
    ast->binary.rightExpr = rightExpr;
    ast->binary.typeId = TYPE_UNKNOWN;

    return ast;
}

ASTNode* createBooleanNode(Token token, bool value)
{
    ASTNode* ast = createASTNode(AST_BOOLEAN);
    ast->booleanLiteral.value = value;
    ast->booleanLiteral.token = token;

    return ast;
}

ASTNode* createBuiltinCallNode(Token token, const Builtin* builtin)
{
    ASTNode* ast = createASTNode(AST_BUILTIN_CALL);
    ast->builtinCall.builtin = builtin;
    ast->builtinCall.token = token;

    return ast;
}

ASTNode* createCharacterNode(Token token, uint32_t value)
{
    ASTNode* ast = createASTNode(AST_CHARACTER);
    ast->characterLiteral.value = value;
    ast->characterLiteral.token = token;

    return ast;
}

ASTNode* createCompoundNode(Scope* scope)
{
    ASTNode* ast = createASTNode(AST_COMPOUND);
    ast->compound.scope = scope;

    return ast;
}

ASTNode* createConditionalNode(Token token, ASTNode* condition, ASTNode* thenBranch, ASTNode* elseBranch)
{
    ASTNode* ast = createASTNode(AST_CONDITIONAL);
    ast->conditional.token = token;
    ast->conditional.condition = condition;
    ast->conditional.thenBranch = thenBranch;
    ast->conditional.elseBranch = elseBranch;
    ast->conditional.typeId = TYPE_VOID;
    ast->conditional.referenceType = REFERENCE_NONE;
    ast->conditional.expression = true;

    return ast;
}

ASTNode* createFloatNode(Token token, double value, DataType typeId)
{
    ASTNode* ast = createASTNode(AST_FLOAT);
    ast->floatLiteral.value = value;
    ast->floatLiteral.token = token;
    ast->floatLiteral.typeId = typeId;

    return ast;
}

ASTNode* createFunctionCallNode(Token token)
{
    ASTNode* ast = createASTNode(AST_FUNCTION_CALL);
    ast->functionCall.scope = NULL;
    ast->functionCall.id = copyStringObject(token.chars, token.length);
    ast->functionCall.token = token;
    ast->functionCall.symbol = NULL;

    return ast;
}

ASTNode* createFunctionDefinitionNode(Token token, StringObject* id)
{
    ASTNode* ast = createASTNode(AST_FUNCTION_DEFINITION);
    ast->functionDefinition.scope = NULL;
    ast->functionDefinition.id = id;
    ast->functionDefinition.token = token;
    ast->functionDefinition.returnTypeId = TYPE_I32;
    ast->functionDefinition.returnReferenceType = REFERENCE_NONE;
    ast->functionDefinition.returnReferenceOrigin = NULL;
    ast->functionDefinition.hasExplicitReturnType = false;
    ast->functionDefinition.body = NULL;

    return ast;
}

ASTNode* createIntegerNode(Token token, uint64_t value, DataType typeId)
{
    ASTNode* ast = createASTNode(AST_INTEGER);
    ast->integerLiteral.value = value;
    ast->integerLiteral.token = token;
    ast->integerLiteral.typeId = typeId;

    return ast;
}

ASTNode* createMatchNode(Token token)
{
    ASTNode* ast = createASTNode(AST_MATCH);
    ast->match.token = token;
    ast->match.typeId = TYPE_VOID;
    ast->match.referenceType = REFERENCE_NONE;
    ast->match.expression = true;

    return ast;
}

ASTNode* createParameterNode(Token token, StringObject* id)
{
    ASTNode* ast = createASTNode(AST_PARAMETER);
    ast->parameter.scope = NULL;
    ast->parameter.id = id;
    ast->parameter.token = token;
    ast->parameter.typeId = TYPE_I32;
    ast->parameter.referenceType = REFERENCE_NONE;

    return ast;
}

ASTNode* createPrefixNode(Token token, ASTNode* expr)
{
    ASTNode* ast = createASTNode(AST_PREFIX);
    ast->prefix.operator = token;
    ast->prefix.expr = expr;

    return ast;
}

ASTNode* createReturnNode(Token token, ASTNode* expr)
{
    ASTNode* ast = createASTNode(AST_RETURN);
    ast->returnStatement.token = token;
    ast->returnStatement.expr = expr;

    return ast;
}

ASTNode* createStringNode(Token token, StringObject* value)
{
    ASTNode* ast = createASTNode(AST_STRING);
    ast->stringLiteral.value = value;
    ast->stringLiteral.token = token;

    return ast;
}

ASTNode* createVariableDefinitionNode(Token token, StringObject* id, bool fixed)
{
    ASTNode* ast = createASTNode(AST_VARIABLE_DEFINITION);
    ast->variableDefinition.scope = NULL;
    ast->variableDefinition.id = id;
    ast->variableDefinition.token = token;
    ast->variableDefinition.fixed = fixed;
    ast->variableDefinition.typeId = TYPE_UNKNOWN;
    ast->variableDefinition.referenceType = REFERENCE_NONE;
    ast->variableDefinition.position = 0;
    ast->variableDefinition.expr = NULL;

    return ast;
}

ASTNode* createVariableNode(Token token)
{
    ASTNode* ast = createASTNode(AST_VARIABLE);
    ast->variable.scope = NULL;
    ast->variable.id = copyStringObject(token.chars, token.length);
    ast->variable.token = token;
    ast->variable.symbol = NULL;

    return ast;
}

ASTNode* createWhileNode(Token token, ASTNode* condition, ASTNode* body)
{
    ASTNode* ast = createASTNode(AST_WHILE);
    ast->whileStatement.token = token;
    ast->whileStatement.condition = condition;
    ast->whileStatement.body = body;

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
        freeASTNode(arm->guard);
        freeASTNode(arm->branch);
        freeMemory(arm);
    }

    freeVector(arms);
}

void freeASTNode(ASTNode* ast)
{
    if (!ast) {
        return;
    }

    switch (ast->type) {
        case AST_CONVERSION:
            freeASTNode(ast->conversion.expr);
            break;
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
        case AST_WHILE:
            freeASTNode(ast->whileStatement.condition);
            freeASTNode(ast->whileStatement.body);
            break;
        case AST_RETURN:
            freeASTNode(ast->returnStatement.expr);
            break;
        case AST_STRING:
            freeStringObject(ast->stringLiteral.value);
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

    freeMemory(ast);
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

DataType getNodeDataType(const ASTNode* ast)
{
    if (!ast) {
        return TYPE_VOID;
    }

    switch (ast->type) {
        case AST_CONVERSION:
            return ast->conversion.typeId;
        case AST_BINARY:
            return ast->binary.typeId;
        case AST_CONDITIONAL:
            return ast->conditional.typeId;
        case AST_BOOLEAN:
            return TYPE_BOOL;
        case AST_BUILTIN_CALL:
            return ast->builtinCall.builtin->returnTypeId;
        case AST_CHARACTER:
            return TYPE_CHAR;
        case AST_FLOAT:
            return ast->floatLiteral.typeId;
        case AST_FUNCTION_CALL:
            return getNodeDataType(ast->functionCall.symbol);
        case AST_FUNCTION_DEFINITION:
            return ast->functionDefinition.returnTypeId;
        case AST_PARAMETER:
            return ast->parameter.typeId;
        case AST_VARIABLE:
            return getNodeDataType(ast->variable.symbol);
        case AST_VARIABLE_DEFINITION:
            return ast->variableDefinition.typeId;
        case AST_PREFIX:
            return getNodeDataType(ast->prefix.expr);
        case AST_INTEGER:
            return ast->integerLiteral.typeId;
        case AST_MATCH:
            return ast->match.typeId;
        case AST_STRING:
            return TYPE_STRING;
        default:
            return TYPE_VOID;
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

static ReferenceType getPrefixReferenceType(TokenType type)
{
    if (type == TOKEN_AMPERSAND) {
        return REFERENCE_SHARED;
    }

    if (type == TOKEN_CIRCUMFLEX) {
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
        case AST_CONVERSION:
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

bool isTextLiteral(const ASTNode* ast)
{
    return ast && (ast->type == AST_STRING || ast->type == AST_CHARACTER);
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
        case AST_CONVERSION:
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
