#ifndef AST_H
#define AST_H

#include "common/type.h"
#include "common/vector.h"
#include "compiler/lexer/token.h"
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
    ASTArray,
    ASTSlice,
    ASTIndex,
    ASTIndexAssignment,
    ASTAssignment,
    ASTBinary,
    ASTBuiltinCall,
    ASTBoolean,
    ASTBreak,
    ASTCharacter,
    ASTCompound,
    ASTConditional,
    ASTContinue,
    ASTConversion,
    ASTFloat,
    ASTFor,
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
            Token token;
            Vector elements;
            Type type_id;
            bool flat;
            bool fill;
        } array;

        struct {
            Token token;
            ASTNode* expr;
            ASTNode* start;
            ASTNode* end;
            Type type_id;
            ReferenceType reference_type;
            Vector reference_origins;
        } slice;

        struct {
            Token token;
            ASTNode* array;
            ASTNode* index;
            ASTNode* value;
            Token operator;
            Type type_id;
            size_t position;
            bool constant;
        } index;

        struct {
            ASTNode* expr;
            Token token;
            Type type_id;
        } conversion;

        struct {
            Token token;
            ASTNode* condition;
            ASTNode* body;
        } while_statement;

        struct {
            Token token;
            ASTNode* array;
            ASTNode* end;
            ASTNode* index;
            ASTNode* value;
            ASTNode* body;
        } for_statement;

        struct {
            Token token;
        } loop_control;

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
            Type type_id;
            Type operand_type;
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
            Type type_id;
            ReferenceType reference_type;
            Vector reference_origins;
            bool expression;
        } conditional;

        struct {
            double value;
            Token token;
            Type type_id;
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
            Vector array_moves;
            Vector array_initializations;
            Vector array_possible_initializations;
            Vector array_uninitialized_returns;
            bool is_test;
            bool is_test_skipped;
            Token test_name;
            bool has_explicit_return_type;
            Type return_type_id;
            ReferenceType return_reference_type;
            ASTNode* return_reference_origin;
            ASTNode* body;
            size_t call_count;
        } function_definition;

        struct {
            uint64_t value;
            Token token;
            Type type_id;
        } integer_literal;

        struct {
            Token token;
            ASTNode* subject;
            Vector arms;
            Type type_id;
            ReferenceType reference_type;
            Vector reference_origins;
            bool expression;
        } match;

        struct {
            Scope* scope;
            StringObject* id;
            Token token;
            Type type_id;
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
            bool possibly_initialized;
            Type type_id;
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

// Construction
ASTNode* create_ast_node(ASTNodeType type);
ASTNode* create_array_node(Token token, Type type, bool flat);
ASTNode* create_assignment_node(Token operator, Token token, ASTNode* expr);
ASTNode* create_binary_node(ASTNode* left_expr, ASTNode* right_expr, Token token);
ASTNode* create_typed_binary_node(ASTNode* left, ASTNode* right, Token token, Type operand_type, Type result_type);
ASTNode* create_boolean_node(Token token, bool value);
ASTNode* create_builtin_call_node(Token token, const Builtin* builtin);
ASTNode* create_character_node(Token token, uint32_t value);
ASTNode* create_compound_node(Scope* scope);
ASTNode* create_conditional_node(Token token, ASTNode* condition, ASTNode* then_branch, ASTNode* else_branch);
ASTNode* create_conversion_node(Token token, ASTNode* expr, Type type_id);
ASTNode* create_float_node(Token token, double value, Type type_id);
ASTNode* create_for_node(Token token);
ASTNode* create_function_call_node(Token token);
ASTNode* create_function_definition_node(Token token, StringObject* id);
ASTNode* create_integer_node(Token token, uint64_t value, Type type_id);
ASTNode* create_loop_control_node(Token token);
ASTNode* create_match_node(Token token);
ASTNode* create_parameter_node(Token token, StringObject* id);
ASTNode* create_prefix_node(Token token, ASTNode* expr);
ASTNode* create_return_node(Token token, ASTNode* expr);
ASTNode* create_slice_node(Token token, ASTNode* expr, ASTNode* start);
ASTNode* create_string_node(Token token, StringObject* value);
ASTNode* create_variable_node(Token token);
ASTNode* create_variable_definition_node(Token token, StringObject* id, bool fixed);
ASTNode* create_while_node(Token token, ASTNode* condition, ASTNode* body);

// Lifetime
void replace_ast_node(ASTNode* ast, ASTNode* replacement);
void free_ast_node(ASTNode* ast);

// Predicates
bool is_none(const ASTNode* ast);
bool is_expression_statement(const ASTNode* ast);
bool is_function_call(const ASTNode* ast);
bool is_function_definition(const ASTNode* ast);
bool is_index(const ASTNode* ast);
bool is_constant_index(const ASTNode* ast);
bool is_parameter(const ASTNode* ast);
bool is_prefix(const ASTNode* ast);
bool is_prefix_operand(const ASTNode* ast);
bool is_variable(const ASTNode* ast);
bool is_variable_definition(const ASTNode* ast);
bool is_variable_type(const ASTNode* ast);

// Queries
Scope* get_scope(const ASTNode* ast);
Type get_node_data_type(const ASTNode* ast);
ASTNode* get_root_binding(const ASTNode* ast);
ASTNode* get_index_constant_expression(ASTNode* ast);

// Literals
bool is_literal(const ASTNode* ast);
bool is_text_literal(const ASTNode* ast);
ASTNode* get_integer_literal(ASTNode* expression, bool* negative);
ASTNode* get_float_literal(ASTNode* expression);
bool is_numeric_literal(ASTNode* argument);
bool is_literal_of_type(ASTNode* ast, Type type);
void set_literal_token(ASTNode* ast, Token token);
ASTNode* copy_literal_node(const ASTNode* literal);
bool is_literal_array_tree(ASTNode* ast);
void copy_flattened_array_literals(ASTNode* ast, Vector* literals);

// Control flow
ASTNode* get_branch_result(ASTNode* branch);
ASTNode* find_node_return(ASTNode* ast);
bool is_match_alternative(ASTNode* pattern);
bool is_literal_match_pattern(ASTNode* pattern);
bool is_simple_match_value(const ASTNode* ast);
bool match_has_wildcard(ASTNode* ast);

// Reference metadata
ReferenceType get_reference_type(const ASTNode* ast);
void set_expression_reference_type(ASTNode* ast, ReferenceType type);
Vector* get_reference_origins(ASTNode* ast);
ASTNode* get_reference_origin(const ASTNode* ast);
ASTNode* get_reference_origin_or_self(ASTNode* symbol);
ASTNode* get_reference_origin_from_expression(ASTNode* ast);
Vector* get_reference_origins_from_expression(ASTNode* ast);
ASTNode* get_argument_reference_origin(ASTNode* argument);
bool contains_reference_origin(Vector* origins, ASTNode* origin);
void add_reference_origin(Vector* origins, ASTNode* origin);
void merge_reference_origins(Vector* destination, Vector* source);
void copy_reference_origins(Vector* destination, Vector* source);
size_t* get_shared_access_count(ASTNode* symbol);
bool has_exclusive_access(ASTNode* symbol);
void set_exclusive_access(ASTNode* symbol, bool active);
void apply_slice_type(ASTNode* ast, Type type, ReferenceType reference_type);
void convert_string_reference_to_slice(ASTNode* ast, Type type);

// Binding metadata
bool is_initialized(const ASTNode* ast);
void initialize_variable(ASTNode* ast);
bool is_moved(ASTNode* symbol);
void set_moved_state(ASTNode* symbol, bool moved);
void mark_address_taken(ASTNode* symbol);
void track_variable_read(ASTNode* symbol);
bool has_no_value_reads(ASTNode* ast);

#endif
