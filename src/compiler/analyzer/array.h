#ifndef ANALYZER_ARRAY_H
#define ANALYZER_ARRAY_H

#include "compiler/analyzer/analyzer.h"

bool infer_variable_array_count(Analyzer* analyzer, ASTNode* ast);
void resolve_dimensions(Analyzer* analyzer, Type* type);
void analyze_array(Analyzer* analyzer, ASTNode* ast);
void analyze_index(Analyzer* analyzer, ASTNode* ast);
void analyze_index_assignment(Analyzer* analyzer, ASTNode* ast);

#endif
