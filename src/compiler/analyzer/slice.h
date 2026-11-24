#ifndef ANALYZER_SLICE_H
#define ANALYZER_SLICE_H

#include "compiler/analyzer/analyzer.h"

void analyze_slice_argument(Analyzer* analyzer, ASTNode* ast);
void analyze_slice(Analyzer* analyzer, ASTNode* ast);

#endif
