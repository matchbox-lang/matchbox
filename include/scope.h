#ifndef SCOPE_H
#define SCOPE_H

#include "object.h"
#include "table.h"

typedef struct AST AST;
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
size_t getLevel(Scope* scope);
bool isTopLevel(Scope* scope);
AST* setLocalSymbol(Scope* scope, StringObject* id, AST* symbol);
AST* setLocalVariableSymbol(Scope* scope, StringObject* id, AST* symbol);
AST* getLocalSymbol(Scope* scope, StringObject* id);
AST* getSymbol(Scope* scope, StringObject* id);

#endif
