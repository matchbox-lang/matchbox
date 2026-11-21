#ifndef RUNNER_H
#define RUNNER_H

#include "common/memory.h"
#include "compiler/ast.h"
#include <stdbool.h>

typedef bool (*RunnerAction)(const void* data, MemoryGroup* result_memory);
bool validate_source_path(const char* path);
char* read_source(const char* path);
ASTNode* analyze_source(const char* source);
bool run_isolated(RunnerAction action, const void* data);

#endif
