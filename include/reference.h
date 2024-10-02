#ifndef REFERENCE_H
#define REFERENCE_H

#include "ast.h"

typedef struct Reference
{
    AST* ast;
    size_t position;
} Reference;

Reference* createReference(AST* ast, size_t position);
void freeReference(Reference* reference);

#endif
