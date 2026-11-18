#ifndef ANALYZER_LIFETIME_H
#define ANALYZER_LIFETIME_H

#include "compiler/analyzer/analyzer.h"
#include "common/table.h"

typedef struct ReferenceLifetimes
{
    size_t position;
    Table last_uses;
    Vector* releases;
} ReferenceLifetimes;

void init_reference_lifetimes(ReferenceLifetimes* lifetimes, Vector* nodes, size_t start);
void schedule_reference_release(ReferenceLifetimes* lifetimes, ASTNode* ast, size_t position);
void free_reference_lifetimes(ReferenceLifetimes* lifetimes, size_t count);
bool branch_uses_reference(ASTNode* branch, StringObject* id, ASTNode* binding);
bool continuation_uses_reference(Analyzer* analyzer, StringObject* id);

#endif
