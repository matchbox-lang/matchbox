#ifndef AST_H
#define AST_H

#include "lexer.h"
#include "vector.h"
#include <stdbool.h>

typedef struct AST AST;
typedef struct Scope Scope;

typedef enum ASTType
{
    AST_ASSIGNMENT,
    AST_BINARY,
    AST_BOOLEAN,
    AST_CHARACTER,
    AST_FLOAT,
    AST_FUNCTION_CALL,
    AST_FUNCTION_DEFINITION,
    AST_INTEGER,
    AST_NONE,
    AST_PARAMETER,
    AST_POSTFIX,
    AST_PREFIX,
    AST_RETURN,
    AST_STATEMENTS,
    AST_STRING,
    AST_VARIABLE,
    AST_VARIABLE_DEFINITION
} ASTType;

typedef struct AST
{
    ASTType type;

    union {
        struct {
            Scope* scope;
            StringObject* id;
            Token operator;
            AST* expr;
        } assignment;

        struct {
            AST* leftExpr;
            Token operator;
            AST* rightExpr;
        } binary;

        struct {
            Scope* scope;
            StringObject* id;
            Vector args;
        } funcCall;

        struct {
            Scope* scope;
            StringObject* id;
            Vector params;
            int typeId;
            AST* body;
        } funcDef;

        struct {
            Token operator;
            AST* expr;
        } postfix;

        struct {
            Token operator;
            AST* expr;
        } prefix;

        struct {
            Scope* scope;
            StringObject* id;
        } var;

        struct {
            Scope* scope;
            StringObject* id;
            int typeId;
            AST* expr;
            int position;
        } varDef;

        bool boolVal;
        float floatVal;
        int intVal;
        Token character;
        Token string;
        Vector statements;
        AST* expr;
    };
} AST;

AST* createAST(TokenType type);
void freeAST(AST* ast);

#endif
