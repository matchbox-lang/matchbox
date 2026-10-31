#include "compiler/scope.h"
#include "common/memory.h"
#include "common/table.h"
#include "common/util.h"
#include "compiler/ast.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

static Scope* getFrameScope(Scope* scope, Scope* parent)
{
    if (parent) {
        return parent->frameScope;
    }

    return scope;
}

static size_t getLocalOffset(const Scope* parent)
{
    if (!parent) {
        return 0;
    }

    return parent->localOffset + parent->localCount;
}

Scope* createScope(Scope* parent)
{
    Scope* scope = allocateMemory(sizeof(Scope));
    scope->parent = parent;
    scope->frameScope = getFrameScope(scope, parent);
    scope->localOffset = getLocalOffset(parent);
    scope->localCount = 0;
    scope->maxLocalCount = scope->localOffset;
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
    freeMemory(scope);
}

static size_t getSymbolSlotCount(const ASTNode* symbol)
{
    if (!isVariableType(symbol)) {
        return 0;
    }

    if (getReferenceType(symbol) != REFERENCE_NONE) {
        return 1;
    }

    return getTypeSlotCount(getNodeDataType(symbol));
}

size_t getMaxLocalCount(const Scope* scope)
{
    return scope->frameScope->maxLocalCount;
}

size_t getNextLocalPosition(const Scope* scope)
{
    return scope->localOffset + scope->localCount;
}

size_t getScopeLevel(const Scope* scope)
{
    return scope ? scope->level : 0;
}

bool isTopLevelScope(const Scope* scope)
{
    return scope->level == 1;
}

ASTNode* setLocalSymbol(Scope* scope, StringView id, ASTNode* symbol)
{
    if (setTableAt(&scope->symbols, id, symbol)) {
        return symbol;
    }

    return NULL;
}

ASTNode* setLocalVariableSymbol(Scope* scope, StringView id, ASTNode* symbol)
{
    scope->localCount += getSymbolSlotCount(symbol);
    size_t localExtent = getNextLocalPosition(scope);

    scope->frameScope->maxLocalCount = MAX(scope->frameScope->maxLocalCount, localExtent);
    
    return setLocalSymbol(scope, id, symbol);
}

ASTNode* getLocalSymbol(const Scope* scope, StringView id)
{
    return getTableAt(&scope->symbols, id);
}

ASTNode* getSymbol(const Scope* scope, StringView id)
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
