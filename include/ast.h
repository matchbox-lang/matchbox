#ifndef AST_H
#define AST_H

#include "builtin.h"
#include "token.h"
#include "vector.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct ASTNode ASTNode;
typedef struct StringObject StringObject;
typedef struct Scope Scope;
typedef struct Builtin Builtin;

typedef enum ReferenceType
{
    REFERENCE_SHARED,
    REFERENCE_EXCLUSIVE,
    REFERENCE_NONE
} ReferenceType;

typedef enum ASTNodeType
{
    AST_ASSIGNMENT,
    AST_BINARY,
    AST_BUILTIN_CALL,
    AST_BOOLEAN,
    AST_CHARACTER,
    AST_COMPOUND,
    AST_FLOAT,
    AST_FUNCTION_CALL,
    AST_FUNCTION_DEFINITION,
    AST_INTEGER,
    AST_PARAMETER,
    AST_PREFIX,
    AST_RETURN,
    AST_STRING,
    AST_VARIABLE,
    AST_VARIABLE_DEFINITION,
    AST_NONE
} ASTNodeType;

typedef struct ASTNode
{
    ASTNodeType type;

    union {
        struct {
            Scope* scope;
            Token operator;
            Token token;
            ASTNode* expr;
            ASTNode* symbol;
            bool initializesBinding;
        } assignment;

        struct {
            Token operator;
            ASTNode* leftExpr;
            ASTNode* rightExpr;
            TokenType typeId;
        } binary;

        struct {
            bool value;
            Token token;
        } booleanLiteral;

        struct {
            BuiltinId id;
            Vector args;
            Builtin* builtin;
        } builtinCall;

        struct {
            Token token;
        } characterLiteral;

        struct {
            Scope* scope;
            Vector statements;
        } compound;

        struct {
            double value;
            Token token;
            TokenType typeId;
        } floatLiteral;

        struct {
            Scope* scope;
            Vector args;
            StringObject* id;
            Token token;
            ASTNode* symbol;
            ASTNode* referenceOrigin;
        } functionCall;

        struct {
            Scope* scope;
            StringObject* id;
            Token token;
            Vector params;
            bool hasExplicitReturnType;
            TokenType typeId;
            ReferenceType returnReferenceType;
            ASTNode* returnReferenceOrigin;
            ASTNode* body;
        } functionDefinition;

        struct {
            uint64_t value;
            Token token;
            TokenType typeId;
        } integerLiteral;

        struct {
            Scope* scope;
            StringObject* id;
            Token token;
            TokenType typeId;
            ReferenceType referenceType;
            ASTNode* referenceOrigin;
            size_t sharedAccessCount;
            bool exclusiveAccessActive;
            bool moved;
            int position;
        } parameter;

        struct {
            Token operator;
            ASTNode* expr;
        } prefix;

        struct {
            Token token;
            ASTNode* expr;
        } returnStatement;

        struct {
            Token token;
        } stringLiteral;

        struct {
            Scope* scope;
            StringObject* id;
            Token token;
            ASTNode* symbol;
        } variable;

        struct {
            Scope* scope;
            StringObject* id;
            Token token;
            bool fixed;
            bool initialized;
            TokenType typeId;
            ReferenceType referenceType;
            ASTNode* referenceOrigin;
            size_t sharedAccessCount;
            bool exclusiveAccessActive;
            bool moved;
            bool referenceAccessActive;
            int position;
            ASTNode* expr;
        } variableDefinition;
    };
} ASTNode;

ASTNode* createASTNode(ASTNodeType type);
void freeASTNode(ASTNode* ast);
Scope* getScope(const ASTNode* ast);
TokenType getTypeId(const ASTNode* ast);
ASTNode* getReferenceOrigin(const ASTNode* ast);
ReferenceType getReferenceType(const ASTNode* ast);
bool isExpressionStatement(const ASTNode* ast);
bool isFunctionCall(const ASTNode* ast);
bool isFunctionDefinition(const ASTNode* ast);
bool isLiteral(const ASTNode* ast);
bool isParameter(const ASTNode* ast);
bool isPrefix(const ASTNode* ast);
bool isPrefixOperand(const ASTNode* ast);
bool isVariable(const ASTNode* ast);
bool isVariableDefinition(const ASTNode* ast);
bool isVariableType(const ASTNode* ast);
bool isNone(const ASTNode* ast);
bool isInitialized(const ASTNode* ast);
void initializeVariable(ASTNode* ast);

#endif
