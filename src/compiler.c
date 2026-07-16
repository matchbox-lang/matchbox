#include "compiler.h"
#include "ast.h"
#include "builtin.h"
#include "code_object.h"
#include "function_object.h"
#include "module_object.h"
#include "opcode.h"
#include "parser.h"
#include "optimizer.h"
#include "scope.h"
#include "token.h"
#include "util.h"
#include "value.h"
#include "vector.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef void (*CompileStatements)(Compiler* compiler, Vector* nodes);

typedef struct Operand
{
    int reg;
    bool temporary;
} Operand;

typedef struct CallArea
{
    int resultRegister;
    int frameRegister;
} CallArea;

typedef struct PreviousInstruction
{
    CodeObject* code;
    size_t start;
    Opcode opcode;
    uint8_t a;
    uint8_t b;
    uint8_t c;
} PreviousInstruction;

static Operand compileExpression(Compiler* compiler, ASTNode* ast, bool discard);
static void compileBlocklevelStatements(Compiler* compiler, Vector* nodes);
static void compileTopLevelStatements(Compiler* compiler, Vector* nodes);

static void functionPositionOverflowError()
{
    fprintf(stderr, "Error: Cannot create more than 65536 functions\n");
    exit(1);
}

static void functionNotFoundError(ASTNode* ast)
{
    fprintf(stderr, "Error: Could not find function %s\n", ast->functionDefinition.id->chars);
    exit(1);
}

static CodeObject* currentCodeObject(Compiler* compiler)
{
    return &compiler->function->code;
}

static bool isCompilingTopLevel(Compiler* compiler)
{
    return compiler->function == getVectorAt(&compiler->module->functions, 0);
}

static int allocateRegister(Compiler* compiler)
{
    int reg = compiler->registerCount++;
    int extent = compiler->registerCount + 2;

    if (extent > compiler->function->maxStackCount) {
        compiler->function->maxStackCount = extent;
    }

    return reg;
}

static int releaseRegister(Compiler* compiler)
{
    return --compiler->registerCount;
}

static void write8(Compiler* compiler, uint8_t n)
{
    pushByte(currentCodeObject(compiler), n);
}

static void emitInstruction(Compiler* compiler, Opcode opcode, uint8_t a, uint8_t b, uint8_t c)
{
    write8(compiler, opcode);
    write8(compiler, a);
    write8(compiler, b);
    write8(compiler, c);
}

static void emitHlt(Compiler* compiler)
{
    emitInstruction(compiler, OP_HLT, 0, 0, 0);
}

static bool getPreviousInstruction(Compiler* compiler, PreviousInstruction* instruction)
{
    CodeObject* code = currentCodeObject(compiler);
    size_t count = countCodeObject(code);

    if (count < INSTRUCTION_SIZE) {
        return false;
    }

    size_t start = count - INSTRUCTION_SIZE;

    instruction->code = code;
    instruction->start = start;
    instruction->opcode = (Opcode)code->data[start];
    instruction->a = code->data[start + 1];
    instruction->b = code->data[start + 2];
    instruction->c = code->data[start + 3];

    return true;
}

static bool fuseMovMov(Compiler* compiler, int dst, int src)
{
    PreviousInstruction previous;

    if (!getPreviousInstruction(compiler, &previous)
        || previous.a + 1 != dst
        || previous.opcode != OP_MOV) {
        return false;
    }

    setByteAt(previous.code, previous.start, OP_MOV2);
    setByteAt(previous.code, previous.start + 3, src);

    return true;
}

static void emitMov(Compiler* compiler, int dst, int src)
{
    if (dst == src) {
        return;
    }

    if (fuseMovMov(compiler, dst, src)) {
        return;
    }

    emitInstruction(compiler, OP_MOV, dst, src, 0);
}

static Operand makeOperand(int reg, bool temporary)
{
    Operand operand = {reg, temporary};

    return operand;
}

static Operand noOperand(void)
{
    return makeOperand(-1, false);
}

static void releaseOperand(Compiler* compiler, Operand operand)
{
    if (operand.temporary) {
        releaseRegister(compiler);
    }
}

static Operand materializeOperand(Compiler* compiler, Operand operand)
{
    if (operand.temporary) {
        return operand;
    }

    int reg = allocateRegister(compiler);

    emitMov(compiler, reg, operand.reg);
    
    return makeOperand(reg, true);
}

static int emitLdc(Compiler* compiler, uint16_t imm)
{
    int reg = allocateRegister(compiler);

    emitInstruction(compiler, OP_LDC, reg, imm >> 8, imm);

    return reg;
}

static bool fuseLdiLdi(Compiler* compiler, int reg, int16_t imm)
{
    PreviousInstruction previous;

    if (isLargerThan8BitSigned(imm) || !getPreviousInstruction(compiler, &previous)) {
        return false;
    }

    if (previous.opcode != OP_LDI || previous.a + 1 != reg) {
        return false;
    }

    int16_t previousImm = (int16_t)(previous.b << 8 | previous.c);

    if (isLargerThan8BitSigned(previousImm)) {
        return false;
    }

    setByteAt(previous.code, previous.start, OP_LDI2);
    setByteAt(previous.code, previous.start + 2, previousImm);
    setByteAt(previous.code, previous.start + 3, imm);

    return true;
}

static int emitLdi(Compiler* compiler, int16_t imm)
{
    int reg = allocateRegister(compiler);

    if (fuseLdiLdi(compiler, reg, imm)) {
        return reg;
    }

    emitInstruction(compiler, OP_LDI, reg, imm >> 8, imm);

    return reg;
}

static int emitLdg(Compiler* compiler, uint16_t imm)
{
    int reg = allocateRegister(compiler);

    emitInstruction(compiler, OP_LDG, reg, imm >> 8, imm);

    return reg;
}

static Opcode getLoadCallOpcode(Opcode opcode)
{
    switch (opcode) {
        case OP_LDC:
            return OP_LDC_CALL;
        case OP_LDG:
            return OP_LDG_CALL;
        case OP_LDI:
            return OP_LDI_CALL;
        default:
            return OP_HLT;
    }
}

static bool isLoadCallOperandValid(Opcode opcode, uint16_t operands)
{
    if (opcode != OP_LDI) {
        return operands <= LOAD_CALL_OPERAND_MASK;
    }

    int16_t imm = (int16_t)operands;

    return imm >= LOAD_CALL_SIGNED_OPERAND_MIN
        && imm <= LOAD_CALL_SIGNED_OPERAND_MAX;
}

static bool fuseLoadCall(Compiler* compiler, uint8_t frameRegister, uint16_t functionPosition)
{
    PreviousInstruction previous;

    if (functionPosition > LOAD_CALL_FUNCTION_MASK
        || !getPreviousInstruction(compiler, &previous)) {
        return false;
    }

    Opcode fusedOpcode = getLoadCallOpcode(previous.opcode);

    if (fusedOpcode == OP_HLT) {
        return false;
    }

    if (previous.a != frameRegister) {
        return false;
    }

    uint16_t operands = previous.b << 8 | previous.c;

    if (!isLoadCallOperandValid(previous.opcode, operands)) {
        return false;
    }

    operands = ((operands & LOAD_CALL_OPERAND_MASK) << LOAD_CALL_FUNCTION_BITS)
        | functionPosition;

    setByteAt(previous.code, previous.start, fusedOpcode);
    setByteAt(previous.code, previous.start + 2, operands >> 8);
    setByteAt(previous.code, previous.start + 3, operands);

    return true;
}

static bool isBuiltinFunctionAtPosition(Compiler* compiler, uint16_t position)
{
    FunctionObject* function = getVectorAt(&compiler->module->functions, position);

    return function->type == FUNCTION_BUILTIN;
}

static bool fuseCallCall(Compiler* compiler, uint8_t frameRegister, uint16_t functionPosition)
{
    PreviousInstruction previous;

    if (functionPosition > UINT8_MAX
        || !getPreviousInstruction(compiler, &previous)) {
        return false;
    }

    if (previous.opcode != OP_CALL
        || previous.a + 1 != frameRegister
        || previous.b != 0) {
        return false;
    }

    uint8_t previousFunctionPosition = previous.c;

    if (!isBuiltinFunctionAtPosition(compiler, previousFunctionPosition)) {
        return false;
    }

    setByteAt(previous.code, previous.start, OP_CALL2);
    setByteAt(previous.code, previous.start + 2, previousFunctionPosition);
    setByteAt(previous.code, previous.start + 3, functionPosition);

    return true;
}

static void emitCallInstruction(Compiler* compiler, uint8_t frameRegister, uint16_t functionPosition)
{
    if (fuseCallCall(compiler, frameRegister, functionPosition)) {
        return;
    }

    if (fuseLoadCall(compiler, frameRegister, functionPosition)) {
        return;
    }

    emitInstruction(compiler, OP_CALL, frameRegister, functionPosition >> 8, functionPosition);
}

static void emitRet(Compiler* compiler)
{
    emitInstruction(compiler, OP_RET, 0, 0, 0);
}

static void emitRetv(Compiler* compiler, int src)
{
    emitInstruction(compiler, OP_RETV, src, 0, 0);
}

static int getCallAreaCount(FunctionObject* function)
{
    return function->paramCount;
}

static size_t makeConstant(Compiler* compiler, Value value)
{
    return pushValue(&compiler->module->constants, value) - 1;
}

static CallArea openCallArea(Compiler* compiler)
{
    CallArea call = { .resultRegister = compiler->registerCount };

    allocateRegister(compiler);
    allocateRegister(compiler);
    call.frameRegister = compiler->registerCount;

    return call;
}

static void allocateCallAreaRegister(Compiler* compiler)
{
    emitLdi(compiler, 0);
}

static Operand closeCallArea(Compiler* compiler, FunctionObject* function,
    uint16_t functionPosition, CallArea call, bool discard)
{
    int end = call.frameRegister + getCallAreaCount(function);

    while (compiler->registerCount < end) {
        allocateCallAreaRegister(compiler);
    }

    emitCallInstruction(compiler, (uint8_t)call.frameRegister, functionPosition);

    if (!discard && function->returnCount) {
        compiler->registerCount = call.resultRegister + 1;
        
        return makeOperand(call.resultRegister, true);
    } else {
        compiler->registerCount = call.resultRegister;

        return noOperand();
    }
}

static int getLocalPosition(Compiler* compiler, ASTNode* ast)
{
    if (isParameter(ast)) {
        return ast->parameter.position;
    }
    
    return compiler->frameBaseCount + ast->variableDefinition.position;
}

static Operand loadGlobalWithStoreForwarding(Compiler* compiler, int position)
{
    PreviousInstruction previous;
    uint16_t storedPosition;

    if (!getPreviousInstruction(compiler, &previous)) {
        return makeOperand(emitLdg(compiler, position), true);
    }

    if (previous.opcode == OP_STG) {
        storedPosition = previous.b << 8 | previous.c;
    } else if (previous.opcode == OP_LDI_STG) {
        storedPosition = previous.c;
    } else {
        return makeOperand(emitLdg(compiler, position), true);
    }

    if (storedPosition != position) {
        return makeOperand(emitLdg(compiler, position), true);
    }

    return makeOperand(previous.a, false);
}

static Operand loadGlobalVariable(Compiler* compiler, ASTNode* ast)
{
    int position = ast->variableDefinition.position;

    if (isCompilingTopLevel(compiler)) {
        return makeOperand(position, false);
    }

    return loadGlobalWithStoreForwarding(compiler, position);
}

static Operand loadLocalVariable(Compiler* compiler, ASTNode* ast)
{
    int position = getLocalPosition(compiler, ast);

    return makeOperand(position, false);
}

static Operand loadVariable(Compiler* compiler, ASTNode* ast)
{
    if (isTopLevelScope(ast->variableDefinition.scope)) {
        return loadGlobalVariable(compiler, ast);
    }

    return loadLocalVariable(compiler, ast);
}

static bool fuseLdiStore(Compiler* compiler, Operand value, int position)
{
    PreviousInstruction previous;

    if (!value.temporary || position > UINT8_MAX
        || !getPreviousInstruction(compiler, &previous)) {
        return false;
    }

    if (previous.opcode != OP_LDI || previous.a != value.reg) {
        return false;
    }

    int16_t imm = (int16_t)(previous.b << 8 | previous.c);

    if (isLargerThan8BitSigned(imm)) {
        return false;
    }

    setByteAt(previous.code, previous.start, OP_LDI_STG);
    setByteAt(previous.code, previous.start + 2, imm);
    setByteAt(previous.code, previous.start + 3, position);

    return true;
}

static void storeGlobalVariable(Compiler* compiler, ASTNode* ast, Operand value)
{
    int position = ast->variableDefinition.position;

    if (fuseLdiStore(compiler, value, position)) {
        return;
    }

    emitInstruction(compiler, OP_STG, value.reg, position >> 8, position);
}

static bool retargetTemporary(Compiler* compiler, Operand value, int dst)
{
    PreviousInstruction previous;

    if (!value.temporary
        || !getPreviousInstruction(compiler, &previous)
        || previous.a != value.reg) {
        return false;
    }

    if (!(getOpcodeFlags(previous.opcode) & OP_FLAG_WRITES_A)) {
        return false;
    }

    setByteAt(previous.code, previous.start + OPERAND_A_OFFSET, dst);

    return true;
}

static void storeLocalVariable(Compiler* compiler, ASTNode* ast, Operand value)
{
    int position = getLocalPosition(compiler, ast);

    if (!retargetTemporary(compiler, value, position)) {
        emitMov(compiler, position, value.reg);
    }
}

static void storeVariable(Compiler* compiler, ASTNode* ast, Operand value)
{
    if (isTopLevelScope(ast->variableDefinition.scope)) {
        storeGlobalVariable(compiler, ast, value);
    } else {
        storeLocalVariable(compiler, ast, value);
    }

    releaseOperand(compiler, value);
}

static Operand compileNumber(Compiler* compiler, ASTNode* ast)
{
    if (isLargerThan16BitSigned(ast->integerLiteral.value)) {
        size_t position = makeConstant(compiler, INT_VALUE(ast->integerLiteral.value));
        return makeOperand(emitLdc(compiler, position), true);
    }

    return makeOperand(emitLdi(compiler, ast->integerLiteral.value), true);
}

static bool isDirectVariable(Compiler* compiler, ASTNode* ast)
{
    if (!isVariable(ast)) {
        return false;
    }

    ASTNode* symbol = ast->variable.symbol;

    return !isTopLevelScope(symbol->variableDefinition.scope)
        || isCompilingTopLevel(compiler);
}

static Operand emitBinaryOperands(Compiler* compiler, Opcode opcode,
    Operand left, Operand right)
{
    int dst;

    if (left.temporary) {
        dst = left.reg;
        releaseOperand(compiler, right);
    } else if (right.temporary) {
        dst = right.reg;
    } else {
        dst = allocateRegister(compiler);
    }

    emitInstruction(compiler, opcode, dst, left.reg, right.reg);
    
    return makeOperand(dst, true);
}

static Operand compileBinary(Compiler* compiler, ASTNode* ast)
{
    Operand left = compileExpression(compiler, ast->binary.leftExpr, false);

    if (!left.temporary && !isDirectVariable(compiler, ast->binary.rightExpr)) {
        left = materializeOperand(compiler, left);
    }

    Operand right = compileExpression(compiler, ast->binary.rightExpr, false);

    switch (ast->binary.operator.type) {
        case TOKEN_PLUS:
            return emitBinaryOperands(compiler, OP_ADD, left, right);
        case TOKEN_MINUS:
            return emitBinaryOperands(compiler, OP_SUB, left, right);
        case TOKEN_STAR:
            return emitBinaryOperands(compiler, OP_MUL, left, right);
        case TOKEN_SLASH:
        case TOKEN_FLOOR:
            return emitBinaryOperands(compiler, OP_DIV, left, right);
        case TOKEN_PERCENT:
            return emitBinaryOperands(compiler, OP_REM, left, right);
        case TOKEN_POWER:
            return emitBinaryOperands(compiler, OP_POW, left, right);
        case TOKEN_AMPERSAND:
            return emitBinaryOperands(compiler, OP_BAND, left, right);
        case TOKEN_PIPE:
            return emitBinaryOperands(compiler, OP_BOR, left, right);
        case TOKEN_CIRCUMFLEX:
            return emitBinaryOperands(compiler, OP_BXOR, left, right);
        case TOKEN_LSHIFT:
            return emitBinaryOperands(compiler, OP_LSL, left, right);
        case TOKEN_RSHIFT:
            return emitBinaryOperands(compiler, OP_LSR, left, right);
        default:
            return noOperand();
    }
}

static Operand emitUnaryOperand(Compiler* compiler, Opcode opcode, Operand operand)
{
    if (operand.temporary) {
        emitInstruction(compiler, opcode, operand.reg, operand.reg, 0);

        return operand;
    }

    int dst = allocateRegister(compiler);

    emitInstruction(compiler, opcode, dst, operand.reg, 0);

    return makeOperand(dst, true);
}

static Operand compilePrefix(Compiler* compiler, ASTNode* ast)
{
    Operand operand = compileExpression(compiler, ast->prefix.expr, false);

    switch (ast->prefix.operator.type) {
        case TOKEN_EXCLAMATION:
            return emitUnaryOperand(compiler, OP_NOT, operand);
        case TOKEN_TILDE:
            return emitUnaryOperand(compiler, OP_BNOT, operand);
        case TOKEN_MINUS:
            return emitUnaryOperand(compiler, OP_NEG, operand);
        default:
            return noOperand();
    }
}

static Operand compileVariable(Compiler* compiler, ASTNode* ast)
{
    return loadVariable(compiler, ast->variable.symbol);
}

static void compileCompoundAssignment(Compiler* compiler, ASTNode* ast, Opcode opcode)
{
    Operand left = loadVariable(compiler, ast->assignment.symbol);

    if (!left.temporary && !isDirectVariable(compiler, ast->assignment.expr)) {
        left = materializeOperand(compiler, left);
    }

    Operand right = compileExpression(compiler, ast->assignment.expr, false);
    Operand result = emitBinaryOperands(compiler, opcode, left, right);

    storeVariable(compiler, ast->assignment.symbol, result);
}

static void compileSimpleAssignment(Compiler* compiler, ASTNode* ast)
{
    Operand value = compileExpression(compiler, ast->assignment.expr, false);

    storeVariable(compiler, ast->assignment.symbol, value);
}

static void compileAssignment(Compiler* compiler, ASTNode* ast)
{
    switch (ast->assignment.operator.type) {
        case TOKEN_PLUS_EQUAL:
            return compileCompoundAssignment(compiler, ast, OP_ADD);
        case TOKEN_MINUS_EQUAL:
            return compileCompoundAssignment(compiler, ast, OP_SUB);
        case TOKEN_STAR_EQUAL:
            return compileCompoundAssignment(compiler, ast, OP_MUL);
        case TOKEN_FLOOR_EQUAL:
        case TOKEN_SLASH_EQUAL:
            return compileCompoundAssignment(compiler, ast, OP_DIV);
        case TOKEN_PERCENT_EQUAL:
            return compileCompoundAssignment(compiler, ast, OP_REM);
        case TOKEN_POWER_EQUAL:
            return compileCompoundAssignment(compiler, ast, OP_POW);
        case TOKEN_EQUAL:
            return compileSimpleAssignment(compiler, ast);
        default:
            return;
    }
}

static void compileArguments(Compiler* compiler, Vector* args)
{
    size_t count = countVector(args);

    for (size_t i = 0; i < count; i++) {
        int argumentRegister = compiler->registerCount;
        Operand argument = compileExpression(compiler, args->data[i], false);

        if (!argument.temporary) {
            allocateRegister(compiler);
            emitMov(compiler, argumentRegister, argument.reg);
        }
    }
}

static uint16_t makeFunctionPosition(size_t position)
{
    if (position > UINT16_MAX) {
        functionPositionOverflowError();
    }

    return (uint16_t)position;
}

static FunctionObject* createBuiltinFunctionObject(BuiltinId id)
{
    FunctionObject* function = createFunctionObject();
    function->type = FUNCTION_BUILTIN;
    function->entry = builtins[id].entry;
    function->paramCount = builtins[id].paramCount;
    function->returnCount = 1;

    if (builtins[id].typeId == TOKEN_VOID) {
        function->returnCount = 0;
    }

    function->maxStackCount = 2 + getCallAreaCount(function);

    return function;
}

static FunctionObject* createDefinedFunctionObject(ASTNode* ast)
{
    ASTNode* body = ast->functionDefinition.body;
    FunctionObject* function = createFunctionObject();
    function->paramCount = countVector(&ast->functionDefinition.params);
    function->returnCount = 1;

    if (ast->functionDefinition.typeId == TOKEN_VOID) {
        function->returnCount = 0;
    }

    function->localCount = body->compound.scope->localCount;
    int base = getCallAreaCount(function);
    function->maxStackCount = base + 2;

    return function;
}

static uint16_t getBuiltinFunctionPosition(Compiler* compiler, BuiltinId id)
{
    size_t functionCount = countVector(&compiler->module->functions);

    for (size_t i = 0; i < functionCount; i++) {
        FunctionObject* function = getVectorAt(&compiler->module->functions, i);

        if (function->type == FUNCTION_BUILTIN && function->entry == builtins[id].entry) {
            return makeFunctionPosition(i);
        }
    }

    uint16_t position = makeFunctionPosition(functionCount);
    FunctionObject* function = createBuiltinFunctionObject(id);

    pushVectorItem(&compiler->module->functions, function);
    pushVectorItem(&compiler->functionReferences, NULL);

    return position;
}

static uint16_t getFunctionPosition(Compiler* compiler, ASTNode* ast)
{
    size_t functionCount = countVector(&compiler->functionReferences);
    
    for (size_t i = 0; i < functionCount; i++) {
        if (ast == compiler->functionReferences.data[i]) {
            return makeFunctionPosition(i);
        }
    }

    functionNotFoundError(ast);

    return 0;
}

static Operand compileCall(Compiler* compiler, FunctionObject* function,
    uint16_t functionPosition, Vector* args, bool discard)
{
    if (function->type == FUNCTION_BUILTIN && !function->returnCount
        && function->paramCount == 1) {
        CallArea call = openCallArea(compiler);
        Operand argument = compileExpression(compiler, args->data[0], false);

        emitCallInstruction(compiler, (uint8_t)argument.reg, functionPosition);
        releaseOperand(compiler, argument);
        compiler->registerCount = call.resultRegister;

        return noOperand();
    }

    CallArea call = openCallArea(compiler);

    compileArguments(compiler, args);
    return closeCallArea(compiler, function, functionPosition, call, discard);
}

static void compileCallWithArgument(Compiler* compiler, FunctionObject* function,
    uint16_t functionPosition, Operand argument, bool discard)
{
    if (function->type == FUNCTION_BUILTIN && !function->returnCount) {
        emitCallInstruction(compiler, (uint8_t)argument.reg, functionPosition);
        releaseOperand(compiler, argument);

        return;
    }

    CallArea call = openCallArea(compiler);
    int callArgumentRegister = compiler->registerCount;

    allocateRegister(compiler);
    emitMov(compiler, callArgumentRegister, argument.reg);
    closeCallArea(compiler, function, functionPosition, call, discard);
    releaseOperand(compiler, argument);
}

static Operand compileBuiltinCall(Compiler* compiler, ASTNode* ast, bool discard)
{
    uint16_t position = getBuiltinFunctionPosition(compiler, ast->builtinCall.id);
    FunctionObject* function = getVectorAt(&compiler->module->functions, position);

    return compileCall(compiler, function, position, &ast->builtinCall.args, discard);
}

static Operand compileFunctionCall(Compiler* compiler, ASTNode* ast, bool discard)
{
    uint16_t position = getFunctionPosition(compiler, ast->functionCall.symbol);
    FunctionObject* function = getVectorAt(&compiler->module->functions, position);

    return compileCall(compiler, function, position, &ast->functionCall.args, discard);
}

static void compileFunctionDefinition(Compiler* compiler, ASTNode* ast)
{
    int previousFrameBaseCount = compiler->frameBaseCount;
    int previousRegisterCount = compiler->registerCount;
    FunctionObject* previousFunction = compiler->function;
    FunctionObject* function = createDefinedFunctionObject(ast);
    ASTNode* body = ast->functionDefinition.body;
    
    compiler->frameBaseCount = getCallAreaCount(function);
    compiler->registerCount = compiler->frameBaseCount;
    compiler->function = function;
    
    pushVectorItem(&compiler->module->functions, function);
    pushVectorItem(&compiler->functionReferences, ast);
    compileBlocklevelStatements(compiler, &body->compound.statements);

    size_t statementCount = countVector(&body->compound.statements);
    ASTNode* last = NULL;

    if (statementCount > 0) {
        last = getVectorAt(&body->compound.statements, statementCount - 1);
    }

    if (!last || last->type != AST_RETURN) {
        compiler->registerCount = compiler->frameBaseCount;
        emitRet(compiler);
    }
    
    compiler->function = previousFunction;
    compiler->frameBaseCount = previousFrameBaseCount;
    compiler->registerCount = previousRegisterCount;
}

static void compileReturnStatement(Compiler* compiler, ASTNode* ast)
{
    if (isNone(ast->returnStatement.expr)) {
        compiler->registerCount = compiler->frameBaseCount;
        
        return emitRet(compiler);
    }

    Operand value = compileExpression(compiler, ast->returnStatement.expr, false);

    emitRetv(compiler, value.reg);
    releaseOperand(compiler, value);
    compiler->registerCount = compiler->frameBaseCount;
}

static void compileVariableDefinition(Compiler* compiler, ASTNode* ast)
{
    Operand value;

    if (isNone(ast->variableDefinition.expr)) {
        value = makeOperand(emitLdi(compiler, 0), true);
    } else {
        value = compileExpression(compiler, ast->variableDefinition.expr, false);
    }

    if (!value.temporary) {
        int reg = allocateRegister(compiler);

        emitMov(compiler, reg, value.reg);
    }
}

static Operand compileExpression(Compiler* compiler, ASTNode* ast, bool discard)
{
    Operand result;

    switch (ast->type) {
        case AST_BINARY:
            result = compileBinary(compiler, ast);
            break;
        case AST_BUILTIN_CALL:
            return compileBuiltinCall(compiler, ast, discard);
        case AST_FUNCTION_CALL:
            return compileFunctionCall(compiler, ast, discard);
        case AST_INTEGER:
            result = compileNumber(compiler, ast);
            break;
        case AST_PREFIX:
            result = compilePrefix(compiler, ast);
            break;
        case AST_VARIABLE:
            result = compileVariable(compiler, ast);
            break;
        default:
            return noOperand();
    }

    if (discard) {
        releaseOperand(compiler, result);

        return noOperand();
    }

    return result;
}

static Operand compileStatement(Compiler* compiler, ASTNode* ast, bool discard)
{
    switch (ast->type) {
        case AST_ASSIGNMENT:
            compileAssignment(compiler, ast);
            return noOperand();
        case AST_BUILTIN_CALL:
            return compileBuiltinCall(compiler, ast, discard);
        case AST_FUNCTION_CALL:
            return compileFunctionCall(compiler, ast, discard);
        case AST_FUNCTION_DEFINITION:
            compileFunctionDefinition(compiler, ast);
            return noOperand();
        case AST_RETURN:
            compileReturnStatement(compiler, ast);
            return noOperand();
        case AST_VARIABLE_DEFINITION:
            compileVariableDefinition(compiler, ast);
            return noOperand();
        default:
            return compileExpression(compiler, ast, discard);
    }
}

static void compileBlocklevelStatements(Compiler* compiler, Vector* nodes)
{
    size_t count = countVector(nodes);

    for (size_t i = 0; i < count; i++) {
        compileStatement(compiler, nodes->data[i], true);
    }
}

static void compileTopLevelStatements(Compiler* compiler, Vector* nodes)
{
    size_t count = countVector(nodes);

    while (compiler->statementIndex < count) {
        compileStatement(compiler, nodes->data[compiler->statementIndex], true);
        compiler->statementIndex++;
    }
}

static void compileReplStatement(Compiler* compiler, ASTNode* ast, bool isLast)
{
    bool display = isLast && isExpressionStatement(ast) && getTypeId(ast) != TOKEN_VOID;
    Operand value = compileStatement(compiler, ast, !display);

    if (!display) {
        return;
    }

    uint16_t position = getBuiltinFunctionPosition(compiler, BUILTIN_PRINT);
    FunctionObject* function = getVectorAt(&compiler->module->functions, position);

    compileCallWithArgument(compiler, function, position, value, true);
}

static void compileReplStatements(Compiler* compiler, Vector* nodes)
{
    size_t count = countVector(nodes);

    while (compiler->statementIndex < count) {
        ASTNode* ast = nodes->data[compiler->statementIndex++];
        bool isLast = compiler->statementIndex == count;

        compileReplStatement(compiler, ast, isLast);
    }
}

void initCompiler(Compiler* compiler, ModuleObject* module)
{
    initVector(&compiler->functionReferences);

    for (size_t i = 0; i < countVector(&module->functions); i++) {
        pushVectorItem(&compiler->functionReferences, NULL);
    }

    ASTNode* ast = createASTNode(AST_COMPOUND);
    ast->compound.scope = createScope(NULL);

    initParser(&compiler->parser, ast);
    initAnalyzer(&compiler->analyzer, ast);

    compiler->module = module;
    compiler->function = getVectorAt(&module->functions, 0);
    compiler->ast = ast;
    compiler->statementIndex = 0;
    compiler->registerCount = 0;
    compiler->frameBaseCount = 0;
}

void freeCompiler(Compiler* compiler)
{
    freeVector(&compiler->functionReferences);
    freeASTNode(compiler->ast);
}

static bool compileSource(Compiler* compiler, char* source, CompileStatements compileStatements)
{
    if (!compiler->module) {
        return false;
    }

    size_t start = countVector(&compiler->ast->compound.statements);

    if (!parse(&compiler->parser, source)) {
        return false;
    }

    analyze(&compiler->analyzer, start);
    optimize(compiler->ast, start);
    clearCodeObject(currentCodeObject(compiler));
    compileStatements(compiler, &compiler->ast->compound.statements);
    emitHlt(compiler);

    return true;
}

bool compile(Compiler* compiler, char* source)
{
    return compileSource(compiler, source, compileTopLevelStatements);
}

bool compileRepl(Compiler* compiler, char* source)
{
    return compileSource(compiler, source, compileReplStatements);
}
