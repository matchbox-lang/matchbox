#include "compiler.h"
#include "ast.h"
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

typedef void (*CompileStatements)(Compiler* compiler, Vector* nodes);

static void expression(Compiler* compiler, AST* ast, bool discard);
static void blocklevelStatements(Compiler* compiler, Vector* nodes);
static void toplevelStatements(Compiler* compiler, Vector* nodes);

static CodeObject* currentCodeObject(Compiler* compiler)
{
    return &compiler->function->code;
}

static void incStackCount(Compiler* compiler)
{
    if (++compiler->stackCount > compiler->function->maxStackCount) {
        compiler->function->maxStackCount = compiler->stackCount;
    }
}

static void decStackCount(Compiler* compiler)
{
    compiler->stackCount--;
}

static void write8(Compiler* compiler, uint8_t n)
{
    pushByte(currentCodeObject(compiler), n);
}

static void write16(Compiler* compiler, int16_t n)
{
    pushByte(currentCodeObject(compiler), (n >> 8) & 0xFF);
    pushByte(currentCodeObject(compiler), n & 0xFF);
}

static void emitHlt(Compiler* compiler)
{
    write8(compiler, OP_HLT);
}

static void emitLdc(Compiler* compiler, uint8_t imm)
{
    incStackCount(compiler);
    write8(compiler, OP_LDC);
    write8(compiler, imm);
}

static void emitReg(Compiler* compiler)
{
    decStackCount(compiler);
    write8(compiler, OP_REG);
}

static void emitLdg(Compiler* compiler, uint8_t imm)
{
    incStackCount(compiler);
    write8(compiler, OP_LDG);
    write8(compiler, imm);
}

static void emitStg(Compiler* compiler, uint8_t imm)
{
    decStackCount(compiler);
    write8(compiler, OP_STG);
    write8(compiler, imm);
}

static void emitLdl(Compiler* compiler, int8_t imm)
{
    incStackCount(compiler);

    switch (imm) {
        case 0:
            write8(compiler, OP_LDL_0);
            break;
        case 1:
            write8(compiler, OP_LDL_1);
            break;
        case 2:
            write8(compiler, OP_LDL_2);
            break;
        case 3:
            write8(compiler, OP_LDL_3);
            break;
        default:
            write8(compiler, OP_LDL);
            write8(compiler, imm);
            break;
    }
}

static void emitStl(Compiler* compiler, int8_t imm)
{
    decStackCount(compiler);

    switch (imm) {
        case 0:
            write8(compiler, OP_STL_0);
            break;
        case 1:
            write8(compiler, OP_STL_1);
            break;
        case 2:
            write8(compiler, OP_STL_2);
            break;
        case 3:
            write8(compiler, OP_STL_3);
            break;
        default:
            write8(compiler, OP_STL);
            write8(compiler, imm);
            break;
    }
}

static void emitPushb(Compiler* compiler, int8_t imm)
{
    incStackCount(compiler);

    switch (imm) {
        case 0:
            write8(compiler, OP_PUSH_0);
            break;
        case 1:
            write8(compiler, OP_PUSH_1);
            break;
        case 2:
            write8(compiler, OP_PUSH_2);
            break;
        case 3:
            write8(compiler, OP_PUSH_3);
            break;
        default:
            write8(compiler, OP_PUSHB);
            write8(compiler, imm);
            break;
    }
}

static void emitPushh(Compiler* compiler, int16_t imm)
{
    incStackCount(compiler);
    write8(compiler, OP_PUSHH);
    write16(compiler, imm);
}

static void emitPop(Compiler* compiler)
{
    decStackCount(compiler);
    write8(compiler, OP_POP);
}

static void emitAdd(Compiler* compiler)
{
    decStackCount(compiler);
    write8(compiler, OP_ADD);
}

static void emitSub(Compiler* compiler)
{
    decStackCount(compiler);
    write8(compiler, OP_SUB);
}

static void emitMul(Compiler* compiler)
{
    decStackCount(compiler);
    write8(compiler, OP_MUL);
}

static void emitDiv(Compiler* compiler)
{
    decStackCount(compiler);
    write8(compiler, OP_DIV);
}

static void emitRem(Compiler* compiler)
{
    decStackCount(compiler);
    write8(compiler, OP_REM);
}

static void emitPow(Compiler* compiler)
{
    decStackCount(compiler);
    write8(compiler, OP_POW);
}

static void emitBand(Compiler* compiler)
{
    decStackCount(compiler);
    write8(compiler, OP_BAND);
}

static void emitBor(Compiler* compiler)
{
    decStackCount(compiler);
    write8(compiler, OP_BOR);
}

static void emitBxor(Compiler* compiler)
{
    decStackCount(compiler);
    write8(compiler, OP_BXOR);
}

static void emitBnot(Compiler* compiler)
{
    decStackCount(compiler);
    write8(compiler, OP_BNOT);
}

static void emitLsl(Compiler* compiler)
{
    decStackCount(compiler);
    write8(compiler, OP_LSL);
}

static void emitLsr(Compiler* compiler)
{
    decStackCount(compiler);
    write8(compiler, OP_LSR);
}

static void emitNeg(Compiler* compiler)
{
    write8(compiler, OP_NEG);
}

static void emitNot(Compiler* compiler)
{
    write8(compiler, OP_NOT);
}

static void emitCallBuiltin(Compiler* compiler, uint8_t imm)
{
    write8(compiler, OP_CALL_BUILTIN);
    write8(compiler, imm);
}

static void emitCall(Compiler* compiler, uint16_t imm)
{
    write8(compiler, OP_CALL);
    write16(compiler, imm);
}

static void emitRet(Compiler* compiler)
{
    incStackCount(compiler);
    write8(compiler, OP_RET);
}

static void emitRetv(Compiler* compiler)
{
    write8(compiler, OP_RETV);
}

static size_t makeConstant(Compiler* compiler, Value value)
{
    return pushValue(&compiler->module->constants, value) - 1;
}

static int getLocalPosition(AST* ast)
{
    if (isParameter(ast)) {
        return -(ast->parameter.position + 4);
    }
    
    return ast->variableDefinition.position;
}

static void loadGlobalVariable(Compiler* compiler, AST* ast)
{
    int position = getLocalPosition(ast);

    emitLdg(compiler, position);
}

static void loadLocalVariable(Compiler* compiler, AST* ast)
{
    int position = getLocalPosition(ast);

    emitLdl(compiler, position);
}

static void loadVariable(Compiler* compiler, AST* ast)
{
    if (isTopLevel(ast->variableDefinition.scope)) {
        loadGlobalVariable(compiler, ast);
    } else {
        loadLocalVariable(compiler, ast);
    }
}

static void storeGlobalVariable(Compiler* compiler, AST* ast)
{
    int position = getLocalPosition(ast);

    emitStg(compiler, position);
}

static void storeLocalVariable(Compiler* compiler, AST* ast)
{
    int position = getLocalPosition(ast);

    emitStl(compiler, position);
}

static void storeVariable(Compiler* compiler, AST* ast)
{
    if (isTopLevel(ast->variableDefinition.scope)) {
        storeGlobalVariable(compiler, ast);
    } else {
        storeLocalVariable(compiler, ast);
    }
}

static void number(Compiler* compiler, AST* ast)
{
    if (isLargerThan16BitSigned(ast->intValue)) {
        size_t position = makeConstant(compiler, INT_VALUE(ast->intValue));
        emitLdc(compiler, position);
    } else if (isLargerThan8BitSigned(ast->intValue)) {
        emitPushh(compiler, ast->intValue);
    } else {
        emitPushb(compiler, ast->intValue);
    }
}

static void binary(Compiler* compiler, AST* ast)
{
    expression(compiler, ast->binary.leftExpr, false);
    expression(compiler, ast->binary.rightExpr, false);

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

static void bitNot(Compiler* compiler, AST* ast)
{
    expression(compiler, ast->prefix.expr, false);
    emitBnot(compiler);
}

static void logNot(Compiler* compiler, AST* ast)
{
    expression(compiler, ast->prefix.expr, false);
    emitNot(compiler);
}

static void negate(Compiler* compiler, AST* ast)
{
    expression(compiler, ast->prefix.expr, false);
    emitNeg(compiler);
}

static void prefix(Compiler* compiler, AST* ast)
{
    switch (ast->prefix.operator.type) {
        case TOKEN_EXCLAMATION:
            return logNot(compiler, ast);
        case TOKEN_TILDE:
            return bitNot(compiler, ast);
        case TOKEN_MINUS:
            return negate(compiler, ast);
        default:
            return;
    }
}

static void variable(Compiler* compiler, AST* ast)
{
    loadVariable(compiler, ast->variable.symbol);
}

static void additionAssignment(Compiler* compiler, AST* ast)
{
    loadVariable(compiler, ast->assignment.symbol);
    expression(compiler, ast->assignment.expr, false);
    emitAdd(compiler);
    storeVariable(compiler, ast->assignment.symbol);
}

static void subtractionAssignment(Compiler* compiler, AST* ast)
{
    loadVariable(compiler, ast->assignment.symbol);
    expression(compiler, ast->assignment.expr, false);
    emitSub(compiler);
    storeVariable(compiler, ast->assignment.symbol);
}

static void muliplicationAssignment(Compiler* compiler, AST* ast)
{
    loadVariable(compiler, ast->assignment.symbol);
    expression(compiler, ast->assignment.expr, false);
    emitMul(compiler);
    storeVariable(compiler, ast->assignment.symbol);
}

static void divisionAssignment(Compiler* compiler, AST* ast)
{
    loadVariable(compiler, ast->assignment.symbol);
    expression(compiler, ast->assignment.expr, false);
    emitDiv(compiler);
    storeVariable(compiler, ast->assignment.symbol);
}

static void remainderAssignment(Compiler* compiler, AST* ast)
{
    loadVariable(compiler, ast->assignment.symbol);
    expression(compiler, ast->assignment.expr, false);
    emitRem(compiler);
    storeVariable(compiler, ast->assignment.symbol);
}

static void exponentiationAssignment(Compiler* compiler, AST* ast)
{
    loadVariable(compiler, ast->assignment.symbol);
    expression(compiler, ast->assignment.expr, false);
    emitPow(compiler);
    storeVariable(compiler, ast->assignment.symbol);
}

static void simpleAssignment(Compiler* compiler, AST* ast)
{
    expression(compiler, ast->assignment.expr, false);
    storeVariable(compiler, ast->assignment.symbol);
}

static void assignment(Compiler* compiler, AST* ast)
{
    switch (ast->assignment.operator.type) {
        case TOKEN_PLUS_EQUAL:
            return additionAssignment(compiler, ast);
        case TOKEN_MINUS_EQUAL:
            return subtractionAssignment(compiler, ast);
        case TOKEN_STAR_EQUAL:
            return muliplicationAssignment(compiler, ast);
        case TOKEN_FLOOR_EQUAL:
        case TOKEN_SLASH_EQUAL:
            return divisionAssignment(compiler, ast);
        case TOKEN_PERCENT_EQUAL:
            return remainderAssignment(compiler, ast);
        case TOKEN_POWER_EQUAL:
            return exponentiationAssignment(compiler, ast);
        case TOKEN_EQUAL:
            return simpleAssignment(compiler, ast);
        default:
            return;
    }
}

static void arguments(Compiler* compiler, Vector* args)
{
    size_t count = countVector(args);

    for (size_t i = 0; i < count; i++) {
        expression(compiler, args->data[i], false);
    }
}

static int getFunctionPosition(Compiler* compiler, AST* ast)
{
    size_t functionCount = countVector(&compiler->functionReferences);
    
    for (int i = 0; i < functionCount; i++) {
        if (ast == compiler->functionReferences.data[i]) {
            return i;
        }
    }

    return -1;
}

static void builtinCall(Compiler* compiler, AST* ast, bool discard)
{
    arguments(compiler, &ast->builtinCall.args);
    emitCallBuiltin(compiler, ast->builtinCall.id);

    if (discard) {
        emitPop(compiler);
    }
}

static void functionCall(Compiler* compiler, AST* ast, bool discard)
{
    uint16_t position = getFunctionPosition(compiler, ast->functionCall.symbol);
    arguments(compiler, &ast->functionCall.args);
    emitCall(compiler, position);

    if (discard) {
        emitPop(compiler);
    }
}

static void functionDefinition(Compiler* compiler, AST* ast)
{
    AST* body = ast->functionDefinition.body;
    FunctionObject* previousFunction = compiler->function;
    FunctionObject* function = createFunctionObject();
    function->paramCount = countVector(&ast->functionDefinition.params);
    function->localCount = body->compound.scope->localCount;
    function->maxStackCount = function->localCount + 3;
    
    compiler->stackCount = function->maxStackCount;
    compiler->function = function;
    pushVectorItem(&compiler->module->functions, function);
    pushVectorItem(&compiler->functionReferences, ast);
    blocklevelStatements(compiler, &body->compound.statements);

    size_t statementCount = countVector(&body->compound.statements);
    AST* last = NULL;

    if (statementCount > 0) {
        last = getVectorAt(&body->compound.statements, statementCount - 1);
    }

    if (!last || last->type != AST_RETURN) {
        emitRet(compiler);
    }
    
    compiler->function = previousFunction;
}

static void ret(Compiler* compiler, AST* ast)
{
    if (isNone(ast->returnStatement.expr)) {
        return emitRet(compiler);
    }

    expression(compiler, ast->returnStatement.expr, false);
    emitRetv(compiler);
}

static void variableDefinitionUninitialized(Compiler* compiler, AST* ast)
{
    emitPushb(compiler, 0);

    if (isTopLevel(ast->variableDefinition.scope)) {
        emitReg(compiler);
    }
}

static void variableDefinition(Compiler* compiler, AST* ast)
{
    if (isNone(ast->variableDefinition.expr)) {
        return variableDefinitionUninitialized(compiler, ast);
    }

    expression(compiler, ast->variableDefinition.expr, false);

    if (isTopLevel(ast->variableDefinition.scope)) {
        emitReg(compiler);
    }
}

static void expression(Compiler* compiler, AST* ast, bool discard)
{
    switch (ast->type) {
        case AST_BINARY:
            binary(compiler, ast);
            break;
        case AST_BUILTIN_CALL:
            return builtinCall(compiler, ast, discard);
        case AST_FUNCTION_CALL:
            return functionCall(compiler, ast, discard);
        case AST_INTEGER:
            number(compiler, ast);
            break;
        case AST_PREFIX:
            prefix(compiler, ast);
            break;
        case AST_VARIABLE:
            variable(compiler, ast);
            break;
        default:
            return;
    }

    if (discard) {
        emitPop(compiler);
    }
}

static void statement(Compiler* compiler, AST* ast, bool discard)
{
    switch (ast->type) {
        case AST_ASSIGNMENT:
            assignment(compiler, ast);
            return;
        case AST_BUILTIN_CALL:
            builtinCall(compiler, ast, discard);
            return;
        case AST_FUNCTION_CALL:
            functionCall(compiler, ast, discard);
            return;
        case AST_FUNCTION_DEFINITION:
            functionDefinition(compiler, ast);
            return;
        case AST_RETURN:
            ret(compiler, ast);
            return;
        case AST_VARIABLE_DEFINITION:
            variableDefinition(compiler, ast);
            return;
        default:
            expression(compiler, ast, discard);
            return;
    }
}

static void blocklevelStatements(Compiler* compiler, Vector* nodes)
{
    size_t count = countVector(nodes);

    for (size_t i = 0; i < count; i++) {
        statement(compiler, nodes->data[i], true);
    }
}

static void toplevelStatements(Compiler* compiler, Vector* nodes)
{
    size_t count = countVector(nodes);

    while (compiler->statementIndex < count) {
        statement(compiler, nodes->data[compiler->statementIndex], true);
        compiler->statementIndex++;
    }
}

static void replStatement(Compiler* compiler, AST* ast, bool isLast)
{
    bool display = isLast && isExpressionStatement(ast) && getTypeId(ast) != TOKEN_NONE;
    statement(compiler, ast, !display);

    if (!display) {
        return;
    }

    emitCallBuiltin(compiler, BUILTIN_PRINT);
    emitPop(compiler);
}

static void replStatements(Compiler* compiler, Vector* nodes)
{
    size_t count = countVector(nodes);

    while (compiler->statementIndex < count) {
        AST* ast = nodes->data[compiler->statementIndex++];
        bool isLast = compiler->statementIndex == count;
        replStatement(compiler, ast, isLast);
    }
}

void initCompiler(Compiler* compiler, ModuleObject* module)
{
    initVector(&compiler->functionReferences);
    pushVectorItem(&compiler->functionReferences, NULL);

    AST* ast = createAST(AST_COMPOUND);
    ast->compound.scope = createScope(NULL);

    initParser(&compiler->parser, ast);
    initAnalyzer(&compiler->analyzer, ast);

    compiler->module = module;
    compiler->function = getVectorAt(&module->functions, 0);
    compiler->ast = ast;
    compiler->statementIndex = 0;
    compiler->stackCount = 0;
}

void freeCompiler(Compiler* compiler)
{
    freeVector(&compiler->functionReferences);
    freeAST(compiler->ast);
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
    return compileSource(compiler, source, toplevelStatements);
}

bool compileRepl(Compiler* compiler, char* source)
{
    return compileSource(compiler, source, replStatements);
}
