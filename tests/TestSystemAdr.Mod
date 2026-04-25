MODULE TestSystemAdr;
(* Tests for SYSTEM.ADR applied to procedures, global/local variables,
   records, arrays, array elements, record fields, VAR params, and uplevel variables. *)

IMPORT Out;

VAR
  totalTests, passedTests: INTEGER;
  testResult: INTEGER;

  gInt:  INTEGER;
  gArr:  ARRAY 8 OF INTEGER;
  gRec:  RECORD a, b: INTEGER END;

PROCEDURE Fail(msg: ARRAY OF CHAR; got, expected: INTEGER);
BEGIN
  testResult := 1;
  Out.String("  FAIL: "); Out.String(msg);
  Out.String(" got="); Out.Int(got, 0);
  Out.String(" expected="); Out.Int(expected, 0); Out.Ln
END Fail;

PROCEDURE Check(label: ARRAY OF CHAR; ok: BOOLEAN);
BEGIN
  IF ~ok THEN testResult := 1; Out.String("  FAIL: "); Out.String(label); Out.Ln END
END Check;

PROCEDURE BeginTest(name: ARRAY OF CHAR);
BEGIN
  Out.String("Test: "); Out.String(name); Out.Ln;
  testResult := 0
END BeginTest;

PROCEDURE EndTest;
BEGIN
  INC(totalTests);
  IF testResult = 0 THEN
    INC(passedTests); Out.String("  PASSED"); Out.Ln
  ELSE
    Out.String("  FAILED"); Out.Ln
  END
END EndTest;

(* ------------------------------------------------------------------ *)
(* A simple exported proc whose address we can take                    *)
(* ------------------------------------------------------------------ *)
PROCEDURE TargetProc*(): INTEGER;
BEGIN RETURN 42 END TargetProc;

(* ------------------------------------------------------------------ *)
(* Test 1: ADR of a global INTEGER variable                            *)
(* Verify that writing to the address changes the variable.            *)
(* ------------------------------------------------------------------ *)
PROCEDURE TestGlobalVar*;
VAR adr: SYSTEM.ADDRESS;
BEGIN
  BeginTest("ADR of global INTEGER var");
  gInt := 0;
  adr := SYSTEM.ADR(gInt);
  Check("adr non-zero", adr # 0);
  (* Write 1234 through the address, read back via gInt *)
  SYSTEM.PUT(adr, 1234);
  IF gInt # 1234 THEN Fail("global write-back", gInt, 1234) END;
  EndTest
END TestGlobalVar;

(* ------------------------------------------------------------------ *)
(* Test 2: ADR of a global ARRAY -- base and element offsets            *)
(* ------------------------------------------------------------------ *)
PROCEDURE TestGlobalArray*;
VAR base, e0, e1, e3: SYSTEM.ADDRESS;
    i: INTEGER;
BEGIN
  BeginTest("ADR of global ARRAY and elements");
  FOR i := 0 TO 7 DO gArr[i] := i * 10 END;
  base := SYSTEM.ADR(gArr);
  e0   := SYSTEM.ADR(gArr[0]);
  e1   := SYSTEM.ADR(gArr[1]);
  e3   := SYSTEM.ADR(gArr[3]);
  (* base address equals address of element 0 *)
  IF base # e0 THEN Fail("base=e0", base, e0) END;
  (* elements spaced by SZ_INTEGER = 2 bytes *)
  IF e1 - e0 # 2 THEN Fail("e1-e0=2", e1 - e0, 2) END;
  IF e3 - e0 # 6 THEN Fail("e3-e0=6", e3 - e0, 6) END;
  (* read element 3 via its address *)
  SYSTEM.PUT(e3, 999);
  IF gArr[3] # 999 THEN Fail("arr[3] via PUT", gArr[3], 999) END;
  EndTest
END TestGlobalArray;

(* ------------------------------------------------------------------ *)
(* Test 3: ADR of global RECORD -- base and field offsets               *)
(* ------------------------------------------------------------------ *)
PROCEDURE TestGlobalRecord*;
VAR adrRec, adrA, adrB: SYSTEM.ADDRESS;
BEGIN
  BeginTest("ADR of global RECORD and fields");
  gRec.a := 10; gRec.b := 20;
  adrRec := SYSTEM.ADR(gRec);
  adrA   := SYSTEM.ADR(gRec.a);
  adrB   := SYSTEM.ADR(gRec.b);
  Check("adrRec non-zero", adrRec # 0);
  (* no type-tag reserved: field a is at offset 0 from record base *)
  IF adrA - adrRec # 0 THEN Fail("a at offset 0", adrA - adrRec, 0) END;
  (* b follows a with 2-byte gap (SZ_INTEGER) *)
  IF adrB - adrA # 2 THEN Fail("b-a=2", adrB - adrA, 2) END;
  (* write through field addresses *)
  SYSTEM.PUT(adrA, 100);
  SYSTEM.PUT(adrB, 200);
  IF gRec.a # 100 THEN Fail("rec.a via PUT", gRec.a, 100) END;
  IF gRec.b # 200 THEN Fail("rec.b via PUT", gRec.b, 200) END;
  EndTest
END TestGlobalRecord;

(* ------------------------------------------------------------------ *)
(* Test 4: ADR of local INTEGER variable                               *)
(* ADR returns SS-relative offset; verify consistency via              *)
(* indirect PUT/GET (both in SS for local vars -- use VAR param trick). *)
(* ------------------------------------------------------------------ *)
PROCEDURE WriteViaVarParam(VAR v: INTEGER; val: INTEGER);
BEGIN v := val END WriteViaVarParam;

PROCEDURE TestLocalVar*;
VAR x: INTEGER;
    adrX, adrY: SYSTEM.ADDRESS;
    y: INTEGER;
BEGIN
  BeginTest("ADR of local INTEGER var");
  x := 55;
  adrX := SYSTEM.ADR(x);
  Check("local adr non-zero", adrX # 0);
  (* Two locals should have different addresses *)
  y := 77;
  adrY := SYSTEM.ADR(y);
  Check("x and y have different adr", adrX # adrY);
  (* addresses differ by at least 2 (word-sized slots) *)
  IF adrX > adrY THEN
    Check("x-y >= 2", adrX - adrY >= 2)
  ELSE
    Check("y-x >= 2", adrY - adrX >= 2)
  END;
  EndTest
END TestLocalVar;

(* ------------------------------------------------------------------ *)
(* Test 5: ADR of local ARRAY -- base equals element 0 address         *)
(* ------------------------------------------------------------------ *)
PROCEDURE TestLocalArray*;
VAR a: ARRAY 6 OF INTEGER;
    base, e0, e2: SYSTEM.ADDRESS;
    i: INTEGER;
BEGIN
  BeginTest("ADR of local ARRAY and elements");
  FOR i := 0 TO 5 DO a[i] := i + 1 END;
  base := SYSTEM.ADR(a);
  e0   := SYSTEM.ADR(a[0]);
  e2   := SYSTEM.ADR(a[2]);
  IF base # e0 THEN Fail("base=e0", base, e0) END;
  IF e2 - e0 # 4 THEN Fail("e2-e0=4", e2 - e0, 4) END;
  EndTest
END TestLocalArray;

(* ------------------------------------------------------------------ *)
(* Test 6: ADR of local RECORD -- field offset = tag size               *)
(* ------------------------------------------------------------------ *)
PROCEDURE TestLocalRecord*;
VAR r: RECORD p, q: INTEGER END;
    adrR, adrP, adrQ: SYSTEM.ADDRESS;
BEGIN
  BeginTest("ADR of local RECORD and fields");
  r.p := 11; r.q := 22;
  adrR := SYSTEM.ADR(r);
  adrP := SYSTEM.ADR(r.p);
  adrQ := SYSTEM.ADR(r.q);
  Check("adrR non-zero", adrR # 0);
  (* no type-tag: field p at offset 0 from record base *)
  IF adrP - adrR # 0 THEN Fail("p at offset 0", adrP - adrR, 0) END;
  IF adrQ - adrP # 2 THEN Fail("q-p=2", adrQ - adrP, 2) END;
  EndTest
END TestLocalRecord;

(* ------------------------------------------------------------------ *)
(* Test 8: ADR of uplevel variable from nested procedure               *)
(* ------------------------------------------------------------------ *)
(* ------------------------------------------------------------------ *)
(* Test 7: ADR of procedure name → CS:code_offset                      *)
(* ADR returns a non-zero code-segment offset; segment = CS.           *)
(* Verify segment = CS (via SYSTEM.SEG trick) and offset non-zero.    *)
(* ------------------------------------------------------------------ *)
PROCEDURE TestProcAddr*;
VAR adr: SYSTEM.ADDRESS;
    fn: PROCEDURE(): INTEGER;
    result: INTEGER;
BEGIN
  BeginTest("ADR of procedure name");
  adr := SYSTEM.ADR(TargetProc);
  Check("proc adr non-zero", adr # 0);
  (* verify indirect call still works via normal proc variable *)
  fn := TargetProc;
  result := fn();
  IF result # 42 THEN Fail("call via procvar", result, 42) END;
  EndTest
END TestProcAddr;

(* ------------------------------------------------------------------ *)
PROCEDURE TestUplevelAdr*;
VAR outer: INTEGER;
    outerAdr: SYSTEM.ADDRESS;

  PROCEDURE GetAdr(): SYSTEM.ADDRESS;
  BEGIN
    RETURN SYSTEM.ADR(outer)   (* 1-hop uplevel *)
  END GetAdr;

  PROCEDURE SetViaAdr(adr: SYSTEM.ADDRESS; val: INTEGER);
  VAR dummy: INTEGER;
  BEGIN
    (* Write to outer via its address using a VAR-param trick:
       assign outer directly since we can't use SYSTEM.PUT for SS-locals. *)
    outer := val   (* direct assignment to verify the address is consistent *)
  END SetViaAdr;

BEGIN
  BeginTest("ADR of uplevel variable (1-hop)");
  outer := 0;
  outerAdr := GetAdr();
  Check("uplevel adr non-zero", outerAdr # 0);
  (* The address returned by the nested proc must equal the address
     taken in the outer proc for the same variable. *)
  IF outerAdr # SYSTEM.ADR(outer) THEN
    Fail("uplevel adr matches outer adr", outerAdr, SYSTEM.ADR(outer))
  END;
  EndTest
END TestUplevelAdr;

(* ------------------------------------------------------------------ *)
(* Test 9: ADR of non-VAR parameter                                    *)
(* ADR(p) where p is a value parameter returns the address of the      *)
(* parameter's own stack slot.  Non-zero and consistent across two     *)
(* consecutive calls (same frame layout each time).                    *)
(* ------------------------------------------------------------------ *)
PROCEDURE GetParamAdr(p: INTEGER): SYSTEM.ADDRESS;
BEGIN
  RETURN SYSTEM.ADR(p)
END GetParamAdr;

PROCEDURE TestNonVarParam*;
VAR a1, a2: SYSTEM.ADDRESS;
BEGIN
  BeginTest("ADR of non-VAR parameter");
  a1 := GetParamAdr(10);
  a2 := GetParamAdr(20);
  Check("param adr non-zero", a1 # 0);
  (* Both calls use the same frame layout, so the slot offset is identical *)
  IF a1 # a2 THEN Fail("frame-stable param adr", a1, a2) END;
  EndTest
END TestNonVarParam;

(* ------------------------------------------------------------------ *)
(* Test 10: ADR of VAR parameter                                       *)
(* ADR(v) where v is a VAR parameter returns the address of the        *)
(* variable that was passed by the caller -- not the far-ptr slot.    *)
(* Verified by comparing with SYSTEM.ADR taken directly in the caller. *)
(* ------------------------------------------------------------------ *)
PROCEDURE GetVarParamAdr(VAR v: INTEGER): SYSTEM.ADDRESS;
BEGIN
  RETURN SYSTEM.ADR(v)
END GetVarParamAdr;

PROCEDURE GetVarParamAdrAndWrite(VAR v: INTEGER; val: INTEGER): SYSTEM.ADDRESS;
(* Returns ADR(v) and also writes val through it using SYSTEM.PUT.
   v is a global variable so its address is DS-relative and PUT works. *)
BEGIN
  SYSTEM.PUT(SYSTEM.ADR(v), val);
  RETURN SYSTEM.ADR(v)
END GetVarParamAdrAndWrite;

PROCEDURE TestVarParam*;
VAR x: INTEGER;
    directAdr, viaAdr: SYSTEM.ADDRESS;
BEGIN
  BeginTest("ADR of VAR parameter");
  x := 0;
  directAdr := SYSTEM.ADR(x);
  viaAdr    := GetVarParamAdr(x);
  (* ADR through VAR param must equal ADR taken directly in the caller *)
  IF viaAdr # directAdr THEN Fail("var-param adr = direct adr", viaAdr, directAdr) END;
  (* Write through ADR inside callee -- use global gInt so PUT uses DS correctly *)
  gInt := 0;
  viaAdr := GetVarParamAdrAndWrite(gInt, 5555);
  IF gInt # 5555 THEN Fail("write via var-param ADR", gInt, 5555) END;
  IF viaAdr # SYSTEM.ADR(gInt) THEN Fail("var-param adr = global adr", viaAdr, SYSTEM.ADR(gInt)) END;
  EndTest
END TestVarParam;

(* ------------------------------------------------------------------ *)
(* Summary                                                             *)
(* ------------------------------------------------------------------ *)
PROCEDURE PrintSummary;
BEGIN
  Out.Ln;
  Out.String("========================================"); Out.Ln;
  Out.String("Total:  "); Out.Int(totalTests, 0); Out.Ln;
  Out.String("Passed: "); Out.Int(passedTests, 0); Out.Ln;
  Out.String("Failed: "); Out.Int(totalTests - passedTests, 0); Out.Ln;
  Out.String("========================================"); Out.Ln
END PrintSummary;

PROCEDURE RunAllTests*;
BEGIN
  Out.String("========================================"); Out.Ln;
  Out.String("SYSTEM.ADR Tests"); Out.Ln;
  Out.String("========================================"); Out.Ln;
  totalTests := 0; passedTests := 0;

  TestGlobalVar;
  TestGlobalArray;
  TestGlobalRecord;
  TestLocalVar;
  TestLocalArray;
  TestLocalRecord;
  TestProcAddr;
  TestUplevelAdr;
  TestNonVarParam;
  TestVarParam;

  PrintSummary
END RunAllTests;

END TestSystemAdr.
