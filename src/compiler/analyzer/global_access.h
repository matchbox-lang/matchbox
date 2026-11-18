#ifndef ANALYZER_GLOBAL_ACCESS_H
#define ANALYZER_GLOBAL_ACCESS_H

#include "compiler/analyzer/analyzer.h"

void validate_global_binding_access(Analyzer* analyzer, ASTNode* origin, ReferenceType type, Token token);
bool contains_binding(const Vector* bindings, const ASTNode* symbol);
void validate_function_global_access(Analyzer* analyzer, ASTNode* function, bool moves_arrays);

#endif
