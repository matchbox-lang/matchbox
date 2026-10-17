#include "module_object.h"
#include "bytecode.h"
#include "function_object.h"
#include "object.h"
#include "string_object.h"
#include "value.h"
#include <stdio.h>
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

void freeModuleObject(ModuleObject* module)
{
    size_t functionCount = countVector(&module->functions);

    for (size_t i = 0; i < functionCount; i++) {
        FunctionObject* function = getVectorAt(&module->functions, i);
        freeFunctionObject(function);
    }

    size_t objectCount = countVector(&module->objects);

    for (size_t i = 0; i < objectCount; i++) {
        StringObject* string = getVectorAt(&module->objects, i);

        freeStringObject(string);
    }

    freeVector(&module->objects);
    freeVector(&module->functions);
    freeValueArray(&module->constants);
    free(module);
}

void disassembleModule(ModuleObject* module)
{
    size_t functionCount = countVector(&module->functions);
    FunctionObject* function;

    for (size_t i = 0; i < functionCount; i++) {
        function = getVectorAt(&module->functions, i);

        if (function->type == FUNCTION_BUILTIN) {
            continue;
        }

        disassemble(&function->code);

        if (i < functionCount - 1) {
            printf("\n");
        }
    }
}
