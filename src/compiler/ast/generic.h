#ifndef AST_GENERIC_H
#define AST_GENERIC_H

#include "compiler/ast/ast.h"

struct GenericFunction
{
    GenericBindings bindings;
    Token source;
    ASTNode* template;
    ASTNode* specializations;
    Scope* declaration_scope;
    bool analyzed;
    bool cached;
};

#endif
