#ifndef ANALYZER_H
#define ANALYZER_H

#include "ast.h"
#include <stddef.h>

typedef struct Analyzer
{
    AST* topLevel;
    Scope* currentScope;
} Analyzer;

void initAnalyzer(Analyzer* analyzer, AST* ast);
bool analyze(Analyzer* analyzer, size_t start);

#endif
