#include "compiler/analyzer/tuple.h"
#include "common/tuple_type.h"
#include "compiler/analyzer/array.h"
#include "compiler/analyzer/conversion.h"
#include "compiler/analyzer/ownership.h"
#include "compiler/analyzer/reference.h"
#include "compiler/analyzer/type_alias.h"
#include "compiler/ast/binding.h"
#include "compiler/ast/function.h"
#include "compiler/ast/predicate.h"
#include "compiler/ast/query.h"
#include "compiler/ast/reference.h"
#include "compiler/diagnostics.h"
#include "compiler/scope.h"

static size_t get_tuple_binding_offset(ASTNode* ast);

static bool apply_tuple_move_effect(ASTNode* symbol, const TupleMoveEffect* move)
{
    if (move->symbol != symbol) {
        return false;
    }

    if (move->restores) {
        forget_tuple_moves(symbol, move->offset, 1);

        return true;
    }

    remove_tuple_references(symbol, move->offset, 1);
    record_tuple_move(symbol, move->offset, 1);

    return true;
}

bool apply_tuple_move_effects(ASTNode* symbol, const FunctionArrayEffects* effects)
{
    bool partial = false;

    for (size_t i = 0; i < effects->tuple_moves.count; i++) {
        bool applied = apply_tuple_move_effect(symbol, effects->tuple_moves.data[i]);
        partial = partial || applied;
    }

    return partial;
}

void validate_tuple_copy(Type type, Token token)
{
    if (has_exclusive_tuple_reference(type)) {
        semantic_error("Cannot copy a tuple containing an exclusive reference", token);
    }
}

void resolve_tuple_type(Analyzer* analyzer, Type* type)
{
    if (type->kind != TypeTuple || type->tuple->resolved) {
        return;
    }

    TupleType* tuple = (TupleType*)type->tuple;

    for (size_t i = 0; i < tuple->count; i++) {
        TupleElement* element = &tuple->elements[i];

        resolve_named_type(analyzer, &element->type, &element->reference);
        resolve_dimensions(analyzer, &element->type);
    }

    layout_tuple_type(tuple);
    push_vector_item(&analyzer->tuple_types, tuple);
}

static void analyze_tuple_element(Analyzer* analyzer, ASTNode* ast, TupleElement* element, Type expected,
    ReferenceType reference, Token token)
{
    apply_declared_type(ast, expected, reference);
    analyze_node(analyzer, ast);

    element->type = get_node_data_type(ast);
    element->reference = get_reference_type(ast);
    bool compatible = types_equal(element->type, expected);
    compatible = compatible || can_implicitly_widen_type(element->type.kind, expected.kind);

    if (element->type.kind == TypeVoid || element->type.kind == TypeUnknown) {
        semantic_error("Invalid tuple element type", token);
    }

    if (expected.kind != TypeUnknown && (!compatible || element->reference != reference)) {
        semantic_error("Tuple element type does not match", token);
    }

    if (expected.kind != TypeUnknown) {
        element->type = expected;
    }

    consume_array(ast);
}

static TupleElement get_expected_tuple_element(Type type, size_t index)
{
    if (type.kind == TypeTuple) {
        return type.tuple->elements[index];
    }

    return (TupleElement){.type = {.kind = TypeUnknown}, .reference = RefNone};
}

void analyze_tuple(Analyzer* analyzer, ASTNode* ast)
{
    size_t count = ast->array.elements.count;
    Type declared = ast->array.type_id;

    if (declared.kind != TypeUnknown && (declared.kind != TypeTuple || declared.tuple->count != count)) {
        semantic_error("Tuple element count does not match its type", ast->array.token);
    }

    TupleType* tuple = create_tuple_type(count);

    for (size_t i = 0; i < count; i++) {
        TupleElement expected = get_expected_tuple_element(declared, i);

        analyze_tuple_element(analyzer, ast->array.elements.data[i], &tuple->elements[i],
            expected.type, expected.reference, ast->array.token);
    }

    layout_tuple_type(tuple);
    push_vector_item(&analyzer->tuple_types, tuple);

    ast->array.type_id = (Type){.kind = TypeTuple, .tuple = tuple};

    for (size_t i = 0; i < count; i++) {
        collect_tuple_element_references(ast, ast->array.elements.data[i], tuple->elements[i]);
    }
}

void analyze_tuple_index(ASTNode* ast, Type type, uint64_t value)
{
    Type index_type = get_node_data_type(ast->index.index);
    bool integer = is_integer_type(index_type.kind);

    if (!integer || get_reference_type(ast->index.index) != RefNone) {
        semantic_error("Tuple index requires a compile-time integer", ast->index.token);
    }

    if (!ast->index.constant) {
        semantic_error("Tuple index must be a compile-time constant", ast->index.token);
    }

    if (value >= type.tuple->count) {
        semantic_error("Tuple index is out of bounds", ast->index.token);
    }

    ast->index.position = (size_t)value;
    TupleElement element = type.tuple->elements[value];
    ast->index.type_id = element.type;

    validate_tuple_projection(ast);
    size_t slots = get_value_slot_count(element.type);

    copy_tuple_references(ast, ast->index.array, element.offset, 0, slots);

    for (size_t i = 0; element.reference != RefNone && i < ast->tuple_references.count; i++) {
        TupleReference* access = ast->tuple_references.data[i];

        add_reference_origin(&ast->index.reference_origins, access->origin);
    }
}

static bool tuple_references_conflict(const TupleReference* previous, TupleReference access)
{
    bool exclusive = previous->reference == RefExclusive || access.reference == RefExclusive;

    return previous->origin == access.origin && exclusive;
}

static void validate_tuple_reference(ASTNode* ast, TupleReference access)
{
    size_t index = 0;

    while (index < ast->tuple_references.count
        && !tuple_references_conflict(ast->tuple_references.data[index], access)) {
        index++;
    }

    if (index == ast->tuple_references.count) {
        return;
    }

    Token token = ast->array.token;

    if (is_variable_definition(ast)) {
        token = ast->variable_definition.token;
    }

    semantic_error("Conflicting references in tuple", token);
}

static void transfer_tuple_reference_variable(ASTNode* expression)
{
    ASTNode* source = expression->variable.symbol;
    bool active = is_variable_definition(source) && source->variable_definition.reference_access_active;

    if (active) {
        release_reference_access(source);
    }

    transfer_exclusive_reference(expression);
}

static void transfer_tuple_reference(ASTNode* expression, ReferenceType reference)
{
    if (reference != RefExclusive) {
        return;
    }

    if (expression->type == ASTVariable) {
        transfer_tuple_reference_variable(expression);

        return;
    }

    if (expression->type == ASTIndex) {
        consume_tuple_element(expression);
    }
}

void collect_tuple_element_references(ASTNode* tuple, ASTNode* expression, TupleElement element)
{
    if (element.reference == RefNone) {
        size_t slots = get_value_slot_count(element.type);

        copy_tuple_references(tuple, expression, 0, element.offset, slots);

        return;
    }

    transfer_tuple_reference(expression, element.reference);

    Vector* carried = get_tuple_references(expression);

    if (carried->count) {
        size_t slots = get_tuple_element_slot_count(element);

        copy_tuple_references(tuple, expression, 0, element.offset, slots);

        return;
    }

    Vector* origins = get_reference_origins_from_expression(expression);

    for (size_t i = 0; i < origins->count; i++) {
        TupleReference access = {origins->data[i], element.reference, element.offset, SIZE_MAX};

        validate_tuple_reference(tuple, access);
        add_tuple_reference(tuple, access);
    }

}

void set_tuple_binding_references(ASTNode* binding, ASTNode* expression)
{
    Type type = get_node_data_type(binding);

    ReferenceType reference = get_reference_type(binding);
    Vector* carried = get_tuple_references(expression);

    if (type.kind != TypeTuple && (reference == RefNone || !carried->count)) {
        return;
    }

    size_t slots = get_value_slot_count(type);

    clear_tuple_references(binding);
    copy_tuple_references(binding, expression, 0, 0, slots);
}

static void activate_tuple_element_access(ASTNode* binding, TupleReference* access)
{
    Scope* origin_scope = get_scope(access->origin);
    Scope* scope = get_scope(binding);

    if (origin_scope && scope && origin_scope->level > scope->level) {
        semantic_error("Tuple reference cannot outlive its underlying storage", binding->variable_definition.token);
    }

    validate_reference_binding_access(access->origin, access->reference, binding->variable_definition.token);

    if (access->reference == RefShared) {
        (*get_shared_access_count(access->origin))++;

        return;
    }

    set_exclusive_access(access->origin, true);
}

void activate_tuple_access(ASTNode* binding)
{
    if (!binding->tuple_references.count) {
        return;
    }

    for (size_t i = 0; i < binding->tuple_references.count; i++) {
        activate_tuple_element_access(binding, binding->tuple_references.data[i]);
    }

    binding->variable_definition.reference_access_active = true;
}

static void release_tuple_element_access(TupleReference* access)
{
    if (access->reference == RefShared) {
        (*get_shared_access_count(access->origin))--;

        return;
    }

    set_exclusive_access(access->origin, false);
}

void release_tuple_access(ASTNode* binding)
{
    for (size_t i = 0; i < binding->tuple_references.count; i++) {
        release_tuple_element_access(binding->tuple_references.data[i]);
    }

    binding->variable_definition.reference_access_active = false;
}

static void initialize_tuple_parameter_element(ASTNode* parameter, TupleElement element, size_t offset)
{
    size_t position = offset + element.offset;

    if (element.reference != RefNone) {
        TupleReference access = {parameter, element.reference, position, position};

        add_tuple_reference(parameter, access);

        return;
    }

    initialize_tuple_parameter(parameter, element.type, position);
}

void initialize_tuple_parameter(ASTNode* parameter, Type type, size_t offset)
{
    if (type.kind != TypeTuple) {
        return;
    }

    for (size_t i = 0; i < type.tuple->count; i++) {
        initialize_tuple_parameter_element(parameter, type.tuple->elements[i], offset);
    }
}

static void validate_tuple_return_reference(TupleReference access, Token token)
{
    Scope* scope = get_scope(access.origin);
    bool parameter = is_parameter(access.origin);
    Type type = get_node_data_type(access.origin);
    bool reference = get_reference_type(access.origin) != RefNone
        || (type.kind == TypeTuple && access.origin_offset != SIZE_MAX);
    bool global = scope && is_top_level_scope(scope);

    if (!global && !(parameter && reference)) {
        semantic_error("Cannot return tuple containing a reference to local storage", token);
    }
}

void track_tuple_return(Analyzer* analyzer, ASTNode* ast)
{
    ASTNode* expression = ast->return_statement.expr;
    Vector* references = get_tuple_references(expression);
    ASTNode* function = analyzer->function;

    for (size_t i = 0; i < references->count; i++) {
        TupleReference access = *(TupleReference*)references->data[i];

        validate_tuple_return_reference(access, ast->return_statement.token);
        add_tuple_reference(function, access);
    }
}

static ASTNode* find_tuple_origin_argument(ASTNode* caller, ASTNode* callee, ASTNode* origin)
{
    Vector* params = &callee->function_definition.params;
    size_t index = 0;

    while (index < params->count && params->data[index] != origin) {
        index++;
    }

    if (index == params->count) {
        return NULL;
    }

    return caller->function_call.args.data[index];
}

static void resolve_tuple_call_reference(ASTNode* caller, ASTNode* callee, TupleReference access)
{
    ASTNode* argument = find_tuple_origin_argument(caller, callee, access.origin);

    if (!argument) {
        add_tuple_reference(caller, access);

        return;
    }

    Type type = get_node_data_type(argument);

    if (type.kind == TypeTuple && get_reference_type(argument) == RefNone) {
        copy_tuple_references(caller, argument, access.origin_offset, access.offset, 1);

        return;
    }

    access.origin = get_argument_reference_origin(argument);

    if (access.origin) {
        add_tuple_reference(caller, access);
    }
}

static void set_tuple_call_reference_origins(ASTNode* caller)
{
    if (get_reference_type(caller) == RefNone) {
        return;
    }

    Vector* origins = get_reference_origins(caller);

    for (size_t i = 0; i < caller->tuple_references.count; i++) {
        TupleReference* access = caller->tuple_references.data[i];

        add_reference_origin(origins, access->origin);
    }
}

void set_tuple_call_references(ASTNode* caller, ASTNode* callee)
{
    for (size_t i = 0; i < callee->tuple_references.count; i++) {
        TupleReference access = *(TupleReference*)callee->tuple_references.data[i];

        resolve_tuple_call_reference(caller, callee, access);
    }

    set_tuple_call_reference_origins(caller);
}

static size_t get_tuple_binding_offset(ASTNode* ast)
{
    if (ast->type != ASTIndex && ast->type != ASTIndexAssignment) {
        return 0;
    }

    Type type = get_node_data_type(ast->index.array);

    if (type.kind != TypeTuple || get_reference_type(ast->index.array) != RefNone) {
        semantic_error("Cannot change reference origins through a tuple reference", ast->index.token);
    }

    size_t offset = get_tuple_binding_offset(ast->index.array);

    return offset + type.tuple->elements[ast->index.position].offset;
}

static void update_tuple_index_references(ASTNode* ast)
{
    if (ast->index.operator.type != TokenEqual) {
        return;
    }

    ASTNode* owner = get_root_binding(ast->index.array);
    Vector* replacement = get_tuple_references(ast->index.value);
    Vector* previous = get_tuple_references(ast->index.array);

    if (!previous->count && !replacement->count && get_reference_type(ast) == RefNone) {
        return;
    }

    Type owner_type = get_node_data_type(owner);

    if (!is_variable_definition(owner) || owner_type.kind != TypeTuple || get_reference_type(owner) != RefNone) {
        semantic_error("Cannot change reference origins through a tuple reference", ast->index.token);
    }

    size_t offset = get_tuple_binding_offset(ast);
    Type type = get_node_data_type(ast->index.array);
    TupleElement element = type.tuple->elements[ast->index.position];
    size_t slots = get_tuple_element_slot_count(element);
    element.offset = offset;

    if (owner->variable_definition.reference_access_active) {
        release_reference_access(owner);
    }

    remove_tuple_references(owner, offset, slots);
    collect_tuple_element_references(owner, ast->index.value, element);
    activate_tuple_access(owner);
}

static void restore_tuple_element(ASTNode* ast)
{
    if (ast->index.operator.type != TokenEqual) {
        return;
    }

    ASTNode* owner = get_root_binding(ast->index.array);
    Vector* moves = get_tuple_moves(owner);

    if (!moves->count) {
        return;
    }

    size_t offset = get_tuple_binding_offset(ast);
    Type type = get_node_data_type(ast->index.array);
    TupleElement element = type.tuple->elements[ast->index.position];
    size_t slots = get_tuple_element_slot_count(element);

    forget_tuple_moves(owner, offset, slots);
}

void analyze_tuple_index_value(Analyzer* analyzer, ASTNode* ast)
{
    Type type = ast->index.type_id;
    ReferenceType reference = get_reference_type(ast);
    bool compound = ast->index.operator.type != TokenEqual;

    if (compound && reference == RefShared) {
        semantic_error("Cannot mutate through a shared tuple reference", ast->index.token);
    }

    if (compound) {
        reference = RefNone;
    }

    apply_declared_type(ast->index.value, type, reference);
    analyze_node(analyzer, ast->index.value);

    Type actual = get_node_data_type(ast->index.value);
    bool same_reference = get_reference_type(ast->index.value) == reference;
    bool compatible = types_equal(actual, type);
    compatible = compatible || (reference == RefNone && can_implicitly_widen_type(actual.kind, type.kind));

    if (!same_reference || !compatible) {
        semantic_error("Tuple element assignment type does not match", ast->index.token);
    }

    consume_array(ast->index.value);
    update_tuple_index_references(ast);
    restore_tuple_element(ast);
}

void validate_tuple_projection(ASTNode* ast)
{
    ASTNode* owner = get_root_binding(ast->index.array);

    if (!owner) {
        return;
    }

    Vector* moves = get_tuple_moves(owner);

    if (!moves->count || (ast->type == ASTIndexAssignment && ast->index.operator.type == TokenEqual)) {
        return;
    }

    size_t offset = get_tuple_binding_offset(ast);
    Type type = get_node_data_type(ast->index.array);
    TupleElement element = type.tuple->elements[ast->index.position];
    size_t slots = get_tuple_element_slot_count(element);
    bool moved = tuple_moves_overlap(moves, offset, slots);

    if (moved) {
        semantic_error("Tuple element is moved", ast->index.token);
    }
}

bool consume_tuple_element(ASTNode* ast)
{
    Type source = get_node_data_type(ast->index.array);
    ASTNode* owner = get_root_binding(ast->index.array);

    if (source.kind != TypeTuple || !owner || get_reference_type(owner) != RefNone) {
        return false;
    }

    validate_tuple_projection(ast);

    size_t offset = get_tuple_binding_offset(ast);
    TupleElement element = source.tuple->elements[ast->index.position];
    size_t slots = get_tuple_element_slot_count(element);
    bool has_reference_access = *get_shared_access_count(owner) || has_exclusive_access(owner);

    if (has_reference_access) {
        semantic_error("Cannot move an element while the tuple has active references", ast->index.token);
    }

    bool active = is_variable_definition(owner) && owner->variable_definition.reference_access_active;

    if (active) {
        release_reference_access(owner);
    }

    remove_tuple_references(owner, offset, slots);
    record_tuple_move(owner, offset, slots);

    if (active) {
        activate_tuple_access(owner);
    }

    return true;
}
