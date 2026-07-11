#ifndef SCOPE_H
#define SCOPE_H

#include "object.h"
#include "table.h"

typedef struct ASTNode ASTNode;
typedef struct Scope Scope;

typedef struct Scope
{
    Scope* parent;
    size_t localCount;
    size_t level;
    Table symbols;
} Scope;

Scope* createScope(Scope* parent);
void freeScope(Scope* scope);
size_t getLocalCount(Scope* scope);
size_t getScopeLevel(Scope* scope);
bool isTopLevelScope(Scope* scope);
ASTNode* setLocalSymbol(Scope* scope, StringObject* id, ASTNode* symbol);
ASTNode* setLocalVariableSymbol(Scope* scope, StringObject* id, ASTNode* symbol);
ASTNode* getLocalSymbol(Scope* scope, StringObject* id);
ASTNode* getSymbol(Scope* scope, StringObject* id);

#endif
