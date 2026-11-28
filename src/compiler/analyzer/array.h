#ifndef ANALYZER_ARRAY_H
#define ANALYZER_ARRAY_H

#include "compiler/analyzer/analyzer.h"

bool infer_variable_array_count(Analyzer* analyzer, ASTNode* ast);
void resolve_array_dimensions(Analyzer* analyzer, Type* type);
void analyze_array(Analyzer* analyzer, ASTNode* ast);
Type analyze_array_binary_types(Analyzer* analyzer, Token operator, Type left, Type right, Type* operand_type);
void analyze_array_element(Analyzer* analyzer, ASTNode* element, Type expected, Token token);

#endif
