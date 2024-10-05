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

    initVector(&scope->references);
    initTable(&scope->symbols, 32);
    
    return scope;
}

void freeScope(Scope* scope)
{
    size_t count = countVector(&scope->references);

    for (size_t i = 0; i < count; i++) {
        Reference* ref = vectorGet(&scope->references, i);
        freeReference(ref);
    }

    freeVector(&scope->references);
    freeTable(&scope->symbols);
    free(scope);
}

AST* setLocalSymbol(Scope* scope, StringObject* id, AST* ast)
{
    if (tableSet(&scope->symbols, id, ast)) {
        return ast;
    }

    return NULL;
}

AST* setLocalVariable(Scope* scope, StringObject* id, AST* ast)
{
    ast->varDef.position = scope->localCount++;

    return setLocalSymbol(scope, id, ast);
}

AST* getLocalSymbol(Scope* scope, StringObject* id)
{
    return tableGet(&scope->symbols, id);
}

AST* getSymbol(Scope* scope, StringObject* id)
{
    AST* symbol = tableGet(&scope->symbols, id);

    if (symbol) {
        return symbol;
    }

    if (scope->parent) {
        return getSymbol(scope->parent, id);
    }

    return NULL;
}
