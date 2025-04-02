#ifndef MODULE_OBJECT_H
#define MODULE_OBJECT_H

#include "object.h"

#define AS_MODULE_OBJECT(value) ((ModuleObject*)AS_OBJECT(value))

typedef struct ModuleObject
{
    Object obj;
    ValueArray constants;
} ModuleObject;

ModuleObject* createModuleObject();
void freeModuleObject(ModuleObject* module);
void disassembleModule(ModuleObject* module);

#endif
