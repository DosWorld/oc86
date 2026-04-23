# MANUAL — Oberon-07 dialect

## Purpose
This manual documents the Oberon-07 dialect implemented by the `oc86` compiler and describes the associated tools (`oc`, `olink`), the FAR/NEAR calling model, pointer conventions, and typed-variable semantics used by this toolchain. It targets compiler users and library authors who need ABI, linking, and assembly-library guidance.

## 1. Dialect overview and notable extensions
This compiler implements an Oberon-07 dialect with a few deliberate extensions and target-specific ABI details. The following describes the language structure, common declarations, scoping rules, and dialect differences you must know when writing source for `oc86`.

### Modules and imports
- Source unit: `MODULE Name; IMPORT ...; ... END Name.` Modules group declarations and a single optional BEGIN/END body.
- IMPORT syntax supports `A := B` aliases. SYSTEM is implicitly imported for normal modules (use `-SYSTEM` to disable when compiling SYSTEM.mod itself).

### Declarations and scope
- Sections: CONST, TYPE, VAR, nested PROCEDURE declarations. Identifiers are case-sensitive; keywords are reserved in uppercase.
- Scopes: Universe (predeclared symbols) → Module → nested PROCEDURE scopes. Nested procedures can access outer locals via the static link (SL).

### Types and variables
- Built-in types: INTEGER, BOOLEAN, CHAR, BYTE, SET (16-bit), ADDRESS (far pointer), LONGINT (32-bit), REAL (32-bit), LONGREAL (64-bit).
- ARRAY and RECORD support; forward `POINTER TO` references are allowed within a TYPE section and patched later.

### Procedures and calling convention
- PROCEDURE declarations may be annotated with `*` (export) and context-sensitive `FAR`/`NEAR` modifiers. Exported procs are FAR by default.
- Nested (inner) procedures receive a hidden static-link argument (SL) enabling uplevel variable access. SL changes BP-relative offsets of parameters and locals.
- VAR parameters are passed as 4-byte far addresses. Non-VAR open-array formals use a 6-byte slot {offset,segment,LEN}.

### Expressions and coercion
- Standard Oberon expression rules apply. Mixed INTEGER/REAL or INTEGER/LONGREAL operands are coerced automatically to the floating type for expression evaluation (FPU), but assignments require explicit casts when converting to a narrower target.

### CASE, SET, and control flow
- CASE supports integer labels and ranges `lo..hi` and is compiled with a hidden local for the selector and a linear scan of arms.
- SET is 16-bit; literals, ranges and set operators (+,-,*,/) are implemented.
- FOR supports `BY` with negative or positive integer step values.

### Extensions and target-specific details
- Line comment `//` and system comment directives `(*$...*)` / `//$...` are supported. `$L path.rdf` packs extra .rdf files into the .om.
- Typeless VAR (Turbo-Pascal style): `VAR name` without a type yields an ADDRESS in the callee (useful for low-level APIs and forwarding).
- OPEN arrays: `ARRAY OF T` only as formal parameter; caller/callee ABI described in the Open arrays section.
- EXTERNAL procedures (context-sensitive `EXTERNAL`) are declared in source but implemented in hand-assembled .rdf; oc emits IMPORT records for them so they are resolved at link time.

### Compatibility notes
- Identifiers are case-sensitive (unlike some Oberon implementations). Module names must not contain '_'.
- The dialect is tuned for the DOS real-mode large memory model (CS<>DS<>SS); the language-level ADR/ADDRESS semantics reflect that ABI.

For authoritative grammar and small syntactic details, consult docs/oberon07.ebnf; for ABI and codegen rules see docs/implementation-rules.md.

## 2. `oc` (compiler) and generated artifacts
- `oc` compiles .Mod → produces Module.rdf and Module.def and packs them into Module.om (ustar). On success intermediates removed.
- `-entry ProcName` creates MAIN.RDF (exports start) for linking an executable entry point.
- `-SYSTEM` disables the implicit SYSTEM import (used only when compiling the SYSTEM module itself).
- Use `(*$L path.rdf*)` in source to include extra .rdf files into the .om. This is the simplest and recommended way to package hand-built .rdf into an .om: write a small .Mod containing the `$L` directive and compile it with `oc` to produce an .om that contains the .rdf.

## 3. `olink` (smart linker) summary
- Input: `.om` archives only (ustar). Standalone `.rdf` are rejected.
- Smart reachability: starts from `start` (MAIN.RDF) and marks reachable modules; only reachable code emitted.
- Grouping: all members of a single .om form one code group → becomes one paragraph-aligned code segment. Combined data segment (DS) is common to all modules. Stack is in SS (distinct).

## Memory model
The compiler and linker target DOS real-mode large model where CS, DS and SS are distinct. Key invariants and layout:

DOS memory model (brief): real-mode 16-bit segmented addressing (segment:offset). A paragraph is 16 bytes; segment limits ≈ 64KB (offsets are 16-bit). The large model enforces CS ≠ DS ≠ SS; the MZ loader initialises CS and SS but DS must be set by runtime startup (SYSTEM_INIT) using linker-patched paragraph values.

- Three primary segments at runtime:
  - CS: code segment paragraph of the module's code group (one CS per .om group).
  - DS: combined data segment containing all modules' initialised data and BSS (single DS for the program).
  - SS: stack segment, separate from DS, placed after data+BSS in the load image.

- Code grouping and paragraphs:
  - All `.rdf` members from the same `.om` are placed into a single code group (one code segment). The group's byte start is paragraph-aligned (16 bytes).
  - Each module inside a group has a group-relative offset (word/byte aligned as required). The group's paragraph index (group_code_para) is recorded in the MZ header.

- Data and BSS:
  - Data from all modules is concatenated into the combined data segment (DS). BSS is appended (zero-filled at runtime).
  - Linker computes `total_data_len` and `total_bss_len`. Data+BSS must fit within a single 64KB segment (implementation enforces a limit ≈ 65520 bytes).

- Stack placement:
  - SS is set to a paragraph after code+data_bss; the loader initialises SS:SP. The compiler/linker reserve STACK_SIZE (default 8192) in the image layout.

- Addressing conventions and ADR/SEG/OFS:
  - ADR(variable) yields a full far ADDRESS (DX:AX): DX = segment paragraph, AX = offset within that segment. Locals use SS, globals use DS. Procedure addresses use the code group's CS paragraph.
  - GET/PUT/MOVE/FILL accept full far addresses and always set ES from DX and BX from AX before memory ops.

- Loader and runtime initialisation:
  - The MZ header contains CS paragraph for the entry group and SS/ SP for stack. The MZ loader maps the image and sets CS/ IP/ SS/ SP; DS is not set by the DOS loader and must be initialised by runtime startup code (SYSTEM_INIT) using paragraph values written by the linker.

- Practical constraints:
  - Do not assume DS==SS or DS==CS. Always use ADR to obtain full addresses when passing pointers across segments.
  - Keep combined data+BSS ≤ segment limit; avoid very large static arrays — prefer heap allocation and linked lists.

## 4. FAR vs NEAR procedures (practical rules)
- `FAR`/`NEAR` are context-sensitive identifiers.
- Default rules:
  - Exported procedures are FAR (cannot be NEAR).
  - Top-level non-exported default to NEAR.
  - Nested procs default to NEAR and get a hidden static-link (SL).
- ABI effects:
  - FAR procs: far return (RETF), params at BP+6; callers use CALL FAR
  - NEAR procs: near return (RET), params at BP+4 (or +6 if SL present); callers use CALL NEAR
- Procedure variables: FAR proc var size=4 ({offset,segment}); NEAR proc var size=2 (offset only). Assignability requires same kind.

## 5. Pointers and memory intrinsics
- `POINTER TO T` is a far pointer (4 bytes); `^` dereferences a pointer and loads a full far pointer into ES:BX so subsequent selectors access ES:[BX+ofs].
- `SYSTEM.ADR(v)` returns a full ADDRESS (DX:AX) with behavior depending on v:
  - Local variables → SS:offset; globals → DS:offset; procedure → code offset with code-segment reloc; pointer deref n^ returns the pointer value (segment:offset).
- `SYSTEM.GET/PUT/MOVE/FILL` accept full far ADDRESS. They always set ES from DX and BX from AX and perform memory ops in ES:[BX]. MOVE preserves DS with `PUSH DS` / `POP DS` during copy.
- `NEW(p)` / `NEW(p,n)` allocate via SYSTEM_Alloc and store a far pointer in p. `DISPOSE` frees pointer or ADDRESS.

## 6. Typed variables, numeric types and FPU
- Predeclared types: INTEGER, BOOLEAN, CHAR, BYTE, SET (16-bit), ADDRESS, LONGINT (32-bit), REAL (32-bit), LONGREAL (64-bit).
- REAL and LONGREAL are implemented with x87 FPU. If FPU is used, compiler sets __init to call SYSTEM_REQUIRE87.
- Type casts: INTEGER(x), BYTE(x), LONGINT(x), REAL(x), LONGREAL(x); FLOAT(x) is synonym for REAL(x); ENTIER(x) synonym for FLOOR(x).

## 7. Open arrays — writing and calling (examples and rules)
- Callee declaration example:

  PROCEDURE Sum(a: ARRAY OF INTEGER): INTEGER;
  VAR i, s: INTEGER;
  BEGIN
    s := 0;
    FOR i := 0 TO LEN(a)-1 DO s := s + a[i] END;
    RETURN s
  END Sum;

- ABI notes for callers and callees:
  - For a open-array actual (e.g., a global array or a local array slice), the caller pushes three words: offset, segment, LEN — but pushes LEN deepest first, then segment, then offset. At callee BP-based access the LEN is at [BP+adr+4].
  - For forwarding open-array formals to another call, copy the 6-byte slot directly: copy the offset and segment words and the LEN word. The compiler supports forwarding by copying the three words when both caller/callee are compatible.

- Example caller usage:

  VAR buf: ARRAY 100 OF INTEGER;
  BEGIN
    x := Sum(buf);   (* compiler generates: PUSH LEN(100); PUSH DS; PUSH offset_of_buf; CALL Sum *)
  END

- Important: global arrays use DS as segment; locals use SS.

## 8. Writing external libraries in assembler (practical workflow)
This section gives a minimal, practical workflow to produce an .om containing your hand-assembled .rdf; a .def file is optional for human readers but not required to produce a usable .om.

Recommended steps (minimal):

1) Assemble your code to an RDOFF2 object (.RDF):
   - Use an assembler that can emit RDOFF2 (older NASM or other tool). Example (if supported):
     nasm -f rdoff2 mylib.asm -o MyLib.RDF
   - The RDF must include exported symbols (GLOBAL/PROC records) and segment blocks. Look at `src-lib/SYS.RDF` for a working example.

2) Pack the RDF into a .om using oc's $L directive (recommended, simplest):
   - Create a tiny Oberon module that contains the system comment directive referencing your RDF, e.g.:
     MODULE MyLibPack;(*$L MyLib.RDF*) END MyLibPack.
   - Run: `./src/oc MyLibPack.Mod` → MyLibPack.om (the produced .om contains MyLib.RDF as a member). No .def is required for the .om to be usable by olink; .def files are optional annotations for humans.

3) Install or place MyLibPack.om into lib/ or pass its directory to olink with `-L`.

4) Link with olink as usual; olink will read the RDF records in the .om and resolve imports/exports.

Notes:
- Ensure exported symbol names match exactly what callers expect (case-sensitive).

### Assembler example (NASM) — exported FAR procedure
This minimal NASM example implements a FAR procedure named `MyLib_Add` that takes two 16-bit INTEGER parameters and returns their sum in AX. The exported symbol name must match the module-based name callers expect (e.g., `MyLib_Add`).

mylib.asm

section .text
global MyLib_Add
MyLib_Add:
    push bp
    mov bp, sp
    mov ax, [bp+6]    ; first parameter (WORD)
    add ax, [bp+8]    ; second parameter
    pop bp
    retf               ; far return (FAR procedure)

Assemble to RDOFF2 (if your NASM supports it):

nasm -f rdoff2 mylib.asm -o MYLIB.RDF

Pack into a .om using an Oberon wrapper that references the RDF via $L:

MODULE MyLibPack;
PROCEDURE Add*(a, b : INTEGER);EXTERNAL;
(*$L MYLIB.RDF*)
END MyLibPack.

./src/oc MyLibPack.Mod   -> MyLibPack.om (contains MYLIB.RDF)

Use from Oberon code (caller module):

MODULE UseMyLib;
IMPORT MyLib;   (* assumes callers import the module that defines Add as MyLib_Add *)
VAR r: INTEGER;
BEGIN
  r := MyLib.Add(2, 3);   (* compiler emits CALL FAR MyLib_Add *)
END UseMyLib.

Notes:
- Parameters are read from the callee frame: FAR procs params start at BP+6 (first param at +6, second at +8).
- Return value for INTEGER is returned in AX.
- The assembler must export the symbol named exactly as the linker will look it up (`MyLib_Add`).

## 9. Examples and references
- See `src-lib/SYS.RDF` (checked-in binary) and `src-lib/SYS.ASM` for a real-world example of hand-assembled RDOFF2 objects and how they are packed into libraries.

## 10. Quick troubleshooting
- Linking failures (unresolved IMPORT): ensure your .rdf is included in a .om (use `$L` in a .Mod) and exported symbol names match exactly.
- DS/SS confusion: remember locals use SS, globals use DS; use SYSTEM.ADR to obtain correct full far addresses.
- FPU errors: if code uses FPU ensure SYSTEM_REQUIRE87 is available or avoid floating point.

## License and contact
License in LICENSE.TXT.
