#ifndef ANALYZER_BUILTIN_H
#define ANALYZER_BUILTIN_H

#include "compiler/analyzer/analyzer.h"

void validate_constant_builtin(Analyzer* analyzer, const Builtin* builtin, Token token);
void analyze_builtin_call(Analyzer* analyzer, ASTNode* ast);
void analyze_resolved_builtin_call(Analyzer* analyzer, ASTNode* ast);

#endif
