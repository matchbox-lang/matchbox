#ifndef ANALYZER_H
#define ANALYZER_H

#include "ast.h"
#include <stddef.h>

typedef struct Analyzer
{
    ASTNode* topLevel;
    ASTNode* function;
    Scope* currentScope;
} Analyzer;

void initAnalyzer(Analyzer* analyzer, ASTNode* ast);
bool analyze(Analyzer* analyzer, size_t start);

#endif
