#include "compiler/bytecode_optimizer.h"
#include "common/memory.h"
#include "common/util.h"
#include "runtime/function_object.h"
#include "vm/opcode.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct OptimizationWord
{
    Opcode opcode;
    uint8_t a, b, c;
    bool removed;
    bool target;
    bool wide;
    size_t destination;
} OptimizationWord;

typedef struct KnownConstant
{
    bool known;
    bool wide;
    int64_t value;
    Opcode load_opcode;
    uint16_t operand;
} KnownConstant;

static bool uses_register_pairs(Opcode opcode)
{
    switch (opcode) {
        case OpCmpI64:
        case OpCmpU64:
        case OpCmplF64:
        case OpCmpgF64:
        case OpMov64:
        case OpLdc64:
        case OpLdg64:
        case OpLdc64Call:
        case OpLdg64Call:
        case OpStg64:
        case OpLdr64:
        case OpStr64:
        case OpF32ToF64:
        case OpF64ToF32:
        case OpF32ToI64:
        case OpF32ToU64:
        case OpF64ToI32:
        case OpF64ToU32:
        case OpF64ToI64:
        case OpF64ToU64:
        case OpI64ToF32:
        case OpI64ToF64:
        case OpU64ToF32:
        case OpU64ToF64:
        case OpI32ToF64:
        case OpU32ToF64:
        case OpAddF64:
        case OpSubF64:
        case OpMulF64:
        case OpDivF64:
        case OpRemF64:
        case OpNegF64:
        case OpRet64:
        case OpAddI64:
        case OpAddiI64:
        case OpSubI64:
        case OpSubiI64:
        case OpMulI64:
        case OpMuliI64:
        case OpIdivI64:
        case OpIdivU64:
        case OpRemI64:
        case OpRemU64:
        case OpBandI64:
        case OpBandiI64:
        case OpBorI64:
        case OpBoriI64:
        case OpBxorI64:
        case OpBxoriI64:
        case OpBnotI64:
        case OpLslI64:
        case OpLsliI64:
        case OpLsrI64:
        case OpLsriI64:
        case OpAsrI64:
        case OpAsriI64:
        case OpNegI64:
            return true;
        default: return false;
    }
}

static bool is_branch(Opcode opcode)
{
    return opcode >= OpBz && opcode <= OpJmp;
}

static bool ends_block(Opcode opcode)
{
    bool branch = is_branch(opcode);

    return branch || opcode == OpHlt || opcode == OpRet || opcode == OpRetv || opcode == OpRet64;
}

static int branch_offset(const OptimizationWord* word)
{
    if (word->opcode == OpBz || word->opcode == OpBnz) {
        return (int16_t)(word->b << 8 | word->c);
    }

    if (word->opcode != OpJmp) {
        return (int8_t)word->c;
    }

    uint32_t offset = word->a << 16 | word->b << 8 | word->c;

    return (int32_t)(offset ^ 0x800000u) - 0x800000;
}

static void decode_words(const CodeObject* code, OptimizationWord* words, size_t count)
{
    for (size_t i = 0; i < count; i++) {
        const uint8_t* bytes = code->data + i * INSTRUCTION_SIZE;
        OptimizationWord* word = &words[i];

        word->opcode = bytes[0];
        word->a = bytes[1];
        word->b = bytes[2];
        word->c = bytes[3];
        word->wide = i > 0 && words[i - 1].opcode == OpWide;
        word->destination = count;
    }

    for (size_t i = 0; i < count; i++) {
        OptimizationWord* word = &words[i];
        bool branch = is_branch(word->opcode);

        if (!branch) {
            continue;
        }

        int offset = branch_offset(word);
        size_t destination = i + 1 + offset;

        word->destination = destination;
        words[destination].target = true;
    }
}

static Opcode immediate_opcode(Opcode opcode)
{
    switch (opcode) {
        case OpAddI64:    return OpAddiI64;
        case OpAddI32:    return OpAddiI32;
        case OpAddI16:    return OpAddiI16;
        case OpAddI8:     return OpAddiI8;
        case OpSubI64:    return OpSubiI64;
        case OpSubI32:    return OpSubiI32;
        case OpSubI16:    return OpSubiI16;
        case OpSubI8:     return OpSubiI8;
        case OpMulI64:    return OpMuliI64;
        case OpMulI32:    return OpMuliI32;
        case OpMulI16:    return OpMuliI16;
        case OpMulI8:     return OpMuliI8;
        case OpBandI64:   return OpBandiI64;
        case OpBandI32:   return OpBandiI32;
        case OpBandI16:   return OpBandiI16;
        case OpBandI8:    return OpBandiI8;
        case OpBorI64:    return OpBoriI64;
        case OpBorI32:    return OpBoriI32;
        case OpBorI16:    return OpBoriI16;
        case OpBorI8:     return OpBoriI8;
        case OpBxorI64:   return OpBxoriI64;
        case OpBxorI32:   return OpBxoriI32;
        case OpBxorI16:   return OpBxoriI16;
        case OpBxorI8:    return OpBxoriI8;
        case OpLslI64:    return OpLsliI64;
        case OpLslI32:    return OpLsliI32;
        case OpLslI16:    return OpLsliI16;
        case OpLslI8:     return OpLsliI8;
        case OpLsrI64:    return OpLsriI64;
        case OpLsrI32:    return OpLsriI32;
        case OpLsrI16:    return OpLsriI16;
        case OpLsrI8:     return OpLsriI8;
        case OpAsrI64:    return OpAsriI64;
        case OpAsrI32:    return OpAsriI32;
        case OpAsrI16:    return OpAsriI16;
        case OpAsrI8:     return OpAsriI8;
        default:            return OpHlt;
    }
}

static bool invalidates_constants(Opcode opcode)
{
    bool end = ends_block(opcode);

    return end || opcode == OpCall || opcode == OpStgCall || opcode == OpStg || opcode == OpStg64
        || opcode == OpStr || opcode == OpStr64
        || opcode == OpRefl || opcode == OpRefg || opcode == OpWide;
}

static void select_immediate_arithmetic(OptimizationWord* word, const KnownConstant* known)
{
    KnownConstant right = known[word->c];
    Opcode immediate = immediate_opcode(word->opcode);
    bool signed_operand = word->opcode >= OpAddI64 && word->opcode <= OpMuliI8;
    int64_t minimum = signed_operand ? INT8_MIN : 0;
    int64_t maximum = signed_operand ? INT8_MAX : UINT8_MAX;
    bool pairs = uses_register_pairs(word->opcode);

    if (immediate == OpHlt || !right.known || right.wide != pairs
        || right.value < minimum || right.value > maximum) {
        return;
    }

    word->opcode = immediate;
    word->c = right.value;
}

static void select_constant_move(OptimizationWord* word, const KnownConstant* known)
{
    KnownConstant value = known[word->b];
    bool move = word->opcode == OpMov || word->opcode == OpMov64;
    bool wide = word->opcode == OpMov64;

    if (!move || !value.known || value.wide != wide) {
        return;
    }

    word->opcode = value.load_opcode;
    word->b = value.operand >> 8;
    word->c = value.operand;
}

static void invalidate_known_register(KnownConstant* known, size_t reg)
{
    if (reg >= 256) {
        return;
    }

    known[reg] = (KnownConstant){0};

    if (reg > 0 && known[reg - 1].wide) {
        known[reg - 1] = (KnownConstant){0};
    }
}

static int64_t read_constant(const ModuleObject* module, Opcode opcode, size_t position)
{
    if (opcode == OpLdc64) {
        return read_i64(module->constants.data, position);
    }

    return module->constants.data[position].signed_value;
}

static void track_constant(const ModuleObject* module, const OptimizationWord* word, KnownConstant* known)
{
    KnownConstant value = {0};
    OpcodeFlag flags = get_opcode_flags(word->opcode);
    bool pairs = uses_register_pairs(word->opcode);

    if (word->opcode == OpLdi) {
        value.known = true;
        value.value = (int16_t)(word->b << 8 | word->c);
    }

    if (word->opcode == OpLdc || word->opcode == OpLdc64) {
        size_t position = word->b << 8 | word->c;

        value.known = true;
        value.wide = pairs;
        value.value = read_constant(module, word->opcode, position);
    }

    if (value.known) {
        value.load_opcode = word->opcode;
        value.operand = word->b << 8 | word->c;
    }

    if (word->opcode == OpMov || word->opcode == OpMov64) {
        value = known[word->b];
        value.known = value.known && value.wide == pairs;
    }

    if (!(flags & OpFlagWritesA)) {
        return;
    }

    invalidate_known_register(known, word->a);

    if (pairs) {
        invalidate_known_register(known, word->a + 1);
    }

    if (pairs && word->a == UINT8_MAX) {
        return;
    }

    known[word->a] = value;
}

static bool supports_wide_constants(Opcode opcode)
{
    if (opcode == OpLdc64 || opcode == OpMov64) {
        return true;
    }

    Opcode immediate = immediate_opcode(opcode);

    return immediate != OpHlt;
}

static void optimize_constant_word(const ModuleObject* module, OptimizationWord* word, KnownConstant* known)
{
    bool invalidate = invalidates_constants(word->opcode);
    bool pairs = uses_register_pairs(word->opcode);
    bool supported = supports_wide_constants(word->opcode);
    bool barrier = word->wide || invalidate || (pairs && !supported);

    if (word->target || barrier) {
        memset(known, 0, 256 * sizeof(KnownConstant));
    }

    if (barrier) {
        return;
    }

    select_constant_move(word, known);
    select_immediate_arithmetic(word, known);
    track_constant(module, word, known);
}

static void optimize_constants(const ModuleObject* module, OptimizationWord* words, size_t count)
{
    KnownConstant known[256] = {0};

    for (size_t i = 0; i < count; i++) {
        optimize_constant_word(module, &words[i], known);
    }
}

static bool is_wide_immediate_arithmetic(Opcode opcode)
{
    switch (opcode) {
        case OpAddiI64:
        case OpSubiI64:
        case OpMuliI64:
        case OpBandiI64:
        case OpBoriI64:
        case OpBxoriI64:
        case OpLsliI64:
        case OpLsriI64:
        case OpAsriI64:
            return true;
        default: return false;
    }
}

static bool update_wide_liveness(OptimizationWord* word, bool* live)
{
    bool immediate = is_wide_immediate_arithmetic(word->opcode);
    bool pairs = uses_register_pairs(word->opcode);
    Opcode selected = immediate_opcode(word->opcode);
    bool binary = pairs && selected != OpHlt;
    bool constant = word->opcode == OpLdc64;
    bool load = constant || word->opcode == OpLdg64;
    bool move = word->opcode == OpMov64;

    if (word->wide || word->a == UINT8_MAX || (!immediate && !binary && !load && !move)) {
        return false;
    }

    if ((!load && word->b == UINT8_MAX) || (binary && word->c == UINT8_MAX)) {
        return false;
    }

    word->removed = constant && !word->target && !live[word->a] && !live[word->a + 1];
    live[word->a] = false;
    live[word->a + 1] = false;

    if (!load) {
        live[word->b] = true;
        live[word->b + 1] = true;
    }

    if (binary) {
        live[word->c] = true;
        live[word->c + 1] = true;
    }

    return true;
}

static bool update_builtin_call_liveness(const ModuleObject* module, const OptimizationWord* word, bool* live)
{
    if (word->opcode != OpCall || word->wide) {
        return false;
    }

    size_t position = word->b << 8 | word->c;
    const FunctionObject* function = module->functions.data[position];
    int argument_end = word->a + function->param_count;

    if (function->type != FuncBuiltin || argument_end > 256) {
        return false;
    }

    for (int i = 0; i < function->return_count; i++) {
        int result = (int)word->a - 2 + i;

        live[result] = false;
    }

    for (int reg = word->a; reg < argument_end; reg++) {
        live[reg] = true;
    }

    return true;
}

static bool update_global_store_liveness(const OptimizationWord* word, bool* live)
{
    if (word->wide || (word->opcode != OpStg && word->opcode != OpStg64)) {
        return false;
    }

    int slots = word->opcode == OpStg64 ? 2 : 1;

    if (word->a + slots > 256) {
        return false;
    }

    for (int i = 0; i < slots; i++) {
        live[word->a + i] = true;
    }

    return true;
}

static void update_liveness(const ModuleObject* module, OptimizationWord* word, bool* live,
    size_t persistent_registers)
{
    bool wide = update_wide_liveness(word, live);

    if (wide) {
        return;
    }

    bool is_builtin_call = update_builtin_call_liveness(module, word, live);

    if (is_builtin_call) {
        return;
    }

    bool store = update_global_store_liveness(word, live);

    if (store) {
        return;
    }

    OpcodeRegisterFlag registers = get_opcode_register_flag(word->opcode);
    OpcodeFlag flags = get_opcode_flags(word->opcode);
    bool pairs = uses_register_pairs(word->opcode);
    bool barrier = invalidates_constants(word->opcode);
    bool terminal = word->opcode == OpHlt || word->opcode == OpRet || word->opcode == OpRetv;

    if (terminal) {
        for (size_t reg = 0; reg < 256; reg++) {
            live[reg] = reg < persistent_registers;
        }
    }

    if (word->wide || pairs || (barrier && !terminal)) {
        memset(live, 1, 256 * sizeof(bool));

        return;
    }

    bool load = word->opcode == OpLdi || word->opcode == OpLdc;

    word->removed = word->opcode == OpNop || (load && !word->target && !live[word->a]);

    if (flags & OpFlagWritesA) {
        live[word->a] = false;
    }

    if ((registers & OpRegisterA) && !(flags & OpFlagWritesA)) {
        live[word->a] = true;
    }

    if (registers & OpRegisterB) {
        live[word->b] = true;
    }

    if (registers & OpRegisterC) {
        live[word->c] = true;
    }
}

static void remove_dead_loads(const ModuleObject* module, OptimizationWord* words, size_t count,
    size_t persistent_registers)
{
    bool live[256];

    memset(live, 1, sizeof(live));

    for (size_t end = count; end > 0; end--) {
        OptimizationWord* word = &words[end - 1];

        update_liveness(module, word, live, persistent_registers);

        if (word->target) {
            memset(live, 1, sizeof(live));
        }
    }
}

static bool is_immediate_arithmetic(Opcode opcode)
{
    switch (opcode) {
        case OpSextI32:
        case OpZextI32:
        case OpAddiI32:
        case OpAddiI16:
        case OpAddiI8:
        case OpSubiI32:
        case OpSubiI16:
        case OpSubiI8:
        case OpMuliI32:
        case OpMuliI16:
        case OpMuliI8:
        case OpBandiI32:
        case OpBandiI16:
        case OpBandiI8:
        case OpBoriI32:
        case OpBoriI16:
        case OpBoriI8:
        case OpBxoriI32:
        case OpBxoriI16:
        case OpBxoriI8:
        case OpLsliI32:
        case OpLsliI16:
        case OpLsliI8:
        case OpLsriI32:
        case OpLsriI16:
        case OpLsriI8:
        case OpAsriI32:
        case OpAsriI16:
        case OpAsriI8:
            return true;
        default: return false;
    }
}

static void remove_immediate_operand_move(OptimizationWord* move, OptimizationWord* arithmetic)
{
    bool wide = is_wide_immediate_arithmetic(arithmetic->opcode);
    bool immediate = is_immediate_arithmetic(arithmetic->opcode) || wide;
    Opcode move_opcode = OpMov;

    if (wide) {
        move_opcode = OpMov64;
    }

    if (!immediate || move->opcode != move_opcode || move->target || move->wide
        || arithmetic->target || arithmetic->wide || move->a != arithmetic->a
        || move->a != arithmetic->b) {
        return;
    }

    arithmetic->b = move->b;
    move->removed = true;
}

static void remove_immediate_operand_moves(OptimizationWord* words, size_t count)
{
    size_t previous = count;

    for (size_t i = 0; i < count; i++) {
        OptimizationWord* word = &words[i];

        if (word->removed) {
            continue;
        }

        if (previous != count) {
            remove_immediate_operand_move(&words[previous], word);
        }

        previous = i;
    }
}

static bool small_load(const OptimizationWord* word)
{
    int operand = word->b << 8 | word->c;

    if (word->opcode == OpLdg || word->opcode == OpLdc) {
        return operand <= (int)UINT8_MAX;
    }

    if (word->opcode != OpLdi) {
        return false;
    }

    int16_t immediate = operand;

    return immediate >= INT8_MIN && immediate <= INT8_MAX;
}

static Opcode load_pair_opcode(Opcode first, Opcode second)
{
    if (first == OpLdi && second == OpLdi) {
        return OpLdiLdi;
    }

    if (first == OpLdc && second == OpLdc) {
        return OpLdcLdc;
    }

    if (first == OpLdg && second == OpLdg) {
        return OpLdgLdg;
    }

    if (first == OpLdi && second == OpLdg) {
        return OpLdiLdg;
    }

    if (first == OpLdg && second == OpLdi) {
        return OpLdgLdi;
    }

    return OpNop;
}

static bool fuse_loads(OptimizationWord* first, OptimizationWord* second)
{
    bool first_small = small_load(first);
    bool second_small = small_load(second);
    Opcode opcode = load_pair_opcode(first->opcode, second->opcode);

    if (first->a + 1 != second->a || !first_small || !second_small || opcode == OpNop) {
        return false;
    }

    first->opcode = opcode;
    first->b = first->c;
    first->c = second->c;

    return true;
}

static bool fuse_move_load(OptimizationWord* first, OptimizationWord* second)
{
    bool small = small_load(second);

    if (first->opcode != OpMov || first->a + 1 != second->a || !small) {
        return false;
    }

    switch (second->opcode) {
        case OpLdi:
            first->opcode = OpMovLdi;
            break;
        case OpLdc:
            first->opcode = OpMovLdc;
            break;
        case OpLdg:
            first->opcode = OpMovLdg;
            break;
        default:
            return false;
    }

    first->c = second->c;

    return true;
}

static Opcode load_call_opcode(Opcode opcode)
{
    switch (opcode) {
        case OpLdi:    return OpLdiCall;
        case OpLdc:    return OpLdcCall;
        case OpLdg:    return OpLdgCall;
        case OpLdc64: return OpLdc64Call;
        case OpLdg64: return OpLdg64Call;
        case OpLdr:    return OpLdrCall;
        default:        return OpHlt;
    }
}

static bool fuse_load_call(OptimizationWord* first, OptimizationWord* second)
{
    Opcode fused = load_call_opcode(first->opcode);
    uint16_t function = second->b << 8 | second->c;
    int operand = first->b << 8 | first->c;

    if (first->opcode == OpLdr) {
        operand = first->b;
    }

    if (first->opcode == OpLdi) {
        operand = (int16_t)operand;
    }

    int minimum = first->opcode == OpLdi ? LOAD_CALL_SIGNED_OPERAND_MIN : 0;
    int maximum = first->opcode == OpLdi ? LOAD_CALL_SIGNED_OPERAND_MAX : LOAD_CALL_OPERAND_MASK;

    if (second->opcode != OpCall || fused == OpHlt || first->a != second->a
        || function > LOAD_CALL_FUNCTION_MASK || operand < minimum || operand > maximum) {
        return false;
    }

    uint16_t packed = (operand & LOAD_CALL_OPERAND_MASK) << LOAD_CALL_FUNCTION_BITS | function;

    first->opcode = fused;
    first->b = packed >> 8;
    first->c = packed;

    return true;
}

static bool fuse_store_call(OptimizationWord* first, OptimizationWord* second)
{
    if (first->opcode != OpStg || second->opcode != OpCall || first->a != second->a
        || first->b != 0 || second->b != 0) {
        return false;
    }

    first->opcode = OpStgCall;
    first->b = first->c;
    first->c = second->c;

    return true;
}

static bool fuse_pair(const ModuleObject* module, OptimizationWord* first, OptimizationWord* second)
{
    bool loads = fuse_loads(first, second);

    if (loads) {
        return true;
    }

    bool call = fuse_load_call(first, second);

    if (call) {
        return true;
    }

    bool store_call = fuse_store_call(first, second);

    if (store_call) {
        return true;
    }

    bool move_load = fuse_move_load(first, second);

    if (move_load) {
        return true;
    }

    if (first->opcode == OpMov && second->opcode == OpMov && first->a + 1 == second->a) {
        first->opcode = OpMovMov;
        first->c = second->b;

        return true;
    }

    bool small = small_load(first);

    if (first->opcode == OpLdi && second->opcode == OpStg && first->a == second->a
        && second->b == 0 && small) {
        first->opcode = OpStgi;
        first->b = first->c;
        first->c = second->c;

        return true;
    }

    if (first->opcode != OpCall || second->opcode != OpCall || first->a + 1 != second->a
        || first->b != 0 || second->b != 0) {
        return false;
    }

    const FunctionObject* function = module->functions.data[first->c];

    if (function->type != FuncBuiltin) {
        return false;
    }

    first->opcode = OpCallCall;
    first->b = first->c;
    first->c = second->c;

    return true;
}

static void fuse_instructions(const ModuleObject* module, OptimizationWord* words, size_t count)
{
    size_t previous = count;

    for (size_t i = 0; i < count; i++) {
        OptimizationWord* word = &words[i];

        if (word->removed) {
            continue;
        }

        if (word->target || word->wide || word->opcode == OpWide) {
            previous = count;
        }

        if (previous != count) {
            word->removed = fuse_pair(module, &words[previous], word);
        }

        previous = word->removed ? count : i;

        if (word->wide || word->opcode == OpWide) {
            previous = count;
        }
    }
}

static bool is_duplicate_instruction(const OptimizationWord* previous, const OptimizationWord* word)
{
    OpcodeFlag flags = get_opcode_flags(word->opcode);
    OpcodeRegisterFlag registers = get_opcode_register_flag(word->opcode);
    bool independent = (flags & OpFlagWritesA) && !(registers & (OpRegisterB | OpRegisterC));

    return independent && previous && previous->opcode == word->opcode
        && previous->a == word->a && previous->b == word->b && previous->c == word->c
        && !previous->wide && !word->wide && !word->target;
}

static bool is_redundant_global_load(const OptimizationWord* previous, const OptimizationWord* word)
{
    bool matching_opcodes = previous && ((previous->opcode == OpStg && word->opcode == OpLdg)
        || (previous->opcode == OpStg64 && word->opcode == OpLdg64));

    return matching_opcodes && previous->a == word->a && previous->b == word->b && previous->c == word->c
        && !previous->wide && !word->wide && !word->target;
}

static OptimizationWord* remove_redundant_instruction(OptimizationWord* previous, OptimizationWord* word)
{
    if (word->removed) {
        return previous;
    }

    if (is_duplicate_instruction(previous, word) || is_redundant_global_load(previous, word)) {
        word->removed = true;

        return previous;
    }

    return word;
}

static void remove_redundant_instructions(OptimizationWord* words, size_t count)
{
    OptimizationWord* previous = NULL;

    for (size_t i = 0; i < count; i++) {
        previous = remove_redundant_instruction(previous, &words[i]);
    }
}

static void relocate_branch(OptimizationWord* word, size_t position, const size_t* positions)
{
    int offset = (int)positions[word->destination] - (int)position - 1;

    if (word->opcode == OpJmp) {
        word->a = offset >> 16;
    }

    if (word->opcode == OpJmp || word->opcode == OpBz || word->opcode == OpBnz) {
        word->b = offset >> 8;
    }

    word->c = offset;
}

static void write_words(CodeObject* code, OptimizationWord* words, size_t count, size_t* positions)
{
    size_t output = 0;

    for (size_t i = 0; i <= count; i++) {
        positions[i] = output;
        output += i < count && !words[i].removed;
    }

    clear_code_object(code);

    for (size_t i = 0; i < count; i++) {
        OptimizationWord* word = &words[i];
        bool branch = is_branch(word->opcode);

        if (word->removed) {
            continue;
        }

        if (branch) {
            relocate_branch(word, positions[i], positions);
        }

        push_byte(code, word->opcode);
        push_byte(code, word->a);
        push_byte(code, word->b);
        push_byte(code, word->c);
    }
}

void optimize_bytecode(const ModuleObject* module, CodeObject* code, size_t persistent_registers)
{
    size_t count = code->count / INSTRUCTION_SIZE;

    if (!count) {
        return;
    }

    OptimizationWord* words = allocate_zeroed_memory(count + 1, sizeof(OptimizationWord));
    size_t* positions = allocate_memory((count + 1) * sizeof(size_t));

    decode_words(code, words, count);
    optimize_constants(module, words, count);
    remove_dead_loads(module, words, count, persistent_registers);
    remove_immediate_operand_moves(words, count);
    fuse_instructions(module, words, count);
    remove_redundant_instructions(words, count);
    write_words(code, words, count, positions);
    free_memory(words);
    free_memory(positions);
}
