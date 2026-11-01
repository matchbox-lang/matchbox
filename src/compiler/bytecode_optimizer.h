#ifndef BYTECODE_OPTIMIZER_H
#define BYTECODE_OPTIMIZER_H

#include "runtime/code_object.h"
#include "runtime/module_object.h"
#include <stddef.h>

void optimize_bytecode(const ModuleObject* module, CodeObject* code, size_t persistent_registers);

#endif
