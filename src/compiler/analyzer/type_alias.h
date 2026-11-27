#ifndef ANALYZER_TYPE_ALIAS_H
#define ANALYZER_TYPE_ALIAS_H

#include "compiler/analyzer/analyzer.h"

void register_type_aliases(Analyzer* analyzer, Vector* nodes, size_t start);
void resolve_type_alias(Analyzer* analyzer, ASTNode* ast);
void resolve_named_type(Analyzer* analyzer, Type* type, ReferenceType* reference);

#endif
