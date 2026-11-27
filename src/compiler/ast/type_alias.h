#ifndef AST_TYPE_ALIAS_H
#define AST_TYPE_ALIAS_H

#include "compiler/ast/ast.h"

struct NamedType
{
    Token token;
    Token suffix;
    GenericBindings* bindings;
    bool reference;
};

void free_named_type(const NamedType* named);

#endif
