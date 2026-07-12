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

static void compileExpression(Compiler* compiler, ASTNode* ast, bool discard);
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

static int peekRegister(Compiler* compiler)
{
    int reg = releaseRegister(compiler);
    allocateRegister(compiler);

    return reg;
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
    emitInstruction(compiler, OP_MOV, dst, src, 0);
}

static int emitLdc(Compiler* compiler, uint16_t imm)
{
    int reg = allocateRegister(compiler);

    emitInstruction(compiler, OP_LDC, reg, imm >> 8, imm);

    return reg;
}

static void emitLdi(Compiler* compiler, int16_t imm)
{
    int reg = allocateRegister(compiler);

    emitInstruction(compiler, OP_LDI, reg, imm >> 8, imm);
}

static void emitReg(Compiler* compiler)
{
    int reg = releaseRegister(compiler);

    emitInstruction(compiler, OP_REG, reg, 0, 0);
}

static void emitLdg(Compiler* compiler, uint16_t imm)
{
    int reg = allocateRegister(compiler);

    emitInstruction(compiler, OP_LDG, reg, imm >> 8, imm);
}

static void emitStg(Compiler* compiler, uint16_t imm)
{
    int reg = releaseRegister(compiler);

    emitInstruction(compiler, OP_STG, reg, imm >> 8, imm);
}

static void emitLdl(Compiler* compiler, uint8_t src)
{
    int dst = allocateRegister(compiler);

    emitMov(compiler, dst, src);
}

static void emitStl(Compiler* compiler, uint8_t dst)
{
    int src = releaseRegister(compiler);

    emitMov(compiler, dst, src);
}

static void emitAdd(Compiler* compiler)
{
    int right = releaseRegister(compiler);
    int left = right - 1;

    emitInstruction(compiler, OP_ADD, left, left, right);
}

static void emitSub(Compiler* compiler)
{
    int right = releaseRegister(compiler);
    int left = right - 1;

    emitInstruction(compiler, OP_SUB, left, left, right);
}

static void emitMul(Compiler* compiler)
{
    int right = releaseRegister(compiler);
    int left = right - 1;

    emitInstruction(compiler, OP_MUL, left, left, right);
}

static void emitDiv(Compiler* compiler)
{
    int right = releaseRegister(compiler);
    int left = right - 1;

    emitInstruction(compiler, OP_DIV, left, left, right);
}

static void emitRem(Compiler* compiler)
{
    int right = releaseRegister(compiler);
    int left = right - 1;

    emitInstruction(compiler, OP_REM, left, left, right);
}

static void emitPow(Compiler* compiler)
{
    int right = releaseRegister(compiler);
    int left = right - 1;

    emitInstruction(compiler, OP_POW, left, left, right);
}

static void emitBand(Compiler* compiler)
{
    int right = releaseRegister(compiler);
    int left = right - 1;

    emitInstruction(compiler, OP_BAND, left, left, right);
}

static void emitBor(Compiler* compiler)
{
    int right = releaseRegister(compiler);
    int left = right - 1;

    emitInstruction(compiler, OP_BOR, left, left, right);
}

static void emitBxor(Compiler* compiler)
{
    int right = releaseRegister(compiler);
    int left = right - 1;

    emitInstruction(compiler, OP_BXOR, left, left, right);
}

static void emitBnot(Compiler* compiler)
{
    int reg = peekRegister(compiler);

    emitInstruction(compiler, OP_BNOT, reg, reg, 0);
}

static void emitLsl(Compiler* compiler)
{
    int right = releaseRegister(compiler);
    int left = right - 1;

    emitInstruction(compiler, OP_LSL, left, left, right);
}

static void emitLsr(Compiler* compiler)
{
    int right = releaseRegister(compiler);
    int left = right - 1;

    emitInstruction(compiler, OP_LSR, left, left, right);
}

static void emitNeg(Compiler* compiler)
{
    int reg = peekRegister(compiler);

    emitInstruction(compiler, OP_NEG, reg, reg, 0);
}

static void emitNot(Compiler* compiler)
{
    int reg = peekRegister(compiler);

    emitInstruction(compiler, OP_NOT, reg, reg, 0);
}

static void emitCallInstruction(Compiler* compiler, uint8_t functionRegister)
{
    emitInstruction(compiler, OP_CALL, functionRegister, 0, 0);
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
    int functionRegister;
} CallArea;

static CallArea openCallArea(Compiler* compiler, FunctionObject* function)
{
    CallArea call = { .resultRegister = compiler->registerCount };

    allocateRegister(compiler);
    size_t constant = makeConstant(compiler, POINTER_VALUE(function));
    call.functionRegister = emitLdc(compiler, (uint16_t)constant);

    return call;
}

static void closeCallArea(Compiler* compiler, FunctionObject* function,
    CallArea call, bool discard)
{
    int end = call.functionRegister + 1 + getCallAreaCount(function);

    while (compiler->registerCount < end) {
        emitLdi(compiler, 0);
    }

    emitCallInstruction(compiler, (uint8_t)call.functionRegister);

    if (!discard && function->returnCount) {
        emitMov(compiler, call.resultRegister, call.functionRegister + 1);
        compiler->registerCount = call.resultRegister + 1;
    } else {
        compiler->registerCount = call.resultRegister;
    }
}

static int getLocalPosition(Compiler* compiler, ASTNode* ast)
{
    if (isParameter(ast)) {
        return ast->parameter.position;
    }
    
    return compiler->frameBaseCount + ast->variableDefinition.position;
}

static void loadGlobalVariable(Compiler* compiler, ASTNode* ast)
{
    int position = getLocalPosition(compiler, ast);

    emitLdg(compiler, position);
}

static void loadLocalVariable(Compiler* compiler, ASTNode* ast)
{
    int position = getLocalPosition(compiler, ast);

    emitLdl(compiler, position);
}

static void loadVariable(Compiler* compiler, ASTNode* ast)
{
    if (isTopLevelScope(ast->variableDefinition.scope)) {
        loadGlobalVariable(compiler, ast);
    } else {
        loadLocalVariable(compiler, ast);
    }
}

static void storeGlobalVariable(Compiler* compiler, ASTNode* ast)
{
    int position = getLocalPosition(compiler, ast);

    emitStg(compiler, position);
}

static void storeLocalVariable(Compiler* compiler, ASTNode* ast)
{
    int position = getLocalPosition(compiler, ast);

    emitStl(compiler, position);
}

static void storeVariable(Compiler* compiler, ASTNode* ast)
{
    if (isTopLevelScope(ast->variableDefinition.scope)) {
        storeGlobalVariable(compiler, ast);
    } else {
        storeLocalVariable(compiler, ast);
    }
}

static void compileNumber(Compiler* compiler, ASTNode* ast)
{
    if (isLargerThan16BitSigned(ast->integerLiteral.value)) {
        size_t position = makeConstant(compiler, INT_VALUE(ast->integerLiteral.value));
        emitLdc(compiler, position);
    } else {
        emitLdi(compiler, ast->integerLiteral.value);
    }
}

static void compileBinary(Compiler* compiler, ASTNode* ast)
{
    compileExpression(compiler, ast->binary.leftExpr, false);
    compileExpression(compiler, ast->binary.rightExpr, false);

    switch (ast->binary.operator.type) {
        case TOKEN_PLUS:
            return emitAdd(compiler);
        case TOKEN_MINUS:
            return emitSub(compiler);
        case TOKEN_STAR:
            return emitMul(compiler);
        case TOKEN_SLASH:
        case TOKEN_FLOOR:
            return emitDiv(compiler);
        case TOKEN_PERCENT:
            return emitRem(compiler);
        case TOKEN_POWER:
            return emitPow(compiler);
        case TOKEN_AMPERSAND:
            return emitBand(compiler);
        case TOKEN_PIPE:
            return emitBor(compiler);
        case TOKEN_CIRCUMFLEX:
            return emitBxor(compiler);
        case TOKEN_LSHIFT:
            return emitLsl(compiler);
        case TOKEN_RSHIFT:
            return emitLsr(compiler);
        default:
            return;
    }
}

static void compileBitwiseNOT(Compiler* compiler, ASTNode* ast)
{
    compileExpression(compiler, ast->prefix.expr, false);
    emitBnot(compiler);
}

static void compileLogNot(Compiler* compiler, ASTNode* ast)
{
    compileExpression(compiler, ast->prefix.expr, false);
    emitNot(compiler);
}

static void compileNegate(Compiler* compiler, ASTNode* ast)
{
    compileExpression(compiler, ast->prefix.expr, false);
    emitNeg(compiler);
}

static void compilePrefix(Compiler* compiler, ASTNode* ast)
{
    switch (ast->prefix.operator.type) {
        case TOKEN_EXCLAMATION:
            return compileLogNot(compiler, ast);
        case TOKEN_TILDE:
            return compileBitwiseNOT(compiler, ast);
        case TOKEN_MINUS:
            return compileNegate(compiler, ast);
        default:
            return;
    }
}

static void compileVariable(Compiler* compiler, ASTNode* ast)
{
    loadVariable(compiler, ast->variable.symbol);
}

static void compileAdditionAssignment(Compiler* compiler, ASTNode* ast)
{
    loadVariable(compiler, ast->assignment.symbol);
    compileExpression(compiler, ast->assignment.expr, false);
    emitAdd(compiler);
    storeVariable(compiler, ast->assignment.symbol);
}

static void compileSubtractionAssignment(Compiler* compiler, ASTNode* ast)
{
    loadVariable(compiler, ast->assignment.symbol);
    compileExpression(compiler, ast->assignment.expr, false);
    emitSub(compiler);
    storeVariable(compiler, ast->assignment.symbol);
}

static void compileMultiplicationAssignment(Compiler* compiler, ASTNode* ast)
{
    loadVariable(compiler, ast->assignment.symbol);
    compileExpression(compiler, ast->assignment.expr, false);
    emitMul(compiler);
    storeVariable(compiler, ast->assignment.symbol);
}

static void compileDivisionAssignment(Compiler* compiler, ASTNode* ast)
{
    loadVariable(compiler, ast->assignment.symbol);
    compileExpression(compiler, ast->assignment.expr, false);
    emitDiv(compiler);
    storeVariable(compiler, ast->assignment.symbol);
}

static void compileRemainderAssignment(Compiler* compiler, ASTNode* ast)
{
    loadVariable(compiler, ast->assignment.symbol);
    compileExpression(compiler, ast->assignment.expr, false);
    emitRem(compiler);
    storeVariable(compiler, ast->assignment.symbol);
}

static void compileExponentiationAssignment(Compiler* compiler, ASTNode* ast)
{
    loadVariable(compiler, ast->assignment.symbol);
    compileExpression(compiler, ast->assignment.expr, false);
    emitPow(compiler);
    storeVariable(compiler, ast->assignment.symbol);
}

static void compileSimpleAssignment(Compiler* compiler, ASTNode* ast)
{
    compileExpression(compiler, ast->assignment.expr, false);
    storeVariable(compiler, ast->assignment.symbol);
}

static void compileAssignment(Compiler* compiler, ASTNode* ast)
{
    switch (ast->assignment.operator.type) {
        case TOKEN_PLUS_EQUAL:
            return compileAdditionAssignment(compiler, ast);
        case TOKEN_MINUS_EQUAL:
            return compileSubtractionAssignment(compiler, ast);
        case TOKEN_STAR_EQUAL:
            return compileMultiplicationAssignment(compiler, ast);
        case TOKEN_FLOOR_EQUAL:
        case TOKEN_SLASH_EQUAL:
            return compileDivisionAssignment(compiler, ast);
        case TOKEN_PERCENT_EQUAL:
            return compileRemainderAssignment(compiler, ast);
        case TOKEN_POWER_EQUAL:
            return compileExponentiationAssignment(compiler, ast);
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
        compileExpression(compiler, args->data[i], false);
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

static void compileCall(Compiler* compiler, FunctionObject* function, Vector* args, bool discard)
{
    CallArea call = openCallArea(compiler, function);
    compileArguments(compiler, args);
    closeCallArea(compiler, function, call, discard);
}

static void compileCallWithArgument(Compiler* compiler, FunctionObject* function, int argumentRegister, bool discard)
{
    CallArea call = openCallArea(compiler, function);
    int callArgumentRegister = compiler->registerCount;

    allocateRegister(compiler);
    emitMov(compiler, callArgumentRegister, argumentRegister);
    closeCallArea(compiler, function, call, discard);
    releaseRegister(compiler);
}

static void compileBuiltinCall(Compiler* compiler, ASTNode* ast, bool discard)
{
    uint16_t position = getBuiltinFunctionPosition(compiler, ast->builtinCall.id);
    FunctionObject* function = getVectorAt(&compiler->module->functions, position);

    compileCall(compiler, function, &ast->builtinCall.args, discard);
}

static void compileFunctionCall(Compiler* compiler, ASTNode* ast, bool discard)
{
    uint16_t position = getFunctionPosition(compiler, ast->functionCall.symbol);
    FunctionObject* function = getVectorAt(&compiler->module->functions, position);

    compileCall(compiler, function, &ast->functionCall.args, discard);
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

    compileExpression(compiler, ast->returnStatement.expr, false);
    emitStl(compiler, 0);
    compiler->registerCount = compiler->frameBaseCount;
    emitRetv(compiler);
}

static void compileUninitializedVariableDefinition(Compiler* compiler, ASTNode* ast)
{
    emitLdi(compiler, 0);

    if (isTopLevelScope(ast->variableDefinition.scope)) {
        emitReg(compiler);
    }
}

static void compileVariableDefinition(Compiler* compiler, ASTNode* ast)
{
    if (isNone(ast->variableDefinition.expr)) {
        return compileUninitializedVariableDefinition(compiler, ast);
    }

    compileExpression(compiler, ast->variableDefinition.expr, false);

    if (isTopLevelScope(ast->variableDefinition.scope)) {
        emitReg(compiler);
    }
}

static void compileExpression(Compiler* compiler, ASTNode* ast, bool discard)
{
    switch (ast->type) {
        case AST_BINARY:
            compileBinary(compiler, ast);
            break;
        case AST_BUILTIN_CALL:
            return compileBuiltinCall(compiler, ast, discard);
        case AST_FUNCTION_CALL:
            return compileFunctionCall(compiler, ast, discard);
        case AST_INTEGER:
            compileNumber(compiler, ast);
            break;
        case AST_PREFIX:
            compilePrefix(compiler, ast);
            break;
        case AST_VARIABLE:
            compileVariable(compiler, ast);
            break;
        default:
            return;
    }

    if (discard) {
        releaseRegister(compiler);
    }
}

static void compileStatement(Compiler* compiler, ASTNode* ast, bool discard)
{
    switch (ast->type) {
        case AST_ASSIGNMENT:
            compileAssignment(compiler, ast);
            return;
        case AST_BUILTIN_CALL:
            compileBuiltinCall(compiler, ast, discard);
            return;
        case AST_FUNCTION_CALL:
            compileFunctionCall(compiler, ast, discard);
            return;
        case AST_FUNCTION_DEFINITION:
            compileFunctionDefinition(compiler, ast);
            return;
        case AST_RETURN:
            compileReturnStatement(compiler, ast);
            return;
        case AST_VARIABLE_DEFINITION:
            compileVariableDefinition(compiler, ast);
            return;
        default:
            compileExpression(compiler, ast, discard);
            return;
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

    compileStatement(compiler, ast, !display);

    if (!display) {
        return;
    }

    int valueRegister = peekRegister(compiler);
    uint16_t position = getBuiltinFunctionPosition(compiler, BUILTIN_PRINT);
    FunctionObject* function = getVectorAt(&compiler->module->functions, position);

    compileCallWithArgument(compiler, function, valueRegister, true);
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
