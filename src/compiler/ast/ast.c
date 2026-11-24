#include "compiler/ast/ast.h"
#include "common/array_type.h"
#include "common/memory.h"
#include "common/vector.h"
#include "compiler/scope.h"
#include "runtime/string_object.h"

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

static void free_type_dimensions(Type type)
{
    if (!is_contiguous_type(type.kind) || !type.array || !type.array->dimensions) {
        return;
    }

    const ArrayType* array_type = type.array;
    Vector* dimensions = array_type->dimensions;

    free_type_dimensions(array_type->element_type);
    free_ast_node_vector(dimensions);
    free_memory(dimensions);
    free_memory((void*)array_type);
}

ASTNode* create_array_node(Token token, Type type, bool flat)
{
    ASTNode* ast = create_ast_node(ASTArray);
    ast->array.token = token;
    ast->array.type_id = type;
    ast->array.flat = flat;

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

ASTNode* create_ast_node(ASTNodeType type)
{
    ASTNode* ast = allocate_zeroed_memory(1, sizeof(ASTNode));
    ast->type = type;

    switch (type) {
        case ASTArray:
            init_vector(&ast->array.elements);
            break;
        case ASTSlice:
            init_vector(&ast->slice.reference_origins);
            break;
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
            init_vector(&ast->function_definition.array_moves);
            init_vector(&ast->function_definition.array_initializations);
            init_vector(&ast->function_definition.array_possible_initializations);
            init_vector(&ast->function_definition.array_uninitialized_returns);
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

ASTNode* create_binary_node(ASTNode* left_expr, ASTNode* right_expr, Token token)
{
    if (!right_expr) {
        return NULL;
    }

    ASTNode* ast = create_ast_node(ASTBinary);
    ast->binary.left_expr = left_expr;
    ast->binary.operator = token;
    ast->binary.right_expr = right_expr;
    ast->binary.type_id = (Type){.kind = TypeUnknown};

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
    ast->conditional.type_id = (Type){.kind = TypeVoid};
    ast->conditional.reference_type = RefNone;
    ast->conditional.expression = true;

    return ast;
}

ASTNode* create_conversion_node(Token token, ASTNode* expr, Type type_id)
{
    ASTNode* ast = create_ast_node(ASTConversion);
    ast->conversion.expr = expr;
    ast->conversion.token = token;
    ast->conversion.type_id = type_id;

    return ast;
}

ASTNode* create_float_node(Token token, double value, Type type_id)
{
    ASTNode* ast = create_ast_node(ASTFloat);
    ast->float_literal.value = value;
    ast->float_literal.token = token;
    ast->float_literal.type_id = type_id;

    return ast;
}

ASTNode* create_for_node(Token token)
{
    ASTNode* ast = create_ast_node(ASTFor);
    ast->for_statement.token = token;

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
    ast->function_definition.return_type_id = (Type){.kind = TypeI32};
    ast->function_definition.return_reference_type = RefNone;
    ast->function_definition.return_reference_origin = NULL;
    ast->function_definition.has_explicit_return_type = false;
    ast->function_definition.body = NULL;

    return ast;
}

ASTNode* create_integer_node(Token token, uint64_t value, Type type_id)
{
    ASTNode* ast = create_ast_node(ASTInteger);
    ast->integer_literal.value = value;
    ast->integer_literal.token = token;
    ast->integer_literal.type_id = type_id;

    return ast;
}

ASTNode* create_loop_control_node(Token token)
{
    ASTNodeType type = ASTBreak;

    if (token.type == TokenKeywordContinue) {
        type = ASTContinue;
    }

    ASTNode* ast = create_ast_node(type);
    ast->loop_control.token = token;

    return ast;
}

ASTNode* create_match_node(Token token)
{
    ASTNode* ast = create_ast_node(ASTMatch);
    ast->match.token = token;
    ast->match.type_id = (Type){.kind = TypeVoid};
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
    ast->parameter.type_id = (Type){.kind = TypeI32};
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

ASTNode* create_slice_node(Token token, ASTNode* expr, ASTNode* start)
{
    ASTNode* ast = create_ast_node(ASTSlice);
    ast->slice.token = token;
    ast->slice.expr = expr;
    ast->slice.start = start;
    ast->slice.reference_type = RefShared;

    return ast;
}

ASTNode* create_string_node(Token token, StringObject* value)
{
    ASTNode* ast = create_ast_node(ASTString);
    ast->string_literal.value = value;
    ast->string_literal.token = token;

    return ast;
}

ASTNode* create_typed_binary_node(ASTNode* left, ASTNode* right, Token token, Type operand_type, Type result_type)
{
    ASTNode* ast = create_binary_node(left, right, token);
    ast->binary.operand_type = operand_type;
    ast->binary.type_id = result_type;

    return ast;
}

ASTNode* create_variable_definition_node(Token token, StringObject* id, bool fixed)
{
    ASTNode* ast = create_ast_node(ASTVariableDefinition);
    ast->variable_definition.scope = NULL;
    ast->variable_definition.id = id;
    ast->variable_definition.token = token;
    ast->variable_definition.fixed = fixed;
    ast->variable_definition.type_id = (Type){.kind = TypeUnknown};
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

Type get_parameter_argument_type(ASTNode* parameter)
{
    if (parameter->parameter.variadic) {
        return parameter->parameter.type_id.array->element_type;
    }

    return parameter->parameter.type_id;
}

void free_ast_node(ASTNode* ast)
{
    if (!ast) {
        return;
    }

    switch (ast->type) {
        case ASTArray:
            free_type_dimensions(ast->array.type_id);
            free_ast_node_vector(&ast->array.elements);
            break;
        case ASTSlice:
            free_ast_node(ast->slice.expr);
            free_ast_node(ast->slice.start);
            free_ast_node(ast->slice.end);
            free_vector(&ast->slice.reference_origins);
            break;
        case ASTIndex:
        case ASTIndexAssignment:
            free_ast_node(ast->index.array);
            free_ast_node(ast->index.index);
            free_ast_node(ast->index.value);
            break;
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
            free_type_dimensions(ast->function_definition.return_type_id);
            free_string_object(ast->function_definition.id);
            free_ast_node_vector(&ast->function_definition.params);
            free_vector(&ast->function_definition.array_moves);
            free_vector(&ast->function_definition.array_initializations);
            free_vector(&ast->function_definition.array_possible_initializations);
            free_vector(&ast->function_definition.array_uninitialized_returns);
            free_ast_node(ast->function_definition.body);
            break;
        case ASTMatch:
            free_ast_node(ast->match.subject);
            free_match_arms(&ast->match.arms);
            free_vector(&ast->match.reference_origins);
            break;
        case ASTParameter:
            free_type_dimensions(ast->parameter.type_id);
            free_ast_node(ast->parameter.default_value);
            free_string_object(ast->parameter.id);
            free_vector(&ast->parameter.reference_origins);
            break;
        case ASTPrefix:
            free_ast_node(ast->prefix.expr);
            free_vector(&ast->prefix.reference_origins);
            break;
        case ASTFor:
            free_ast_node(ast->for_statement.array);
            free_ast_node(ast->for_statement.end);
            free_ast_node(ast->for_statement.index);
            free_ast_node(ast->for_statement.value);
            free_ast_node(ast->for_statement.body);
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
            free_type_dimensions(ast->variable_definition.type_id);
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

void replace_ast_node(ASTNode* ast, ASTNode* replacement)
{
    *ast = *replacement;

    free_memory(replacement);
}
