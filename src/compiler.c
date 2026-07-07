#include "compiler.h"
#include "ast.h"
#include "code_object.h"
#include "function_object.h"
#include "module_object.h"
#include "opcode.h"
#include "parser.h"
#include "scope.h"
#include "token.h"
#include "util.h"
#include "value.h"
#include "vector.h"
#include <stddef.h>
#include <stdint.h>

static void expression(Compiler* compiler, AST* ast);
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

static void op_hlt(Compiler* compiler)
{
    write8(compiler, OP_HLT);
}

static void op_reqs(Compiler* compiler, uint8_t imm)
{
    write8(compiler, OP_REQS);
    write8(compiler, imm);
}

static void op_ldc(Compiler* compiler, uint8_t imm)
{
    incStackCount(compiler);
    write8(compiler, OP_LDC);
    write8(compiler, imm);
}

static void op_reg(Compiler* compiler)
{
    decStackCount(compiler);
    write8(compiler, OP_REG);
}

static void op_ldg(Compiler* compiler, uint8_t imm)
{
    incStackCount(compiler);
    write8(compiler, OP_LDG);
    write8(compiler, imm);
}

static void op_stg(Compiler* compiler, uint8_t imm)
{
    decStackCount(compiler);
    write8(compiler, OP_STG);
    write8(compiler, imm);
}

static void op_ldl(Compiler* compiler, int8_t imm)
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

static void op_stl(Compiler* compiler, int8_t imm)
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

static void op_pushb(Compiler* compiler, int8_t imm)
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

static void op_pushh(Compiler* compiler, int16_t imm)
{
    incStackCount(compiler);
    write8(compiler, OP_PUSHH);
    write16(compiler, imm);
}

static void op_pop(Compiler* compiler)
{
    decStackCount(compiler);
    write8(compiler, OP_POP);
}

static void op_add(Compiler* compiler)
{
    decStackCount(compiler);
    write8(compiler, OP_ADD);
}

static void op_sub(Compiler* compiler)
{
    decStackCount(compiler);
    write8(compiler, OP_SUB);
}

static void op_mul(Compiler* compiler)
{
    decStackCount(compiler);
    write8(compiler, OP_MUL);
}

static void op_div(Compiler* compiler)
{
    decStackCount(compiler);
    write8(compiler, OP_DIV);
}

static void op_rem(Compiler* compiler)
{
    decStackCount(compiler);
    write8(compiler, OP_REM);
}

static void op_pow(Compiler* compiler)
{
    decStackCount(compiler);
    write8(compiler, OP_POW);
}

static void op_band(Compiler* compiler)
{
    decStackCount(compiler);
    write8(compiler, OP_BAND);
}

static void op_bor(Compiler* compiler)
{
    decStackCount(compiler);
    write8(compiler, OP_BOR);
}

static void op_bxor(Compiler* compiler)
{
    decStackCount(compiler);
    write8(compiler, OP_BXOR);
}

static void op_bnot(Compiler* compiler)
{
    decStackCount(compiler);
    write8(compiler, OP_BNOT);
}

static void op_lsl(Compiler* compiler)
{
    decStackCount(compiler);
    write8(compiler, OP_LSL);
}

static void op_lsr(Compiler* compiler)
{
    decStackCount(compiler);
    write8(compiler, OP_LSR);
}

static void op_neg(Compiler* compiler)
{
    write8(compiler, OP_NEG);
}

static void op_not(Compiler* compiler)
{
    write8(compiler, OP_NOT);
}

static void op_call(Compiler* compiler, uint16_t imm)
{
    write8(compiler, OP_CALL);
    write16(compiler, imm);
}

static void op_ret(Compiler* compiler)
{
    incStackCount(compiler);
    write8(compiler, OP_RET);
}

static void op_retv(Compiler* compiler)
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

    op_ldg(compiler, position);
}

static void loadLocalVariable(Compiler* compiler, AST* ast)
{
    int position = getLocalPosition(ast);

    op_ldl(compiler, position);
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

    op_stg(compiler, position);
}

static void storeLocalVariable(Compiler* compiler, AST* ast)
{
    int position = getLocalPosition(ast);

    op_stl(compiler, position);
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
        op_ldc(compiler, position);
        pushVectorItem(&compiler->functionReferences, ast);
    } else if (isLargerThan8BitSigned(ast->intValue)) {
        op_pushh(compiler, ast->intValue);
    } else {
        op_pushb(compiler, ast->intValue);
    }
}

static void binary(Compiler* compiler, AST* ast)
{
    expression(compiler, ast->binary.leftExpr);
    expression(compiler, ast->binary.rightExpr);

    switch (ast->binary.operator.type) {
        case T_PLUS:
            return op_add(compiler);
        case T_MINUS:
            return op_sub(compiler);
        case T_STAR:
            return op_mul(compiler);
        case T_SLASH:
        case T_FLOOR:
            return op_div(compiler);
        case T_PERCENT:
            return op_rem(compiler);
        case T_POWER:
            return op_pow(compiler);
        case T_AMPERSAND:
            return op_band(compiler);
        case T_PIPE:
            return op_bor(compiler);
        case T_CIRCUMFLEX:
            return op_bxor(compiler);
        case T_LSHIFT:
            return op_lsl(compiler);
        case T_RSHIFT:
            return op_lsr(compiler);
        default:
            return;
    }
}

static void bitNot(Compiler* compiler, AST* ast)
{
    expression(compiler, ast->prefix.expr);
    op_bnot(compiler);
}

static void logNot(Compiler* compiler, AST* ast)
{
    expression(compiler, ast->prefix.expr);
    op_not(compiler);
}

static void negate(Compiler* compiler, AST* ast)
{
    expression(compiler, ast->prefix.expr);
    op_neg(compiler);
}

static void prefix(Compiler* compiler, AST* ast)
{
    switch (ast->prefix.operator.type) {
        case T_EXCLAMATION:
            return logNot(compiler, ast);
        case T_TILDE:
            return bitNot(compiler, ast);
        case T_MINUS:
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
    expression(compiler, ast->assignment.expr);
    op_add(compiler);
    storeVariable(compiler, ast->assignment.symbol);
}

static void subtractionAssignment(Compiler* compiler, AST* ast)
{
    loadVariable(compiler, ast->assignment.symbol);
    expression(compiler, ast->assignment.expr);
    op_sub(compiler);
    storeVariable(compiler, ast->assignment.symbol);
}

static void muliplicationAssignment(Compiler* compiler, AST* ast)
{
    loadVariable(compiler, ast->assignment.symbol);
    expression(compiler, ast->assignment.expr);
    op_mul(compiler);
    storeVariable(compiler, ast->assignment.symbol);
}

static void divisionAssignment(Compiler* compiler, AST* ast)
{
    loadVariable(compiler, ast->assignment.symbol);
    expression(compiler, ast->assignment.expr);
    op_div(compiler);
    storeVariable(compiler, ast->assignment.symbol);
}

static void remainderAssignment(Compiler* compiler, AST* ast)
{
    loadVariable(compiler, ast->assignment.symbol);
    expression(compiler, ast->assignment.expr);
    op_rem(compiler);
    storeVariable(compiler, ast->assignment.symbol);
}

static void exponentiationAssignment(Compiler* compiler, AST* ast)
{
    loadVariable(compiler, ast->assignment.symbol);
    expression(compiler, ast->assignment.expr);
    op_pow(compiler);
    storeVariable(compiler, ast->assignment.symbol);
}

static void simpleAssignment(Compiler* compiler, AST* ast)
{
    expression(compiler, ast->assignment.expr);
    storeVariable(compiler, ast->assignment.symbol);
}

static void assignment(Compiler* compiler, AST* ast)
{
    switch (ast->assignment.operator.type) {
        case T_PLUS_EQUAL:
            return additionAssignment(compiler, ast);
        case T_MINUS_EQUAL:
            return subtractionAssignment(compiler, ast);
        case T_STAR_EQUAL:
            return muliplicationAssignment(compiler, ast);
        case T_FLOOR_EQUAL:
        case T_SLASH_EQUAL:
            return divisionAssignment(compiler, ast);
        case T_PERCENT_EQUAL:
            return remainderAssignment(compiler, ast);
        case T_POWER_EQUAL:
            return exponentiationAssignment(compiler, ast);
        case T_EQUAL:
            return simpleAssignment(compiler, ast);
        default:
            return;
    }
}

static void arguments(Compiler* compiler, Vector* args)
{
    size_t count = countVector(args);

    for (size_t i = 0; i < count; i++) {
        expression(compiler, args->data[i]);
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

static void functionCall(Compiler* compiler, AST* ast)
{
    uint16_t position = getFunctionPosition(compiler, ast->functionCall.symbol);
    arguments(compiler, &ast->functionCall.args);
    op_call(compiler, position);
}

static void serviceRequest(Compiler* compiler, AST* ast)
{
    arguments(compiler, &ast->serviceRequest.args);
    op_reqs(compiler, ast->serviceRequest.opcode);
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
    makeConstant(compiler, POINTER_VALUE(function));
    pushVectorItem(&compiler->functionReferences, ast);
    blocklevelStatements(compiler, &body->compound.statements);

    AST* last = vectorEnd(&body->compound.statements);

    if (!last || last->type != AST_RETURN) {
        op_ret(compiler);
    }
    
    compiler->function = previousFunction;
}

static void ret(Compiler* compiler, AST* ast)
{
    if (isNone(ast->expression)) {
        return op_ret(compiler);
    }

    expression(compiler, ast->expression);
    op_retv(compiler);
}

static void variableDefinitionUninitialized(Compiler* compiler, AST* ast)
{
    op_pushb(compiler, 0);

    if (isTopLevel(ast->variableDefinition.scope)) {
        op_reg(compiler);
    }
}

static void variableDefinition(Compiler* compiler, AST* ast)
{
    if (isNone(ast->variableDefinition.expr)) {
        return variableDefinitionUninitialized(compiler, ast);
    }

    expression(compiler, ast->variableDefinition.expr);

    if (isTopLevel(ast->variableDefinition.scope)) {
        op_reg(compiler);
    }
}

static void expression(Compiler* compiler, AST* ast)
{
    switch (ast->type) {
        case AST_BINARY:
            return binary(compiler, ast);
        case AST_FUNCTION_CALL:
            return functionCall(compiler, ast);
        case AST_INTEGER:
            return number(compiler, ast);
        case AST_PREFIX:
            return prefix(compiler, ast);
        case AST_SERVICE_REQUEST:
            return serviceRequest(compiler, ast);
        case AST_VARIABLE:
            return variable(compiler, ast);
        default:
            return;
    }
}

static void statement(Compiler* compiler, AST* ast)
{
    switch (ast->type) {
        case AST_ASSIGNMENT:
            assignment(compiler, ast);
            break;
        case AST_FUNCTION_CALL:
            functionCall(compiler, ast);
            op_pop(compiler);
            break;
        case AST_FUNCTION_DEFINITION:
            functionDefinition(compiler, ast);
            break;
        case AST_RETURN:
            ret(compiler, ast);
            break;
        case AST_SERVICE_REQUEST:
            serviceRequest(compiler, ast);
            op_pop(compiler);
            break;
        case AST_VARIABLE_DEFINITION:
            variableDefinition(compiler, ast);
            break;
        default:
            expression(compiler, ast);
            op_pop(compiler);
    }
}

static void blocklevelStatements(Compiler* compiler, Vector* nodes)
{
    size_t count = countVector(nodes);

    for (size_t i = 0; i < count; i++) {
        statement(compiler, nodes->data[i]);
    }
}

static void toplevelStatements(Compiler* compiler, Vector* nodes)
{
    size_t count = countVector(nodes);

    for (; compiler->statementIndex < count; compiler->statementIndex++) {
        statement(compiler, nodes->data[compiler->statementIndex]);
    }
}

void initCompiler(Compiler* compiler, ModuleObject* module)
{
    initVector(&compiler->functionReferences);
    pushVectorItem(&compiler->functionReferences, NULL);

    AST* ast = createAST(AST_COMPOUND);
    ast->compound.scope = createScope(NULL);

    initParser(&compiler->parser, ast);

    compiler->module = module;
    compiler->function = AS_POINTER(module->constants.data[0]);
    compiler->ast = ast;
    compiler->statementIndex = 0;
    compiler->stackCount = 0;
}

void freeCompiler(Compiler* compiler)
{
    freeVector(&compiler->functionReferences);
    freeAST(compiler->ast);
}

void compile(Compiler* compiler, char* source)
{
    if (!compiler->module) {
        return;
    }
    
    parse(&compiler->parser, source);
    clearCodeObject(currentCodeObject(compiler));
    toplevelStatements(compiler, &compiler->ast->compound.statements);
    op_hlt(compiler);
}
