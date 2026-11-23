#include "compiler/ast/literal.h"
#include "common/vector.h"
#include "compiler/ast/ast.h"
#include "compiler/ast/query.h"
#include "runtime/string_object.h"
#include <stdint.h>

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

bool is_literal_of_type(ASTNode* ast, Type type)
{
    if (!is_literal(ast)) {
        return false;
    }

    Type literal_type = get_node_data_type(ast);

    return types_equal(literal_type, type);
}

bool is_numeric_literal(ASTNode* argument)
{
    bool negative;
    ASTNode* integer = get_integer_literal(argument, &negative);
    ASTNode* floating = get_float_literal(argument);

    return integer || floating;
}

bool is_text_literal(const ASTNode* ast)
{
    return ast && (ast->type == ASTString || ast->type == ASTCharacter);
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
