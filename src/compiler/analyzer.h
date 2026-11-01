#ifndef ANALYZER_H
#define ANALYZER_H

#include "compiler/ast.h"
#include <stddef.h>

typedef struct ReferenceLifetimes ReferenceLifetimes;
typedef struct FunctionPath FunctionPath;
typedef struct LoopPath LoopPath;

typedef struct Analyzer
{
    ASTNode* top_level;
    ASTNode* function;
    Scope* current_scope;
    Scope* constant_scope;
    Vector* current_nodes;
    size_t next_node;
    ReferenceLifetimes* reference_lifetimes;
    FunctionPath* function_path;
    FunctionPath* constant_functions;
    LoopPath* loop_path;
} Analyzer;

void init_analyzer(Analyzer* analyzer, ASTNode* ast);
bool analyze(Analyzer* analyzer, size_t start);
void rebuild_analyzer(Analyzer* analyzer, Vector* functions);

#endif
