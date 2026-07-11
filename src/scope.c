#include "scope.h"
#include "ast.h"
#include "table.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

Scope* createScope(Scope* parent)
{
    Scope* scope = malloc(sizeof(Scope));
    scope->parent = parent;
    scope->localCount = 0;
    scope->level = getScopeLevel(parent) + 1;

    initTable(&scope->symbols, 32);
    
    return scope;
}

void freeScope(Scope* scope)
{
    if (!scope) {
        return;
    }

    freeTable(&scope->symbols);
    free(scope);
}

size_t getLocalCount(Scope* scope)
{
    return scope->localCount;
}

size_t getScopeLevel(Scope* scope)
{
    return scope ? scope->level : 0;
}

bool isTopLevelScope(Scope* scope)
{
    return scope->level == 1;
}

ASTNode* setLocalSymbol(Scope* scope, StringObject* id, ASTNode* symbol)
{
    if (setTableAt(&scope->symbols, id, symbol)) {
        return symbol;
    }

    return NULL;
}

ASTNode* setLocalVariableSymbol(Scope* scope, StringObject* id, ASTNode* symbol)
{
    scope->localCount++;
    
    return setLocalSymbol(scope, id, symbol);
}

ASTNode* getLocalSymbol(Scope* scope, StringObject* id)
{
    return getTableAt(&scope->symbols, id);
}

ASTNode* getSymbol(Scope* scope, StringObject* id)
{
    ASTNode* symbol = getTableAt(&scope->symbols, id);

    if (symbol) {
        return symbol;
    }

    if (scope->parent) {
        return getSymbol(scope->parent, id);
    }

    return NULL;
}
