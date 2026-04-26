# Implementation Rules — C89 Compiler for MS-DOS

Rules that govern how the compiler itself is written (not the Oberon language it compiles).
See also: [Oberon language rules](oberon-language.md).

---

## C standard and portability

The source (`src/`) must compile cleanly under both:
- `gcc/clang -std=c89` (host development, Linux/Mac)
- `wcc -bt=dos -ml` (Open Watcom, DOS target, large model, effectively C89)

**Rules (target C89 — the lower common denominator):**
- No `//` comments — use `/* */` only
- All variable declarations at the top of each block
- No `unistd.h` — use `compat.h` alternatives (`file_exists` instead of `access`)
- All `fopen` for binary files (`.rdf`, `.om`) must use `"rb"` / `"wb"` mode
- Text files (`.def`, `.Mod` source) use `"r"` / `"w"` — `\r\n` translation acceptable
- No VLAs, no designated initializers, no `inline`, no compound literals
- No variable-length function argument lists beyond `fprintf`/`snprintf`
- Can't allocate more 65000 bytes with malloc.

---

## Codegen emit function types  ← CRITICAL

All `cg_emit*` functions in `codegen.c`/`codegen.h` use `stdint.h` types — not `int`.
This is required for correct behaviour when compiled with Open Watcom (16-bit `int`).

| Function | Parameter types |
|----------|----------------|
| `cg_emit1(b)` | `uint8_t` |
| `cg_emit2(b0, b1)` | `uint8_t, uint8_t` |
| `cg_emit3(b0, b1, b2)` | `uint8_t, uint8_t, uint8_t` |
| `cg_emitw(w)` | `uint16_t` |
| `cg_emitd(d)` | `uint32_t` |
| `cg_emit_data_byte(b)` | `uint8_t` |
| `cg_emit_data_word(w)` | `uint16_t` |
| `cg_emit_data_zero(n)` | `uint16_t` |

Jump offset arithmetic (`cg_patch_short`, `cg_patch_near`, `cg_jmp_back`, `cg_cond_back`,
`cg_call_near`) uses `int32_t` for the `rel` intermediate. Offsets span 0..64000,
so subtraction can reach −64000 — too large for signed 16-bit `int`.

When passing an `int` opcode to `cg_emit1`, cast explicitly: `cg_emit1((uint8_t)opcode)`.

---

## Integer type rules  ← CRITICAL

`int` is **16 bits** on DOS (Open Watcom) and 32 bits on Linux/Mac.
Using plain `int` for anything that can exceed 32767 will silently overflow on DOS.
**Use `<stdint.h>` types everywhere a specific width matters.**

| What it represents | Type to use |
|--------------------|-------------|
| Byte buffer / binary data | `uint8_t` |
| RDOFF/MZ 16-bit word fields | `uint16_t` |
| Code/data segment offsets (0..64KB) | `uint16_t` |
| Data segment variable address | `uint16_t` |
| Code buffer PC (`cg_pc`) | `uint16_t` |
| Backpatch offset in code buffer | `int32_t` |
| BP-relative frame offset (signed, ±32KB) | `int32_t` |
| Record field offset | `uint16_t` |
| Type/array size in bytes | `uint16_t` |
| Proc arg_size / local frame size | `uint16_t` |
| Header build buffer index (`hlen` in rdf_write) | `uint32_t` |
| RDOFF record counts (n_relocs, n_imports, n_globals) | `int` (fine — max 8192 < 32K) |
| Small counts, loop indices, flags, enums | `int` (fine — never > 32K) |
| `fgetc` return (EOF detection) | `int` (required by C standard) |
| `ftell` result | `long` (platform canonical) |
| `malloc` size argument | `size_t` (16-bit unsigned on DOS, holds up to 65535) |
| `fread`/`fwrite` count/return | `size_t` |

Do **not** use `unsigned char` / `unsigned short` / `unsigned long` / `int` / for binary data.
Use the appropriate `uintN_t` instead.
Do **not** over-widen: use the narrowest correct type.

---

## DOS memory model constraints  ← CRITICAL

The compiler runs as a real-mode DOS application.
**No single struct or static/global array may exceed 64KB.**

Open Watcom **large model** (`-ml`): far code (multiple code segments), far data (multiple
data segments). `malloc` returns far pointers automatically. Both code and data can exceed
one 64KB segment.

### malloc size limit — 65000 bytes maximum

**Never call `malloc` with more than 65000 bytes.**
On DOS the heap lives in a single 64KB segment. Requesting more will fail or
wrap around, corrupting memory silently. The fixed limit is `65000` bytes
(not 65535 — leave a small margin for heap bookkeeping overhead).

```c
/* WRONG — may fail on DOS */
hdr = malloc(262144);

/* CORRECT */
hdr = malloc(65000);
```

### ObjFile — heap-allocated buffers

`ObjFile` in `rdoff.h` stores all large arrays as **heap pointers**, not inline
arrays. Call `rdf_init(obj)` to `malloc` all buffers; call `rdf_free(obj)` when done.

| Buffer  | Pre-alloc   | Segment overflow limit |
|---------|-------------|------------------------|
| code    | 65500 bytes | 64000 bytes → exit(1)  |
| data    | 65500 bytes | 64000 bytes → exit(1)  |
| bss     | (int only)  | 64000 bytes → exit(1)  |
| relocs  | MAX_RELOCS × sizeof(Reloc) | |
| imports | MAX_IMPORTS × sizeof(Import) | |
| globals | MAX_GLOBALS × sizeof(Global) | |

`#define MAX_BUF 65500` — pre-alloc size.
`#define SEG_LIMIT 64000` — error threshold checked in `emit_byte`/`rdf_set_bss`.

`rdf_write` heap-allocates its header build buffer (65000 bytes, not a stack local).

### Array size limit — use linked lists for > 64 elements

**Never use a fixed-size array with more than 64 elements** in a struct, at file
scope, or on the stack. DOS near-data and stack segments are 64KB total; a single
large array consumes that budget.

For variable-length collections (symbol tables, import lists, relocation tables,
module lists, etc.) use **linked lists** with individually `malloc`'d nodes.

```c
/* WRONG — 8192-element array exhausts DOS stack/data */
static Reloc relocs[8192];

/* CORRECT — linked list with heap nodes */
typedef struct RelocNode { Reloc r; struct RelocNode *next; } RelocNode;
static RelocNode *reloc_head;
```

The `olink.c` linker already follows this pattern: `TModule`, `TSymbol`, `TMZReloc`,
`TImportNode` are all linked-list nodes allocated with `malloc`.

`ObjFile` fixed arrays (`relocs[]`, `imports[]`, `globals[]`) are a known exception:
they are **heap-allocated** by `rdf_init` and their counts are bounded by DOS segment
size limits anyway. (Tracked improvement: convert to linked lists.)

### Rules
- **No inline arrays of 64KB or more** in any struct or at file scope.
- **No fixed arrays > 64 elements** — use linked lists.
- **No `malloc` call requesting > 65000 bytes.**
- Use `malloc`/`free` for large buffers; use linked lists for variable-length data.
- Any stack-local array ≥ ~1KB is a DOS stack-overflow risk — heap-alloc it.
- `cg_free()` wraps `rdf_free(&cg_obj)` — call at program exit or before re-init.
- `test_codegen.c::reset()` calls `cg_free()` before `cg_init()` to prevent leaks.

---

## Large memory model — codegen segment register rules  ← CRITICAL

The **compiled Oberon program** runs in large model (CS≠DS≠SS).  The **compiler
itself** also builds with Open Watcom large model (`-ml`), but this section is
about the code the compiler **generates**, not the compiler binary.

### Segment register values at Oberon program runtime

```
CS  = this module's code segment paragraph   (unique per module)
DS  = combined data segment                  (same for all modules; set by SYSTEM_INIT)
SS  = stack segment                          (separate from DS; set by MZ header)
```

**CS ≠ DS ≠ SS at all times** — this is the defining invariant of the large model.

### Codegen rules (enforced in parser.c / pexpr.c)

| Situation | Correct code | Wrong (small-model) code |
|-----------|-------------|--------------------------|
| VAR param — local actual | `PUSH SS; PUSH AX` | ~~`PUSH DS; PUSH AX`~~ |
| VAR param — global actual | `PUSH DS; PUSH AX` | ~~`PUSH SS; PUSH AX`~~ |
| Array elem in DS via ES:[BX] | `MOV DX,DS; MOV ES,DX` before access | ~~assume ES=DS~~ |
| SYSTEM.GET/PUT (far ADDRESS) | `MOV ES,DX; MOV BX,AX` then `ES:[BX]` | ~~`ES:[BX]` with DS offset~~ |
| SYSTEM.MOVE (far src, far dst) | `PUSH DS; MOV DS,src_seg; REP MOVSB; POP DS` | ~~`PUSH SS; POP DS` to restore~~ |
| SYSTEM.FILL (far dst) | `MOV ES,dst_seg; REP STOSB` | ~~assume ES=DS~~ |
| SYSTEM.SEG(local) | `MOV AX, SS` | ~~`MOV AX, DS`~~ |
| SYSTEM.SEG(global) | `MOV AX, DS` | ~~`MOV AX, SS`~~ |
| FPU stack temps (fpush/fpop) | `SS:` prefix on `[BX]` where BX=SP | ~~no prefix (uses DS)~~ |
| Restore DS after clobbering | `PUSH DS` before, `POP DS` after | ~~`PUSH SS; POP DS` (SS≠DS)~~ |

### Why SYSTEM.GET/PUT load the segment from the ADDRESS argument

`SYSTEM.GET(a, var)` and `SYSTEM.PUT(a, expr)` take a full `ADDRESS` (far pointer,
DX:AX = segment:offset).  The intrinsic does `MOV ES,DX; MOV BX,AX` and accesses
via `ES:[BX]`.  This works for any segment (heap, stack, data).  ES is a **scratch
register** — callee-clobbered — so it must always be set explicitly from DX.

### Why locals use SS and globals use DS

- **Locals** (K_VAR at level>0, K_PARAM, K_VARPARAM) live in the stack frame.
  The stack is in SS.  `LEA AX,[BP+ofs]` gives an SS-relative offset.
- **Globals** (K_VAR at level=0) live in the data segment.
  Data is in DS.  `LEA AX,[data_ofs]+RELOC` gives a DS-relative offset.
- **Uplevel vars** (accessed via static-link chain) live in an outer stack frame
  still in SS — use SS.

---

## Known 16-bit portability issues  ← FIX BEFORE DOS BUILD

**Fixed in current codebase:**
- `codegen.c` emit functions and jump offset arithmetic — use correct `stdint.h` types
- `scanner.c` hex/char literal accumulator — changed from `int` to `int32_t`
- `scanner.h` added `double rval` field for floating-point literals (`T_REAL`)
- `codegen.h` `Item.val` — changed from `int` to `int32_t`
- `codegen.h/.c` `cg_load_imm`, `cg_load_imm_cx`, `cg_cmp_ax_imm` — `int` → `int16_t`
- `rdoff.c` `hlen` in `rdf_write` — changed from `int` to `uint32_t`; malloc reduced to 65000
- `def.c` all `sscanf`/`fprintf` for `int32_t` fields — now use `%ld`/`(long)` correctly
- `symbols.h` TF_REAL=14, TF_LONGREAL=15, SZ_REAL=4, SZ_LONGREAL=8 added
- `codegen.h` M_FREG=7 item mode added; all FPU helpers declared
- `src/Makefile.wat` uses `-ml` (large model) matching `src/Makefile` comment
- `compat.c` `str_upcase`, `olink.c` `str_upper`/`str_ends_with` — replaced locale-dependent `toupper` with ASCII-only `ascii_upper` helper; removed `#include <ctype.h>` from both files

**Still open (tracked):** none — all known issues fixed.

**Fixed in this pass (stabilization):**

| File | Issue | Fix applied |
|------|-------|-------------|
| `rdoff.c` | `%d` with `SEG_LIMIT=64000` in error messages | Already used `%u` + `(unsigned)` cast — no change needed |
| `tar.c` | `malloc(uint32_t fsize)` may truncate on 16-bit | Already has `fsize > 65000u` guard — no change needed |
| `olink.c` | `malloc(total_code/data_len)` without 64KB guard | Already has `> 65000u` guard in `olink_perform_linking` — no change needed |
| `olink.c` | MZ header padding loop used `zeros[32]` buffer (overflow for large reloc tables) | Changed to `zeros[512]` with chunked write loop |
| `olink.h` | `lib_paths[16][256]` = 4096-byte inline array in `LinkerState` (struct too large for stack) | Changed to `char *lib_paths[16]` (heap-allocated via `xstrdup`) |
| `olink.c` | `lib_paths` stored with `strncpy` into fixed array | Updated to use `xstrdup`; `olink_free` now frees each entry |
| `main.c` | `set_lib_path` used two 4096-byte stack locals (`buf`, `env_buf`) | Moved to `static` variables (512+528 bytes, not on stack) |
| `src/Makefile.wat` | `wlink` command line exceeded DOS 127-char limit | Rewrote to use wmake `%create`/`%append` response files (`oc.lnk`, `olink.lnk`) |
| `scanner.c` | CR (`\r`) handling | Already skipped in whitespace loop — no change needed |

---

## Path handling

All file path operations use `compat.h` macros and functions:

| Symbol | DOS | Unix |
|--------|-----|------|
| `PATH_SEP` | `\\` | `/` |
| `IS_SEP(c)` | `c=='\\' \|\| c=='/'` | `c=='/'` |
| `ENV_SEP` | `;` | `:` |

**Rules:**
- Use `path_basename(path)` instead of `strrchr(path, '/')`
- Use `path_join(dst, dstsz, dir, file)` to build paths
- OBERON_LIB is split by `ENV_SEP` — supports multiple directories
- All path buffers: 256 bytes (safe on both DOS max-128 and Unix)
- Library search order: current directory → OBERON_LIB entries (left to right)

---

## .def file format  ← CANONICAL

The `.def` file written by `def_write` and read by `def_read`.

**Write order** (guaranteed by `def_write`): CONST → TYPE → VAR → PROC/INLINE.
TYPE is emitted in two sub-passes: root records (no base) first, then extended records
(with BASE).  This ensures every named type (`POINTER SomeRec`, `BASE ModName_Base`) is
defined before it is referenced, so `def_read` never needs a forward-lookup for types.

```
MODULE ModuleName
CONST  ModuleName_SymName  intValue
TYPE   ModuleName_TypeName
TYPE   ModuleName_RecName RECORD totalSize
  FIELD fieldName type offset
  ...
END
TYPE   ModuleName_DerivedName RECORD totalSize
  BASE  ModuleName_BaseName
  FIELD fieldName type offset
  ...
END
VAR    ModuleName_SymName  segId  byteOffset
PROC   ModuleName_ProcName FAR rettype
  PARAM name type
  PARAM VAR name type
END
INLINE ModuleName_ProcName rettype
  PARAM name type
  BYTES byte ...
END
```

**Type tokens** used in FIELD, PARAM, and PROC return positions:
`VOID` / `INTEGER` / `LONGINT` / `BOOLEAN` / `CHAR` / `BYTE` / `REAL` / `LONGREAL` /
`SET` / `ADDRESS` / `ARRAY` / `POINTER name` (two tokens; `name` is the record symbol
short name or `VOID` for anonymous pointer base).

`REAL` and `LONGREAL` are **primitive base types** — resolved directly without scope
lookup in `resolve_type_name`, just like `INTEGER`.  No `K_TYPE` symbol is needed.

**RECORD block format:**
- `TYPE fullname RECORD totalSize` — opens a record definition
- `  BASE ModuleName_BaseName` — optional; sets the base (extended) record; collected
  during parsing and resolved after all types in the file are loaded (linked-list of
  deferred `BasePatchNode` entries, freed after resolution)
- `  FIELD name type offset` — one line per exported field; type is a type token
- `END` — closes the record and restores the authoritative total size

**PROC line format** (always used — no legacy form):
- `PROC fullname FAR rettype` or `PROC fullname NEAR rettype`
- Followed by zero or more `  PARAM [VAR] name type` lines
- Terminated by `END`
- Even procedures with **no parameters** use this format
