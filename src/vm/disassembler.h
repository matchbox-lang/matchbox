#ifndef DISASSEMBLER_H
#define DISASSEMBLER_H

#include "runtime/code_object.h"
#include "runtime/module_object.h"

void disassemble(CodeObject* code);
void disassembleModule(ModuleObject* module);

#endif
