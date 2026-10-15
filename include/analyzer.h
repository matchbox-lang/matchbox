#ifndef ANALYZER_H
#define ANALYZER_H

#include "ast.h"
#include <stddef.h>

typedef struct ReferenceLifetimes ReferenceLifetimes;

typedef struct Analyzer
{
    ASTNode* topLevel;
    ASTNode* function;
    Scope* currentScope;
    Vector* currentNodes;
    size_t nextNode;
    ReferenceLifetimes* referenceLifetimes;
} Analyzer;

void initAnalyzer(Analyzer* analyzer, ASTNode* ast);
bool analyze(Analyzer* analyzer, size_t start);

#endif
