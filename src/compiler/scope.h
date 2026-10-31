#ifndef SCOPE_H
#define SCOPE_H

#include "common/table.h"

typedef struct ASTNode ASTNode;
typedef struct Scope Scope;

typedef struct Scope
{
    Scope* parent;
    Scope* frameScope;
    size_t localOffset;
    size_t localCount;
    size_t maxLocalCount;
    size_t level;
    Table symbols;
} Scope;

Scope* createScope(Scope* parent);
void freeScope(Scope* scope);
size_t getMaxLocalCount(const Scope* scope);
size_t getNextLocalPosition(const Scope* scope);
size_t getScopeLevel(const Scope* scope);
bool isTopLevelScope(const Scope* scope);
ASTNode* setLocalSymbol(Scope* scope, StringView id, ASTNode* symbol);
ASTNode* setLocalVariableSymbol(Scope* scope, StringView id, ASTNode* symbol);
ASTNode* getLocalSymbol(const Scope* scope, StringView id);
ASTNode* getSymbol(const Scope* scope, StringView id);

#endif
