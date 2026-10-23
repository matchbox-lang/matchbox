#ifndef BYTECODE_OPTIMIZER_H
#define BYTECODE_OPTIMIZER_H

#include "runtime/code_object.h"
#include "runtime/module_object.h"
#include <stddef.h>

void optimizeBytecode(ModuleObject* module, CodeObject* code, size_t persistentRegisters);

#endif
