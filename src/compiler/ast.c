#include "compiler/ast.h"
#include "builtin/builtin.h"
#include "common/memory.h"
#include "common/util.h"
#include "common/vector.h"
#include "compiler/scope.h"
#include "compiler/token.h"
#include "runtime/string_object.h"
#include <stdbool.h>
#include <stdlib.h>

ASTNode* create_ast_node(ASTNodeType type)
{
    ASTNode* ast = allocate_zeroed_memory(1, sizeof(ASTNode));

    ast->type = type;

    switch (type) {
        case ASTBuiltinCall:
            init_vector(&ast->builtin_call.args);
            break;
        case ASTCompound:
            init_vector(&ast->compound.statements);
            break;
        case ASTConditional:
            init_vector(&ast->conditional.reference_origins);
            break;
        case ASTFunctionCall:
            init_vector(&ast->function_call.args);
            init_vector(&ast->function_call.reference_origins);
            break;
        case ASTFunctionDefinition:
            init_vector(&ast->function_definition.params);
            break;
        case ASTMatch:
            init_vector(&ast->match.arms);
            init_vector(&ast->match.reference_origins);
            break;
        case ASTParameter:
            init_vector(&ast->parameter.reference_origins);
            break;
        case ASTPrefix:
            init_vector(&ast->prefix.reference_origins);
            break;
        case ASTVariableDefinition:
            init_vector(&ast->variable_definition.reference_origins);
            break;
        default:
            break;
    }
    
    return ast;
}

ASTNode* create_assignment_node(Token operator, Token token, ASTNode* expr)
{
    ASTNode* ast = create_ast_node(ASTAssignment);
    ast->assignment.scope = NULL;
    ast->assignment.operator = operator;
    ast->assignment.token = token;
    ast->assignment.symbol = NULL;
    ast->assignment.expr = expr;
    ast->assignment.initializes_binding = false;

    return ast;
}

ASTNode* create_binary_node(ASTNode* left_expr, ASTNode* right_expr, Token token)
{
    if (!right_expr) {
        return NULL;
    }

    ASTNode* ast = create_ast_node(ASTBinary);
    ast->binary.left_expr = left_expr;
    ast->binary.operator = token;
    ast->binary.right_expr = right_expr;
    ast->binary.type_id = TypeUnknown;

    return ast;
}

ASTNode* create_boolean_node(Token token, bool value)
{
    ASTNode* ast = create_ast_node(ASTBoolean);
    ast->boolean_literal.value = value;
    ast->boolean_literal.token = token;

    return ast;
}

ASTNode* create_builtin_call_node(Token token, const Builtin* builtin)
{
    ASTNode* ast = create_ast_node(ASTBuiltinCall);
    ast->builtin_call.builtin = builtin;
    ast->builtin_call.token = token;

    return ast;
}

ASTNode* create_character_node(Token token, uint32_t value)
{
    ASTNode* ast = create_ast_node(ASTCharacter);
    ast->character_literal.value = value;
    ast->character_literal.token = token;

    return ast;
}

ASTNode* create_compound_node(Scope* scope)
{
    ASTNode* ast = create_ast_node(ASTCompound);
    ast->compound.scope = scope;

    return ast;
}

ASTNode* create_conditional_node(Token token, ASTNode* condition, ASTNode* then_branch, ASTNode* else_branch)
{
    ASTNode* ast = create_ast_node(ASTConditional);
    ast->conditional.token = token;
    ast->conditional.condition = condition;
    ast->conditional.then_branch = then_branch;
    ast->conditional.else_branch = else_branch;
    ast->conditional.type_id = TypeVoid;
    ast->conditional.reference_type = RefNone;
    ast->conditional.expression = true;

    return ast;
}

ASTNode* create_float_node(Token token, double value, DataType type_id)
{
    ASTNode* ast = create_ast_node(ASTFloat);
    ast->float_literal.value = value;
    ast->float_literal.token = token;
    ast->float_literal.type_id = type_id;

    return ast;
}

ASTNode* create_function_call_node(Token token)
{
    ASTNode* ast = create_ast_node(ASTFunctionCall);
    ast->function_call.scope = NULL;
    ast->function_call.id = copy_string_object(token.chars, token.length);
    ast->function_call.token = token;
    ast->function_call.symbol = NULL;

    return ast;
}

ASTNode* create_function_definition_node(Token token, StringObject* id)
{
    ASTNode* ast = create_ast_node(ASTFunctionDefinition);
    ast->function_definition.scope = NULL;
    ast->function_definition.id = id;
    ast->function_definition.token = token;
    ast->function_definition.return_type_id = TypeI32;
    ast->function_definition.return_reference_type = RefNone;
    ast->function_definition.return_reference_origin = NULL;
    ast->function_definition.has_explicit_return_type = false;
    ast->function_definition.body = NULL;

    return ast;
}

ASTNode* create_integer_node(Token token, uint64_t value, DataType type_id)
{
    ASTNode* ast = create_ast_node(ASTInteger);
    ast->integer_literal.value = value;
    ast->integer_literal.token = token;
    ast->integer_literal.type_id = type_id;

    return ast;
}

ASTNode* create_match_node(Token token)
{
    ASTNode* ast = create_ast_node(ASTMatch);
    ast->match.token = token;
    ast->match.type_id = TypeVoid;
    ast->match.reference_type = RefNone;
    ast->match.expression = true;

    return ast;
}

ASTNode* create_parameter_node(Token token, StringObject* id)
{
    ASTNode* ast = create_ast_node(ASTParameter);
    ast->parameter.scope = NULL;
    ast->parameter.id = id;
    ast->parameter.token = token;
    ast->parameter.type_id = TypeI32;
    ast->parameter.reference_type = RefNone;

    return ast;
}

ASTNode* create_prefix_node(Token token, ASTNode* expr)
{
    ASTNode* ast = create_ast_node(ASTPrefix);
    ast->prefix.operator = token;
    ast->prefix.expr = expr;

    return ast;
}

ASTNode* create_return_node(Token token, ASTNode* expr)
{
    ASTNode* ast = create_ast_node(ASTReturn);
    ast->return_statement.token = token;
    ast->return_statement.expr = expr;

    return ast;
}

ASTNode* create_string_node(Token token, StringObject* value)
{
    ASTNode* ast = create_ast_node(ASTString);
    ast->string_literal.value = value;
    ast->string_literal.token = token;

    return ast;
}

ASTNode* create_variable_definition_node(Token token, StringObject* id, bool fixed)
{
    ASTNode* ast = create_ast_node(ASTVariableDefinition);
    ast->variable_definition.scope = NULL;
    ast->variable_definition.id = id;
    ast->variable_definition.token = token;
    ast->variable_definition.fixed = fixed;
    ast->variable_definition.type_id = TypeUnknown;
    ast->variable_definition.reference_type = RefNone;
    ast->variable_definition.position = 0;
    ast->variable_definition.expr = NULL;

    return ast;
}

ASTNode* create_variable_node(Token token)
{
    ASTNode* ast = create_ast_node(ASTVariable);
    ast->variable.scope = NULL;
    ast->variable.id = copy_string_object(token.chars, token.length);
    ast->variable.token = token;
    ast->variable.symbol = NULL;

    return ast;
}

ASTNode* create_while_node(Token token, ASTNode* condition, ASTNode* body)
{
    ASTNode* ast = create_ast_node(ASTWhile);
    ast->while_statement.token = token;
    ast->while_statement.condition = condition;
    ast->while_statement.body = body;

    return ast;
}

static void free_ast_node_vector(Vector* nodes)
{
    size_t count = count_vector(nodes);

    for (size_t i = 0; i < count; i++) {
        free_ast_node(nodes->data[i]);
    }

    free_vector(nodes);
}

static void free_match_arms(Vector* arms)
{
    size_t count = count_vector(arms);

    for (size_t i = 0; i < count; i++) {
        MatchArm* arm = get_vector_at(arms, i);

        free_ast_node(arm->pattern);
        free_ast_node(arm->binding);
        free_ast_node(arm->guard);
        free_ast_node(arm->branch);
        free_memory(arm);
    }

    free_vector(arms);
}

void free_ast_node(ASTNode* ast)
{
    if (!ast) {
        return;
    }

    switch (ast->type) {
        case ASTConversion:
            free_ast_node(ast->conversion.expr);
            break;
        case ASTAssignment:
            free_ast_node(ast->assignment.expr);
            break;
        case ASTBinary:
            free_ast_node(ast->binary.left_expr);
            free_ast_node(ast->binary.right_expr);
            break;
        case ASTBuiltinCall:
            free_ast_node_vector(&ast->builtin_call.args);
            break;
        case ASTCompound:
            free_scope(ast->compound.scope);
            free_ast_node_vector(&ast->compound.statements);
            break;
        case ASTConditional:
            free_ast_node(ast->conditional.condition);
            free_ast_node(ast->conditional.then_branch);
            free_ast_node(ast->conditional.else_branch);
            free_vector(&ast->conditional.reference_origins);
            break;
        case ASTFunctionCall:
            free_string_object(ast->function_call.id);
            free_ast_node_vector(&ast->function_call.args);
            free_vector(&ast->function_call.reference_origins);
            break;
        case ASTFunctionDefinition:
            free_string_object(ast->function_definition.id);
            free_ast_node_vector(&ast->function_definition.params);
            free_ast_node(ast->function_definition.body);
            break;
        case ASTMatch:
            free_ast_node(ast->match.subject);
            free_match_arms(&ast->match.arms);
            free_ast_node(ast->match.default_branch);
            free_vector(&ast->match.reference_origins);
            break;
        case ASTParameter:
            free_ast_node(ast->parameter.default_value);
            free_string_object(ast->parameter.id);
            free_vector(&ast->parameter.reference_origins);
            break;
        case ASTPrefix:
            free_ast_node(ast->prefix.expr);
            free_vector(&ast->prefix.reference_origins);
            break;
        case ASTWhile:
            free_ast_node(ast->while_statement.condition);
            free_ast_node(ast->while_statement.body);
            break;
        case ASTReturn:
            free_ast_node(ast->return_statement.expr);
            break;
        case ASTString:
            free_string_object(ast->string_literal.value);
            break;
        case ASTVariableDefinition:
            free_string_object(ast->variable_definition.id);
            free_ast_node(ast->variable_definition.expr);
            free_vector(&ast->variable_definition.reference_origins);
            break;
        case ASTVariable:
            free_string_object(ast->variable.id);
            break;
        default:
            break;
    }

    free_memory(ast);
}

Scope* get_scope(const ASTNode* ast)
{
    if (!ast) {
        return NULL;
    }

    switch (ast->type) {
        case ASTAssignment:
            return ast->assignment.scope;
        case ASTCompound:
            return ast->compound.scope;
        case ASTFunctionCall:
            return ast->function_call.scope;
        case ASTFunctionDefinition:
            return ast->function_definition.scope;
        case ASTParameter:
            return ast->parameter.scope;
        case ASTVariable:
            return ast->variable.scope;
        case ASTVariableDefinition:
            return ast->variable_definition.scope;
        default:
            return NULL;
    }
}

DataType get_node_data_type(const ASTNode* ast)
{
    if (!ast) {
        return TypeVoid;
    }

    switch (ast->type) {
        case ASTConversion:
            return ast->conversion.type_id;
        case ASTBinary:
            return ast->binary.type_id;
        case ASTConditional:
            return ast->conditional.type_id;
        case ASTBoolean:
            return TypeBool;
        case ASTBuiltinCall:
            return ast->builtin_call.builtin->return_type_id;
        case ASTCharacter:
            return TypeChar;
        case ASTFloat:
            return ast->float_literal.type_id;
        case ASTFunctionCall:
            return get_node_data_type(ast->function_call.symbol);
        case ASTFunctionDefinition:
            return ast->function_definition.return_type_id;
        case ASTParameter:
            return ast->parameter.type_id;
        case ASTVariable:
            return get_node_data_type(ast->variable.symbol);
        case ASTVariableDefinition:
            return ast->variable_definition.type_id;
        case ASTPrefix:
            return get_node_data_type(ast->prefix.expr);
        case ASTInteger:
            return ast->integer_literal.type_id;
        case ASTMatch:
            return ast->match.type_id;
        case ASTString:
            return TypeString;
        default:
            return TypeVoid;
    }
}

Vector* get_reference_origins(ASTNode* ast)
{
    if (!ast) {
        return NULL;
    }

    switch (ast->type) {
        case ASTConditional:
            return &ast->conditional.reference_origins;
        case ASTFunctionCall:
            return &ast->function_call.reference_origins;
        case ASTMatch:
            return &ast->match.reference_origins;
        case ASTParameter:
            return &ast->parameter.reference_origins;
        case ASTPrefix:
            return &ast->prefix.reference_origins;
        case ASTVariable:
            return get_reference_origins(ast->variable.symbol);
        case ASTVariableDefinition:
            return &ast->variable_definition.reference_origins;
        default:
            return NULL;
    }
}

ASTNode* get_reference_origin(const ASTNode* ast)
{
    if (!ast) {
        return NULL;
    }

    Vector* origins = get_reference_origins((ASTNode*)ast);
    if (origins && origins->count) {
        return origins->data[0];
    }

    if (ast->type == ASTVariable) {
        return get_reference_origin(ast->variable.symbol);
    }

    return NULL;
}

static ReferenceType get_prefix_reference_type(TokenType type)
{
    if (type == TokenAmpersand) {
        return RefShared;
    }

    if (type == TokenCircumflex) {
        return RefExclusive;
    }

    return RefNone;
}

ReferenceType get_reference_type(const ASTNode* ast)
{
    if (!ast) {
        return RefNone;
    }

    switch (ast->type) {
        case ASTConditional:
            return ast->conditional.reference_type;
        case ASTFunctionCall:
            return ast->function_call.symbol->function_definition.return_reference_type;
        case ASTFunctionDefinition:
            return ast->function_definition.return_reference_type;
        case ASTMatch:
            return ast->match.reference_type;
        case ASTParameter:
            return ast->parameter.reference_type;
        case ASTPrefix:
            return get_prefix_reference_type(ast->prefix.operator.type);
        case ASTVariable:
            return get_reference_type(ast->variable.symbol);
        case ASTVariableDefinition:
            return ast->variable_definition.reference_type;
        default:
            return RefNone;
    }
}

bool is_expression_statement(const ASTNode* ast)
{
    if (!ast) {
        return false;
    }

    switch (ast->type) {
        case ASTBinary:
        case ASTBoolean:
        case ASTBuiltinCall:
        case ASTCharacter:
        case ASTConditional:
        case ASTConversion:
        case ASTFloat:
        case ASTFunctionCall:
        case ASTInteger:
        case ASTMatch:
        case ASTPrefix:
        case ASTString:
        case ASTVariable:
            return true;
        default:
            return false;
    }
}

bool is_literal(const ASTNode* ast)
{
    if (!ast) {
        return false;
    }

    switch (ast->type) {
        case ASTInteger:
        case ASTFloat:
        case ASTString:
        case ASTCharacter:
        case ASTBoolean:
            return true;
        default:
            return false;
    }
}

bool is_text_literal(const ASTNode* ast)
{
    return ast && (ast->type == ASTString || ast->type == ASTCharacter);
}

bool is_function_call(const ASTNode* ast)
{
    return ast && ast->type == ASTFunctionCall;
}

bool is_function_definition(const ASTNode* ast)
{
    return ast && ast->type == ASTFunctionDefinition;
}

bool is_parameter(const ASTNode* ast)
{
    return ast && ast->type == ASTParameter;
}

bool is_prefix(const ASTNode* ast)
{
    return ast && ast->type == ASTPrefix;
}

bool is_prefix_operand(const ASTNode* ast)
{
    if (!ast) {
        return false;
    }

    switch (ast->type) {
        case ASTAssignment:
        case ASTBinary:
        case ASTBoolean:
        case ASTBuiltinCall:
        case ASTCharacter:
        case ASTConditional:
        case ASTConversion:
        case ASTFloat:
        case ASTFunctionCall:
        case ASTInteger:
        case ASTMatch:
        case ASTString:
        case ASTVariable:
            return true;
        default:
            return false;
    }
}

bool is_variable(const ASTNode* ast)
{
    return ast && ast->type == ASTVariable;
}

bool is_variable_definition(const ASTNode* ast)
{
    return ast && ast->type == ASTVariableDefinition;
}

bool is_variable_type(const ASTNode* ast)
{
    return ast && (ast->type == ASTParameter || ast->type == ASTVariableDefinition);
}

bool is_none(const ASTNode* ast)
{
    return ast && ast->type == ASTNone;
}

bool is_initialized(const ASTNode* ast)
{
    if (!ast) {
        return false;
    }

    if (ast->type == ASTVariableDefinition) {
        return ast->variable_definition.initialized;
    }

    return ast->type == ASTParameter;
}

void initialize_variable(ASTNode* ast)
{
    if (is_variable_definition(ast)) {
        ast->variable_definition.initialized = true;
    }
}
