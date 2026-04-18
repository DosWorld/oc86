# Oberon-07 Language — Compiler Rules

Rules for how Oberon-07 source is compiled to 8086 machine code.
See also: [Implementation rules](implementation-rules.md), [RDOFF2 spec](rdoff2-spec.md).

---

## Language dialect: Oberon-07 (with extensions)

Wirth 2007 revision plus practical extensions for DOS targets. Key differences from Oberon-2:
- No type-bound procedures (no methods)
- `FLOOR(x)` built-in (standard Oberon-07); `ENTIER(x)` accepted as synonym
- `FLOAT(x)` accepted as synonym for `REAL(x)`
- Integer operands are implicitly coerced to REAL/LONGREAL in binary expressions (`* / + - = # < > <= >=`); assignment still requires an explicit `REAL(x)` cast
- `FOR` loop with optional `BY` and multi-branch `ELSIF`
- `WHILE` loop has `ELSIF` branches
- No `WITH` statement
- `IMPORT` must list module names explicitly
- `LONGINT` = signed 32-bit integer (extension; uses DX:AX pair)
- `REAL` = 32-bit IEEE 754 float (extension; uses x87 FPU)
- `LONGREAL` = 64-bit IEEE 754 float (extension; uses x87 FPU)
- `SET` = 16-bit (fits in one word)
- Modules using `REAL` or `LONGREAL` require an x87 FPU coprocessor

Grammar reference: `docs/oberon07.ebnf`

### Identifier case sensitivity

**All identifiers are case-sensitive.** `Foo`, `foo`, and `FOO` are three distinct names.
This applies uniformly to: procedure names, variable names, constant names, type names,
parameter names, field names, module names, and module aliases.

Keywords (`BEGIN`, `END`, `VAR`, etc.) are all uppercase; their lowercase or mixed-case
variants (e.g. `begin`, `Begin`) are legal identifiers, not keywords.

Context-sensitive identifiers used as modifiers (`FAR`, `NEAR`, `EXTERNAL`) are also
matched case-sensitively and must appear in uppercase.

---

## Target: MS-DOS Real Mode

- **CPU:** 8086/8088 — NO 286+ instructions (except LES which is 8086 ✓)
- **FPU:** 8087 — NO 287+ instructions
- **Mode:** Real mode, 16-bit
- **Memory model:** Large model — CS≠DS≠SS at all times (see section below).
- **No DOS extender** — pure real mode, INT 21h for all OS calls
- Each procedure address must be aligned to 2.
- All pointers are FAR (4 bytes: offset word, segment word)

---

## Large memory model rules  ← CRITICAL for codegen

The linker produces a **large model** DOS executable.  The three segment registers have
distinct values at all times during execution:

```
CS  = current module's code segment paragraph    (differs per module)
DS  = combined data segment paragraph            (same for all modules)
SS  = stack segment paragraph                    (separate from DS)
```

**`CS ≠ DS ≠ SS`** — All three are distinct at all times.  Code that assumes any two
are equal (as in tiny/small/compact models) is WRONG for this target.

### Segment register usage at codegen level

| Variable kind       | Where stored              | Segment to push for VAR param |
|---------------------|---------------------------|-------------------------------|
| Local (stack)       | SS:[BP+ofs]               | `PUSH SS`                     |
| Global (data seg)   | DS:[data_ofs]             | `PUSH DS`                     |
| Uplevel static link | SS:[outer_BP+ofs]         | `PUSH SS`                     |
| Heap object         | arbitrary far segment:ofs | already a far ptr (ES:BX)     |

### Codegen rules derived from large model

1. **VAR parameter actual argument**: push `{segment, offset}` as 4 bytes.
   - Local var → `PUSH SS; PUSH AX`
   - Global var → `PUSH DS; PUSH AX`
   - Uplevel var (via static link) → `PUSH SS; PUSH AX` (outer frame is on the same stack)

2. **Array element access in data segment**: use `MOV DX,DS; MOV ES,DX` to set ES=DS
   before accessing via the `ES:[BX]` convention.  **Do not assume ES=DS.**

3. **`SYSTEM.GET(addr, var)` / `SYSTEM.PUT(addr, val)`**: addr is a full `ADDRESS`
   (far pointer, any segment — heap, stack, data).  Codegen: load DX:AX via
   `cg_load_item`, then `MOV ES,DX; MOV BX,AX`; access via `ES:[BX]`.
   DS is never modified.

4. **`SYSTEM.MOVE(src, dst, n)` / `SYSTEM.FILL(dst, n, byte)`**: src and dst are
   full `ADDRESS` values (any segment — heap, stack, data).  `MOVE`: saves DS
   via `PUSH DS`, sets `DS=src_seg`, runs `REP MOVSB` (DS:SI→ES:DI), then
   `POP DS` to restore.  `FILL`: sets `ES=dst_seg`, `DI=dst_ofs`, runs `REP STOSB`;
   DS is never modified.

5. **`SYSTEM.SEG(v)`**: returns SS for locals, DS for globals.

6. **`SYSTEM_INIT`** (in SYSTEM.ASM): sets DS via a SEGRELOC-patched paragraph
   constant (`mov ax, seg segd_mark; mov ds, ax`).  This runs at program entry
   before any Oberon module body executes — after `SYSTEM_INIT` returns, DS is
   valid for all modules.

7. **Module `__init` guards**: the guard word is in CS (code segment) and accessed
   with a `CS:` override prefix (`2E`) + RELOC.  This avoids touching DS/SS
   before `SYSTEM_INIT` has set them up.

8. **Stack-based FPU temporaries**: `cg_fpush`/`cg_fpop` use SS-relative SP
   (`MOV BX,SP` then SS: prefix) because DS ≠ SS and the stack is in SS.

---

## Type sizes  (COMPLETE — do not deviate)

| Type      | Size | Align | Notes                                                  |
|-----------|------|-------|--------------------------------------------------------|
| BOOLEAN   | 1    | 1     | 0=FALSE, 1=TRUE                                        |
| CHAR      | 1    | 1     | ASCII                                                  |
| BYTE      | 1    | 1     | unsigned 0..255                                        |
| INTEGER   | 2    | 2     | signed −32768..32767                                   |
| LONGINT   | 4    | 2     | signed −2147483648..2147483647; DX:AX pair             |
| REAL      | 4    | 2     | IEEE 754 single precision; FPU ST(0)                   |
| LONGREAL  | 8    | 2     | IEEE 754 double precision; FPU ST(0)                   |
| SET       | 2    | 2     | elements 0..15 (one word)                              |
| POINTER   | 4    | 2     | FAR: {offset:word, segment:word}, offset first         |
| PROC ptr (FAR)  | 4 | 2 | FAR proc variable: {offset:word, segment:word}         |
| PROC ptr (NEAR) | 2 | 2 | NEAR proc variable: {offset:word} only                 |

`SZ_POINTER=4`, `SZ_LONGINT=4`, `SZ_REAL=4`, `SZ_LONGREAL=8`, `SZ_SET=2` in `symbols.h`.
NIL = 0000:0000 (both words zero).

**TF_ form constants** (symbols.h):
```
TF_INTEGER=0  TF_BOOLEAN=1  TF_CHAR=2   TF_BYTE=3   TF_SET=4
TF_NILTYPE=5  TF_NOTYPE=6   TF_ARRAY=7  TF_RECORD=8 TF_POINTER=9
TF_PROC=10    TF_ADDRESS=11 TF_LONGINT=12
TF_REAL=14    /* 32-bit float; FPU FLD/FSTP dword */
TF_LONGREAL=15/* 64-bit float; FPU FLD/FSTP qword */
```

Note: TF_ADDRESS=11 is SYSTEM.ADDRESS (4-byte far pointer, compatible with any POINTER or LONGINT).
TF value 13 is reserved.

**Multi-dimensional array syntax:** `ARRAY m, n OF T` is parsed and automatically desugared
to `ARRAY m OF ARRAY n OF T` (nested arrays). Up to 7 dimensions supported. The resulting
type tree is identical to the manually-written nested form — same ABI, same codegen.

---

## Calling convention: Pascal (callee cleans)  ← CRITICAL

**Argument push order:** LEFT TO RIGHT — first param pushed first, lands deepest.

There are two calling conventions depending on the procedure's `is_far` flag:

### NEAR procedure (`is_far=0`)

NEAR call (`E8 rel16`) pushes only a 2-byte return IP. Stack layout:
```
[BP + 0]              saved BP
[BP + 2]              return IP (2 bytes — NEAR return address only)
[BP + 4]              last param (pushed last = topmost)
[BP + 4 + ...]        first param (pushed first = deepest)
[BP - 2]              first local variable
```

Epilogue: `RET` (C3) or `RET N` (C2 nn nn) — pops IP only.

### FAR procedure (`is_far=1`)

FAR call (`0E PUSH CS + E8 rel16` for intra-module, or `9A` inter-module) pushes 4-byte return address (IP:CS). Stack layout:
```
[BP + 0]              saved BP
[BP + 2]              return IP (2 bytes)
[BP + 4]              return CS (2 bytes)
[BP + 6]              last param (pushed last = topmost)
[BP + 6 + ...]        first param (pushed first = deepest)
[BP - 2]              first local variable
```

Epilogue: `RETF` (CB) or `RETF N` (CA nn nn) — pops IP then CS.

### Nested procedure with static link (`has_sl=1`)

The caller pushes a hidden **static link** (BP of defining scope) AFTER all params. For NEAR nested procs, this sits at `[BP+4]` and params are shifted to `[BP+6]`. For FAR nested procs (uncommon), SL would be at `[BP+6]` and params at `[BP+8]`.

Summary table:

| Convention | Return addr | Params start | SL location (if has_sl) |
|------------|-------------|--------------|------------------------|
| NEAR, no SL | `[BP+2]` IP | `[BP+4]` | — |
| NEAR, has SL | `[BP+2]` IP | `[BP+6]` | `[BP+4]` |
| FAR, no SL | `[BP+2]` IP + `[BP+4]` CS | `[BP+6]` | — |
| FAR, has SL | `[BP+2]` IP + `[BP+4]` CS | `[BP+8]` | `[BP+6]` |

The formula in `type_calc_arg_size()`:
```c
ofs = (pt->is_far ? 6 : (pt->has_sl ? 6 : 4)) + total;
```

For 4-byte params (POINTER, VAR param address): offset word at `[BP+n]`, segment at `[BP+n+2]`.
`LES BX, [BP+4]` loads offset→BX, segment→ES correctly (NEAR proc, no SL).

**VAR param address encoding (large model):**  Caller pushes `{segment, offset}` = 4 bytes.
- Local var actual: `PUSH SS; PUSH AX`  (SS = stack segment)
- Global var actual: `PUSH DS; PUSH AX`  (DS = data segment)
**Never** push SS for a global var — DS ≠ SS in the large model.

**Prologue** (always 7 bytes, same for NEAR and FAR):
```asm
55          PUSH BP
8B EC       MOV  BP, SP
81 EC ww ww SUB  SP, imm16   ; 4-byte form even if localSize=0; never SUB SP,0
```

**NEAR epilogue:**
```asm
8B E5       MOV  SP, BP
5D          POP  BP
C2 nn nn    RET  imm16    ; callee-cleans (argBytes > 0)
C3          RET            ; no args
```

**FAR epilogue:**
```asm
8B E5       MOV  SP, BP
5D          POP  BP
CA nn nn    RETF imm16    ; callee-cleans (argBytes > 0)
CB          RETF           ; no args
```

**`EmitCleanArgs` does NOT exist** — caller emits NOTHING after a CALL.

**Return values:**
- Scalar (BOOLEAN/CHAR/BYTE/INTEGER/SET) → AX
- LONGINT → DX:AX (lo word in AX, hi word in DX)
- FAR POINTER / FAR PROC ptr → DX:AX (offset in AX, segment in DX)
- NEAR PROC ptr → AX only (offset; same code segment assumed)
- Records/arrays by value → hidden VAR param

**Callee-saved:** BP, SI, DI  
**Scratch:** AX, BX, CX, DX, ES

---

## Open-array parameters  ← CRITICAL ABI

An `ARRAY OF T` formal (with no fixed length, i.e. `type->len = -1`) is an
**open-array parameter**.  It occupies **6 bytes** on the stack:

```
[BP+adr+0]  offset word   } far pointer to first element
[BP+adr+2]  segment word  }   (LES BX,[BP+adr] loads it)
[BP+adr+4]  LEN word      hidden length parameter
```

**Caller push order** (Pascal left-to-right; first pushed = deepest = highest BP offset):

```asm
; actual arg is an array; LEN=N, far ptr = SEG:OFS
PUSH N        ; LEN  (pushed first → [BP+adr+4])
PUSH SEG      ; segment word  → [BP+adr+2]
PUSH OFS      ; offset word   → [BP+adr+0]
```

**Cases for the actual argument:**

| Actual arg kind | LEN | Segment pushed | Offset pushed |
|-----------------|-----|----------------|---------------|
| Global fixed array | compile-time `type->len` | `PUSH DS` | `LEA AX,[data_ofs]+RELOC; PUSH AX` |
| Local fixed array | compile-time `type->len` | `PUSH SS` | `LEA AX,[BP+adr]; PUSH AX` |
| Open-array formal forwarded | `MOV AX,[BP+adr+4]; PUSH AX` | `MOV AX,[BP+adr+2]; PUSH AX` | `MOV AX,[BP+adr]; PUSH AX` |

**`LEN(arr)` in callee** (arr is an open-array formal):
```asm
MOV AX, [BP+adr+4]   ; read hidden LEN word
```

**`LEN(arr)` for fixed-size array**: compile-time constant `type->len` — `MOV AX, imm16`.

**Array element access** (`arr[i]`):
- Open-array formal: `LES BX,[BP+adr]` loads far ptr; then `ADD BX, AX` (index×elemsize).
- Global fixed array: `LEA AX,[data_ofs]; MOV ES,DS; MOV BX,AX` then `ADD BX, AX`.
- Local fixed array: `LEA AX,[BP+adr]; MOV ES,SS; MOV BX,AX` then `ADD BX, AX`.
- All element accesses then use `ES:[BX]` convention (M_REG, is_ref=1).

**`param_slot_size` in symbols.c:**
- `K_VARPARAM` → 4 bytes (far ptr: offset+segment)
- non-VAR open-array → **6 bytes** (far ptr + LEN)
- other non-VAR → `max(type->size, 2)`

---

## LONGINT (signed 32-bit integer)

**Register pair:** DX (high word) : AX (low word). Memory: lo-word at `[ofs]`, hi-word at `[ofs+2]`.

**Type-cast expressions** (parse_factor handles K_TYPE with castable form):
- `INTEGER(x)` — truncate to low 16 bits; just use AX. Result type = INTEGER.
- `BYTE(x)` — `AND AX, 00FFh` after loading. Result type = BYTE.
- `LONGINT(x)` — if source is already LONGINT: no-op. Otherwise: `CWD` (sign-extend AX→DX:AX). Result type = LONGINT.

**Widening assignment** (type_assign_compat): INTEGER and BYTE can be assigned to LONGINT (implicit `CWD` on store).

**Arithmetic:**

| Operation | Code generated |
|-----------|---------------|
| `a + b`   | `ADD AX, BX` / `ADC DX, CX` (cg_add32) |
| `a - b`   | XCHG AX↔BX, XCHG DX↔CX, `SUB AX,BX` / `SBB DX,CX` (cg_sub32) |
| `-a`      | `NOT AX` / `NOT DX` / `ADD AX,1` / `ADC DX,0` (cg_neg32) |
| `a * b`   | FAR call `SYSTEM_S32MUL` from SYSTEM (lazy RDOFF import) |
| `a DIV b` | FAR call `SYSTEM_S32DIV` from SYSTEM |
| `a MOD b` | FAR call `SYSTEM_S32MOD` from SYSTEM |

**Stack convention for SYSTEM_S32MUL/SYSTEM_S32DIV/SYSTEM_S32MOD** (Pascal calling, RETF 8):
```
Caller pushes: PUSH DX(lhs_hi) PUSH AX(lhs_lo) PUSH DX(rhs_hi) PUSH AX(rhs_lo)
Callee sees:   [BP+12]=lhs_hi [BP+10]=lhs_lo [BP+8]=rhs_hi [BP+6]=rhs_lo
Result returned in DX:AX.
```

**Comparison (parse_expr → cg_cmp32_bool or cg_cmp32_eq/neq):**

Two-stage: compare hi words (CX vs DX, signed); if unequal → use JL/JG result; if equal → compare lo words (BX vs AX, unsigned JB/JA/JBE/JAE).

```
CMP CX, DX           ; hi-word signed compare
Jhi_true → .true     ; e.g. JL for LSS
Jhi_false → .false   ; e.g. JG for LSS
CMP BX, AX           ; lo-word unsigned compare
Jlo_true → .true     ; e.g. JB for LSS
.false: XOR AX,AX; JMP .end
.true:  MOV AX, 1
.end:
```

For `=` and `#`: XOR hi-words, XOR lo-words, OR together, then JZ/JNZ.

**LONGINT parameters:**

Pushed as 4 bytes: `PUSH DX` (hi) then `PUSH AX` (lo). `type_calc_arg_size` uses `sz=4` (SZ_LONGINT).

**VAR LONGINT parameters:**

Passed as 4-byte far address (same as any VAR param). Load/store uses `cg_load_long_bp(ofs)` / `cg_store_long_bp(ofs)`.

**codegen.h helpers:**
```c
cg_load_long_bp(ofs)    /* DX:AX = [BP+ofs+2]:[BP+ofs] */
cg_store_long_bp(ofs)   /* [BP+ofs]=AX, [BP+ofs+2]=DX */
cg_load_long_mem(ofs)   /* from DS:[ofs] and DS:[ofs+2] */
cg_store_long_mem(ofs)  /* DS:[ofs]=AX, DS:[ofs+2]=DX */
cg_load_long_imm(val)   /* AX=lo16, DX=hi16 */
cg_add32()   cg_sub32()   cg_neg32()
cg_and32()   cg_or32()    cg_xor32()
cg_push_dxax()   cg_pop_cxbx()
cg_cmp32_bool(hi_true_jcc, hi_false_jcc, lo_true_jcc)
cg_cmp32_eq()   cg_cmp32_neq()
```

**Lazy SYSTEM imports for arithmetic:**
```c
get_system_import("SYSTEM_S32MUL")  /* parser.c helper — finds or adds RDOFF import */
get_system_import("SYSTEM_S32DIV")
get_system_import("SYSTEM_S32MOD")
```

---

## FPU types: REAL, LONGREAL  ← CRITICAL

### Prerequisites

Any module that declares or uses `REAL` or `LONGREAL` requires an x87 FPU
(8087/80287/80387 coprocessor or integrated FPU).  The compiler **detects FPU type usage**
during parsing and sets a module-level flag `mod_uses_fpu`.  When this flag is set the
`__init` function emits a FAR call to `SYSTEM_REQUIRE87` immediately after the guard
INC and before any dependency `__init` calls.

```asm
; __init with FPU types used — layout:
[guard_ofs]   DW 0
              CS: CMP [guard], 0
              JE .cont
              RETF
; .cont:
              CS: INC [guard]
              CALL FAR SYSTEM_REQUIRE87   ← only when mod_uses_fpu
              CALL FAR Dep1__init
              ; ... module body ...
              RETF
```

`SYSTEM_REQUIRE87` checks for the presence of a real 8087/287/387 coprocessor using
the standard DOS detection sequence.  If no FPU is found it prints an error message and
terminates the process with exit code 87.  It is provided by `SYSTEM.RDF` and exported as a GLOBAL.

The RDOFF import for `SYSTEM_REQUIRE87` is created lazily the same way as other SYSTEM
imports — via `get_system_import("SYSTEM_REQUIRE87")`.

**`mod_uses_fpu` flag** (parser.c file-scope static `int`):  set to 1 the first time
the parser encounters `TF_REAL` or `TF_LONGREAL` in any type position (variable
declaration, parameter, record field) or a `T_REAL` literal in an expression.
Reset to 0 at the start of each `parse_module`.

---

### FPU register model

The x87 FPU has an 8-level stack: `ST(0)` is the top.  All arithmetic operates on
`ST(0)` (and `ST(1)` for binary ops).  The compiler uses a **strict stack discipline**:
every expression leaves exactly one value on the FPU stack; every statement that uses
an FPU value pops it.

**Value in FPU register for Item:** `item->mode = M_FREG` (mode constant 7).
`M_FREG` means the value is on top of the FPU stack (`ST(0)`).

```c
#define M_FREG  7   /* value in FPU ST(0) */
```

The integer CPU registers (AX, DX, etc.) are **not** used for REAL/LONGREAL
values except as memory address operands.

---

### Memory layout

| Type     | Memory layout                    | FPU load/store instructions      |
|----------|----------------------------------|----------------------------------|
| REAL     | 4 bytes, IEEE 754 single (dword) | `FLD dword [addr]` / `FSTP dword [addr]` |
| LONGREAL | 8 bytes, IEEE 754 double (qword) | `FLD qword [addr]` / `FSTP qword [addr]` |

Both types must be stored in **memory** (stack frame or data segment) — never in
CPU registers.  The FPU stack is only used as a transient during expression evaluation.

For local variables: allocated by `sym_alloc_local` with their full size (4 or 8 bytes),
word-aligned.

---

### Load / store codegen

#### Local variable (stack frame, BP-relative)

```asm
; REAL load:     FLD  dword ptr [BP + ofs]  — D9 45 ofs8  or  D9 85 ofs16
; REAL store:    FSTP dword ptr [BP + ofs]  — D9 5D ofs8  or  D9 9D ofs16
; LONGREAL load: FLD  qword ptr [BP + ofs]  — DD 45 ofs8  or  DD 85 ofs16
; LONGREAL store:FSTP qword ptr [BP + ofs]  — DD 5D ofs8  or  DD 9D ofs16
```

Encoding: `D9`=FLD/FSTP float dword, `DD`=FLD/FSTP double qword.
ModRM for `[BP + disp8]`: `45 ofs8`. For `[BP + disp16]`: `85 ofs16` (little-endian).

`codegen.h` helpers:
```c
void cg_fld_real_bp(int32_t ofs);       /* FLD  dword [BP+ofs] */
void cg_fstp_real_bp(int32_t ofs);      /* FSTP dword [BP+ofs] */
void cg_fld_longreal_bp(int32_t ofs);   /* FLD  qword [BP+ofs] */
void cg_fstp_longreal_bp(int32_t ofs);  /* FSTP qword [BP+ofs] */
```

#### Global variable (data segment, DS-relative)

Same opcodes with ModRM `05 lo hi` for `[disp16]` plus a DATA RELOC record:
```c
void cg_fld_real_mem(uint16_t ofs);      /* FLD  dword [DS:ofs] */
void cg_fstp_real_mem(uint16_t ofs);     /* FSTP dword [DS:ofs] */
void cg_fld_longreal_mem(uint16_t ofs);  /* FLD  qword [DS:ofs] */
void cg_fstp_longreal_mem(uint16_t ofs); /* FSTP qword [DS:ofs] */
```

#### REAL/LONGREAL literal constants

Floating-point literals are stored as raw bytes in the **data segment** at parse time.
There is no FPU "load immediate" instruction.

- `T_REAL` literal → 4 bytes (IEEE 754 single) emitted via `cg_emit_data_byte` × 4
- LONGREAL literal → 8 bytes (IEEE 754 double) emitted via `cg_emit_data_byte` × 8
- `item->adr` = data segment offset of the constant
- `item->mode = M_CONST`; loaded by `cg_fld_real_const(item->adr)` or `cg_fld_longreal_const`

```c
void cg_fld_real_const(uint16_t data_ofs);      /* FLD dword [CS:data_ofs] */
void cg_fld_longreal_const(uint16_t data_ofs);  /* FLD qword [CS:data_ofs] */
```

**Important:** the current scanner always produces `T_REAL` tokens for any floating-point
literal. The compiler determines REAL vs LONGREAL by context (variable type), NOT by
any `L` suffix. All float literals are initially stored as 4-byte REAL unless the
assignment/parameter context requires LONGREAL (then 8 bytes are emitted).

---

### Arithmetic codegen

The compiler uses `cg_fpush` / `cg_fpop` to manage operand order:

```
1. Evaluate LHS → ST(0)  [M_FREG]
2. cg_fpush()            ; SUB SP,8; FSTP qword SS:[BX] — saves LHS from ST(0) to CPU stack
3. Evaluate RHS → ST(0)  [M_FREG]
4. cg_fpop()             ; FLD qword SS:[BX]; ADD SP,8 — pushes LHS back onto FPU stack
   ; After fpop: ST(0)=LHS, ST(1)=RHS
5. Emit FPU binary op    ; result in ST(0), both ST(0) and ST(1) consumed
```

The critical point: after step 4, **ST(0)=LHS and ST(1)=RHS**.

`FSUBP ST(1),ST` computes `ST(1) - ST(0) = RHS - LHS` — **wrong**.
`FSUBRP ST(1),ST` computes `ST(0) - ST(1) = LHS - RHS` — **correct**.

Similarly for division: `FDIVRP ST(1),ST` = `ST(0)/ST(1) = LHS/RHS`.

**Implemented opcodes (ST(0)=LHS, ST(1)=RHS after fpush/fpop):**

| Operation | Opcode bytes  | Mnemonic              | Result         |
|-----------|---------------|-----------------------|----------------|
| `a + b`   | `DE C1`       | `FADDP  ST(1),ST`     | ST(1)+ST(0)=LHS+RHS ✓ (commutative) |
| `a - b`   | `DE E1`       | `FSUBRP ST(1),ST`     | ST(0)-ST(1)=LHS-RHS ✓ |
| `a * b`   | `DE C9`       | `FMULP  ST(1),ST`     | ST(1)*ST(0)=LHS*RHS ✓ (commutative) |
| `a / b`   | `DE F1`       | `FDIVRP ST(1),ST`     | ST(0)/ST(1)=LHS/RHS ✓ |
| `-a`      | `D9 E0`       | `FCHS`                | negate ST(0) |
| `ABS(a)`  | `D9 E1`       | `FABS`                | \|ST(0)\| |

**SS: prefix required for FPU memory operands via BX** (8086 cannot use SP as memory base):
```asm
; cg_fpush:
SUB SP, 8        ; 83 EC 08 — allocate 8 bytes
MOV BX, SP       ; 89 E3
SS: FSTP qword [BX]   ; 36 DD 1F — store ST(0) to CPU stack, pop FPU

; cg_fpop:
MOV BX, SP       ; 89 E3
SS: FLD qword [BX]    ; 36 DD 07 — load from CPU stack → pushes onto FPU
ADD SP, 8        ; 83 C4 08
```

---

### Comparison codegen

After `cg_fpush` + evaluate RHS + `cg_fpop`: ST(0)=LHS, ST(1)=RHS.
FPU comparison uses `FCOMPP` + `FNSTSW AX` + `SAHF`.

```asm
; After fpush+rhs+fpop: ST(0)=LHS, ST(1)=RHS
FCOMPP          ; DE D9 — compare ST(0) with ST(1), pop both
                ;   C0=1 if ST(0) < ST(1), i.e. LHS < RHS
FNSTSW AX       ; DF E0 — store FPU status word into AX (no WAIT)
SAHF            ; 9E    — load AH into flags: CF=C0, ZF=C3, PF=C2
```

After `SAHF`, the x86 flags reflect the comparison of LHS vs RHS:
- `CF=1` → LHS < RHS (C0=1)
- `ZF=1` → LHS = RHS (C3=1)
- `CF=0, ZF=0` → LHS > RHS

Mapping to Oberon comparison operators (function `frelop_jcc` in parser.c):

| Operator | Opcode | Jump name | Condition after SAHF |
|----------|--------|-----------|----------------------|
| `=`      | 74     | JZ        | ZF=1                 |
| `#`      | 75     | JNZ       | ZF=0                 |
| `<`      | 72     | JB        | CF=1                 |
| `<=`     | 76     | JBE       | CF=1 or ZF=1         |
| `>`      | 77     | JA        | CF=0 and ZF=0        |
| `>=`     | 73     | JAE       | CF=0                 |

The standard `cg_setcc(jcc_opcode)` helper works unchanged after `SAHF`.

**FCOMPP opcode: `DE D9`** (not `D9 D9` which is `FLDL2T`).

**Full comparison sequence for `a < b`:**
```asm
; After fpush+eval_b+fpop: ST(0)=a, ST(1)=b (but we compare a<b)
; Correction: standard flow is cg_fpush saves a; then b evaluated;
; cg_fpop restores a → ST(0)=a=LHS, ST(1)=b=RHS
FCOMPP          ; DE D9 — compare ST(0) [LHS/a] with ST(1) [RHS/b], pop both
                ; C0=1 if a < b → CF=1 after SAHF
FNSTSW AX       ; DF E0
SAHF            ; 9E
; JB (72) = jump if a < b
```

---

### Type casts and conversions

All conversions via explicit cast syntax `REAL(x)`, `LONGREAL(x)`, `INTEGER(x)`, `LONGINT(x)`.
`FLOAT(x)` is accepted as a synonym for `REAL(x)`; `ENTIER(x)` is accepted as a synonym for `FLOOR(x)`.

**Integer → float** (result in ST(0), mode=M_FREG): push value to SS stack, FILD, restore SP:
```asm
; INTEGER → REAL or LONGREAL:
SUB SP, 2        ; 83 EC 02
MOV BX, SP       ; 89 E3
SS: MOV [BX], AX ; 36 89 07   — store AX (INTEGER value)
SS: FILD word [BX] ; 36 DF 07 — load as 16-bit signed int → ST(0)
ADD SP, 2        ; 83 C4 02

; LONGINT → REAL or LONGREAL (DX:AX → ST(0)):
SUB SP, 4        ; 83 EC 04
MOV BX, SP       ; 89 E3
SS: MOV [BX], AX   ; 36 89 07
SS: MOV [BX+2], DX ; 36 89 57 02
SS: FILD dword [BX] ; 36 DB 07
ADD SP, 4        ; 83 C4 04
```

| Cast | Code generated |
|------|----------------|
| `REAL(INTEGER)` | FILD word via SS stack → ST(0) |
| `REAL(LONGINT)` | FILD dword via SS stack → ST(0) |
| `LONGREAL(INTEGER)` | same as REAL(INTEGER) — FPU extended precision |
| `LONGREAL(LONGINT)` | same as REAL(LONGINT) |
| `LONGREAL(REAL)` | no-op (FPU operates in extended precision internally) |
| `REAL(LONGREAL)` | no-op (FPU extended → effectively REAL on store) |
| `INTEGER(REAL)` | `cg_fist_ax()`: FISTP word via SS stack → AX (truncate toward 0) |
| `INTEGER(LONGREAL)` | same as INTEGER(REAL) |
| `LONGINT(REAL)` | FISTP dword via SS stack → DX:AX |
| `LONGINT(LONGREAL)` | same |

**FLOOR(x) / ENTIER(x)** for REAL/LONGREAL → INTEGER:
Uses `cg_floor_ax()`: saves FPU control word, sets RC=01 (round toward −∞), FISTP word → AX, restores CW.

**Assignment compatibility:**
- `INTEGER → LONGINT`: implicit widening (CWD sign-extend)
- `LONGINT accepts INTEGER/BYTE` via `type_assign_compat`
- `REAL` and `LONGREAL`: assignment requires explicit cast (`REAL(i)`, `LONGREAL(i)`). In binary expressions (`*`, `/`, `+`, `-`, `=`, `#`, `<`, `>`, `<=`, `>=`) an integer operand (INTEGER, BYTE, LONGINT) is automatically coerced to REAL/LONGREAL via FILD when the other operand is a float type. Both orderings work: `r * n`, `n * r`, `r < 0`, `1 < r`.

---

### Calling convention for FPU types

FPU types are passed and returned differently from integer types:

**Passing REAL/LONGREAL parameters (caller):**  
Passed on the **CPU stack** as raw bytes via `FSTP` — the FPU stack is NOT used for
parameter passing (DOS/Pascal convention).

```asm
; Pass REAL argument (value currently in ST(0)):
SUB SP, 4        ; 83 EC 04 — allocate 4 bytes
MOV BX, SP       ; 89 E3
SS: FSTP dword [BX] ; 36 D9 1F — store float, pop ST(0)

; Pass LONGREAL argument (value currently in ST(0)):
SUB SP, 8        ; 83 EC 08
MOV BX, SP       ; 89 E3
SS: FSTP qword [BX] ; 36 DD 1F — store double, pop ST(0)
```

Memory layout on stack (Pascal order: left param deepest):
- REAL (4 bytes): `[BP+n]`=lo-word, `[BP+n+2]`=hi-word
- LONGREAL (8 bytes): 8 bytes at `[BP+n..BP+n+7]`

**Returning REAL/LONGREAL:**  
Returned in `ST(0)` of the FPU stack.  Caller receives M_FREG item; must store ST(0).

**`type_calc_arg_size`:** uses `sz = t->size` (4 or 8 bytes) for REAL/LONGREAL,
same as LONGINT uses sz=4.

---

### Module FPU requirement — SYSTEM_REQUIRE87

**Detection:** the parser sets `mod_uses_fpu = 1` the first time it sees `TF_REAL`
or `TF_LONGREAL` in any context (variable declaration, parameter, record field,
or `T_REAL` literal in an expression).

**`mod_uses_fpu` in parser.c:** file-scope `static int mod_uses_fpu = 0;`  
Reset to 0 at the start of each `parse_module`. Set directly (not via helper)
wherever TF_REAL/TF_LONGREAL type is first encountered.

**Codegen in `parse_module` / `__init` generation:**  
After `CS: INC [guard]` and before any dep-`__init` FAR calls:

```c
if (mod_uses_fpu) {
    int req87_id = get_system_import("SYSTEM_REQUIRE87");
    cg_call_far(req87_id);
}
```

This produces `9A oo oo ss ss` + RELOC + SEGRELOC pointing at `SYSTEM_REQUIRE87`.

`SYSTEM_REQUIRE87` is provided by `SYSTEM.RDF` (FAR, no args, no return value, Pascal).
It halts the program with an error message if no 8087-compatible FPU is detected.
It does not appear in `SYSTEM.DEF` (it is not a user-callable proc).

---

## Far pointer model  ← CRITICAL

All POINTER TO T and procedure-pointer variables are 4-byte far pointers.
VAR parameters also pass a 4-byte far address (seg:offset).

**Memory layout:** `[addr+0]` = offset (low word), `[addr+2]` = segment (high word).
`LES BX, [addr]` → BX = offset, ES = segment. ✓

| Situation | Registers |
|-----------|-----------|
| Far ptr as VALUE (DX:AX) | AX = offset, DX = segment |
| Far ptr for DEREFERENCE (ES:BX) | BX = offset, ES = segment |

Convert DX:AX → ES:BX: `MOV BX,AX` (8B D8) + `MOV ES,DX` (8E C2).

**Loading:**
```asm
; local  [BP+ofs]:  C4 5E ofs8  (LES BX,[BP+ofs])
; global [DS:adr]:  C4 1E lo hi +RELOC
```

**Dereference (read):** `26 8B 07` = MOV AX, ES:[BX]  
**Store through (write):** `26 89 07` = MOV ES:[BX], AX

**Pushing a far pointer:**
```asm
PUSH DX   ; segment first  → lands at [BP+n+2]
PUSH AX   ; offset  second → lands at [BP+n]
```

**VAR parameter passing (caller):**
```asm
LEA AX, [BP+localOfs]    ; or MOV AX, imm16 for globals
PUSH DS                  ; segment = DS
PUSH AX                  ; offset
```

**VAR parameter store (callee):**
```asm
PUSH AX
LES BX, [BP+paramOfs]
POP AX
MOV ES:[BX], AX
```

**NIL comparison:** `OR AX, DX` → JZ = is NIL.  
**NIL check after LES:** `MOV AX,ES / OR AX,AX / JNZ ok / OR BX,BX / JNZ ok / call trap`

**Pointer equality `p = q` (or `p # q`):** BOTH segment (DX) and offset (AX) must be compared.
`SYSTEM_Alloc` returns `DX=segment, AX=0` (offset is always 0 for heap blocks). So comparing only
AX against NIL (AX=0) always equals! Use `cg_ptr_cmp_eq()`:
```
PUSH DX          ; push LHS segment
PUSH AX          ; push LHS offset
<load RHS>       ; RHS DX:AX (NIL = XOR AX,AX; XOR DX,DX)
POP CX           ; LHS offset
XOR AX, CX       ; 0 if offsets equal
POP BX           ; LHS segment
XOR DX, BX       ; 0 if segments equal
OR  AX, DX       ; 0 iff both equal → ZF=1
TEST AX, AX
```
Then `cg_setcc(OP_JZ)` for `=`, `cg_setcc(OP_JNZ)` for `#`.

**Note:** This comparison is correct for `NEW`-allocated pointers (where `SYSTEM_Alloc` returns
offset=0), and for NIL (0000:0000). For pointers constructed via `SYSTEM.PTR` or `SYSTEM.ADR`,
offset may be non-zero — but the two-word XOR+OR comparison handles those correctly too.

**CRITICAL:** When loading a `TF_NILTYPE` constant (NIL), `cg_load_item` MUST zero DX as well as AX.
The `is_ptr` check uses `TF_POINTER` which NIL's type is not — check `TF_NILTYPE` separately.

---

## Jump instructions  (CRITICAL — strict 8086)

8086 conditional jumps are SHORT ONLY (rel8, range -128..127). No `0F 8x` near Jcc.
Solution for long forward conditional jumps: INVERT condition + JMP near.

```
JNZ +3        ; 75 03 — skip the near jump (5 bytes total: inv_opcode 03 E9 lo hi)
JMP target    ; E9 rel16
```

Inversion: `opcode ^ 1` for all 0x70-0x7F Jcc.
`invert_jcc(opcode)` helper in `codegen.c`.

| Helper | Use |
|--------|-----|
| `cg_cond_near` | ALL forward conditional jumps (emits 5 bytes) |
| `cg_cond_short` | Backward jumps only (WHILE/REPEAT/FOR back-edges) — ASSERT fits |
| `cg_jmp_near` | Forward unconditional (E9 rel16) |
| `cg_jmp_back` | Backward unconditional (chooses EB or E9 automatically) |

Patch forward jumps with `cg_patch_near` (not `cg_patch_short`).

---

## Naming convention  ← CRITICAL

All exported symbols: `ModuleName_SymbolName`

- `_` is **forbidden** in module names (compile error)
- `__` prefix is reserved for compiler-generated names
- Every module exports `ModName__init` (double underscore)
- `MAIN.RDF` is the entry-point stub generated by `oc -entry`

Examples: `Hello_WriteStr`, `Kernel_heap`, `Hello__init`

---

## FAR / NEAR procedure rules  ← CRITICAL

### Determining `is_far`

| Condition | `is_far` | Notes |
|-----------|----------|-------|
| Exported (`*`) | 1 (FAR) | Mandatory; cannot be overridden with NEAR |
| `PROCEDURE name; FAR;` | 1 (FAR) | Explicit FAR modifier |
| `PROCEDURE name; NEAR;` | 0 (NEAR) | Explicit NEAR modifier; error if exported |
| Non-exported, no modifier | 0 (NEAR) | Default for top-level and nested non-exported procs |
| `__init` | 1 (FAR) | Always FAR, always exported |

### Call and epilogue dispatch

| `is_far` | Epilogue | Call intra-module | Call inter-module |
|----------|----------|-------------------|-------------------|
| 0 (NEAR) | `RET` / `RET N` (C3/C2) | `cg_call_local` — `E8 rel16` | N/A (NEAR procs not exported) |
| 1 (FAR)  | `RETF` / `RETF N` (CB/CA) | `cg_call_local_far` — `0E PUSH CS` + `E8 rel16` | `cg_call_far` — `9A` + RELOC + SEGRELOC |

**Three call cases:**

1. **Local NEAR** (`is_far=0`): `E8 rel16` — plain CALL NEAR, no PUSH CS, no relocations.  
   Params start at `[BP+4]`; epilogue `RET N` pops only IP.
2. **Local FAR** (`is_far=1`, same module): `0E PUSH CS` + `E8 rel16` — no relocations.  
   Use when `sym->exported && sym->rdoff_id < 0`.  
   Params start at `[BP+6]`; epilogue `RETF N` pops IP then CS.
3. **Imported** (another module, `M_IMPORT`): `9A oo oo ss ss` CALL FAR + RELOC + SEGRELOC.  
   Always FAR (`item->is_far` set from `.def` type, default 1); params start at `[BP+6]`.  
   EXTERNAL NEAR procs are the one exception: `item->is_far=0` → `E8 rel16` NEAR call.

### Procedural variables

| Type | Storage | Assignment | Call |
|------|---------|------------|------|
| `PROCEDURE FAR (...)` (default) | 4 bytes `{offset, segment}` | loads `AX=offset, DX=CS`; stores DX:AX | `CALL FAR [BP+n]` (FF 5E) or `CALL FAR [mem]` (FF 1E) |
| `PROCEDURE NEAR (...)` | 2 bytes `{offset}` | loads `AX=offset` only; stores AX | `CALL NEAR [BP+n]` (FF 56) or `CALL NEAR [mem]` (FF 16) |

FAR and NEAR proc vars are **not assignment-compatible** — `type_assign_compat` rejects mismatched `is_far`.

---

## Module initialization protocol  ← CRITICAL

Every module gets a compiler-generated `__init` FAR procedure, exported as `ModName__init`.

**Trivial module** (no imports, no body): just `CB` (RETF).

**Module with imports or body:**
```asm
[guard_ofs]   DW 0                      ; guard word in CODE segment
              2E 83 3E lo hi 00         ; CS: CMP word [guard], 0
              74 01                     ; JE .cont
              CB                        ; RETF  (already inited)
; .cont:
              2E FF 06 lo hi            ; CS: INC word [guard]  ← BEFORE dep calls
              9A oo oo ss ss            ; CALL FAR Mod1__init
              ; ... module body ...
              CB                        ; RETF
```

Rules:
- No stack frame in `__init` (no PUSH BP / MOV BP,SP / SUB SP)
- INC before dep calls → circular-init safe
- CS: prefix (2Eh) — guard in code segment, not DS
- SYSTEM excluded from dep calls; `IMPORT SYSTEM` is a compile error
- `__init` always exported as RDOFF GLOBAL; never in .def file

---

## SYSTEM pseudo-module  ← CRITICAL

SYSTEM is pre-declared as a K_IMPORT symbol, **always implicitly available**.

- `IMPORT SYSTEM` is a **compile error**
- `SYSTEM.Foo` creates a lazy RDOFF IMPORT for `SYSTEM_Foo` on first use
- `SYSTEM__init` is never called
- Missing `SYSTEM.def` is a **compile error**

### SYSTEM module symbols (from `src-lib/SYSTEM.DEF`)

**Constants:** `FCarry=1  FParity=4  FAuxiliary=16  FZero=64  FSign=128  FOverflow=2048`

**Types:**
```
SYSTEM.Registers — RECORD, 18 bytes, 9 INTEGER fields:
  AX(0) BX(2) CX(4) DX(6) SI(8) DI(10) DS(12) ES(14) Flags(16)
```
No direct access to 8-bit registers (AL, etc.).

**Procedures:**
```
SYSTEM_INIT      — FAR, no args, no return
SYSTEM_DONE      — FAR, no args, no return
SYSTEM_CallFar   — FAR; args: ProcPtr:POINTER, Regs:POINTER Registers
SYSTEM_Intr      — FAR; args: IntNo:BYTE, Regs:POINTER Registers
SYSTEM_Alloc     — FAR; arg: sizeInBytes:INTEGER; returns POINTER (DX:AX)
SYSTEM_Free     — FAR; arg: p:POINTER; returns nothing
```

Note: SYSTEM_Intr contains `INT 0`, but it is self-modified code - interrupt number will be patched
in call time.

### SYSTEM compiler intrinsics

Built into compiler — NOT in SYSTEM.def, NOT linked externally.

**Type:** `SYSTEM.ADDRESS` (TF_ADDRESS, size 4) — compatible with any POINTER.

**Expression intrinsics:**

| Intrinsic | Returns | Description |
|-----------|---------|-------------|
| `SYSTEM.ADR(v)` | ADDRESS | Far address of v; `ADR(n^)` = value of pointer `n` (see below) |
| `SYSTEM.VAL(T, expr)` | T | Reinterpret bits; sizes must match |
| `SYSTEM.PTR(s, o)` | ADDRESS | Construct far pointer from seg:ofs |
| `SYSTEM.SEG(v)` | INTEGER | Segment of v; `SEG(n^)` = segment of pointer `n` |
| `SYSTEM.OFS(v)` | INTEGER | Offset of v; `OFS(n^)` = offset of pointer `n` |
| `SYSTEM.LSL(x, n)` | INTEGER | Logical shift left |
| `SYSTEM.ASR(x, n)` | INTEGER | Arithmetic shift right |
| `SYSTEM.ROR(x, n)` | INTEGER | Rotate right |

**SYSTEM.ADR argument forms:**

`SYSTEM.ADR(v)` accepts any of the following:

| Argument form | Segment | Codegen |
|---------------|---------|---------|
| Global variable, array, or record | DS | `LEA AX,[ofs]` (8D 06 ofs) |
| Global array element `gArr[i]` | DS | `MOV AX,BX` + `MOV DX,ES` after index |
| Global record field `gRec.f` | DS | `LEA AX,[ofs]` with field offset |
| Local variable, array, or record | SS | `LEA AX,[BP+ofs]` |
| Local array element `a[i]` | SS | `MOV AX,BX` + `MOV DX,ES` after index |
| Local record field `r.f` | SS | `LEA AX,[BP+ofs]` with field offset |
| Non-VAR parameter `p` | SS | `LEA AX,[BP+ofs]` — address of the parameter slot |
| VAR parameter `v` | caller's segment | `MOV AX,[BP+ofs]` — offset word from the far ptr |
| Uplevel variable (nested proc) | SS | `cg_sl_load_bx(hops)` then `LEA AX,[BX+ofs]` |
| Procedure name `P` | CS | `MOV AX,ofs` + CODE reloc (linker patches) |
| Pointer deref `n^` | — | `MOV AX,BX` + `MOV DX,ES` → `DX:AX` = value of `n` |

Notes:
- Large model: DS ≠ SS. Global vars are DS-relative; locals/parameters are SS-relative.
- For indexed array elements and pointer derefs, the designator sets ES:BX; ADR returns the full far pointer `DX:AX = ES:BX`.
- **Invariant**: `ADR(n^) = n` — the address of a dereferenced pointer equals the pointer itself.
- For record fields, the ADR is computed from the record base plus field offset.
- Record base addresses include the 4-byte tag slot: first field is at record_base + 4.
- Procedure ADR emits a code-segment relocation; the linker patches the correct offset.
- ADR of an uplevel variable in a nested procedure traverses the static link chain.
- `ADR(nonVarParam)` returns the SS-relative address of the parameter's stack slot.
- `ADR(varParam)` returns the offset word of the far pointer passed by the caller;
  the segment is the caller's segment (DS for globals, SS for locals).
- **SYSTEM.PUT/GET accept any ADDRESS** — use `SYSTEM.ADR(v)` to obtain the full far
  pointer (segment:offset); all segments (DS, SS, heap) are valid.

**SYSTEM.SEG / SYSTEM.OFS on pointer dereference:**

`SEG(n^)` and `OFS(n^)` return the segment and offset of the pointer value `n`, not of the pointed-to object. After `n^` the designator leaves ES:BX = value of `n`; `SEG(n^)` emits `MOV AX,ES` (`8C C0`); `OFS(n^)` emits `MOV AX,BX` (`8B C3`). Invariants: `SEG(n^) = SEG(n)`, `OFS(n^) = OFS(n)`.

**Statement intrinsics:**

| Intrinsic | Signature | Description |
|-----------|-----------|-------------|
| `SYSTEM.GET(a, var)` | `a: ADDRESS; VAR var` | Read word at far ptr `a` into var (any segment) |
| `SYSTEM.PUT(a, expr)` | `a: ADDRESS; expr` | Write expr to far ptr `a` (any segment) |
| `SYSTEM.MOVE(s, d, n)` | `s, d: ADDRESS; n: INTEGER` | Copy n bytes from far ptr `s` to far ptr `d` (any segment) |
| `SYSTEM.FILL(d, n, b)` | `d: ADDRESS; n: INTEGER; b: BYTE` | Fill n bytes at far ptr `d` with byte `b` (any segment) |

**Key codegen notes:**
- `ADR(global)`: `LEA AX,[ofs]` (8D 06); `ADR(local)`: `LEA AX,[BP+ofs]`; `ADR(proc)`: `MOV AX,ofs` + CODE reloc
- `SEG(global)`: `MOV AX,DS` (8C D8); local: `MOV AX,SS` (8C D0)
- `GET`/`PUT`: `26 8B 07` / `26 89 07`
- `MOVE`/`FILL`: `8C DA / 8E C2 / FC / F3 A4` (or `F3 AA`)
- **CLD always** before REP MOVSB/STOSB (direction flag may be set)
- `LSL/ASR/ROR`: inline 8086 shifts, no function call.
  Shift-by-1: `D1` form. Shift-by-N (N≥2): `MOV CL,N` then CL form.
  Variable: `MOV CL,AL / AND CL,0Fh` then CL form.

**SP_ constants (symbols.h):**
```c
SP_ADR=12  SP_VAL=13  SP_GET=14  SP_PUT=15  SP_MOVE=16
SP_PTR=17  SP_SEG=18  SP_OFS=19  SP_FILL=20
SP_LSL=21  SP_ASR=22  SP_ROR=23
```

---

## NEW and DISPOSE

**NEW(p)** where `p: POINTER TO T`:
- Calls `SYSTEM_Alloc(sizeof(T))` FAR — Pascal, cleans 2 bytes
- Result DX:AX stored into p
- Codegen: `cg_load_imm(sz)` / `PUSH AX` / `cg_call_far(Alloc_id)` / `cg_store_item`

**NEW(p, n)** sized allocation:
- `n` is any INTEGER expression; always interpreted as **byte count**
- Also works when `p` is `SYSTEM.ADDRESS` (TF_ADDRESS)
- Codegen: evaluate n into AX, `PUSH AX`, `cg_call_far(Alloc_id)`

**DISPOSE(p)** where `p: POINTER TO T` or `SYSTEM.ADDRESS`:
- Calls `SYSTEM_Free(p)` FAR — Pascal, cleans 4 bytes. Does NOT set p to NIL.
- Codegen: `cg_load_item(&ptr)` → DX:AX / `PUSH DX` / `PUSH AX` / `cg_call_far(Free_id)`
- Predeclared as `SP_DISPOSE = 11`

---

## Qualified import access

```oberon
IMPORT H := Hello;
H.WriteStr("hi")
```

Lookup: `H` → K_IMPORT with `mod_name="Hello"` → DOT → construct `Hello_WriteStr`
→ find or add RDOFF IMPORT → item = M_IMPORT with rdoff_id.

**`item->is_far` must be set** from the resolved `.def` symbol's type (`def_sym->type->is_far`).
If no `.def` entry exists, default `is_far = 1` (cross-module calls are always FAR).
Failure to set `is_far` causes `cg_call_near` (E8) to be emitted instead of `cg_call_far` (9A),
producing a stack-corrupting NEAR call into a FAR procedure in another code segment.

---

## Import error handling

- Missing `.def` for explicit `IMPORT Foo`: **fatal error** (exit 1).
- SYSTEM always implicitly available; missing `SYSTEM.def` silently ignored.
- No partial compilation with unresolved imports.
- Tests using `IMPORT X` must create stub `X.def` (`MODULE X\n` minimum).

---

## Entry point: -entry flag

```
oc -entry ProcName File.Mod
```

Generates `MAIN.RDF` containing:
```asm
CALL FAR SYSTEM_INIT        ; seg 3
CALL FAR ModName__init      ; seg 5
CALL FAR ModName_ProcName   ; seg 6
CALL FAR SYSTEM_DONE        ; seg 4
```
Exports GLOBAL `start` at offset 0. Compiler never emits `INT 20h` — DOS exit is `SYSTEM_DONE`'s job.

---

## Known bugs FIXED — do not reintroduce

1. **`parse_type()` out-pointer must be initialised before call.**
   `TypeDesc *t = type_integer;` before every `parse_type(&t)` call.

2. **Token vs type-form constants must never be mixed.**
   T_ARRAY=50, TF_ARRAY=7; T_RECORD=73, TF_RECORD=8; T_POINTER=71, TF_POINTER=9.

3. **Token enum replaced with #defines** — no C sequential-value collisions.

4. **Field access through VAR params:** `cg_les_bx_bp(param.adr)` then
   `cg_add_bx_imm(field.offset)`. Do NOT add field.offset to param.adr directly.

5. **`cg_store_item` for VAR param (is_ref=1, M_LOCAL):** PUSH AX before LES,
   POP AX before store — LES clobbers BX/ES.

6. **Pointer comparison only compared offset (AX), not segment (DX).** Fixed by
   `cg_ptr_cmp_eq()` in `parse_expr` when LHS type is `TF_POINTER` or `TF_NILTYPE`.
   `SYSTEM_Alloc` always returns AX=0 (heap blocks at offset 0 of their segment),
   so AX-only comparison always matched NIL, causing every `p = NIL` test to be TRUE.

7. **NIL constant load did not zero DX.** In `cg_load_item` M_CONST, `is_ptr` checked
   `TF_POINTER` only. NIL has `TF_NILTYPE`, so DX was not zeroed → stale DX corrupted
   pointer equality. Fixed: also zero DX when `item->type->form == TF_NILTYPE`.

8. **SYSTEM_INIT heap probe assumed `INT 21h AH=48h BX=FFFFh` always fails.** On xt
   emulator with abundant memory, the alloc succeeds; original code then zeroed BX and
   called MEM_INIT with size=0, disabling the heap entirely. Fixed: on unexpected success
   free the block (INT 21h AH=49h), then proceed with BX=FFFFh as intended.

9. **`cg_load_item` M_LOCAL/M_GLOBAL with `is_ref=1` did not clear `is_ref` after deref.**
   When a VAR param (is_ref=1) was used as an operand in a binary expression (e.g., `x + 1`),
   `parse_simple_expr` called `cg_load_item` which emitted `LES BX,[BP+adr]; MOV AX,ES:[BX]`
   and set `item->mode = M_REG` but left `is_ref = 1`. Then the outer `cg_load_item(&rhs)` in
   `parse_statement` saw `M_REG, is_ref=1` and emitted a spurious `MOV AX,ES:[BX]`, overwriting
   the computed result with the old value. Fixed: clear `item->is_ref = 0` in the M_LOCAL and
   M_GLOBAL `is_ref` branches of `cg_load_item`.

10. **NIL literal passed as pointer argument pushed only 1 word (offset), not 2.**
    In `parse_actual_params`, the "push DX (segment)" step checked `TF_POINTER || TF_PROC`
    but not `TF_NILTYPE` or `TF_ADDRESS`. A literal `NIL` (type=`TF_NILTYPE`) only pushed
    AX (offset=0); the segment word was missing, corrupting the callee's stack frame.
    For example, `ListPush(NIL, 42)` stored 42 in the wrong location, and subsequent pointer
    dereferences returned garbage or 0. Fixed: also push DX when `arg.type->form == TF_NILTYPE`
    or `TF_ADDRESS` in `parse_actual_params` (parser.c line ~496).

11. **`.def` format — all procedures now use extended `FAR/NEAR` format regardless of parameter count.**
    Previously, procedures with no parameters were written in a legacy format:
    `PROC fullname seg offset`. This format does not record the FAR/NEAR convention.
    When read back, `def_read` created a `type_new_proc` with `is_far=0` (NEAR default),
    causing all no-param imported procedures (e.g. `Out.Ln`, `Out.Open`) to be called via
    NEAR `E8` instead of FAR `9A` — corrupting the return address on the stack.
    Fixed: `def_write` now always emits `PROC name FAR/NEAR rettype\nEND\n`, even for
    procedures with no parameters. The legacy reader path in `def_read` is preserved for
    backward compatibility with hand-written `.def` files.

12. **Qualified import calls emitted NEAR instead of FAR.**
    In `parse_designator`, when resolving `Alias.Name` for a `K_IMPORT` module alias,
    `item->is_far` was never assigned — it remained 0 (the `parse_factor` initialiser).
    All qualified proc calls therefore took the `cg_call_near(rdoff_id)` branch, emitting
    `E8 rel16` (NEAR) instead of `9A oo oo ss ss` (FAR). The callee is in a different
    code segment, so the NEAR return address is wrong, causing a runaway crash.
    Fixed: after resolving the `.def` symbol, set `item->is_far = def_sym->type->is_far`.
    If no `.def` entry exists, default `item->is_far = 1`.

13. **Static link for nested procedure uplevel variable access — fully implemented.**
    Inner procs accessing outer proc's local variables (uplevel access).

    **Mechanism:**
    - `TypeDesc.has_sl=1`: marks a nested proc as needing a static link.
    - Caller pushes outer BP as a hidden last argument before CALL (via `emit_push_static_link`).
      This sits at `[BP+4]` in the callee (NEAR); params shift to `[BP+6]`.
    - `Item.sl_hops` (set in `parse_designator`): number of SL chain hops to reach the
      scope where the variable was declared.
    - `cg_sl_load_bx(hops)`: traverses the SL chain; after the call BX = outer frame's BP.
      `[BP+4]` = parent BP (1 hop); further hops follow `SS:[BX+4]`.
    - `cg_sl_load_ax` / `cg_sl_store_ax`: 16-bit (INTEGER, BOOLEAN, CHAR, BYTE).
    - `cg_sl_load_long_ax` / `cg_sl_store_long_ax`: 32-bit (LONGINT, POINTER, PROC var)
      — loads/stores DX:AX via `SS:[BX+ofs]` + `SS:[BX+ofs+2]`.
    - `cg_sl_fld_real_bx` / `cg_sl_fstp_real_bx`: REAL (4-byte) via `SS:[BX+ofs]`.
    - `cg_sl_fld_longreal_bx` / `cg_sl_fstp_longreal_bx`: LONGREAL (8-byte).
    - `cg_sl_addr_ax`: returns SS-relative address of uplevel var (for VAR param passing).
    - `emit_push_static_link(def_level)`: callers at sibling or ancestor scopes traverse
      the SL chain to find the correct outer BP to pass.
    - All manually-constructed `Item` structs initialise `sl_hops=0`.

    **Verified by `TestNestedProcs.Mod`**: INTEGER, LONGINT, REAL, LONGREAL uplevel
    load/store; 1-hop, 2-hop, 3-hop traversal; passing uplevel var as VAR param;
    sibling nested procs; closure-like accumulators; uplevel params; recursive nested
    procs; deep multi-level mutation.

---

## Known limitations (not yet implemented)

| Feature | Workaround |
|---------|------------|
| Assignment: INTEGER→REAL coercion not implicit | Use `REAL(i)` / `LONGREAL(i)` at assignment |
| Array bounds checking | Not implemented |
| Import formal parameter type checking at call sites | Not enforced |

**Notes on previously-listed limitations that are now implemented:**
- Implicit INTEGER→REAL coercion in binary expressions: `parse_term`, `parse_simple_expr`, `parse_expr` in `pexpr.c` auto-emit `FILD word/dword` when one operand is REAL/LONGREAL and the other is an integer type. Handles both orderings (int LHS or int RHS) for all arithmetic and relational operators. Verified by section 49 in `test_src.sh`.
- `FLOAT(x)` and `ENTIER(x)` synonyms: `FLOAT` registered as K_TYPE alias for `type_real`; `ENTIER` registered as K_SYSPROC with `SP_FLOOR`. Both in `sym_init()` in `symbols.c`. Verified by section 48 in `test_src.sh`.
- SET type: literals `{e1, e2, ...}` and ranges `{lo..hi}`, operators `+` (union), `-` (difference), `*` (intersection), `/` (symmetric diff), and `IN` membership test. SET is 16-bit (bits 0–15). Codegen: accumulator-on-stack for literals; bitwise AND/OR/XOR/NOT for operators; `MOV BX,s; MOV CL,i; MOV AX,1; SHL AX,CL; TEST AX,BX` for IN. Scanner fix: `n..m` without spaces now correct via `SC.pending_sym`. Verified by section 50 in `test_src.sh`.
- `ARRAY m, n OF T` multi-dimensional syntax: parsed and desugared to nested arrays in `parse_type` (parser.c). No dedicated executable test yet.
- `NEW(p, n)` sized allocation: fully implemented — `n` is evaluated, optionally multiplied by element size, and passed to `SYSTEM_Alloc`. See `parse_sysproc_call SP_NEW` in parser.c.
- PROC-type variable call-through: fully implemented and verified by `TestProcVar.Mod`.
