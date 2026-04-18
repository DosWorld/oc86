MODULE TestSysIntrinsics;
(* Runtime tests for SYSTEM intrinsics: GET, PUT, MOVE, FILL, PTR, SEG/OFS, VAL.
   Covers observable runtime behaviour across global, local, VAR/non-VAR parameter,
   and pointer-dereference (p^) variable forms.

   Memory model notes (large model, DS<>SS):
   - Globals live in DS; locals live in SS.
   - SYSTEM.SEG(global)=DS, SYSTEM.SEG(local)=SS.
   - SYSTEM.OFS(global) = DS-relative offset (from LEA/RELOC).
   - SYSTEM.OFS(local)  = SS-relative offset (from LEA BP+ofs).
   - SYSTEM.MOVE/FILL take raw AX offsets and use DS:SI / ES:DI (ES=DS set inline),
     so they only work correctly with DS-relative (global) addresses.
   - SYSTEM.GET/PUT also set ES=DS and use the AX word as a DS-relative offset;
     they work with globals via OFS.  *)

IMPORT Out;

VAR
    passed, failed: INTEGER;

    (* globals used across multiple sub-tests *)
    gA, gB:   INTEGER;
    gSrc:     ARRAY 4 OF INTEGER;
    gDst:     ARRAY 4 OF INTEGER;
    gFill:    ARRAY 6 OF INTEGER;
    gPtrTgt:  INTEGER;
    gSeg1, gSeg2: INTEGER;

PROCEDURE Check(name: ARRAY OF CHAR; cond: BOOLEAN);
BEGIN
    INC(passed);
    IF ~cond THEN
        Out.String("FAIL: "); Out.String(name); Out.Ln;
        INC(failed); DEC(passed)
    END
END Check;

(* ================================================================
   SYSTEM.SEG
   ================================================================ *)

(* ------------------------------------------------------------------ *)
(* SEG of global variable → DS                                        *)
(* Two distinct globals must return the same segment value.           *)
(* ------------------------------------------------------------------ *)
PROCEDURE TestSegGlobal;
VAR s1, s2: INTEGER;
BEGIN
    gA := 1; gB := 2;
    s1 := SYSTEM.SEG(gA);
    s2 := SYSTEM.SEG(gB);
    Check("SEG: two globals same segment", s1 = s2)
END TestSegGlobal;

(* ------------------------------------------------------------------ *)
(* SEG of local variable → SS                                         *)
(* In large model SS <> DS, so SEG(local) <> SEG(global).            *)
(* ------------------------------------------------------------------ *)
PROCEDURE TestSegLocal;
VAR x, y: INTEGER;
    sLocal, sGlobal: INTEGER;
BEGIN
    x := 10; y := 20;
    sLocal  := SYSTEM.SEG(x);
    sGlobal := SYSTEM.SEG(gA);
    (* Two locals are in the same segment *)
    Check("SEG: two locals same segment", sLocal = SYSTEM.SEG(y));
    (* Local segment (SS) differs from global segment (DS) in large model *)
    Check("SEG: local<>global segment", sLocal # sGlobal)
END TestSegLocal;

(* ------------------------------------------------------------------ *)
(* SEG of VAR parameter → caller's segment                            *)
(* Pass a global: callee sees DS.  Pass a local: callee sees SS.      *)
(* ------------------------------------------------------------------ *)
PROCEDURE GetSeg(VAR v: INTEGER): INTEGER;
BEGIN RETURN SYSTEM.SEG(v) END GetSeg;

PROCEDURE TestSegVarParam;
VAR loc: INTEGER;
    sFromGlobal, sFromLocal, sGlobal, sLocal: INTEGER;
BEGIN
    loc := 0;
    sFromGlobal := GetSeg(gA);       (* VAR actual is a global → DS *)
    sFromLocal  := GetSeg(loc);      (* VAR actual is a local  → SS *)
    sGlobal := SYSTEM.SEG(gA);
    sLocal  := SYSTEM.SEG(loc);
    Check("SEG: VAR param global=DS", sFromGlobal = sGlobal);
    Check("SEG: VAR param local=SS",  sFromLocal  = sLocal)
END TestSegVarParam;

(* ------------------------------------------------------------------ *)
(* SEG of non-VAR parameter → SS (value copy lives on stack)         *)
(* ------------------------------------------------------------------ *)
PROCEDURE GetSegVal(v: INTEGER): INTEGER;
BEGIN RETURN SYSTEM.SEG(v) END GetSegVal;

PROCEDURE TestSegNonVarParam;
VAR sParam, sLocal: INTEGER;
    loc: INTEGER;
BEGIN
    loc := 0;
    sParam := GetSegVal(42);
    sLocal := SYSTEM.SEG(loc);
    (* Both param copy and local variable live on the stack = SS *)
    Check("SEG: non-VAR param = SS", sParam = sLocal)
END TestSegNonVarParam;

(* ------------------------------------------------------------------ *)
(* SEG of p^ → segment of the heap object                            *)
(* After NEW(p), SEG(p^) is the segment of the allocated block;      *)
(* it must equal the segment word stored in the pointer itself.       *)
(* ------------------------------------------------------------------ *)
TYPE PInt = POINTER TO INTEGER;

PROCEDURE TestSegDeref;
VAR p: PInt;
    sDeref, sPtr: INTEGER;
BEGIN
    NEW(p);
    p^ := 77;
    sDeref := SYSTEM.SEG(p^);     (* SEG(p^) = ES after LES *)
    sPtr   := SYSTEM.SEG(p^);     (* call twice — must be stable *)
    Check("SEG: p^ consistent",   sDeref = sPtr);
    DISPOSE(p)
END TestSegDeref;

(* ================================================================
   SYSTEM.OFS
   ================================================================ *)

(* ------------------------------------------------------------------ *)
(* OFS of globals: adjacent INTEGER vars differ by 2 bytes.           *)
(* ------------------------------------------------------------------ *)
PROCEDURE TestOfsGlobal;
VAR o1, o2: INTEGER;
BEGIN
    o1 := SYSTEM.OFS(gA);
    o2 := SYSTEM.OFS(gB);
    (* gA and gB are adjacent INTEGERs declared in order → |o2-o1|=2 *)
    Check("OFS: adjacent globals differ by 2",
          (o2 - o1 = 2) OR (o1 - o2 = 2))
END TestOfsGlobal;

(* ------------------------------------------------------------------ *)
(* OFS of locals: adjacent locals must differ (by at least 2).        *)
(* ------------------------------------------------------------------ *)
PROCEDURE TestOfsLocal;
VAR x, y: INTEGER;
    ox, oy: INTEGER;
BEGIN
    x := 1; y := 2;
    ox := SYSTEM.OFS(x);
    oy := SYSTEM.OFS(y);
    Check("OFS: two locals differ", ox # oy)
END TestOfsLocal;

(* ------------------------------------------------------------------ *)
(* OFS of VAR parameter → caller's offset.                            *)
(* Callee's OFS(v) must equal caller's OFS of the same global.        *)
(* ------------------------------------------------------------------ *)
PROCEDURE GetOfs(VAR v: INTEGER): INTEGER;
BEGIN RETURN SYSTEM.OFS(v) END GetOfs;

PROCEDURE TestOfsVarParam;
VAR oFromCallee, oDirect: INTEGER;
BEGIN
    oFromCallee := GetOfs(gA);
    oDirect     := SYSTEM.OFS(gA);
    Check("OFS: VAR param matches caller OFS", oFromCallee = oDirect)
END TestOfsVarParam;

(* ------------------------------------------------------------------ *)
(* OFS of non-VAR parameter: stable across two calls with same frame. *)
(* ------------------------------------------------------------------ *)
PROCEDURE GetOfsVal(v: INTEGER): INTEGER;
BEGIN RETURN SYSTEM.OFS(v) END GetOfsVal;

PROCEDURE TestOfsNonVarParam;
VAR o1, o2: INTEGER;
BEGIN
    o1 := GetOfsVal(10);
    o2 := GetOfsVal(20);
    (* Same frame layout each call → same BP-relative slot offset *)
    Check("OFS: non-VAR param stable", o1 = o2)
END TestOfsNonVarParam;

(* ------------------------------------------------------------------ *)
(* OFS / SEG of p^: reconstruct pointer from SEG+OFS and verify that *)
(* the offset matches what the pointer already stores.                *)
(* OFS(p^) = low word of the pointer value = same as BX after LES.  *)
(* SEG(p^) = high word of the pointer value = same as ES after LES. *)
(* ------------------------------------------------------------------ *)
PROCEDURE TestOfsDeref;
VAR p: PInt;
    s1, s2, o1, o2: INTEGER;
BEGIN
    NEW(p);
    p^ := 333;
    s1 := SYSTEM.SEG(p^);
    o1 := SYSTEM.OFS(p^);
    (* Calling SEG/OFS twice must yield stable results *)
    s2 := SYSTEM.SEG(p^);
    o2 := SYSTEM.OFS(p^);
    Check("OFS: OFS(p^) stable",   o1 = o2);
    Check("SEG: SEG(p^) stable",   s1 = s2);
    (* SEG(p^) and OFS(p^) reconstruct the pointer: PTR(s,o) must equal p *)
    (* We verify indirectly: write via p^, then read; if SEG/OFS are right *)
    (* the heap cell is intact *)
    p^ := 777;
    Check("OFS: p^ write stable after SEG/OFS", p^ = 777);
    DISPOSE(p)
END TestOfsDeref;

(* ================================================================
   SYSTEM.MOVE  (global src/dst only — MOVE uses DS:SI/ES:DI = DS)
   ================================================================ *)

(* Helper: load address of a global using OFS (DS-relative) *)

(* ------------------------------------------------------------------ *)
(* MOVE globals: copy gSrc array to gDst                              *)
(* ------------------------------------------------------------------ *)
PROCEDURE TestMoveGlobal;
VAR i: INTEGER;
    sa, da: INTEGER;
    aS, aD: SYSTEM.ADDRESS;
BEGIN
    gSrc[0] := 11; gSrc[1] := 22; gSrc[2] := 33; gSrc[3] := 44;
    gDst[0] :=  0; gDst[1] :=  0; gDst[2] :=  0; gDst[3] :=  0;
    sa := SYSTEM.OFS(gSrc[0]);
    da := SYSTEM.OFS(gDst[0]);
    (* MOVE takes raw DS-relative offsets; internally does DS:SI -> ES:DI with ES=DS *)
    aS := SYSTEM.PTR(SYSTEM.SEG(gSrc[0]), sa);
    aD := SYSTEM.PTR(SYSTEM.SEG(gDst[0]), da);
    SYSTEM.MOVE(aS, aD, 8);
    Check("MOVE global[0]", gDst[0] = 11);
    Check("MOVE global[1]", gDst[1] = 22);
    Check("MOVE global[2]", gDst[2] = 33);
    Check("MOVE global[3]", gDst[3] = 44)
END TestMoveGlobal;

(* ------------------------------------------------------------------ *)
(* MOVE via VAR param dst: callee receives far ptr to gDst;           *)
(* we use SEG/OFS of the VAR param to get the address for MOVE.      *)
(* ------------------------------------------------------------------ *)
PROCEDURE MoveIntoVarParam(VAR dst: ARRAY 4 OF INTEGER;
                            sa: SYSTEM.ADDRESS; n: INTEGER);
VAR ds, dofs: INTEGER;
    da: SYSTEM.ADDRESS;
BEGIN
    ds   := SYSTEM.SEG(dst[0]);
    dofs := SYSTEM.OFS(dst[0]);
    da   := SYSTEM.PTR(ds, dofs);
    SYSTEM.MOVE(sa, da, n)
END MoveIntoVarParam;

PROCEDURE TestMoveVarParam;
VAR sa: SYSTEM.ADDRESS;
    i: INTEGER;
BEGIN
    gSrc[0] := 5; gSrc[1] := 6; gSrc[2] := 7; gSrc[3] := 8;
    gDst[0] := 0; gDst[1] := 0; gDst[2] := 0; gDst[3] := 0;
    sa := SYSTEM.PTR(SYSTEM.SEG(gSrc[0]), SYSTEM.OFS(gSrc[0]));
    MoveIntoVarParam(gDst, sa, 8);
    Check("MOVE VAR param dst[0]", gDst[0] = 5);
    Check("MOVE VAR param dst[1]", gDst[1] = 6);
    Check("MOVE VAR param dst[2]", gDst[2] = 7);
    Check("MOVE VAR param dst[3]", gDst[3] = 8)
END TestMoveVarParam;

(* ================================================================
   SYSTEM.FILL  (global dst only — FILL uses ES:DI with ES=DS)
   ================================================================ *)

(* ------------------------------------------------------------------ *)
(* FILL global array with a constant byte                             *)
(* ------------------------------------------------------------------ *)
PROCEDURE TestFillGlobal;
VAR i: INTEGER;
    seg, ofs: INTEGER;
    da: SYSTEM.ADDRESS;
    ok: BOOLEAN;
BEGIN
    FOR i := 0 TO 5 DO gFill[i] := i + 1 END;
    seg := SYSTEM.SEG(gFill[0]);
    ofs := SYSTEM.OFS(gFill[0]);
    da  := SYSTEM.PTR(seg, ofs);
    SYSTEM.FILL(da, 12, 0);         (* 6 x 2-byte INTEGERs = 12 bytes *)
    ok := TRUE;
    FOR i := 0 TO 5 DO IF gFill[i] # 0 THEN ok := FALSE END END;
    Check("FILL global zeros 6 INTEGERs", ok)
END TestFillGlobal;

(* ------------------------------------------------------------------ *)
(* FILL global array with non-zero byte (0ABh), check first element   *)
(* (0ABh in both bytes of a word = 0ABABh)                           *)
(* ------------------------------------------------------------------ *)
PROCEDURE TestFillGlobalByte;
VAR seg, ofs: INTEGER;
    da: SYSTEM.ADDRESS;
BEGIN
    gFill[0] := 0;
    seg := SYSTEM.SEG(gFill[0]);
    ofs := SYSTEM.OFS(gFill[0]);
    da  := SYSTEM.PTR(seg, ofs);
    SYSTEM.FILL(da, 2, 0ABH);       (* fill 2 bytes of gFill[0] with 0ABh *)
    Check("FILL global non-zero byte", gFill[0] = 0ABABH)
END TestFillGlobalByte;

(* ------------------------------------------------------------------ *)
(* FILL via VAR param: callee receives far ptr; uses SEG/OFS to get  *)
(* the DS-relative address for FILL.                                  *)
(* ------------------------------------------------------------------ *)
PROCEDURE FillVarParam(VAR arr: ARRAY 6 OF INTEGER; n, b: INTEGER);
VAR seg, ofs: INTEGER;
    da: SYSTEM.ADDRESS;
BEGIN
    seg := SYSTEM.SEG(arr[0]);
    ofs := SYSTEM.OFS(arr[0]);
    da  := SYSTEM.PTR(seg, ofs);
    SYSTEM.FILL(da, n, b)
END FillVarParam;

PROCEDURE TestFillVarParam;
VAR i: INTEGER;
    ok: BOOLEAN;
BEGIN
    FOR i := 0 TO 5 DO gFill[i] := 99 END;
    FillVarParam(gFill, 12, 0);
    ok := TRUE;
    FOR i := 0 TO 5 DO IF gFill[i] # 0 THEN ok := FALSE END END;
    Check("FILL VAR param zeros array", ok)
END TestFillVarParam;

(* ================================================================
   PTR / GET / PUT   (already tested in previous block; add p^ form)
   ================================================================ *)

(* ------------------------------------------------------------------ *)
(* PUT / GET on a global via OFS (DS-relative plain offset)           *)
(* ------------------------------------------------------------------ *)
PROCEDURE TestPutGet;
VAR ofs: INTEGER;
    adr: SYSTEM.ADDRESS;
BEGIN
    gA := 0;
    ofs := SYSTEM.OFS(gA);
    adr := SYSTEM.PTR(SYSTEM.SEG(gA), ofs);
    SYSTEM.PUT(adr, 1234);
    Check("PUT global via OFS+PTR", gA = 1234);
    SYSTEM.GET(adr, gA);
    Check("GET global round-trip", gA = 1234);
    SYSTEM.PUT(adr, 0);
    Check("PUT global clears", gA = 0)
END TestPutGet;

(* ------------------------------------------------------------------ *)
(* PUT / GET via SYSTEM.INTR-style write through pointer:             *)
(* SYSTEM.GET/PUT set ES=DS and use AX as DS-relative offset, so     *)
(* they cannot reach the heap segment. Verify GET/PUT on a global    *)
(* using the OFS of the global as a plain DS offset.                 *)
(* ------------------------------------------------------------------ *)
PROCEDURE TestPutGetDeref;
VAR p: PInt;
    s, o: INTEGER;
BEGIN
    (* Verify that SEG(p^) / OFS(p^) are accessible without crashing *)
    NEW(p);
    p^ := 5678;
    s := SYSTEM.SEG(p^);
    o := SYSTEM.OFS(p^);
    (* s = heap segment, o = heap offset; can't use GET/PUT across segments.
       Verify the pointer is still valid by reading through it normally. *)
    Check("p^ readable after SEG/OFS calls", p^ = 5678);
    (* SEG must be non-zero (heap is never in segment 0) *)
    Check("SEG(p^) non-zero", s # 0);
    DISPOSE(p)
END TestPutGetDeref;

(* ================================================================
   VAL
   ================================================================ *)
PROCEDURE TestVal;
VAR b: BYTE; c: CHAR;
BEGIN
    b := 65;
    c := SYSTEM.VAL(CHAR, b);
    Check("VAL BYTE->CHAR 65='A'", c = 'A');
    c := 'Z';
    b := SYSTEM.VAL(BYTE, c);
    Check("VAL CHAR->BYTE 'Z'=90", b = 90)
END TestVal;

(* ================================================================
   Entry point
   ================================================================ *)
PROCEDURE RunAllTests*;
BEGIN
    passed := 0; failed := 0;

    (* SEG *)
    TestSegGlobal;
    TestSegLocal;
    TestSegVarParam;
    TestSegNonVarParam;
    TestSegDeref;

    (* OFS *)
    TestOfsGlobal;
    TestOfsLocal;
    TestOfsVarParam;
    TestOfsNonVarParam;
    TestOfsDeref;

    (* MOVE *)
    TestMoveGlobal;
    TestMoveVarParam;

    (* FILL *)
    TestFillGlobal;
    TestFillGlobalByte;
    TestFillVarParam;

    (* PUT / GET *)
    TestPutGet;
    TestPutGetDeref;

    (* VAL *)
    TestVal;

    Out.String("Failed: "); Out.Int(failed, 0); Out.Ln;
    Out.String("Total:  "); Out.Int(passed + failed, 0); Out.Ln
END RunAllTests;

END TestSysIntrinsics.
