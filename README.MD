oc86 — Oberon-07 compiler for MS-DOS real mode (16-bit)
========================================================

Summary
-------
`oc86` is a C89 compiler for the Oberon-07 language that targets MS-DOS real-mode large memory model (CS≠DS≠SS). It produces RDOFF2 object files and `.om` archive libraries, which are linked into DOS MZ executables by the bundled `olink` smart linker. The toolchain runs on Linux/macOS (GCC/Clang) for development and can also be compiled by Open Watcom for native DOS use.

**Self-hosting status:** Stage 2 complete (2026-04-29). All 11 compiler modules ported to Oberon
and compiled by the C89 bootstrap. The self-hosted Oberon compiler sources live in `src-ob/oc/`.
Stage 3 (porting the linker, `src-ob/olink/`) is next.

Key features
------------
- Full Oberon-07 subset with practical extensions (see "Language extensions" below).
- RDOFF2 code generation with smart linking — only reachable modules are emitted.
- x87 FPU support: REAL (32-bit) and LONGREAL (64-bit) arithmetic, comparison, and conversion.
- LONGINT (32-bit signed integers) with helpers for multiplication, division, and modulus.
- SET type (16-bit): literals, ranges, set operators (+, -, *, /), IN membership test, and IS compile-time type test.
- Constant expression folding for CONST declarations.
- Constant folding accepts imported constants (qualified imports). Imported consts can be used in ARRAY dimensions, CASE labels and in other CONST definitions (folding and nested CONST expressions supported). Unit/integration tests added: tests/ImportConstA.Mod, tests/ImportConstB.Mod, and tests/TestIsInstance.Mod.
- FAR/NEAR procedure calling conventions with procedural variables.
- Nested procedures with full static-link uplevel variable access (up to 3 hops).
- Dynamic allocation: NEW/DISPOSE with typed and sized variants; ADDRESS far pointers.
- Forward and self-referential POINTER TO RECORD types within a TYPE section.
- SYSTEM intrinsics: MOVE, FILL, ADR, SEG, OFS, PTR, LSL, LSR, ASR, ROR (compiler built-ins); PORTOUT/PORTIN as INLINE procs in SYSTEM.mod. Note: VAL, GET, PUT removed from dialect; use `POINTER TO T` and `p^` instead.
- EXTERNAL procedure declarations (body provided by an external .rdf at link time).
- $L directive to pack hand-assembled .rdf files into .om archives.
- $M directive to declare stack size; stored as `META-INF/STACK.TXT` in the `.om` when using `-entry`.
- $R directive to enable/disable array bounds checking (calls `SYSTEM_ErrIndexOutOfBounds` on violation).
- Import namespace isolation: imported symbols keyed as `alias.name` to prevent collisions.
- Unbounded linked-list accumulator lists — no silent truncation on large modules.

Language extensions (vs. standard Oberon-07)
---------------------------------------------
- `//` single-line comments.
- `(*$L path.rdf*)` / `//$L path.rdf` system comment to include extra .rdf in .om.
- `(*$M size*)` stack size hint; writes `META-INF/STACK.TXT` to `.om`.
- `(*$R+*)` / `(*$R-*)` enable/disable array bounds checks.
- `FAR` / `NEAR` context-sensitive modifiers on PROCEDURE declarations and types.
- `EXTERNAL` modifier on PROCEDURE declarations (no body; resolved at link time).
- `VAR name` without `: type` (typeless VAR, Turbo-Pascal style) — callee receives ADDRESS.
- `ADDRESS` built-in type (4-byte far pointer, any segment).
- `FLOAT(x)` as a synonym for `REAL(x)`.
- `ENTIER(x)` as a synonym for `FLOOR(x)`.
- `ARRAY m, n OF T` multi-dimensional array sugar (desugared to nested arrays, up to 7 dims).
- Named integer constants as array dimension: `ARRAY N OF T`.
- `NEW(p, n)` sized allocation (n is a runtime byte-count expression).
- `SYSTEM.Registers`, `SYSTEM.Intr`, `SYSTEM.FZero`, `SYSTEM.FSign`, `SYSTEM.FCarry` from DOS/SYSTEM library.
- Implicit INTEGER→REAL/LONGREAL coercion in binary expressions (both orderings, all operators).

Standard features that are NOT implemented
-------------------------------------------
- Mutual recursion via non-pointer fields across a TYPE section (requires two-pass scanner).

Directory layout
----------------
```
oberonc/
  src/          C89 compiler (oc) + smart linker (olink)
  src-lib/      Oberon library sources + hand-assembled SYSTEM helpers
  lib/          Compiled library output (built by src-lib/Makefile install)
  tests/        Unit test sources (.c) and integration test modules (.Mod)
  docs/         Grammar, ABI, build and linker reference
  src-ob/oc/    Self-hosted Oberon compiler (Stage 2 complete; 11 modules)
  src-ob/olink/ Self-hosted Oberon linker (Stage 3, in progress)
  CLAUDE.md     Developer guide for Claude Code sessions
  MANUAL.MD     Programmer manual: dialect, ABI, calling conventions, open arrays
```

Standard library modules (in lib/)
-----------------------------------
| Module   | Purpose                                          |
|----------|--------------------------------------------------|
| SYSTEM   | Runtime helpers: alloc, halt, FPU check, INT calls |
| Out      | Character, string, integer, hex output to stdout |
| Out87    | REAL / LONGREAL formatted output (needs FPU)     |
| In       | Stdin input (integers, strings)                  |
| Strings  | Length, Pos, Append, Insert, Delete, Replace, Extract, Equal, Copy |
| Files    | DOS file I/O (open, read, write, close, seek)    |
| Crt      | Terminal: cursor, clear, colour (DOS INT 10h)    |
| Dos      | DOS interface: ARGCOUNT, ARG, command-line args  |
| IO       | Raw console/file byte I/O (Rider wrapping DOS handle; StdOut, StdErr, ReadChar, WriteChar, WriteStr, WriteInt) |
| Mem      | Heap allocator with 65000-byte limit guard (Alloc/Free via typed Block pointer) |

Getting started
---------------
Requirements:
- GCC or Clang (Linux/macOS) or Open Watcom (wmake, DOS)
- GNU make
- Optional: `xt` DOS emulator (for running and testing .EXE files on host)

Build:

```sh
make            # build oc and olink, compile and install all libraries to lib/
make test       # full clean + rebuild + run all tests
```

Build for DOS (Open Watcom):

```sh
make oc-dos     # runs wmake -f src/Makefile.wat (large model -ml)
```

Compile a module:

```sh
OBERON_LIB=lib ./src/oc Hello.Mod        # produces Hello.om
```

Compile with entry point and link to executable:

```sh
OBERON_LIB=lib ./src/oc -entry RunAllTests Hello.Mod
./src/olink Hello.om lib/Out.om lib/SYSTEM.om hello.exe
```

Run under the xt emulator (Linux/macOS):

```sh
xt run --max=5000000 hello.exe
```

Packaging hand-written assembler code
--------------------------------------
The recommended way to include a hand-assembled `.rdf` in a library archive is via the `$L` directive in a wrapper module:

```oberon
MODULE MyLibPack;
PROCEDURE Add*(a, b: INTEGER): INTEGER; NEAR; EXTERNAL;
(*$L MYLIB.RDF*)
END MyLibPack.
```

```sh
./src/oc MyLibPack.Mod   (* -> MyLibPack.om containing MYLIB.RDF *)
```

Calling convention summary
---------------------------
| Proc type     | Epilogue | First param | Proc-var size |
|---------------|----------|-------------|---------------|
| FAR (exported)| RETF     | [BP+6]      | 4 bytes       |
| NEAR internal | RET      | [BP+4]      | 2 bytes       |
| NEAR nested   | RET      | [BP+6]      | 2 bytes (SL at [BP+4]) |

VAR parameters pass a 4-byte far address; open-array formals use a 6-byte slot {offset, segment, LEN}.

Memory model summary
---------------------
DOS real-mode large model: CS≠DS≠SS. The linker places all modules' data in one combined DS, code groups in their own CS paragraphs, and the stack in a separate SS. `SYSTEM.ADR` returns a full far ADDRESS (segment:offset) for any variable form; locals use SS, globals use DS.

Test suite
----------
853 tests across 8 suites:

| Suite                       | Count | What it covers |
|-----------------------------|-------|----------------|
| test_scanner.c (unit)       | 94    | Tokens, keywords, literals, comments |
| test_symbols.c (unit)       | 54    | Scopes, types, BP offsets, arg sizes |
| test_codegen.c (unit)       | 130   | Exact byte sequences for every codegen path |
| test_rdoff.c (unit)         | 76    | RDOFF2 binary format (IMPORT/GLOBAL/RELOC) |
| test_tar.c (unit)           | 26    | ustar archive writer |
| test_def.c (unit)           | 104   | .def writer / reader round-trip |
| test_src.sh (integration)   | 341   | Compile, link, RDOFF structure, error messages |
| test_system_intrinsics.sh   | 28    | SYSTEM intrinsic runtime behaviour |

Executable integration tests (sections 17–59 in test_src.sh) compile Oberon modules with `-entry RunAllTests`, link them, and run them under `xt`. If `xt` is not in PATH those tests are skipped (not failed).

Documentation
-------------
- `MANUAL.MD` — programmer manual: dialect, ABI, calling conventions, open arrays, assembler workflow
- `docs/oberon07.ebnf` — authoritative grammar (kept in sync with scanner.c / parser.c)
- `docs/oberon-language.md` — type sizes, ABI, naming, FAR/NEAR, module init
- `docs/implementation-rules.md` — C89 rules, 16-bit portability, DOS memory constraints
- `docs/build.md` — detailed build instructions
- `docs/olink.md` — linker design, input formats, relocation, MZ layout
- `docs/tests.md` — all test information: how to run, add, and interpret tests
- `docs/rdoff2-spec.md` — RDOFF2 format specification

License
-------
Public domain. See LICENSE.TXT.
