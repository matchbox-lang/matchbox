#include "compiler/ast.h"
#include "builtin/builtin.h"
#include "common/array_type.h"
#include "common/memory.h"
#include "common/vector.h"
#include "compiler/lexer/token.h"
#include "compiler/scope.h"
#include "runtime/string_object.h"
#include <stdbool.h>
#include <stdint.h>

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

ASTNode* create_typed_binary_node(ASTNode* left, ASTNode* right, Token token, Type operand_type, Type result_type)
{
    ASTNode* ast = create_binary_node(left, right, token);
    ast->binary.operand_type = operand_type;
    ast->binary.type_id = result_type;

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

ASTNode* create_variable_node(Token token)
{
    ASTNode* ast = create_ast_node(ASTVariable);
    ast->variable.scope = NULL;
    ast->variable.id = copy_string_object(token.chars, token.length);
    ast->variable.token = token;
    ast->variable.symbol = NULL;

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

ASTNode* create_while_node(Token token, ASTNode* condition, ASTNode* body)
{
    ASTNode* ast = create_ast_node(ASTWhile);
    ast->while_statement.token = token;
    ast->while_statement.condition = condition;
    ast->while_statement.body = body;

    return ast;
}

void replace_ast_node(ASTNode* ast, ASTNode* replacement)
{
    *ast = *replacement;

    free_memory(replacement);
}

static void free_ast_node_vector(Vector* nodes)
{
    size_t count = count_vector(nodes);

    for (size_t i = 0; i < count; i++) {
        free_ast_node(nodes->data[i]);
    }

    free_vector(nodes);
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

bool is_none(const ASTNode* ast)
{
    return ast && ast->type == ASTNone;
}

bool is_expression_statement(const ASTNode* ast)
{
    if (!ast) {
        return false;
    }

    switch (ast->type) {
        case ASTArray:
        case ASTSlice:
        case ASTIndex:
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

bool is_function_call(const ASTNode* ast)
{
    return ast && ast->type == ASTFunctionCall;
}

bool is_function_definition(const ASTNode* ast)
{
    return ast && ast->type == ASTFunctionDefinition;
}

bool is_index(const ASTNode* ast)
{
    return ast && (ast->type == ASTIndex || ast->type == ASTIndexAssignment);
}

bool is_constant_index(const ASTNode* ast)
{
    return is_index(ast) && ast->index.constant;
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
        case ASTArray:
        case ASTIndex:
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

static Type get_builtin_return_type(const ASTNode* ast)
{
    TypeKind kind = ast->builtin_call.builtin->return_type_id;

    if (kind == TypeArray) {
        return get_node_data_type(ast->builtin_call.args.data[0]);
    }

    return (Type){.kind = kind};
}

Type get_node_data_type(const ASTNode* ast)
{
    if (!ast) {
        return (Type){.kind = TypeVoid};
    }

    switch (ast->type) {
        case ASTArray:
            return ast->array.type_id;
        case ASTSlice:
            return ast->slice.type_id;
        case ASTIndex:
            return ast->index.type_id;
        case ASTConversion:
            return ast->conversion.type_id;
        case ASTBinary:
            return ast->binary.type_id;
        case ASTConditional:
            return ast->conditional.type_id;
        case ASTBoolean:
            return (Type){.kind = TypeBool};
        case ASTBuiltinCall:
            return get_builtin_return_type(ast);
        case ASTCharacter:
            return (Type){.kind = TypeChar};
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
            return (Type){.kind = TypeString};
        default:
            return (Type){.kind = TypeVoid};
    }
}

ASTNode* get_root_binding(const ASTNode* ast)
{
    if (ast->type == ASTIndex) {
        return get_root_binding(ast->index.array);
    }

    if (ast->type == ASTVariable) {
        return ast->variable.symbol;
    }

    return NULL;
}

ASTNode* get_index_constant_expression(ASTNode* ast)
{
    if (!ast) {
        return NULL;
    }

    if (ast->type == ASTPrefix && ast->prefix.operator.type == TokenKeywordConst) {
        return ast;
    }

    if (ast->type != ASTVariable) {
        return NULL;
    }

    ASTNode* symbol = ast->variable.symbol;

    if (!is_variable_definition(symbol)) {
        return NULL;
    }

    if (!symbol->variable_definition.fixed || symbol->variable_definition.address_taken
        || symbol->variable_definition.reference_type != RefNone) {
        return NULL;
    }

    return get_index_constant_expression(symbol->variable_definition.expr);
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

ASTNode* get_integer_literal(ASTNode* expression, bool* negative)
{
    *negative = false;

    if (expression->type == ASTInteger && is_signed_integer_type(expression->integer_literal.type_id.kind)
        && expression->integer_literal.value > INT64_MAX) {
        return NULL;
    }

    if (expression->type == ASTInteger) {
        return expression;
    }

    if (expression->type != ASTPrefix || expression->prefix.operator.type != TokenMinus
        || expression->prefix.expr->type != ASTInteger) {
        return NULL;
    }

    *negative = true;

    return expression->prefix.expr;
}

ASTNode* get_float_literal(ASTNode* expression)
{
    if (expression->type == ASTPrefix && expression->prefix.operator.type == TokenMinus) {
        expression = expression->prefix.expr;
    }

    if (expression->type != ASTFloat) {
        return NULL;
    }

    return expression;
}

bool is_numeric_literal(ASTNode* argument)
{
    bool negative;
    ASTNode* integer = get_integer_literal(argument, &negative);
    ASTNode* floating = get_float_literal(argument);

    return integer || floating;
}

bool is_literal_of_type(ASTNode* ast, Type type)
{
    if (!is_literal(ast)) {
        return false;
    }

    Type literal_type = get_node_data_type(ast);

    return types_equal(literal_type, type);
}

void set_literal_token(ASTNode* ast, Token token)
{
    switch (ast->type) {
        case ASTInteger:
            ast->integer_literal.token = token;
            break;
        case ASTBoolean:
            ast->boolean_literal.token = token;
            break;
        case ASTFloat:
            ast->float_literal.token = token;
            break;
        case ASTString:
            ast->string_literal.token = token;
            break;
        case ASTCharacter:
            ast->character_literal.token = token;
            break;
        default:
            break;
    }
}

ASTNode* copy_literal_node(const ASTNode* literal)
{
    ASTNode* copy = create_ast_node(literal->type);
    *copy = *literal;

    if (literal->type == ASTString) {
        StringObject* string = literal->string_literal.value;
        copy->string_literal.value = copy_string_object(string->chars, string->length);
    }

    return copy;
}

bool is_literal_array_tree(ASTNode* ast)
{
    if (is_literal(ast)) {
        return true;
    }

    if (ast->type != ASTArray || ast->array.fill) {
        return false;
    }

    bool literal = true;

    for (size_t i = 0; i < ast->array.elements.count && literal; i++) {
        literal = is_literal_array_tree(ast->array.elements.data[i]);
    }

    return literal;
}

void copy_flattened_array_literals(ASTNode* ast, Vector* literals)
{
    if (ast->type != ASTArray) {
        ASTNode* literal = copy_literal_node(ast);

        push_vector_item(literals, literal);

        return;
    }

    for (size_t i = 0; i < ast->array.elements.count; i++) {
        copy_flattened_array_literals(ast->array.elements.data[i], literals);
    }
}

ASTNode* get_branch_result(ASTNode* branch)
{
    if (branch->type == ASTConditional) {
        return branch;
    }

    Vector* statements = &branch->compound.statements;
    size_t count = count_vector(statements);

    if (!count) {
        return NULL;
    }

    return get_vector_at(statements, count - 1);
}

static ASTNode* find_function_return(Vector* statements)
{
    size_t count = count_vector(statements);
    size_t index = 0;
    ASTNode* result = NULL;

    while (index < count && !result) {
        ASTNode* statement = get_vector_at(statements, index);
        result = find_node_return(statement);
        index++;
    }

    return result;
}

static ASTNode* find_conditional_return(ASTNode* ast)
{
    ASTNode* result = find_node_return(ast->conditional.then_branch);

    if (result) {
        return result;
    }

    return find_node_return(ast->conditional.else_branch);
}

static ASTNode* find_match_return(ASTNode* ast)
{
    size_t count = count_vector(&ast->match.arms);
    size_t index = 0;
    ASTNode* result = NULL;

    while (index < count && !result) {
        MatchArm* arm = get_vector_at(&ast->match.arms, index);
        result = find_node_return(arm->branch);
        index++;
    }

    return result;
}

ASTNode* find_node_return(ASTNode* ast)
{
    if (!ast) {
        return NULL;
    }

    switch (ast->type) {
        case ASTReturn:
            return ast;
        case ASTCompound:
            return find_function_return(&ast->compound.statements);
        case ASTFor:
            return find_node_return(ast->for_statement.body);
        case ASTWhile:
            return find_node_return(ast->while_statement.body);
        case ASTConditional:
            return find_conditional_return(ast);
        case ASTMatch:
            return find_match_return(ast);
        default:
            return NULL;
    }
}

bool is_match_alternative(ASTNode* pattern)
{
    return pattern->type == ASTBinary && pattern->binary.operator.type == TokenPipe;
}

bool is_literal_match_pattern(ASTNode* pattern)
{
    if (pattern->type == ASTInteger || pattern->type == ASTBoolean) {
        return true;
    }

    if (!is_match_alternative(pattern)) {
        return false;
    }

    return is_literal_match_pattern(pattern->binary.left_expr) && is_literal_match_pattern(pattern->binary.right_expr);
}

static bool are_simple_match_values(const Vector* values)
{
    size_t index = 0;

    while (index < values->count && is_simple_match_value(values->data[index])) {
        index++;
    }

    return index == values->count;
}

bool is_simple_match_value(const ASTNode* ast)
{
    if (!ast) {
        return false;
    }

    switch (ast->type) {
        case ASTBoolean:
        case ASTCharacter:
        case ASTInteger:
        case ASTFloat:
        case ASTVariable:
            return true;
        case ASTBuiltinCall:
            return are_simple_match_values(&ast->builtin_call.args);
        default:
            return false;
    }
}

bool match_has_wildcard(ASTNode* ast)
{
    size_t count = count_vector(&ast->match.arms);
    size_t index = 0;
    bool wildcard = false;

    while (index < count && !wildcard) {
        MatchArm* arm = get_vector_at(&ast->match.arms, index);
        wildcard = !arm->pattern && !arm->guard;
        index++;
    }

    return wildcard;
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
        case ASTSlice:
            return ast->slice.reference_type;
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

void set_expression_reference_type(ASTNode* ast, ReferenceType type)
{
    if (ast->type == ASTConditional) {
        ast->conditional.reference_type = type;

        return;
    }

    ast->match.reference_type = type;
}

Vector* get_reference_origins(ASTNode* ast)
{
    if (!ast) {
        return NULL;
    }

    switch (ast->type) {
        case ASTSlice:
            return &ast->slice.reference_origins;
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

ASTNode* get_reference_origin_or_self(ASTNode* symbol)
{
    ASTNode* origin = get_reference_origin(symbol);
    if (origin) {
        return origin;
    }

    return symbol;
}

ASTNode* get_reference_origin_from_expression(ASTNode* ast)
{
    if (ast->type == ASTPrefix && is_variable(ast->prefix.expr)) {
        ASTNode* symbol = ast->prefix.expr->variable.symbol;

        return get_reference_origin_or_self(symbol);
    }

    return get_reference_origin(ast);
}

Vector* get_reference_origins_from_expression(ASTNode* ast)
{
    Vector* origins = get_reference_origins(ast);
    if (origins) {
        return origins;
    }

    static Vector empty;

    return &empty;
}

ASTNode* get_argument_reference_origin(ASTNode* argument)
{
    ASTNode* origin = get_reference_origin_from_expression(argument);

    if (origin || !is_variable(argument)) {
        return origin;
    }

    return get_reference_origin_or_self(argument->variable.symbol);
}

bool contains_reference_origin(Vector* origins, ASTNode* origin)
{
    size_t count = count_vector(origins);
    size_t index = 0;

    while (index < count && origins->data[index] != origin) {
        index++;
    }

    return index < count;
}

void add_reference_origin(Vector* origins, ASTNode* origin)
{
    if (!origin || contains_reference_origin(origins, origin)) {
        return;
    }

    push_vector_item(origins, origin);
}

void merge_reference_origins(Vector* destination, Vector* source)
{
    size_t count = count_vector(source);

    for (size_t i = 0; i < count; i++) {
        ASTNode* origin = get_vector_at(source, i);

        add_reference_origin(destination, origin);
    }
}

void copy_reference_origins(Vector* destination, Vector* source)
{
    if (destination == source) {
        return;
    }

    destination->count = 0;

    merge_reference_origins(destination, source);
}

size_t* get_shared_access_count(ASTNode* symbol)
{
    if (is_parameter(symbol)) {
        return &symbol->parameter.shared_access_count;
    }

    return &symbol->variable_definition.shared_access_count;
}

bool has_exclusive_access(ASTNode* symbol)
{
    if (is_parameter(symbol)) {
        return symbol->parameter.exclusive_access_active;
    }

    return symbol->variable_definition.exclusive_access_active;
}

void set_exclusive_access(ASTNode* symbol, bool active)
{
    if (is_parameter(symbol)) {
        symbol->parameter.exclusive_access_active = active;

        return;
    }

    symbol->variable_definition.exclusive_access_active = active;
}

void apply_slice_type(ASTNode* ast, Type type, ReferenceType reference_type)
{
    if (ast->type == ASTSlice) {
        ast->slice.type_id = type;
        ast->slice.reference_type = reference_type;

        return;
    }

    ASTNode* expression = allocate_memory(sizeof(ASTNode));
    *expression = *ast;

    *ast = (ASTNode){.type = ASTSlice};
    ast->slice.expr = expression;
    ast->slice.type_id = type;

    if (expression->type == ASTVariable) {
        ast->slice.token = expression->variable.token;
    }

    if (expression->type == ASTFunctionCall) {
        ast->slice.token = expression->function_call.token;
    }

    if (expression->type == ASTString) {
        ast->slice.token = expression->string_literal.token;
    }

    if (expression->type == ASTPrefix) {
        ast->slice.token = expression->prefix.operator;
    }

    ast->slice.reference_type = reference_type;

    init_vector(&ast->slice.reference_origins);
}

void convert_string_reference_to_slice(ASTNode* ast, Type type)
{
    ReferenceType reference = get_reference_type(ast);
    ASTNode* slice = create_slice_node(ast->prefix.operator, ast->prefix.expr, NULL);

    slice->slice.type_id = type;
    slice->slice.reference_type = reference;
    slice->slice.reference_origins = ast->prefix.reference_origins;

    replace_ast_node(ast, slice);
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
        ast->variable_definition.possibly_initialized = true;
    }
}

bool is_moved(ASTNode* symbol)
{
    if (is_parameter(symbol)) {
        return symbol->parameter.moved;
    }

    return symbol->variable_definition.moved;
}

void set_moved_state(ASTNode* symbol, bool moved)
{
    if (is_parameter(symbol)) {
        symbol->parameter.moved = moved;

        return;
    }

    symbol->variable_definition.moved = moved;
}

void mark_address_taken(ASTNode* symbol)
{
    if (!symbol || !is_variable_definition(symbol)) {
        return;
    }

    symbol->variable_definition.address_taken = true;
}

void track_variable_read(ASTNode* symbol)
{
    if (!is_variable_definition(symbol)) {
        return;
    }

    symbol->variable_definition.read_count++;
}

bool has_no_value_reads(ASTNode* ast)
{
    return ast->variable_definition.read_count == ast->variable_definition.discarded_read_count;
}
