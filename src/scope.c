#include "scope.h"
#include "ast.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

Scope* createScope(Scope* parent)
{
    Scope* scope = malloc(sizeof(Scope));
    scope->parent = parent;
    scope->localCount = 0;
    scope->level = getLevel(parent) + 1;

    initReferenceArray(&scope->references);
    initTable(&scope->symbols, 32);
    
    return scope;
}

void freeScope(Scope* scope)
{
    size_t count = countReferenceArray(&scope->references);

    for (size_t i = 0; i < count; i++) {
        Reference ref = getReferenceAt(&scope->references, i);
    }

    freeReferenceArray(&scope->references);
    freeTable(&scope->symbols);
    free(scope);
}

size_t getLocalCount(Scope* scope)
{
    return scope->localCount;
}

size_t getLevel(Scope* scope)
{
    return scope ? scope->level : 0;
}

AST* setLocalSymbol(Scope* scope, StringObject* id, AST* ast, bool local)
{
    if (local) {
        scope->localCount++;
    }
    
    if (setTableAt(&scope->symbols, id, ast)) {
        return ast;
    }

    return NULL;
}

AST* getLocalSymbol(Scope* scope, StringObject* id)
{
    return getTableAt(&scope->symbols, id);
}

AST* getSymbol(Scope* scope, StringObject* id)
{
    AST* symbol = getTableAt(&scope->symbols, id);

    if (symbol) {
        return symbol;
    }

    if (scope->parent) {
        return getSymbol(scope->parent, id);
    }

    return NULL;
}
