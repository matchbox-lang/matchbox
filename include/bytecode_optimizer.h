#ifndef BYTECODE_OPTIMIZER_H
#define BYTECODE_OPTIMIZER_H

#include "code_object.h"
#include "module_object.h"
#include <stddef.h>

// Persistent registers hold top-level bindings needed by later interactive submissions
void optimizeBytecode(ModuleObject* module, CodeObject* code, size_t persistentRegisters);

#endif
