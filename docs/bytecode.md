# Wide register operands

Bytecode keeps aligned 32-bit words: an 8-bit opcode and three 8-bit operand fields. Operations addressing registers above R255 occupy two words:

```text
WIDE [A high] [B high] [C high]
OP   [A low]  [B low]  [C low]
```

`ADD_I32 R300, R511, R2` becomes `WIDE 1, 1, 0` followed by `ADD_I32 44, 255, 2`. The prefix extends register indexes to 16 bits (R0 through R65535). Compiler diagnostics retain their top-level/function context and reject more than 65536 register slots. The existing 1024-slot VM stack limit still applies to globals and active call frames.

Only register fields identified by `getOpcodeRegisterFlag` receive high bytes. All other prefix fields are zero. Immediates, global/constant/function indexes, packed fused operands, and branch offsets retain their existing encodings. The `_64` opcodes specify value width, independently of register-index width.

The VM handles `OP_WIDE` through the existing opcode switch, consumes the body word, and redispatches. Ordinary instructions execute no separate prefix check. The prefix affects only the immediately following operation. Disassembly prints both words and reconstructs the full register indexes.

Branches count 32-bit words relative to the end of the body word. Targets point to prefix words when needed. The compiler rejects offsets outside the branch opcode's signed range rather than truncating them.

Byte-based peephole rewrites skip prefixed operations and rewrites that would introduce register indexes above R255. Narrow operations retain their optimizations.

Native one-argument calls with no return value, including every `print` overload, retain their direct-argument path without reserving unused metadata slots.
