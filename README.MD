oc86 — Oberon-07 compiler targeting MS-DOS real mode
==============================================================

Summary
-------
oc86 is a small Oberon-07 compiler (in C89) that produces RDOFF2 object files and `.om` archives suitable for linking into DOS MZ executables with `olink`. The project targets the MS-DOS real-mode large memory model (CS<>DS<>SS) and is designed to be portable between GCC/Clang (host) and Open Watcom (DOS large model).

Key features
------------
- Oberon-07 dialect with practical extensions (typeless VAR, open-array ABI, system comment directives).
- Generates RDOFF2 `.rdf` + `.def` and packs into `.om` archives.
- Smart linker `olink` that emits DOS MZ executables and performs reachability-based linking.
- x87 FPU support for REAL/LONGREAL and large-model 16-bit portability rules.

Getting started
---------------
Requirements:
- gcc/clang (Linux/macOS) or Open Watcom (wmake) for DOS builds
- make

Build (host):

```sh
make        # builds oc and olink, installs libs to lib/
make test   # build + run tests
```

Build (DOS Open Watcom):

```sh
make oc-dos    # runs wmake -f src/Makefile.wat
```

Basic usage
-----------
Compile a module:

```sh
oc MyMod.Mod   # -> MyMod.om
```

Create an executable (compile with entry and link):

```sh
set OBERON_LIB=C:\oc\lib
oc -entry Run MyMod.Mod
olink MyMod.om SYSTEM.om myapp.exe
```

Packaging hand-written .rdf
---------------------------
Include extra `.rdf` in a `.om` by adding a system comment in a tiny wrapper module:

```oberon
MODULE MyLibPack;

(*$L MYLIB.RDF*)

END MyLibPack.
```

Then run `oc MyLibPack.Mod` to produce `MyLibPack.om`.

Documentation
-------------
- Manual: MANUAL.MD (programmer manual, dialect + ABI)
- Grammar: docs/oberon07.ebnf (authoritative)
- Implementation rules and ABI: docs/implementation-rules.md
- Linker design and output: docs/olink.md
- RDOFF2 spec: docs/rdoff2-spec.md

License
-------
See LICENSE.TXT in the repository root.
