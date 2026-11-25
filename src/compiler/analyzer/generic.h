#ifndef ANALYZER_GENERIC_H
#define ANALYZER_GENERIC_H

#include "compiler/analyzer/analyzer.h"

void discard_generic_candidate(ASTNode* ast);
void register_generic_function(Analyzer* analyzer, ASTNode* ast);
ASTNode* prepare_generic_candidate(Analyzer* analyzer, ASTNode* caller, ASTNode* candidate);

#endif
