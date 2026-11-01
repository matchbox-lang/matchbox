#include "runtime/module_object.h"
#include "common/memory.h"
#include "runtime/function_object.h"
#include "runtime/object.h"
#include "runtime/string_object.h"
#include "runtime/value.h"
#include <stdlib.h>

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
        StringObject* string = get_vector_at(&module->objects, i);

        free_string_object(string);
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
