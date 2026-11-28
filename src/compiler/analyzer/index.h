#ifndef ANALYZER_INDEX_H
#define ANALYZER_INDEX_H

#include "compiler/analyzer/analyzer.h"

void analyze_index(Analyzer* analyzer, ASTNode* ast);
void analyze_index_assignment(Analyzer* analyzer, ASTNode* ast);

#endif
