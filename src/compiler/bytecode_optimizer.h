#ifndef BYTECODE_OPTIMIZER_H
#define BYTECODE_OPTIMIZER_H

#include "runtime/code_object.h"
#include "runtime/module_object.h"
#include <stddef.h>

void optimizeBytecode(const ModuleObject* module, CodeObject* code, size_t persistentRegisters);

#endif
