#include "compiler/analyzer/type.h"
#include "common/slice.h"
#include "compiler/analyzer/array.h"
#include "compiler/analyzer/tuple.h"
#include "compiler/analyzer/type_alias.h"

void resolve_type(Analyzer* analyzer, Type* type)
{
    resolve_named_type(analyzer, type, NULL);
    resolve_tuple_type(analyzer, type);

    if (type->kind == TypeStringSlice) {
        *type = get_string_slice_type(&analyzer->array_types);

        return;
    }

    resolve_array_dimensions(analyzer, type);
}
