#ifndef AST_H
#define AST_H

#include "common/data_type.h"
#include "common/vector.h"
#include "compiler/token.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct ASTNode ASTNode;
typedef struct StringObject StringObject;
typedef struct Scope Scope;
typedef struct Builtin Builtin;

typedef struct MatchArm
{
    ASTNode* pattern;
    ASTNode* binding;
    ASTNode* guard;
    Token guard_token;
    ASTNode* branch;
} MatchArm;

typedef enum ReferenceType
{
    RefShared,
    RefExclusive,
    RefNone
} ReferenceType;

typedef enum ASTNodeType
{
    ASTAssignment,
    ASTBinary,
    ASTBuiltinCall,
    ASTBoolean,
    ASTCharacter,
    ASTCompound,
    ASTConditional,
    ASTConversion,
    ASTFloat,
    ASTFunctionCall,
    ASTFunctionDefinition,
    ASTInteger,
    ASTMatch,
    ASTParameter,
    ASTPrefix,
    ASTReturn,
    ASTString,
    ASTVariable,
    ASTVariableDefinition,
    ASTWhile,
    ASTNone
} ASTNodeType;

typedef struct ASTNode
{
    ASTNodeType type;

    union {
        struct {
            ASTNode* expr;
            Token token;
            DataType type_id;
        } conversion;

        struct {
            Token token;
            ASTNode* condition;
            ASTNode* body;
        } while_statement;

        struct {
            Scope* scope;
            Token operator;
            Token token;
            ASTNode* expr;
            ASTNode* symbol;
            bool initializes_binding;
        } assignment;

        struct {
            Token operator;
            ASTNode* left_expr;
            ASTNode* right_expr;
            DataType type_id;
            DataType operand_type;
        } binary;

        struct {
            bool value;
            Token token;
        } boolean_literal;

        struct {
            Vector args;
            const Builtin* builtin;
            Token token;
        } builtin_call;

        struct {
            uint32_t value;
            Token token;
        } character_literal;

        struct {
            Scope* scope;
            Vector statements;
        } compound;

        struct {
            Token token;
            ASTNode* condition;
            ASTNode* then_branch;
            ASTNode* else_branch;
            DataType type_id;
            ReferenceType reference_type;
            Vector reference_origins;
            bool expression;
        } conditional;

        struct {
            double value;
            Token token;
            DataType type_id;
        } float_literal;

        struct {
            Scope* scope;
            Vector args;
            StringObject* id;
            Token token;
            ASTNode* symbol;
            Vector reference_origins;
        } function_call;

        struct {
            Scope* scope;
            StringObject* id;
            Token token;
            Vector params;
            bool is_test;
            bool is_test_skipped;
            Token test_name;
            bool has_explicit_return_type;
            DataType return_type_id;
            ReferenceType return_reference_type;
            ASTNode* return_reference_origin;
            ASTNode* body;
            size_t call_count;
        } function_definition;

        struct {
            uint64_t value;
            Token token;
            DataType type_id;
        } integer_literal;

        struct {
            Token token;
            ASTNode* subject;
            Vector arms;
            ASTNode* default_branch;
            DataType type_id;
            ReferenceType reference_type;
            Vector reference_origins;
            bool expression;
        } match;

        struct {
            Scope* scope;
            StringObject* id;
            Token token;
            DataType type_id;
            ReferenceType reference_type;
            Vector reference_origins;
            size_t shared_access_count;
            bool exclusive_access_active;
            bool moved;
            int position;
            ASTNode* default_value;
        } parameter;

        struct {
            Token operator;
            ASTNode* expr;
            Vector reference_origins;
            size_t constant_position;
            bool evaluated;
        } prefix;

        struct {
            Token token;
            ASTNode* expr;
        } return_statement;

        struct {
            StringObject* value;
            Token token;
        } string_literal;

        struct {
            Scope* scope;
            StringObject* id;
            Token token;
            ASTNode* symbol;
        } variable;

        struct {
            Scope* scope;
            StringObject* id;
            Token token;
            bool fixed;
            bool constant;
            bool initialized;
            DataType type_id;
            ReferenceType reference_type;
            Vector reference_origins;
            size_t shared_access_count;
            bool exclusive_access_active;
            bool moved;
            bool reference_access_active;
            size_t read_count;
            size_t discarded_read_count;
            bool discarded_reads_analyzed;
            bool address_taken;
            int register_position;
            int position;
            ASTNode* expr;
        } variable_definition;
    };
} ASTNode;

ASTNode* create_ast_node(ASTNodeType type);
ASTNode* create_assignment_node(Token operator, Token token, ASTNode* expr);
ASTNode* create_binary_node(ASTNode* left_expr, ASTNode* right_expr, Token token);
ASTNode* create_boolean_node(Token token, bool value);
ASTNode* create_builtin_call_node(Token token, const Builtin* builtin);
ASTNode* create_character_node(Token token, uint32_t value);
ASTNode* create_compound_node(Scope* scope);
ASTNode* create_conditional_node(Token token, ASTNode* condition, ASTNode* then_branch, ASTNode* else_branch);
ASTNode* create_float_node(Token token, double value, DataType type_id);
ASTNode* create_function_call_node(Token token);
ASTNode* create_function_definition_node(Token token, StringObject* id);
ASTNode* create_integer_node(Token token, uint64_t value, DataType type_id);
ASTNode* create_match_node(Token token);
ASTNode* create_parameter_node(Token token, StringObject* id);
ASTNode* create_prefix_node(Token token, ASTNode* expr);
ASTNode* create_return_node(Token token, ASTNode* expr);
ASTNode* create_string_node(Token token, StringObject* value);
ASTNode* create_variable_definition_node(Token token, StringObject* id, bool fixed);
ASTNode* create_variable_node(Token token);
ASTNode* create_while_node(Token token, ASTNode* condition, ASTNode* body);
void free_ast_node(ASTNode* ast);
Scope* get_scope(const ASTNode* ast);
DataType get_node_data_type(const ASTNode* ast);
ASTNode* get_reference_origin(const ASTNode* ast);
Vector* get_reference_origins(ASTNode* ast);
ReferenceType get_reference_type(const ASTNode* ast);
bool is_expression_statement(const ASTNode* ast);
bool is_literal(const ASTNode* ast);
bool is_text_literal(const ASTNode* ast);
bool is_function_call(const ASTNode* ast);
bool is_function_definition(const ASTNode* ast);
bool is_parameter(const ASTNode* ast);
bool is_prefix(const ASTNode* ast);
bool is_prefix_operand(const ASTNode* ast);
bool is_variable(const ASTNode* ast);
bool is_variable_definition(const ASTNode* ast);
bool is_variable_type(const ASTNode* ast);
bool is_none(const ASTNode* ast);
bool is_initialized(const ASTNode* ast);
void initialize_variable(ASTNode* ast);

#endif
