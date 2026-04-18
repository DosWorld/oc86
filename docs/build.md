# Build Guide

## Toolchain binaries

| Binary | Purpose |
|--------|---------|
| `oc` / `oc.exe` | Oberon-07 compiler → RDOFF2 + .om |
| `olink` / `olink.exe` | RDOFF2 smart linker → DOS MZ EXE |

Both written in C89. Both build on Linux/Mac (gcc/clang) and DOS (Open Watcom large model `-ml`).

---

## Building on Linux / Mac (gcc/clang)

```bash
make              # from oberonc/ root: builds oc + olink, then compiles + installs libs → lib/
make test         # build everything + run full test suite
```

Step by step:
```bash
cd src && make              # builds oc and olink only
cd src-lib && make install  # compiles SYSTEM.om + Out.om, copies to ../lib/
                            # also copies SYSTEM.DEF to ../lib/
cd src && make tests        # builds unit test binaries only (in tests/)
```

Or manually:
```bash
cc -O2 -Wall -std=c89 -o oc main.c scanner.c symbols.c codegen.c rdoff.c parser.c tar.c def.c compat.c
cc -O2 -Wall -std=c89 -o olink olink.c compat.c
```

---

## Building on DOS (Open Watcom)

```bash
# From oberonc/ root:
make oc-dos           # runs: wmake -f src/Makefile.wat → oc.exe and olink.exe

# Or directly:
cd src
wmake -f Makefile.wat
```

Memory model: **large** (`-ml`)
- Far code: multiple code segments (compiler size exceeds one 64KB segment)
- Far data: multiple data segments (required for large symbol/code/heap buffers)
- `int` = 16 bits, `long` = 32 bits — all code must follow portability rules in `docs/implementation-rules.md`

---

## Using the compiler

### Compile a module

```bash
./oc File.Mod              # → File.om  (File.rdf + File.def packed, then deleted)
```

Normal output:
1. Writes `ModName.rdf` and `ModName.def` as intermediates
2. Packs them into `ModName.om` (ustar tar)
3. Deletes `ModName.rdf` and `ModName.def` — only `.om` remains
4. On error: intermediates deleted, no `.om` written

### Compile the SYSTEM module itself

`SYSTEM.om` is built from two sources:

- `src-lib/SYSTEM.mod` — Oberon source for the SYSTEM module (compiled with `-SYSTEM`)
- `src-lib/SYS.RDF` — pre-assembled RDOFF2 object for `src-lib/SYS.ASM` (hand-assembled helpers);
  packed into the output `.om` via a `(*$LSYS.RDF*)` directive in `SYSTEM.mod`

```bash
# Done automatically by: cd src-lib && make install
cd src-lib
../src/oc -SYSTEM SYSTEM.mod   # → SYSTEM.om  (includes SYS.RDF via $L directive)
```

**IMPORTANT — `SYS.RDF` is a checked-in binary:**
`src-lib/SYS.RDF` is the pre-assembled RDOFF2 object for `src-lib/SYS.ASM`.
`src-lib/Makefile` has **no rule** to rebuild it from `SYS.ASM`.
After editing `SYS.ASM`, you must manually reassemble with a RDOFF2-capable NASM
(older version) or patch the binary directly, then run `cd src-lib && make install`.

The `-SYSTEM` flag disables the implicit SYSTEM import that every other module receives:
- No `SYSTEM.def` is auto-loaded at startup
- `IMPORT SYSTEM` in the source is allowed (not rejected as it would be normally)
- `SYSTEM__init` is never emitted as an `__init` dependency (same rule as normal modules)
- All other compiler behaviour is unchanged (SYSTEM universe built-ins remain pre-declared by `sym_init`)

Use this flag only when compiling the SYSTEM module itself. All other modules use the normal
mode and receive the implicit SYSTEM import automatically.

### Include extra .rdf files in the output .om

Use the `$L` system comment directive in the source file to pack an additional `.rdf` into
the `.om` archive alongside the compiled module's own `.rdf` and `.def`:

```oberon
(*$Lhelper.rdf*)       (* block-comment form *)
//$Lhelper.rdf         (* line-comment form  *)
```

The directive is processed during scanning and may appear anywhere in the source.  The file
is stored in the archive under its basename (directory part is stripped).  Up to 128 extra
`.rdf` files total.

Typical use: when the SYSTEM library is split into multiple `.rdf` files (e.g. hand-assembled
helpers like `SYS.RDF`), list them with `$L` so the resulting `.om` is self-contained.

If a `$L` path cannot be opened, a warning is printed to stderr and the `.om` is still
written with all successfully read extra files.

### Compile with entry point

```bash
./oc -entry ProcName File.Mod    # → File.om (contains MAIN.RDF)
```

Also generates `MAIN.RDF` (packed into `File.om`) containing:
```asm
CALL FAR SYSTEM_INIT        ; runtime init   (import seg 3)
CALL FAR ModName__init      ; module init    (import seg 5)
CALL FAR ModName_ProcName   ; entry proc     (import seg 6)
CALL FAR SYSTEM_DONE        ; DOS exit       (import seg 4)
```
`MAIN.RDF` exports GLOBAL `start` at offset 0 (linker entry point).

---

## Environment variable

```bash
export OBERON_LIB=/path/to/lib          # Unix — single lib/ directory after install
set OBERON_LIB=C:\lib                   # DOS
```

After `make` from the root, `lib/` contains `SYSTEM.om`, `Out.om`, `Strings.om`, `Files.om`, `Crt.om`, `Dos.om`, and `SYSTEM.DEF`.
Set `OBERON_LIB=lib` (or the absolute path) so `oc` can resolve imports.
Library search order: current directory → OBERON_LIB entries (left to right).

For linker usage, input formats, output format, and smart linking — see [olink.md](olink.md).
