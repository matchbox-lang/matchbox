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

static Operand compileExpression(Compiler* compiler, ASTNode* ast, bool discard);
static void compileBlocklevelStatements(Compiler* compiler, Vector* nodes);
static void compileToplevelStatements(Compiler* compiler, Vector* nodes);

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

static void emitMov(Compiler* compiler, int dst, int src)
{
    if (dst == src) {
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

static int emitLdi(Compiler* compiler, int16_t imm)
{
    int reg = allocateRegister(compiler);

    emitInstruction(compiler, OP_LDI, reg, imm >> 8, imm);
    return reg;
}

static int emitLdg(Compiler* compiler, uint16_t imm)
{
    int reg = allocateRegister(compiler);

    emitInstruction(compiler, OP_LDG, reg, imm >> 8, imm);
    return reg;
}

static void emitCallInstruction(Compiler* compiler, uint8_t frameRegister,
    uint16_t functionPosition)
{
    emitInstruction(compiler, OP_CALL, frameRegister,
        functionPosition >> 8, functionPosition);
}

static void emitRet(Compiler* compiler)
{
    emitInstruction(compiler, OP_RET, 0, 0, 0);
}

static void emitRetv(Compiler* compiler)
{
    emitInstruction(compiler, OP_RETV, 0, 0, 0);
}

static int getCallAreaCount(FunctionObject* function)
{
    int returnSlots = function->returnCount;

    if (!returnSlots) {
        returnSlots = 1;
    }

    if (function->paramCount > returnSlots) {
        return function->paramCount;
    }

    return returnSlots;
}

static size_t makeConstant(Compiler* compiler, Value value)
{
    return pushValue(&compiler->module->constants, value) - 1;
}

typedef struct CallArea
{
    int resultRegister;
    int frameRegister;
} CallArea;

static CallArea openCallArea(Compiler* compiler)
{
    CallArea call = { .resultRegister = compiler->registerCount };

    allocateRegister(compiler);
    allocateRegister(compiler);
    call.frameRegister = compiler->registerCount;

    return call;
}

static Operand closeCallArea(Compiler* compiler, FunctionObject* function,
    uint16_t functionPosition, CallArea call, bool discard)
{
    int end = call.frameRegister + getCallAreaCount(function);

    while (compiler->registerCount < end) {
        emitLdi(compiler, 0);
    }

    emitCallInstruction(compiler, (uint8_t)call.frameRegister, functionPosition);

    if (!discard && function->returnCount) {
        emitMov(compiler, call.resultRegister, call.frameRegister);
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

static Operand loadGlobalVariable(Compiler* compiler, ASTNode* ast)
{
    int position = ast->variableDefinition.position;

    if (!compiler->frameBaseCount) {
        return makeOperand(position, false);
    }

    return makeOperand(emitLdg(compiler, position), true);
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

static void storeGlobalVariable(Compiler* compiler, ASTNode* ast, Operand value)
{
    emitInstruction(compiler, OP_STG, value.reg,
        ast->variableDefinition.position >> 8,
        ast->variableDefinition.position);
}

static void storeLocalVariable(Compiler* compiler, ASTNode* ast, Operand value)
{
    int position = getLocalPosition(compiler, ast);
    emitMov(compiler, position, value.reg);
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
        || !compiler->frameBaseCount;
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

static void compileCompoundAssignment(Compiler* compiler, ASTNode* ast,
    Opcode opcode)
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
    function->builtinId = id;
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

        if (function->type == FUNCTION_BUILTIN && function->builtinId == id) {
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
    CallArea call = openCallArea(compiler);
    compileArguments(compiler, args);
    return closeCallArea(compiler, function, functionPosition, call, discard);
}

static void compileCallWithArgument(Compiler* compiler, FunctionObject* function,
    uint16_t functionPosition, Operand argument, bool discard)
{
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
    FunctionObject* previousFunction = compiler->function;
    int previousFrameBaseCount = compiler->frameBaseCount;
    int previousRegisterCount = compiler->registerCount;
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
    emitMov(compiler, 0, value.reg);
    releaseOperand(compiler, value);
    compiler->registerCount = compiler->frameBaseCount;
    emitRetv(compiler);
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

static void compileToplevelStatements(Compiler* compiler, Vector* nodes)
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
    return compileSource(compiler, source, compileToplevelStatements);
}

bool compileRepl(Compiler* compiler, char* source)
{
    return compileSource(compiler, source, compileReplStatements);
}
