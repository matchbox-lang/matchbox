#include "compiler.h"
#include "ast.h"
#include "opcode.h"
#include "codeobject.h"
#include "functionobject.h"
#include "moduleobject.h"
#include "parser.h"
#include "scope.h"
#include "token.h"
#include "util.h"
#include "value.h"
#include "vector.h"
#include <stddef.h>
#include <stdint.h>

static void expression(AST* ast);
static void blocklevelStatements(Vector* nodes);
static void toplevelStatements(Vector* nodes);

typedef struct Compiler
{
    Vector functionReferences;
    ModuleObject* module;
    FunctionObject* function;
    AST* ast;
    int stackCount;
} Compiler;

static Compiler compiler;

static CodeObject* currentCodeObject()
{
    return &compiler.function->code;
}

static void incStackCount()
{
    if (++compiler.stackCount > compiler.function->maxStackCount) {
        compiler.function->maxStackCount = compiler.stackCount;
    }
}

static void decStackCount()
{
    compiler.stackCount--;
}

static void write8(uint8_t n)
{
    pushByte(currentCodeObject(), n);
}

static void write16(int16_t n)
{
    pushByte(currentCodeObject(), (n >> 8) & 0xFF);
    pushByte(currentCodeObject(), n & 0xFF);
}

static void op_hlt()
{
    write8(OP_HLT);
}

static void op_reqs(uint8_t imm)
{
    write8(OP_REQS);
    write8(imm);
}

static void op_ldc(uint8_t imm)
{
    incStackCount();
    write8(OP_LDC);
    write8(imm);
}

static void op_reg()
{
    decStackCount();
    write8(OP_REG);
}

static void op_ldg(uint8_t imm)
{
    incStackCount();
    write8(OP_LDG);
    write8(imm);
}

static void op_stg(uint8_t imm)
{
    decStackCount();
    write8(OP_STG);
    write8(imm);
}

static void op_ldl(int8_t imm)
{
    incStackCount();

    switch (imm) {
        case 0:
            write8(OP_LDL_0);
            break;
        case 1:
            write8(OP_LDL_1);
            break;
        case 2:
            write8(OP_LDL_2);
            break;
        case 3:
            write8(OP_LDL_3);
            break;
        default:
            write8(OP_LDL);
            write8(imm);
            break;
    }
}

static void op_stl(int8_t imm)
{
    decStackCount();

    switch (imm) {
        case 0:
            write8(OP_STL_0);
            break;
        case 1:
            write8(OP_STL_1);
            break;
        case 2:
            write8(OP_STL_2);
            break;
        case 3:
            write8(OP_STL_3);
            break;
        default:
            write8(OP_STL);
            write8(imm);
            break;
    }
}

static void op_pushb(int8_t imm)
{
    incStackCount();

    switch (imm) {
        case 0:
            write8(OP_PUSH_0);
            break;
        case 1:
            write8(OP_PUSH_1);
            break;
        case 2:
            write8(OP_PUSH_2);
            break;
        case 3:
            write8(OP_PUSH_3);
            break;
        default:
            write8(OP_PUSHB);
            write8(imm);
            break;
    }
}

static void op_pushh(int16_t imm)
{
    incStackCount();
    write8(OP_PUSHH);
    write16(imm);
}

static void op_pop()
{
    decStackCount();
    write8(OP_POP);
}

static void op_add()
{
    decStackCount();
    write8(OP_ADD);
}

static void op_sub()
{
    decStackCount();
    write8(OP_SUB);
}

static void op_mul()
{
    decStackCount();
    write8(OP_MUL);
}

static void op_div()
{
    decStackCount();
    write8(OP_DIV);
}

static void op_rem()
{
    decStackCount();
    write8(OP_REM);
}

static void op_pow()
{
    decStackCount();
    write8(OP_POW);
}

static void op_band()
{
    decStackCount();
    write8(OP_BAND);
}

static void op_bor()
{
    decStackCount();
    write8(OP_BOR);
}

static void op_bxor()
{
    decStackCount();
    write8(OP_BXOR);
}

static void op_bnot()
{
    decStackCount();
    write8(OP_BNOT);
}

static void op_lsl()
{
    decStackCount();
    write8(OP_LSL);
}

static void op_lsr()
{
    decStackCount();
    write8(OP_LSR);
}

static void op_neg()
{
    write8(OP_NEG);
}

static void op_not()
{
    write8(OP_NOT);
}

static void op_call(uint16_t imm)
{
    write8(OP_CALL);
    write16(imm);
}

static void op_ret()
{
    incStackCount();
    write8(OP_RET);
}

static void op_retv()
{
    write8(OP_RETV);
}

static size_t makeConstant(Value value)
{
    return pushValue(&compiler.module->constants, value) - 1;
}

static int getLocalPosition(AST* ast)
{
    if (isParameter(ast)) {
        return -(ast->parameter.position + 4);
    }
    
    return ast->variableDefinition.position;
}

static void loadGlobalVariable(AST* ast)
{
    int position = getLocalPosition(ast);

    op_ldg(position);
}

static void loadLocalVariable(AST* ast)
{
    int position = getLocalPosition(ast);

    op_ldl(position);
}

static void loadVariable(AST* ast)
{
    if (isTopLevel(ast->variableDefinition.scope)) {
        loadGlobalVariable(ast);
    } else {
        loadLocalVariable(ast);
    }
}

static void storeGlobalVariable(AST* ast)
{
    int position = getLocalPosition(ast);

    op_stg(position);
}

static void storeLocalVariable(AST* ast)
{
    int position = getLocalPosition(ast);

    op_stl(position);
}

static void storeVariable(AST* ast)
{
    if (isTopLevel(ast->variableDefinition.scope)) {
        storeGlobalVariable(ast);
    } else {
        storeLocalVariable(ast);
    }
}

static void number(AST* ast)
{
    if (isLargerThan16BitSigned(ast->intValue)) {
        size_t position = makeConstant(INT_VALUE(ast->intValue));
        op_ldc(position);
        pushVectorItem(&compiler.functionReferences, ast);
    } else if (isLargerThan8BitSigned(ast->intValue)) {
        op_pushh(ast->intValue);
    } else {
        op_pushb(ast->intValue);
    }
}

static void binary(AST* ast)
{
    expression(ast->binary.leftExpr);
    expression(ast->binary.rightExpr);

    switch (ast->binary.operator.type) {
        case T_PLUS:
            return op_add();
        case T_MINUS:
            return op_sub();
        case T_STAR:
            return op_mul();
        case T_SLASH:
        case T_FLOOR:
            return op_div();
        case T_PERCENT:
            return op_rem();
        case T_POWER:
            return op_pow();
        case T_AMPERSAND:
            return op_band();
        case T_PIPE:
            return op_bor();
        case T_CIRCUMFLEX:
            return op_bxor();
        case T_LSHIFT:
            return op_lsl();
        case T_RSHIFT:
            return op_lsr();
        default:
            return;
    }
}

static void bitNot(AST* ast)
{
    expression(ast->prefix.expr);
    op_bnot();
}

static void logNot(AST* ast)
{
    expression(ast->prefix.expr);
    op_not();
}

static void negate(AST* ast)
{
    expression(ast->prefix.expr);
    op_neg();
}

static void prefix(AST* ast)
{
    switch (ast->prefix.operator.type) {
        case T_EXCLAMATION:
            return logNot(ast);
        case T_TILDE:
            return bitNot(ast);
        case T_MINUS:
            return negate(ast);
        default:
            return;
    }
}

static void variable(AST* ast)
{
    loadVariable(ast->variable.symbol);
}

static void additionAssignment(AST* ast)
{
    loadVariable(ast->assignment.symbol);
    expression(ast->assignment.expr);
    op_add();
    storeVariable(ast->assignment.symbol);
}

static void subtractionAssignment(AST* ast)
{
    loadVariable(ast->assignment.symbol);
    expression(ast->assignment.expr);
    op_sub();
    storeVariable(ast->assignment.symbol);
}

static void muliplicationAssignment(AST* ast)
{
    loadVariable(ast->assignment.symbol);
    expression(ast->assignment.expr);
    op_mul();
    storeVariable(ast->assignment.symbol);
}

static void divisionAssignment(AST* ast)
{
    loadVariable(ast->assignment.symbol);
    expression(ast->assignment.expr);
    op_div();
    storeVariable(ast->assignment.symbol);
}

static void remainderAssignment(AST* ast)
{
    loadVariable(ast->assignment.symbol);
    expression(ast->assignment.expr);
    op_rem();
    storeVariable(ast->assignment.symbol);
}

static void exponentiationAssignment(AST* ast)
{
    loadVariable(ast->assignment.symbol);
    expression(ast->assignment.expr);
    op_pow();
    storeVariable(ast->assignment.symbol);
}

static void simpleAssignment(AST* ast)
{
    expression(ast->assignment.expr);
    storeVariable(ast->assignment.symbol);
}

static void assignment(AST* ast)
{
    switch (ast->assignment.operator.type) {
        case T_PLUS_EQUAL:
            return additionAssignment(ast);
        case T_MINUS_EQUAL:
            return subtractionAssignment(ast);
        case T_STAR_EQUAL:
            return muliplicationAssignment(ast);
        case T_FLOOR_EQUAL:
        case T_SLASH_EQUAL:
            return divisionAssignment(ast);
        case T_PERCENT_EQUAL:
            return remainderAssignment(ast);
        case T_POWER_EQUAL:
            return exponentiationAssignment(ast);
        case T_EQUAL:
            return simpleAssignment(ast);
        default:
            return;
    }
}

static void arguments(Vector* args)
{
    size_t count = countVector(args);

    while (count--) {
        expression(args->data[count]);
    }
}

static int getFunctionPosition(AST* ast)
{
    size_t functionCount = countVector(&compiler.functionReferences);
    
    for (int i = 0; i < functionCount; i++) {
        if (ast == compiler.functionReferences.data[i]) {
            return i;
        }
    }

    return -1;
}

static void functionCall(AST* ast)
{
    uint16_t position = getFunctionPosition(ast->functionCall.symbol);
    arguments(&ast->functionCall.args);
    op_call(position);
}

static void serviceRequest(AST* ast)
{
    arguments(&ast->serviceRequest.args);
    op_reqs(ast->serviceRequest.opcode);
}

static void functionDefinition(AST* ast)
{
    AST* body = ast->functionDefinition.body;
    FunctionObject* previousFunction = compiler.function;
    FunctionObject* function = createFunctionObject();
    function->paramCount = countVector(&ast->functionDefinition.params);
    function->localCount = body->compound.scope->localCount;
    function->maxStackCount = function->localCount + 3;
    
    compiler.stackCount = function->maxStackCount;
    compiler.function = function;
    makeConstant(POINTER_VALUE(function));
    pushVectorItem(&compiler.functionReferences, ast);
    blocklevelStatements(&body->compound.statements);

    AST* last = vectorEnd(&body->compound.statements);

    if (!last || last->type != AST_RETURN) {
        op_ret();
    }
    
    compiler.function = previousFunction;
}

static void ret(AST* ast)
{
    if (isNone(ast->expression)) {
        return op_ret();
    }

    expression(ast->expression);
    op_retv();
}

static void variableDefinitionUninitialized(AST* ast)
{
    op_pushb(0);

    if (isTopLevel(ast->variableDefinition.scope)) {
        op_reg();
    }
}

static void variableDefinition(AST* ast)
{
    if (isNone(ast->variableDefinition.expr)) {
        return variableDefinitionUninitialized(ast);
    }

    expression(ast->variableDefinition.expr);

    if (isTopLevel(ast->variableDefinition.scope)) {
        op_reg();
    }
}

static void expression(AST* ast)
{
    switch (ast->type) {
        case AST_BINARY:
            return binary(ast);
        case AST_FUNCTION_CALL:
            return functionCall(ast);
        case AST_INTEGER:
            return number(ast);
        case AST_PREFIX:
            return prefix(ast);
        case AST_SERVICE_REQUEST:
            return serviceRequest(ast);
        case AST_VARIABLE:
            return variable(ast);
        default:
            return;
    }
}

static void statement(AST* ast)
{
    switch (ast->type) {
        case AST_ASSIGNMENT:
            assignment(ast);
            break;
        case AST_FUNCTION_CALL:
            functionCall(ast);
            op_pop();
            break;
        case AST_FUNCTION_DEFINITION:
            functionDefinition(ast);
            break;
        case AST_RETURN:
            ret(ast);
            break;
        case AST_SERVICE_REQUEST:
            serviceRequest(ast);
            op_pop();
            break;
        case AST_VARIABLE_DEFINITION:
            variableDefinition(ast);
            break;
        default:
            expression(ast);
            op_pop();
    }
}

static void blocklevelStatements(Vector* nodes)
{
    size_t count = countVector(nodes);

    for (size_t i = 0; i < count; i++) {
        statement(nodes->data[i]);
    }
}

static void toplevelStatements(Vector* nodes)
{
    static size_t i = 0;
    size_t count = countVector(nodes);

    for (; i < count; i++) {
        statement(nodes->data[i]);
    }
}

void initCompiler(ModuleObject* module)
{
    initVector(&compiler.functionReferences);
    pushVectorItem(&compiler.functionReferences, NULL);

    AST* ast = createAST(AST_COMPOUND);
    ast->compound.scope = createScope(NULL);

    initParser(ast);

    compiler.module = module;
    compiler.function = AS_POINTER(module->constants.data[0]);
    compiler.ast = ast;
    compiler.stackCount = 0;
}

void freeCompiler()
{
    freeVector(&compiler.functionReferences);
    freeAST(compiler.ast);
}

void compile(char* source)
{
    if (!compiler.module) {
        return;
    }
    
    parse(source);
    clearCodeObject(currentCodeObject());
    toplevelStatements(&compiler.ast->compound.statements);
    op_hlt();
}
