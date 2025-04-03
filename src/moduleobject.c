#include "moduleobject.h"
#include "functionobject.h"
#include "bytecode.h"
#include <stdio.h>

ModuleObject* createModuleObject()
{
    ModuleObject* module = ALLOCATE_OBJECT(ModuleObject, OBJ_MODULE);
    initValueArray(&module->constants);

    return module;
}

void freeModuleObject(ModuleObject* module)
{
    freeValueArray(&module->constants);
    free(module);
}

void disassembleModule(ModuleObject* module)
{
    size_t functionCount = countValueArray(&module->constants);
    FunctionObject* function;

    for (int i = 0; i < functionCount; i++) {
        function = AS_FUNCTION_OBJECT(module->constants.data[i]);
        disassemble(&function->code);

        if (i < functionCount - 1) {
            printf("\n");
        }
    }
}
