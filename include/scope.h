#ifndef SCOPE_H
#define SCOPE_H

#include "object.h"
#include "table.h"

typedef struct AST AST;
typedef struct Scope Scope;

typedef struct Scope
{
    Scope* parent;
    Table symbols;
    size_t localOffset;
} Scope;

Scope* createScope(Scope* parent);
void freeScope(Scope* scope);
AST* setLocalSymbol(Scope* scope, StringObject* id, AST* symbol);
AST* setLocalVariable(Scope* scope, StringObject* id, AST* symbol);
AST* getLocalSymbol(Scope* scope, StringObject* id);
AST* getSymbol(Scope* scope, StringObject* id);

#endif
