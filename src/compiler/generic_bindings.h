#ifndef GENERIC_BINDINGS_H
#define GENERIC_BINDINGS_H

#include "common/type.h"
#include "common/vector.h"

typedef struct GenericBindings
{
    struct GenericBindings* parent;
    Vector names;
    Vector arguments;
} GenericBindings;

Type get_generic_argument_type(const GenericBindings* bindings, size_t index);

#endif
