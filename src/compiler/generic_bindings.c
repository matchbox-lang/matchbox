#include "compiler/generic_bindings.h"

Type get_generic_argument_type(const GenericBindings* bindings, size_t index)
{
    if (index < bindings->arguments.count) {
        Type* type = bindings->arguments.data[index];

        return *type;
    }

    return (Type){.kind = TypeGeneric, .generic_index = index};
}
