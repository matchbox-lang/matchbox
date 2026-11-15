#include "runtime/module_object.h"
#include "common/errors.h"
#include "common/memory.h"
#include "runtime/function_object.h"
#include "runtime/object.h"
#include "runtime/string_object.h"
#include "runtime/value.h"
#include <stdlib.h>

static StringObject* get_string_object(ModuleObject* module, const char* chars, size_t length);

ModuleObject* create_module_object()
{
    ModuleObject* module = ALLOCATE_OBJECT(ModuleObject, ObjModule);
    FunctionObject* function = create_function_object();

    init_value_array(&module->constants);
    init_vector(&module->functions);
    init_vector(&module->objects);
    push_vector_item(&module->functions, function);
    
    module->global_count = 0;

    return module;
}

static void free_module_functions(ModuleObject* module)
{
    size_t function_count = count_vector(&module->functions);

    for (size_t i = 0; i < function_count; i++) {
        FunctionObject* function = get_vector_at(&module->functions, i);

        free_function_object(function);
    }

    free_vector(&module->functions);
}

static void free_module_objects(ModuleObject* module)
{
    size_t object_count = count_vector(&module->objects);

    for (size_t i = 0; i < object_count; i++) {
        Object* object = get_vector_at(&module->objects, i);

        free_memory(object);
    }

    free_vector(&module->objects);
}

void free_module_object(ModuleObject* module)
{
    free_module_functions(module);
    free_module_objects(module);
    free_value_array(&module->constants);
    free_memory(module);
}

static size_t find_object_constant(const ModuleObject* module, const Object* object)
{
    size_t position = 0;

    while (position < module->constants.count && AS_POINTER(module->constants.data[position]) != object) {
        position++;
    }

    if (position == module->constants.count) {
        return SIZE_MAX;
    }

    return position;
}

static size_t find_string_object_constant(const ModuleObject* module, const Object* object,
    const char* chars, size_t length)
{
    if (object->type != ObjString) {
        return SIZE_MAX;
    }

    const StringObject* string = (const StringObject*)object;
    bool matches = compare_string_object_chars(string, chars, length);

    if (!matches) {
        return SIZE_MAX;
    }

    return find_object_constant(module, object);
}

size_t find_string_constant(const ModuleObject* module, const char* chars, size_t length)
{
    size_t position = SIZE_MAX;
    size_t index = 0;

    while (index < module->objects.count && position == SIZE_MAX) {
        Object* object = module->objects.data[index++];
        position = find_string_object_constant(module, object, chars, length);
    }

    return position;
}

static StringObject* get_string_object(ModuleObject* module, const char* chars, size_t length)
{
    size_t index = 0;

    while (index < module->objects.count &&
        (((const Object*)module->objects.data[index])->type != ObjString ||
        !compare_string_object_chars((const StringObject*)module->objects.data[index], chars, length))) {
        index++;
    }

    if (index < module->objects.count) {
        return module->objects.data[index];
    }

    StringObject* string = copy_string_object(chars, length);
    push_vector_item(&module->objects, string);

    return string;
}

size_t add_string_constant(ModuleObject* module, const char* chars, size_t length)
{
    size_t constant = find_string_constant(module, chars, length);

    if (constant != SIZE_MAX) {
        return constant;
    }

    if (module->constants.count > UINT16_MAX) {
        fatal_error("Cannot create more than %u constants", UINT16_MAX + 1u);
    }

    StringObject* string = get_string_object(module, chars, length);
    Value value = POINTER_VALUE(string);
    size_t position = push_value(&module->constants, value);

    return position - 1;
}
