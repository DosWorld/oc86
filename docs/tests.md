# Test Suite

> **IMPORTANT — ALL test information lives here.**
> Do NOT add test commands, test descriptions, xt reference, pass criteria, or
> "how to add tests" guidance to CLAUDE.md. Keep it in this file only.

---

## Running tests

**Always run a full clean + rebuild before running tests.** Stale compiler or library
objects will produce false results. `make test` enforces this automatically.

```bash
make test                  # from oberonc/ root — clean, full rebuild, run all tests
cd src && make tests       # build unit test binaries only (no clean, for iteration)
bash tests/test_src.sh     # run integration tests only (assumes fresh build already done)
```

> **NEVER** run `bash tests/test_src.sh` or `cd src && make tests` directly after source
> changes without first doing `make clean && make` — you risk testing stale binaries.

---

## Test philosophy

**Prefer executable tests over byte-sequence checks.**

When adding tests for a new language feature or built-in:
1. Write a `TestXxx.Mod` module with a `RunAllTests*` entry point that exercises the feature at runtime and prints `Failed: N`.
2. Add it as the next numbered section in the executable tests block of `test_src.sh`.
3. Reserve byte-level checks (python3 / `xxd` / `rdf_code_contains`) only for things that **cannot** be verified at runtime: RDOFF object structure, opcode selection, error-message text, compile-error cases, and codegen invariants (e.g. "must not emit INT 3").

Executable tests catch real runtime bugs, survive code-generator refactors without byte-count updates, and serve as documentation of correct behavior. Byte-level tests are brittle and should be the last resort.

---

## Pass criteria

- **All tests must be green before merging any change.**
- `test_codegen.c` checks exact byte sequences — any codegen change must update the expected bytes.
- `test_rdoff.c` checks IMPORT has 2-byte seg_id and GLOBAL has 1-byte seg_id — do not mix them.
- Any new language feature needs at least one entry in `test_src.sh`.
- Any new `.def` format change requires `test_def.c` round-trips to pass and SYSTEM library to build without warnings (see `docs/implementation-rules.md` for canonical format).

---

## Unit tests (C)

| File | What it covers |
|------|----------------|
| `tests/test_scanner.c` | All 33 keywords, identifiers, integer/hex/char/string literals, all operators, nested comments, whitespace, line tracking |
| `tests/test_symbols.c` | Predeclared universe (types/consts/sysprocs/SYSTEM), scope open/close/shadow, AllocLocal BP offsets, CalcArgSize Pascal order, RECORD fields and extension chain, IsExtension |
| `tests/test_codegen.c` | Exact byte sequences: prologue/epilogue (NEAR and FAR), load_imm/bp/mem, **byte load/store (CHAR/BYTE) for locals and globals** (MOV AL/[addr] + XOR AH,AH), far-ptr helpers (LES/ES:/DX:AX), arithmetic, inc/dec/cmp, setcc, 8086-safe jumps (cond_near, jmp_back, **cond_back short and near**), NEAR and FAR calls with RELOC, invert_jcc |
| `tests/test_rdoff.c` | RDOFF2 binary: signature, file size, header size, **IMPORT seg_id = 2 bytes**, **GLOBAL seg_id = 1 byte**, RELOC/SEGRELOC layout, BSS record, code/data segment headers, EOF marker, record ordering |
| `tests/test_tar.c` | ustar header: magic, typeflag, name, size field (octal), checksum, data placement, padding to 512-byte boundary, two-block EOF, two-file archive, system tar validation |
| `tests/test_def.c` | def_write: CONST→TYPE→VAR→PROC order verified; root TYPE before extended TYPE (BASE before derived); RECORD with exported fields; RECORD extension with `BASE` line; `POINTER <name>` emits named base type; REAL/LONGREAL/SET field types; non-exported symbols excluded. def_read: full round-trip; deferred BASE resolution via linked list (`BasePatchNode`); FIELD parsing with multi-token types (`POINTER name`); REAL/LONGREAL resolve as primitive base types (no scope lookup); cross-module record extension; imported type as pointer base; FORWARD proc round-trip; PROC with REAL/LONGREAL params. |

---

## SYSTEM intrinsics tests (test_system_intrinsics.sh)

`tests/test_system_intrinsics.sh` — 28 compile-level checks for SYSTEM pseudo-module
intrinsics, byte patterns, and rule enforcement. Key areas:

| Test | What it checks |
|------|----------------|
| T1 | Basic module with POINTER compiles |
| T2/T2b | SYSTEM.ADR emits correct push+INLINE pattern (1E 50 58 5A); ADR(proc) emits PUSH CS + MOV AX |
| T3/T4 | POINTER TO T deref write/read compile |
| T5/T6 | SYSTEM_VAL, SYSTEM_GET, SYSTEM_PUT absent from SYSTEM.DEF (removed from dialect) |
| T7–T10 | MOVE/PTR/SEG/OFS/FILL compile and emit correct byte patterns |
| T11 | ADR(constant) gives compile error |
| T12 | Missing import gives named error, no output file |
| T13 | Module with parseable but corrupt .def compiles |
| T14/T15 | CLD precedes REP MOVSB and REP STOSB |
| T16 | SYSTEM.SEG(local) emits SS-relative push pattern (16 50 58 58) |
| T17/T17b | **Rule:** bare `LSL(x,n)` is rejected; `SYSTEM.LSL(x,n)` compiles (SYSTEM intrinsics require qualified access) |

**Rule enforced by T17:** SYSTEM-specific intrinsics (LSL, LSR, ASR, ROR, AND, IOR, XOR)
are NOT pre-declared with bare names. Only `SYSTEM.Name(...)` qualified form is accepted.

---

## Integration tests (test_src.sh)

`tests/test_src.sh` compiles Oberon source files and checks the output binary for
correct byte sequences, symbol exports, file structure, and error behaviour.

### What is covered

| Area | Checks |
|------|--------|
| `__init` protocol | Guard word present, CMP/JE/RETF pattern, INC before dep calls |
| `__init` deps | Each imported module's `__init` called in source order |
| SYSTEM exclusion | `SYSTEM__init` never called even when `SYSTEM.*` used |
| `IMPORT SYSTEM` | Compiler exits 1 with error |
| SYSTEM.* access | `SYSTEM.Intr` etc. resolve to lazy RDOFF imports |
| Naming convention | Exported symbols prefixed `ModName_`; `__init` double-underscore |
| Qualified imports | `H := Hello; H.WriteStr` → `Hello_WriteStr` RDOFF import |
| .om contents | tar contains `ModName.rdf` and `ModName.def`; intermediates deleted |
| `-entry` / `MAIN.RDF` | SYSTEM_INIT / ModName__init / ModName_Proc / SYSTEM_DONE in order; RDOFF import IDs 3/4/5/6 |
| Cleanup | `.rdf` and `.def` deleted after `.om` produced; absent on error |
| No INT 20h | `CD 20` never in output (DOS exit is SYSTEM_DONE's job) |
| Exported RETF | Exported proc epilogue uses `CA`/`CB` (RETF); internal uses `C2`/`C3` (RET) |
| Cross-module call | FAR call `9A` + RELOC + SEGRELOC for every imported proc call |
| Underscore in name | `MODULE Foo_Bar` → compiler exits 1 |
| Missing import | `IMPORT Unknown` with no `Unknown.om`/`Unknown.def` → compiler exits 1 |
| NEAR/FAR codegen | NEAR/FAR epilogue bytes, param offsets, call opcodes, proc var encoding (Section 37) |
| EXTERNAL procs | RDOFF IMPORT emitted; FAR external → CALL FAR (9A)+RELOC+SEGRELOC; NEAR external → CALL NEAR (E8)+RELOC; .def export; FAR/NEAR+EXTERNAL combos (Section 38) |
| `-SYSTEM` mode | Compiles without implicit SYSTEM import; IMPORT SYSTEM allowed; no SYSTEM__init dep; no spurious runtime imports (Section 39) |
| `$L` directive | `(*$Lpath.rdf*)` / `//$Lpath.rdf` in source packs extra `.rdf` into `.om` under `USER/<BASENAME>.RDF` (uppercase, `USER/` prefix); up to 128 entries; unreadable path is a fatal compiler error (Section 41) |
| `.om` naming | All member names in `.om` archives are stored in upper case (e.g. `HASBODY.RDF`, `HASBODY.DEF`, `MAIN.RDF`); `tar_extract_file` searches case-insensitively |
| `ADR/SEG/OFS(n^)` | `ADR(n^)` emits `MOV AX,BX`+`MOV DX,ES` = full far pointer = value of `n`; `SEG(n^)` emits `MOV AX,ES`; `OFS(n^)` emits `MOV AX,BX` (Section 43) |
| Typeless VAR param | `VAR name` without type accepted; parameter without type and without VAR keyword rejected with compile error (Section 46e–f) |
| Array dim constants | `ARRAY N OF T` and `ARRAY ROWS, COLS OF T` accept named INTEGER constants as dimensions; non-integer constants rejected (Section 47) |
| `FLOAT`/`ENTIER` synonyms | `FLOAT(x)` compiles identically to `REAL(x)`; `ENTIER(x)` compiles identically to `FLOOR(x)`; FILD/FSTCW byte patterns verified (Section 48) |
| Implicit int→REAL coercion | `r * n`, `n + r`, `r < 0`, `r = 0` etc. compile and emit FILD word (DF 07) for the integer operand; both orderings and all four operator groups covered (Section 49) |
| Filename vs module name | Filename (e.g. `testhello.mod`) used for file open only; output names and RDOFF symbols use module name from `MODULE` keyword (`parser_mod_name`). `-entry` stub imports correct name even with lowercase filename. (Section 53) |
| `$M` stack-size directive | `(*$M size*)` / `//$M size` sets `parser_stack_size`; with `-entry` writes `META-INF/STACK.TXT` (plain decimal) into `.om`; without `-entry` no file written; multiple `$M` last wins; invalid value (zero or non-numeric) is a fatal compile error and no output files are produced. (Section 54) |
| olink stack size override | `olink` reads `META-INF/STACK.TXT` from entry `.om` after smart-link; sets `LinkerState.stack_size`; MZ SP field = `stack_size - 2`; invalid/missing → default `STACK_SIZE=8192`. MZ header verified by parsing SP at byte offset 16. (Section 55) |
| `IS` type test | `p IS Base` compiles (static extension → TRUE); `p IS Ext` where `p: POINTER TO Base` compiles (runtime tag check); `p IS B` for unrelated types compiles (folds to FALSE). `TestTypeTag.Mod` executable verifies 7 runtime IS scenarios including cross-type checks. (Section 57) |
| FORWARD procedures | `PROCEDURE name(params): T; FORWARD;` compiles; body `PROCEDURE name;` resolves and patches all call sites; mutual recursion works; unresolved FORWARD gives error. (Section 58) |
| `$R` directive | `$R-` (default) — no bounds checks emitted. `$R+` — constant out-of-bounds index (negative or ≥ len) gives compile error; variable index emits runtime check calling `SYSTEM_ErrIndexOutOfBounds` (FAR, no args) on violation, which prints a message and halts. Open-array params use LEN slot at `[BP+adr+4]`. Directive toggles mid-file; `$R-` after `$R+` turns checks off. (Section 59) |

---

## Executable tests (Sections 17–36)

Requires `xt` (XT/DW v1.0.2, Java-based DOS emulator) in PATH.
Compile a module with `-entry RunAllTests`, link against `lib/Out.om` + `lib/SYSTEM.om`,
run under `xt run --max=N`, check stdout. Always strip CR: `| tr -d '\r'`.
If `xt` is not in PATH, these tests are skipped (not failed).

All sections 17–36 are implemented in `test_src.sh` and passing.

| Section | Module               | Entry        | --max      | Success condition            |
|---------|----------------------|--------------|------------|------------------------------|
| 17a–d   | HelloWorld.Mod       | RunAllTests  | 5000000    | stdout = `Hello, world!`     |
| 18a–d   | TestTypes.Mod        | RunAllTests  | 5000000    | stdout line `Failed: 0`      |
| 19a–d   | TestControlFlow.Mod  | RunAllTests  | 5000000    | stdout line `Failed: 0`      |
| 20a–d   | TestProcedures.Mod   | RunAllTests  | 50000000   | stdout line `Failed: 0`      |
| 21a–d   | TestLongint.Mod      | RunAllTests  | 5000000    | stdout line `Failed: 0`      |
| 22a–d   | TestLongs.Mod        | RunAllTests  | 10000000   | stdout line `Failed: 0`      |
| 22e–h   | TestBitOps.Mod       | RunAllTests  | 5000000    | stdout line `Failed: 0`      |
| 23a–d   | TestReals.Mod        | RunAllTests  | 10000000   | stdout line `Failed: 0`      |
| 24a–d   | TestLongReals.Mod    | RunAllTests  | 10000000   | stdout line `Failed: 0`      |
| 25a–d   | TestStrings.Mod      | RunAllTests  | 5000000    | stdout line `Failed: 0`      |
| 26a–d   | TestDynamic.Mod      | RunAllTests  | 5000000    | stdout `Failed:` count = 0   |
| 27a–d   | TestProcVar.Mod      | RunAllTests  | 5000000    | stdout line `Failed: 0`      |
| 28a–d   | TestNestedProcs.Mod  | RunAllTests  | 5000000    | stdout `Failed:` count = 0   |
| 29a–d   | TestSystemAdr.Mod    | RunAllTests  | 5000000    | stdout line `Failed: 0`      |
| 30a–d   | TestMultiDim.Mod     | RunAllTests  | 5000000    | stdout line `Failed: 0`      |
| 31a–d   | TestRecords.Mod      | RunAllTests  | 5000000    | stdout line `Failed: 0`      |
| 32a–d   | TestAdrParam.Mod     | RunAllTests  | 5000000    | stdout line `Failed: 0`      |
| 33a–d   | TestPointers.Mod     | RunAllTests  | 5000000    | stdout line `Failed: 0`      |
| 34a–d   | TestParams.Mod       | RunAllTests  | 5000000    | stdout line `Failed: 0`      |
| 35a–d   | TestGlobals.Mod      | RunAllTests  | 5000000    | stdout line `Failed: 0`      |
| 36a–d   | TestNearFar.Mod      | RunAllTests  | 5000000    | stdout line `Failed: 0`      |
| 43a–d   | TestSystem.Mod       | RunAllTests  | 5000000    | stdout line `Failed: 0`      |
| 44a–e   | TestAssert.Mod       | RunAllTests  | 5000000    | stdout line `Failed: 0`; 44e: ASSERT(FALSE) exits non-zero |
| 45a–d   | TestSysIntrinsics.Mod| RunAllTests  | 5000000    | stdout line `Failed: 0`      |
| 46a–d   | TestTypelessVar.Mod  | RunAllTests  | 5000000    | stdout line `Failed: 0`      |
| 51a–d   | TestHello.mod        | RunAllTests  | 5000000    | stdout = `hello world`       |
| 57d–g   | TestTypeTag.Mod      | RunAllTests  | 5000000    | stdout line `Failed: 0`      |
| 58d–g   | TestForward.Mod      | RunAllTests  | 5000000    | stdout line `Failed: 0`      |
| 59g–j   | TestBounds.Mod       | RunAllTests  | 5000000    | stdout line `Failed: 0`      |
| 60c–f   | TestRepeat.Mod       | RunAllTests  | 5000000    | stdout line `Failed: 0`      |

### What each executable test covers

**TestTypes.Mod**: INTEGER arithmetic + boundaries, SYSTEM shift ops (LSL/LSR/ASR/ROR),
BOOLEAN ops, static arrays + indexing, RECORD access + alignment, heap allocation
(NEW/DISPOSE), linked list, far pointers.

**TestControlFlow.Mod**: IF/ELSIF chains, WHILE, REPEAT, FOR ascending/descending/nested,
CASE with single values/ranges/mixed/nested, early exit.

**TestProcedures.Mod**: nested procs without params, value/VAR/mixed params, local variables +
static link uplevel access, recursion (factorial/fibonacci/deep), multiple return points,
deeply nested procs (3 levels), function composition. Uses `--max=50000000`.

**TestLongint.Mod**: LONGINT arithmetic (add/sub/mul/div/mod via SYSTEM_S32MUL/SYSTEM_S32DIV/SYSTEM_S32MOD),
comparisons, INTEGER↔LONGINT casts, LONGINT arrays, LONGINT in records/heap.

**TestLongs.Mod**: additional LONGINT tests (boundaries, shift ops, bitwise ops, large values).

**TestBitOps.Mod**: SYSTEM.AND, SYSTEM.IOR, SYSTEM.XOR on BYTE, INTEGER, and LONGINT operands.
Tests both variable and constant arguments, all three result types.

**TestReals.Mod**: REAL (32-bit float) arithmetic, comparisons, REAL(i)/FLOOR(r) conversions,
static arrays of REAL (1D and 2D via nested ARRAY), REAL in records + alignment,
heap allocation for REAL (NEW/DISPOSE on pointer-to-record and pointer-to-array).
Uses `Out87.Real` for float output; links against `Out87.om` + `Out.om`.

**TestLongReals.Mod**: same coverage as TestReals but for LONGREAL (64-bit double).
Note: all float literals are stored as 4-byte REAL (single precision) — test values
are chosen within single-precision range to avoid precision loss.
Uses `Out87.LongReal` for float output; links against `Out87.om` + `Out.om`.

**TestStrings.Mod**: Strings library — Length, Pos, Append, Insert, Delete, Replace, Extract.
Uses global `ARRAY OF CHAR` variables (buf, dst) so the open-array LEN hidden param
(pushed as second word before near_addr) is correct. Links against `Strings.om`.

**TestProcVar.Mod**: Procedural variables — `PROCEDURE; FAR;` modifier, assignment of FAR proc to
proc-type variable, call through proc variable (both single-arg and two-arg), reassignment,
nested call `fn(fn(x), fn(y))`. Verifies `CALL FAR [BP+ofs]` indirect call and proc-address
load via `MOV AX,offset; PUSH CS; POP DX`.

**TestDynamic.Mod**: Dynamic (heap-allocated) data structures — linked list and binary search
tree using `NEW`/`DISPOSE`. Tests self-referential TYPE declarations (`POINTER TO RECORD ...
next: Node END`). Covers 11 tests: list push/traverse/find/remove/reverse/empty and tree
insert/find/min-max/height/inorder-sum/duplicate/empty. Exercises forward type references
resolved within a single TYPE section.

**TestNestedProcs.Mod**: Nested procedure static link uplevel variable access. Covers:
INTEGER, LONGINT, REAL, LONGREAL, ARRAY, RECORD uplevel load/store (1 hop); 2-hop and 3-hop SL traversal;
passing an uplevel local as a VAR param; sibling nested procs sharing outer state;
closure-like accumulators (FOR loop calling inner proc); uplevel access to outer proc's
formal params; recursive nested proc mutating outer callCount; deep 2-level nesting where
inner proc writes to vars in two different outer frames simultaneously.

**TestSystemAdr.Mod**: `SYSTEM.ADR` applied to global variables, local variables, VAR params,
record fields, array elements, and procedure names. Verifies offset arithmetic,
segment correctness (DS for globals, SS for locals), and round-trip PUT/GET via the
returned ADDRESS value.

**TestMultiDim.Mod**: Multi-dimensional array syntax `ARRAY m, n OF T` (desugared to nested
arrays). Covers: 2D read/write with constant and variable indices; 3D array; global 2D array;
in-place element accumulation and overwrite; 2D BYTE array; 2D CHAR array; row-major layout
verification. Element types: INTEGER, BYTE, CHAR.

**TestRecords.Mod**: Record features — basic field access (local and global), VAR param
pass-by-reference, nested record fields, field alignment with BYTE/INTEGER interleaving,
multiple independent records, CHAR field (CHR/ORD), **CHAR field adjacent to ARRAY field
with string assignment** (regression for byte-load bug: `cg_load_byte_bp/mem` vs
`cg_load_bp/mem`), record extension (single-level and two-level base types), base-field
access via VAR param on extended records, heap-allocated records (NEW/DISPOSE),
heap-allocated extended records, self-referential pointer types and linked-list traversal.

**TestAdrParam.Mod** (section 32): `SYSTEM.ADR` applied to procedure parameters — ADR of
non-VAR parameter (returns slot address, non-zero), ADR of VAR parameter (returns caller's
variable address, equals direct ADR), write through VAR-param ADR using `POINTER TO INTEGER`
deref, ADR of global via VAR parameter. `SYSTEM.ADR` returns the correct segment for all
variable forms.

**TestPointers.Mod** (section 33): Pointer correctness — NIL check before/after NEW/DISPOSE;
pointer-to-record field read/write; pointer-to-array element access; pointer-to-CHAR array;
pointer equality and inequality (two distinct objects, self-alias, NIL=NIL); multiple
independent allocations; global pointer load/store; pointer passed by value to callee; pointer
passed by VAR (callee redirects via `NewAndInit`); pointer chain traversal (singly-linked);
NIL pointer field in heap record; pointer reuse after DISPOSE; self-referential records;
pointer-to-large-array (50 elements); global list build/sum/free via module-level helpers.

**TestParams.Mod** (section 34): Parameter passing — LEN of open-array parameter (global 5-
and 20-element arrays); open-array indexing and sum; VAR open-array fill and read-back; CHAR
open-array LEN, indexing, and upper-case conversion; record via VAR (read-only and scale/swap);
multiple VAR params in one call (MinMax, Divmod); array elements modified via VAR ARRAY param;
record field modified via VAR RECORD param; nested proc using outer VAR; open-array with
different-sized globals; independent VAR params; value param isolation (callee cannot modify
caller's variable); CHAR array VAR parameter write with FillChar.

**TestGlobals.Mod** (section 35): Global variable correctness — global INTEGER read/write
and independence; global BOOLEAN and CHAR; global INTEGER array (fill, sum, element modify,
independence); global ARRAY OF CHAR string literal assignment and element write; global CHAR
array element-by-element write with case conversion; global record field access; two global
records of same type are independent; global record with CHAR field (byte-load isolation);
BYTE/INTEGER interleaved global record; cross-procedure sharing via globals (accumulator);
global array shared across procedures (counter array with BumpCounter).

**TestSystem.Mod** (section 43): SYSTEM module constants (`FZero`/`FSign`/`FCarry`), `SYSTEM.Registers`
field round-trips, `SYSTEM.Intr` (INT 21h write-char), `NEW(addr, n)` / `DISPOSE(addr)` with
`SYSTEM.ADDRESS`, multiple alloc/free cycles.

**TestAssert.Mod** (section 44): `ASSERT(TRUE)` is a no-op; `ASSERT(expr)` continues when true;
`ASSERT(expr, code)` with explicit error code continues when true; 44e verifies that
`ASSERT(FALSE)` halts the program with a non-zero exit code.

**TestSysIntrinsics.Mod** (section 45): Runtime behaviour of SYSTEM intrinsics across all
variable forms — global, local, VAR param, non-VAR param, and pointer dereference (`p^`).
Covers:
- `SYSTEM.SEG`: global → DS; local → SS (≠ DS in large model); VAR param returns caller's
  segment (DS for global actual, SS for local actual); non-VAR param → SS (value copy);
  `p^` → heap segment (non-zero, stable across calls).
- `SYSTEM.OFS`: adjacent globals differ by element size; two locals differ; VAR param
  returns caller's offset; non-VAR param offset stable across calls; `p^` consistent.
- `SYSTEM.MOVE`: global→global array copy (8 bytes); MOVE via VAR param destination
  using full ADDRESS argument (any segment: DS, SS, heap).
  MOVE accepts two far pointers; saves/restores DS around REP MOVSB.
- `SYSTEM.FILL`: zero-fill global array; fill with non-zero byte (verify word value);
  fill via VAR param destination.
- `PTR` + pointer dereference: write/read global via `PTR(SEG, OFS)` and `POINTER TO T`;
  `p^` readable after `SEG`/`OFS` calls; `SEG(p^)` is non-zero.
Note: `SYSTEM.GET`, `SYSTEM.PUT`, and `SYSTEM.VAL` are not available in this dialect.

**TestTypelessVar.Mod** (section 46): Typeless VAR parameters — `VAR p` without `: type`
in a formal parameter list. Covers: `GetAddr(VAR p): ADDRESS` returns non-zero address for
global, returns distinct addresses for two distinct globals, address is stable across repeated
calls; `ReadInt(VAR p): INTEGER` reads a global via `POINTER TO INTEGER` deref;
`WriteInt(VAR p; val)` writes a global via `POINTER TO INTEGER` deref; `GetAddr` on a local
variable (SS-relative) returns non-zero; `SwapInt(VAR a; VAR b)` forwards two typeless VAR
params to a third proc and swaps two globals; `Bump(VAR counter)` increments a global through
a pointer deref three times; cross-proc address relay (WriteInt+ReadInt chain for two distinct
globals). Uses `POINTER TO T` and `p^` to read/write through ADDRESS values.

**TestNearFar.Mod** (section 36): FAR/NEAR calling convention at runtime — NEAR proc called
correctly (RET, params at [BP+4]); FAR proc called correctly (RETF, params at [BP+6]);
NEAR and FAR procs with mixed parameter types; procedural variables of both FAR and NEAR
types; call through FAR proc var; call through NEAR proc var; FAR/NEAR convention preserved
across module boundaries. Verifies that the compiler emits correct call opcodes and epilogues
for all combinations tested by the byte-level section 37 tests.

**Section 48 — FLOAT/ENTIER synonyms** (compile-level, `test_src.sh`): `FLOAT(INTEGER var)` and `FLOAT(integer literal)` compile; `ENTIER(REAL var)` and `ENTIER(LONGREAL var)` compile; `FLOAT(INTEGER)` emits FILD word `DF 07`; `ENTIER(REAL)` emits FSTCW `D9 3F` and FISTP word `DF 1F` — same floor sequence as `FLOOR`. 7 checks. See `tests/TestSynonyms.Mod` for a portable smoke test.

**Section 49 — Implicit INTEGER→REAL coercion** (compile-level, `test_src.sh`): `REAL * INTEGER var`, `INTEGER var * REAL`, `REAL +/- INTEGER`, `r < 0` (REAL vs integer constant zero), `r = 0`, `LONGREAL * INTEGER`, `INTEGER / REAL` all compile. FILD word `DF 07` byte-checked for `REAL * INTEGER` (RHS integer) and `INTEGER + REAL` (LHS integer) and `REAL < integer`. 10 checks. See `tests/TestImplicitCoerce.Mod` for a portable smoke test.

**TestSet.Mod** (section 50): SET type — literals, operators, and IN membership. Covers: `{}` (empty), `{e}` (single element), `{e1,e2,...}` (multi-element), `{lo..hi}` (range without spaces), SET `+` (union), `-` (difference), `*` (intersection), `/` (symmetric difference), `IN` with literal index, `IN` with variable index. 30 checks.

**Section 50 — SET compile-level** (`test_src.sh`): `{}`, `{1,3,5}`, `3 IN s`, `a+b`, `a-b`, `a*b`, `a/b` all compile. 4 checks. Exe tests: compile with `-entry RunAllTests`, link, run, verify `Failed: 0`. 4 checks.

**TestHello.mod** (section 51): Command-line argument printing — `RunAllTests` iterates `Dos.ARGCOUNT()` args starting at index 1 (skipping the empty argv[0] slot), printing each separated by a space and terminated with a newline. Run with `xt run testhello.exe hello world`; expected stdout is `hello world`. Tests `Dos.ARGCOUNT()` and `Dos.ARG()` at runtime with the `xt` emulator passing real command-line arguments.

**TestTypeTag.Mod** (section 57): Runtime `IS` type test — 7 runtime checks. Covers: compile-time fold to TRUE when static type extends RHS (`PDog IS Animal`, `PDog IS Dog`); runtime tag check TRUE when pointer holds a subtype (`PAnimal → Dog IS Dog`); runtime tag check FALSE for sibling types (`pa → Dog IS Cat`); runtime tag check FALSE for plain base type (`PAnimal → Animal IS Dog`); runtime TRUE for Cat type (`PAnimal → Cat IS Cat`); runtime FALSE for wrong subtype (`PAnimal → Cat IS Dog`). Uses `Animal`, `Dog` (extends Animal), `Cat` (extends Animal) type hierarchy.

**TestForward.Mod** (section 58): FORWARD procedure declarations — mutual recursion via FORWARD (`Even`/`Odd` with small integer arguments); forward FAR procedure (`Compute`); forward non-exported procedure (`Helper`). 8 runtime checks. Compile-level checks (58a–58c): simple FORWARD compiles; mutual recursion compiles; unresolved FORWARD gives error.

**TestRepeat.Mod** (section 60): `REPEAT...UNTIL` with a large loop body (> 127 bytes of
generated x86 code). Verifies that `cg_cond_back` correctly emits the 5-byte
`Jcc_inv rel8=+3 / JMP NEAR target` form when the backward offset doesn't fit in an 8-bit
signed value. Compile-level check (60a): compiles without error. Byte-level check (60g):
`75 03 E9` pattern present in `.om`. Runtime checks (60c–60f): `LargeBody(1)=0`,
`LargeBody(3)=3`, `LargeBody(10)=45` — verifies the loop terminates correctly and produces
the right accumulated result.

**TestBounds.Mod** (section 59): `$R` directive and array bounds checking. Uses `(*$R+*)`. In-bounds read/write on fixed-size local array; sum via open-array parameter; boundary element write/read; variable index at boundaries. 7 runtime checks. Compile-level checks (59a–59f): `$R-` (default) allows constant out-of-bounds; `$R+` gives compile error for constant ≥ len; `$R+` gives compile error for negative constant; in-bounds constants compile with `$R+`; `$R-` after `$R+` disables check; `$R+` emits `SYSTEM_ErrIndexOutOfBounds` RDOFF import (not `SYSTEM_Halt`). Runtime halt checks (59k–59l): negative variable index halts with non-zero exit under `$R+`; variable index ≥ len halts with non-zero exit under `$R+`.

**TestConstExpr.Mod** (section 52): Constant expression evaluation — verifies that arithmetic in `CONST` declarations is folded at compile time. Covers: `+`, `-`, `*`, `DIV`, `MOD` on literal integers and named constants; parenthesised sub-expressions `(A+B)*2`; operator precedence `A+B*2`; Oberon-07 floor-division semantics (`(-7) DIV 2 = -4`); unary-minus precedence (`-7 DIV 2 = -(7 DIV 2) = -3`); LONGINT promotion when result exceeds 16-bit (`1000*200 = 200000`). 15 checks. Compile-level checks (section 52a–52f): `+`/`-`/`*`/`DIV`/`MOD`, parentheses, negative operands, large products, chained constants, computed constant as array dimension. 6 checks. Imported constants (qualified as `Alias.Const`) are accepted in constant folding and are usable in array dimension declarations and CASE labels; tests added: `tests/ImportConstA.Mod` and `tests/ImportConstB.Mod`.

---

## Self-hosted compiler module compilation tests

The Oberon self-hosted compiler lives in `src-ob/oc/`. Its modules are compiled with the
C89 bootstrap compiler (`src/oc`). This is not automated in `make test` — run manually
from `src-ob/oc/` when working on self-hosting:

```bash
cd src-ob/oc
OC=../../src/oc
LIB=../../lib

# Compile in dependency order:
OBERON_LIB=$LIB $OC OcScan.Mod
OBERON_LIB=$LIB $OC OcSyms.Mod
OBERON_LIB=$LIB $OC OcRdoff.Mod
OBERON_LIB=$LIB:. $OC OcCgen.Mod
OBERON_LIB=$LIB:. $OC OcTar.Mod
OBERON_LIB=$LIB:. $OC OcDef.Mod
OBERON_LIB=$LIB:. $OC OcImport.Mod
OBERON_LIB=$LIB:. $OC OcPExpr.Mod
OBERON_LIB=$LIB:. $OC OcPStmt.Mod
OBERON_LIB=$LIB:. $OC OcParser.Mod
OBERON_LIB=$LIB:. $OC OcMain.Mod
```

**Pass criterion:** each invocation prints `wrote <Module>.om` with no error lines.
`cg_load_item: bad mode 5` warnings are harmless — they appear for imported procedure
references and do not affect the compiled output.

**Module map:**

| Module file      | Mirrors C89      | Status  |
|------------------|------------------|---------|
| `OcScan.Mod`     | scanner.c        | ✓ Done  |
| `OcSyms.Mod`     | symbols.c        | ✓ Done  |
| `OcRdoff.Mod`    | rdoff.c          | ✓ Done  |
| `OcCgen.Mod`     | codegen.c        | ✓ Done  |
| `OcTar.Mod`      | tar.c            | ✓ Done  |
| `OcDef.Mod`      | def.c            | ✓ Done  |
| `OcImport.Mod`   | import.c         | ✓ Done  |
| `OcPExpr.Mod`    | pexpr.c          | ✓ Done  |
| `OcPStmt.Mod`    | parser.c (stmts) | ✓ Done  |
| `OcParser.Mod`   | parser.c (decls) | ✓ Done  |
| `OcMain.Mod`     | main.c           | ✓ Done  |

All modules compile cleanly as of Stage 2 completion (2026-04-29).

**Known idiom restrictions** (enforced by the bootstrap compiler dialect):
- No `LOOP`/`EXIT` — use `WHILE done := FALSE; WHILE ~done DO ... END`
- No `SHORT()` — use direct assignment for LONGREAL→REAL coercion
- No `SYSTEM.VAL/GET/PUT` — use `POINTER TO T` and `p^` dereference
- `SYSTEM.AND(x, LONGINT(0FFFFH))` instead of `INTEGER MOD 65536`
- All `SYSTEM.*` intrinsics require the `SYSTEM.` qualified prefix
- `REPEAT...UNTIL` with any body size: `CgCondBack` now emits a 5-byte `Jcc_inv +3 / JMP NEAR`
  when the backward offset exceeds 8 bits — no conversion to `WHILE` needed

---

## xt emulator reference

```
xt run   [--max=N] [-c dir] program [args]   # run .EXE/.COM, print stdout
xt trace [--max=N] [--bp=SEG:OFS[:COND]]...
         [--wp=SEG:OFS:type]...
         [--dump=SEG:OFS:LEN]...  program    # trace with register/stack dump per insn
xt int                                        # list supported DOS interrupts
```

**`xt run` options:**
- `--max=N` — stop after N instructions (ALWAYS use to prevent infinite loops)
- `-c dir` — host directory used as the C: drive root (default: cwd)
- `--env=KEY=VALUE` - allow pass environment variables
- Output is DOS CRLF (`\r\n`); always strip CR: `| tr -d '\r'`
- Exit code 255 = emulator error; otherwise = program exit code

**`xt trace` options:**
- `--max=N` — stop after N instructions (mandatory)
- `--bp=SEG:OFS` — breakpoint at hex address; e.g. `--bp=00A0:0020`
- `--bp=SEG:OFS:COND` — conditional: `AX==1234`, `FLAGS.ZERO==1`, etc.
  Supported regs: `AX BX CX DX SI DI BP SP DS ES SS FLAGS.CARRY FLAGS.ZERO FLAGS.OVERFLOW`
- `--wp=SEG:OFS:r|w|a` — watchpoint (read/write/access)
- `--dump=SEG:OFS:LEN` — hex dump of memory region at stop
- `--env=KEY=VALUE` - allow pass environment variables
- Does NOT support `-c`; run from the directory containing the `.EXE`
- Trace format per instruction: `CS:IP | bytes | disasm | registers | SS:SP | FLAGS`
- After stop: full register dump + 32-word stack dump

**Memory layout (EXE files):**
- PSP: `0x0090:0x0000` (256 bytes)
- Code: starts at `0x00A0:0x0000`
- Stack: `0xF000:0xF000`

**Supported interrupts:** INT 20/21 (DOS), INT 1A (time), INT 2F (XMS/clipboard), INT 67 (EMS).
Key INT 21 functions: 02 (write char), 09 (write string), 3C/3D/3E/3F/40/41/42 (files),
48/49/4A (memory), 4B00 (EXEC), 4C (terminate).

---

## Adding tests

### New unit test case

Add to the relevant `test_xxx.c`. Follow the existing pattern: setup, call function,
`assert` or compare byte arrays. Rebuild with `cd src && make tests`.

### New executable test (preferred path)

Write a `TestXxx.Mod` in `tests/` with `PROCEDURE RunAllTests*` that runs subtests and prints
`Failed: N` at the end. Add it as the next numbered section (currently 60+) in the xt block of `test_src.sh` following the
template below. Add a row to the executable tests table and a description paragraph in
"What each executable test covers."

Executable tests are the **preferred** way to add coverage. Only fall back to byte-level checks
when the behaviour is not observable at runtime (RDOFF layout, opcode selection, error messages).

### New integration test in test_src.sh

1. Create a minimal `.Mod` file in `tests/` (or a temp dir inside the script).
2. Run `./oc` on it.
3. Check output with `xxd`, `grep`, or binary comparison against `.expected` file.
4. Print `PASS`/`FAIL` and `exit 1` on failure.
5. Clean up temp files at end of test block.

Pattern:
```bash
echo "=== My new test ==="
cd "$TMPDIR"
cat > MyTest.Mod << 'EOF'
MODULE MyTest;
BEGIN
END MyTest.
EOF
"$OC" MyTest.Mod || { echo "FAIL: compile error"; exit 1; }
# check output...
echo "PASS"
```

### New executable test (xt)

1. Write the `.Mod` file in `tests/` with an exported `RunAllTests*` procedure.
2. Add subtests to `tests/test_src.sh` inside the `if XT available` block:
   ```bash
   # NNa: compile
   (cd "$WORKDIR" && OBERON_LIB="$WORKDIR:$OLDPWD/lib" \
       "$OLDPWD/$OBERONC" -entry RunAllTests "$OLDPWD/tests/MyMod.Mod") >/dev/null 2>&1
   check_pass "exe NNa: MyMod.Mod compiles with -entry RunAllTests"
   # NNb: lib check
   [ -f "$WORKDIR/MyMod.om" ]
   check_pass "exe NNb: MyMod.om produced"
   # NNc: link
   "$OLINK" "$WORKDIR/MyMod.om" "$WORKDIR/Out.om" \
       "$OLDPWD/lib/SYSTEM.om" "$WORKDIR/mymod.exe" >/dev/null 2>&1
   check_pass "exe NNc: MyMod links to mymod.exe"
   # NNd: run — always use --max and tr -d '\r'
   ACTUAL=$(cd "$WORKDIR" && "$XT" run --max=10000000 mymod.exe 2>/dev/null \
       | grep -v '^Maximum instructions' | tr -d '\r' || true)
   FAILED_LINE=$(echo "$ACTUAL" | grep '^Failed: ' || true)
   [ "$FAILED_LINE" = "Failed: 0" ]
   check_pass "exe NNd: mymod.exe reports 0 failures"
   ```
3. Add a row to the executable tests table above.
4. Document what the test covers in the "What each executable test covers" section.

---

## SYSTEM.DEF validity (once `--check-def` is implemented)

These tests run **before all others**:

```bash
oc --check-def src-lib/SYSTEM.DEF
# exit 0 = PASS, non-zero = FAIL

oc --check-def src-lib/SYSTEM.DEF --emit-def /tmp/system_rt.def
diff src-lib/SYSTEM.DEF /tmp/system_rt.def
# diff empty = PASS (round-trip)
```
