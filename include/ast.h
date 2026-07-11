#ifndef AST_H
#define AST_H

#include "builtin.h"
#include "token.h"
#include "vector.h"
#include <stdbool.h>

typedef struct AST AST;
typedef struct StringObject StringObject;
typedef struct Scope Scope;
typedef struct Builtin Builtin;

typedef enum ASTType
{
    AST_ASSIGNMENT,
    AST_BINARY,
    AST_BOOLEAN,
    AST_CHARACTER,
    AST_COMPOUND,
    AST_FLOAT,
    AST_BUILTIN_CALL,
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
} ASTType;

typedef struct AST
{
    ASTType type;

    union {
        struct {
            Scope* scope;
            Token operator;
            Token token;
            AST* expr;
            AST* symbol;
        } assignment;

        struct {
            Token operator;
            AST* leftExpr;
            AST* rightExpr;
            TokenType typeId;
        } binary;

        struct {
            bool value;
            Token token;
        } booleanLiteral;

        struct {
            Token token;
        } characterLiteral;

        struct {
            Scope* scope;
            Vector statements;
        } compound;

        struct {
            float value;
            Token token;
        } floatLiteral;

        struct {
            BuiltinId id;
            Vector args;
            Builtin* builtin;
        } builtinCall;

        struct {
            Scope* scope;
            Vector args;
            StringObject* id;
            Token token;
            AST* symbol;
        } functionCall;

        struct {
            Scope* scope;
            StringObject* id;
            Token token;
            Vector params;
            bool hasExplicitReturnType;
            TokenType typeId;
            AST* body;
        } functionDefinition;

        struct {
            int value;
            Token token;
        } integerLiteral;

        struct {
            Scope* scope;
            StringObject* id;
            Token token;
            TokenType typeId;
            int position;
        } parameter;

        struct {
            Token operator;
            AST* expr;
        } prefix;

        struct {
            Token token;
            AST* expr;
        } returnStatement;

        struct {
            Token token;
        } stringLiteral;

        struct {
            Scope* scope;
            StringObject* id;
            Token token;
            AST* symbol;
        } variable;

        struct {
            Scope* scope;
            StringObject* id;
            Token token;
            bool initialized;
            TokenType typeId;
            int position;
            AST* expr;
        } variableDefinition;
    };
} AST;

AST* createAST(ASTType type);
void freeAST(AST* ast);
Scope* getScope(AST* ast);
TokenType getTypeId(AST* ast);
bool isExpressionStatement(AST* ast);
bool isFunctionCall(AST* ast);
bool isFunctionDefinition(AST* ast);
bool isParameter(AST* ast);
bool isPrefix(AST* ast);
bool isPrefixOperand(AST* ast);
bool isVariable(AST* ast);
bool isVariableDefinition(AST* ast);
bool isVariableType(AST* ast);
bool isNone(AST* ast);
bool isInitialized(AST* ast);
void initializeVariable(AST* ast);

#endif
