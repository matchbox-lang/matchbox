#include "reference.h"
#include <stdlib.h>

Reference* createReference(AST* ast, size_t position)
{
    Reference* ref = malloc(sizeof(Reference));
    ref->ast = ast;
    ref->position = position;

    return ref;
}

void freeReference(Reference* ref)
{
    free(ref);
}