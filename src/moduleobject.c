#include "moduleobject.h"
#include "bytecode.h"
#include "functionobject.h"
#include "object.h"
#include "value.h"
#include <stdio.h>
#include <stdlib.h>

ModuleObject* createModuleObject()
{
    ModuleObject* module = ALLOCATE_OBJECT(ModuleObject, OBJ_MODULE);
    FunctionObject* function = createFunctionObject();

    initValueArray(&module->constants);
    pushValue(&module->constants, POINTER_VALUE(function));

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
        function = AS_POINTER(module->constants.data[i]);
        disassemble(&function->code);

        if (i < functionCount - 1) {
            printf("\n");
        }
    }
}
