#ifndef ANALYZER_H
#define ANALYZER_H

#include "compiler/ast.h"
#include <stddef.h>

typedef struct ReferenceLifetimes ReferenceLifetimes;
typedef struct FunctionPath FunctionPath;
typedef struct LoopPath LoopPath;

typedef struct Analyzer
{
    ASTNode* topLevel;
    ASTNode* function;
    Scope* currentScope;
    Vector* currentNodes;
    size_t nextNode;
    ReferenceLifetimes* referenceLifetimes;
    FunctionPath* functionPath;
    LoopPath* loopPath;
} Analyzer;

void initAnalyzer(Analyzer* analyzer, ASTNode* ast);
bool analyze(Analyzer* analyzer, size_t start);
void rebuildAnalyzer(Analyzer* analyzer, Vector* functions);

#endif
