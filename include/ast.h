#ifndef AST_H
#define AST_H

#include "token.h"
#include "vector.h"
#include <stdbool.h>

typedef struct AST AST;
typedef struct StringObject StringObject;
typedef struct Scope Scope;
typedef struct Service Service;

typedef enum ASTType
{
    AST_ASSIGNMENT,
    AST_BINARY,
    AST_BOOLEAN,
    AST_CHARACTER,
    AST_COMPOUND,
    AST_FLOAT,
    AST_FUNCTION_CALL,
    AST_FUNCTION_DEFINITION,
    AST_INTEGER,
    AST_PARAMETER,
    AST_POSTFIX,
    AST_PREFIX,
    AST_RETURN,
    AST_STRING,
    AST_SYSCALL,
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
            AST* expr;
            AST* symbol;
        } assignment;

        struct {
            Token operator;
            AST* leftExpr;
            AST* rightExpr;
            int typeId;
        } binary;

        struct {
            Scope* scope;
            Vector statements;
        } compound;

        struct {
            Scope* scope;
            Vector args;
            AST* symbol;
        } funcCall;

        struct {
            Scope* scope;
            StringObject* id;
            Vector params;
            int typeId;
            AST* body;
        } funcDef;

        struct {
            Scope* scope;
            StringObject* id;
            int typeId;
            int position;
        } param;

        struct {
            Token operator;
            AST* expr;
        } postfix;

        struct {
            Token operator;
            AST* expr;
        } prefix;

        struct {
            int opcode;
            Vector args;
            Service* service;
        } syscall;

        struct {
            Scope* scope;
            AST* symbol;
        } var;

        struct {
            Scope* scope;
            StringObject* id;
            bool initialized;
            int typeId;
            int position;
            AST* expr;
        } varDef;

        bool boolVal;
        float floatVal;
        int intVal;
        Token character;
        Token string;
        AST* expr;
    };
} AST;

AST* createAST(TokenType type);
void freeAST(AST* ast);
Scope* getScope(AST* ast);
int getTypeId(AST* ast);
bool isParameter(AST* ast);
bool isVariable(AST* ast);
bool isVariableDefinition(AST* ast);
bool isVariableType(AST* ast);
bool isNone(AST* ast);
bool isInitialized(AST* ast);
void initialize(AST* ast);

#endif
