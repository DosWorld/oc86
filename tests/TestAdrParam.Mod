MODULE TestAdrParam;
IMPORT Out;

VAR
  totalTests, passedTests: INTEGER;
  testResult: INTEGER;

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

VAR gX: INTEGER;

(* Non-VAR parameter: ADR returns address of the parameter slot *)
PROCEDURE AdrOfNonVar(p: INTEGER): SYSTEM.ADDRESS;
BEGIN
  RETURN SYSTEM.ADR(p)
END AdrOfNonVar;

(* VAR parameter: ADR returns address of the variable passed by caller *)
PROCEDURE AdrOfVarParam(VAR v: INTEGER): SYSTEM.ADDRESS;
BEGIN
  RETURN SYSTEM.ADR(v)
END AdrOfVarParam;

(* Write to a variable through its address obtained via VAR param ADR *)
PROCEDURE WriteViaAdr(VAR v: INTEGER; val: INTEGER);
VAR a: SYSTEM.ADDRESS;
BEGIN
  a := SYSTEM.ADR(v);
  SYSTEM.PUT(a, val)
END WriteViaAdr;

PROCEDURE TestNonVarParam;
VAR a1, a2: SYSTEM.ADDRESS;
    p: INTEGER;
BEGIN
  BeginTest("ADR of non-VAR parameter");
  p := 0;
  a1 := AdrOfNonVar(p);
  a2 := AdrOfNonVar(p);
  (* Each call has its own stack slot; ADR is non-zero *)
  Check("adr non-zero", a1 # 0);
  (* Two separate calls get different slots (stack frame reused but consistent) *)
  (* Mainly: the address is a valid stack address and non-zero *)
  EndTest
END TestNonVarParam;

PROCEDURE TestVarParam;
VAR x: INTEGER;
    adrX, adrVia: SYSTEM.ADDRESS;
BEGIN
  BeginTest("ADR of VAR parameter");
  x := 0;
  adrX    := SYSTEM.ADR(x);
  adrVia  := AdrOfVarParam(x);
  (* ADR through VAR param must equal ADR taken directly *)
  IF adrVia # adrX THEN Fail("adr via VAR = direct adr", adrVia, adrX) END;
  EndTest
END TestVarParam;

PROCEDURE TestWriteViaVarParamAdr;
(* Use global gX: ADR is DS-relative so SYSTEM.PUT works correctly.
   SYSTEM.PUT on SS-relative (local) addresses is not supported (large model DS#SS). *)
BEGIN
  BeginTest("Write via ADR of VAR parameter (global)");
  gX := 0;
  WriteViaAdr(gX, 7777);
  IF gX # 7777 THEN Fail("write-back via VAR ADR", gX, 7777) END;
  EndTest
END TestWriteViaVarParamAdr;

PROCEDURE TestGlobalVarParam;
VAR adrVia: SYSTEM.ADDRESS;
BEGIN
  BeginTest("ADR of global variable via VAR parameter");
  gX := 0;
  adrVia := AdrOfVarParam(gX);
  Check("adr non-zero", adrVia # 0);
  SYSTEM.PUT(adrVia, 4321);
  IF gX # 4321 THEN Fail("global write via VAR ADR", gX, 4321) END;
  EndTest
END TestGlobalVarParam;

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
  Out.String("SYSTEM.ADR Parameter Tests"); Out.Ln;
  Out.String("========================================"); Out.Ln;
  totalTests := 0; passedTests := 0;

  TestNonVarParam;
  TestVarParam;
  TestWriteViaVarParamAdr;
  TestGlobalVarParam;

  PrintSummary
END RunAllTests;

END TestAdrParam.
