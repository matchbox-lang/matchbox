#include "runtime/module_object.h"
#include "common/memory.h"
#include "runtime/function_object.h"
#include "runtime/object.h"
#include "runtime/string_object.h"
#include "runtime/value.h"
#include <stdlib.h>

ModuleObject* createModuleObject()
{
    ModuleObject* module = ALLOCATE_OBJECT(ModuleObject, OBJ_MODULE);
    FunctionObject* function = createFunctionObject();

    initValueArray(&module->constants);
    initVector(&module->functions);
    initVector(&module->objects);
    pushVectorItem(&module->functions, function);
    
    module->globalCount = 0;

    return module;
}

static void freeModuleFunctions(ModuleObject* module)
{
    size_t functionCount = countVector(&module->functions);

    for (size_t i = 0; i < functionCount; i++) {
        FunctionObject* function = getVectorAt(&module->functions, i);

        freeFunctionObject(function);
    }

    freeVector(&module->functions);
}

static void freeModuleObjects(ModuleObject* module)
{
    size_t objectCount = countVector(&module->objects);

    for (size_t i = 0; i < objectCount; i++) {
        StringObject* string = getVectorAt(&module->objects, i);

        freeStringObject(string);
    }

    freeVector(&module->objects);
}

void freeModuleObject(ModuleObject* module)
{
    freeModuleFunctions(module);
    freeModuleObjects(module);
    freeValueArray(&module->constants);
    freeMemory(module);
}
