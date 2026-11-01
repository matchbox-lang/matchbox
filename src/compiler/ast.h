#ifndef AST_H
#define AST_H

#include "common/data_type.h"
#include "common/vector.h"
#include "compiler/token.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct ASTNode ASTNode;
typedef struct StringObject StringObject;
typedef struct Scope Scope;
typedef struct Builtin Builtin;

typedef struct MatchArm
{
    ASTNode* pattern;
    ASTNode* binding;
    ASTNode* guard;
    Token guardToken;
    ASTNode* branch;
} MatchArm;

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
    AST_CONDITIONAL,
    AST_CONVERSION,
    AST_FLOAT,
    AST_FUNCTION_CALL,
    AST_FUNCTION_DEFINITION,
    AST_INTEGER,
    AST_MATCH,
    AST_PARAMETER,
    AST_PREFIX,
    AST_RETURN,
    AST_STRING,
    AST_VARIABLE,
    AST_VARIABLE_DEFINITION,
    AST_WHILE,
    AST_NONE
} ASTNodeType;

typedef struct ASTNode
{
    ASTNodeType type;

    union {
        struct {
            ASTNode* expr;
            Token token;
            DataType typeId;
        } conversion;

        struct {
            Token token;
            ASTNode* condition;
            ASTNode* body;
        } whileStatement;

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
            DataType typeId;
            DataType operandType;
        } binary;

        struct {
            bool value;
            Token token;
        } booleanLiteral;

        struct {
            Vector args;
            const Builtin* builtin;
            Token token;
        } builtinCall;

        struct {
            uint32_t value;
            Token token;
        } characterLiteral;

        struct {
            Scope* scope;
            Vector statements;
        } compound;

        struct {
            Token token;
            ASTNode* condition;
            ASTNode* thenBranch;
            ASTNode* elseBranch;
            DataType typeId;
            ReferenceType referenceType;
            Vector referenceOrigins;
            bool expression;
        } conditional;

        struct {
            double value;
            Token token;
            DataType typeId;
        } floatLiteral;

        struct {
            Scope* scope;
            Vector args;
            StringObject* id;
            Token token;
            ASTNode* symbol;
            Vector referenceOrigins;
        } functionCall;

        struct {
            Scope* scope;
            StringObject* id;
            Token token;
            Vector params;
            bool hasExplicitReturnType;
            DataType returnTypeId;
            ReferenceType returnReferenceType;
            ASTNode* returnReferenceOrigin;
            ASTNode* body;
            size_t callCount;
        } functionDefinition;

        struct {
            uint64_t value;
            Token token;
            DataType typeId;
        } integerLiteral;

        struct {
            Token token;
            ASTNode* subject;
            Vector arms;
            ASTNode* defaultBranch;
            DataType typeId;
            ReferenceType referenceType;
            Vector referenceOrigins;
            bool expression;
        } match;

        struct {
            Scope* scope;
            StringObject* id;
            Token token;
            DataType typeId;
            ReferenceType referenceType;
            Vector referenceOrigins;
            size_t sharedAccessCount;
            bool exclusiveAccessActive;
            bool moved;
            int position;
        } parameter;

        struct {
            Token operator;
            ASTNode* expr;
            Vector referenceOrigins;
            size_t constantPosition;
            bool evaluated;
        } prefix;

        struct {
            Token token;
            ASTNode* expr;
        } returnStatement;

        struct {
            StringObject* value;
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
            bool constant;
            bool initialized;
            DataType typeId;
            ReferenceType referenceType;
            Vector referenceOrigins;
            size_t sharedAccessCount;
            bool exclusiveAccessActive;
            bool moved;
            bool referenceAccessActive;
            size_t readCount;
            size_t discardedReadCount;
            bool discardedReadsAnalyzed;
            bool addressTaken;
            int registerPosition;
            int position;
            ASTNode* expr;
        } variableDefinition;
    };
} ASTNode;

ASTNode* createASTNode(ASTNodeType type);
ASTNode* createAssignmentNode(Token operator, Token token, ASTNode* expr);
ASTNode* createBinaryNode(ASTNode* leftExpr, ASTNode* rightExpr, Token token);
ASTNode* createBooleanNode(Token token, bool value);
ASTNode* createBuiltinCallNode(Token token, const Builtin* builtin);
ASTNode* createCharacterNode(Token token, uint32_t value);
ASTNode* createCompoundNode(Scope* scope);
ASTNode* createConditionalNode(Token token, ASTNode* condition, ASTNode* thenBranch, ASTNode* elseBranch);
ASTNode* createFloatNode(Token token, double value, DataType typeId);
ASTNode* createFunctionCallNode(Token token);
ASTNode* createFunctionDefinitionNode(Token token, StringObject* id);
ASTNode* createIntegerNode(Token token, uint64_t value, DataType typeId);
ASTNode* createMatchNode(Token token);
ASTNode* createParameterNode(Token token, StringObject* id);
ASTNode* createPrefixNode(Token token, ASTNode* expr);
ASTNode* createReturnNode(Token token, ASTNode* expr);
ASTNode* createStringNode(Token token, StringObject* value);
ASTNode* createVariableDefinitionNode(Token token, StringObject* id, bool fixed);
ASTNode* createVariableNode(Token token);
ASTNode* createWhileNode(Token token, ASTNode* condition, ASTNode* body);
void freeASTNode(ASTNode* ast);
Scope* getScope(const ASTNode* ast);
DataType getNodeDataType(const ASTNode* ast);
ASTNode* getReferenceOrigin(const ASTNode* ast);
Vector* getReferenceOrigins(ASTNode* ast);
ReferenceType getReferenceType(const ASTNode* ast);
bool isExpressionStatement(const ASTNode* ast);
bool isLiteral(const ASTNode* ast);
bool isTextLiteral(const ASTNode* ast);
bool isFunctionCall(const ASTNode* ast);
bool isFunctionDefinition(const ASTNode* ast);
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
