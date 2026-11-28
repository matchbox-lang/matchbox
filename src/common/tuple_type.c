#include "common/tuple_type.h"
#include "common/errors.h"
#include "common/memory.h"

TupleType* create_tuple_type(size_t count)
{
    TupleType* tuple = allocate_zeroed_memory(1, sizeof(TupleType) + count * sizeof(TupleElement));
    tuple->count = count;

    return tuple;
}

size_t get_tuple_element_slot_count(TupleElement element)
{
    if (element.type.kind == TypeVoid) {
        fatal_error("Invalid tuple element type");
    }

    if (element.reference != RefNone && !is_slice_type(element.type.kind)) {
        return 1;
    }

    return get_value_slot_count(element.type);
}

void layout_tuple_type(TupleType* tuple)
{
    size_t slots = 0;

    for (size_t i = 0; i < tuple->count; i++) {
        TupleElement* element = &tuple->elements[i];
        element->offset = slots;
        size_t count = get_tuple_element_slot_count(*element);
        slots += count;
    }

    tuple->slots = slots;
    tuple->resolved = true;
}

static bool tuple_elements_equal(TupleElement left, TupleElement right)
{
    bool same = types_equal(left.type, right.type);

    return same && left.reference == right.reference;
}

bool tuple_types_equal(const TupleType* left, const TupleType* right)
{
    if (left->count != right->count) {
        return false;
    }

    size_t index = 0;

    while (index < left->count && tuple_elements_equal(left->elements[index], right->elements[index])) {
        index++;
    }

    return index == left->count;
}

static bool is_owned_tuple_element(TupleElement element)
{
    if (element.reference == RefExclusive) {
        return true;
    }

    return element.reference == RefNone && is_owned_type(element.type);
}

bool is_owned_type(Type type)
{
    if (type.kind != TypeTuple) {
        return is_array_type(type.kind);
    }

    size_t index = 0;

    while (index < type.tuple->count && !is_owned_tuple_element(type.tuple->elements[index])) {
        index++;
    }

    return index < type.tuple->count;
}

static bool is_exclusive_tuple_element(TupleElement element)
{
    return element.reference == RefExclusive || has_exclusive_tuple_reference(element.type);
}

bool has_exclusive_tuple_reference(Type type)
{
    if (type.kind != TypeTuple) {
        return false;
    }

    size_t index = 0;

    while (index < type.tuple->count && !is_exclusive_tuple_element(type.tuple->elements[index])) {
        index++;
    }

    return index < type.tuple->count;
}
