MODULE TestGlobals;
(* Tests for global variables, global arrays, global records:
   - Global INTEGER/BOOLEAN/CHAR independent read/write
   - Global arrays: fill, index, modify element
   - Global ARRAY OF CHAR: assign string literal, element access
   - Global records: field access, nested record, BYTE/CHAR field
   - Module-level initialisation (VAR section defaults to 0/NIL/FALSE)
   - Cross-procedure sharing of globals
   - Multiple global records of same type are independent *)

IMPORT Out;

VAR
  totalTests, passedTests: INTEGER;
  testResult: INTEGER;

PROCEDURE BeginTest(name: ARRAY OF CHAR);
BEGIN
  Out.String("Testing: "); Out.String(name); Out.Ln;
  testResult := 0
END BeginTest;

PROCEDURE Fail(msg: ARRAY OF CHAR; got, expected: INTEGER);
BEGIN
  testResult := 1;
  Out.String("  FAIL: "); Out.String(msg);
  Out.String(" got="); Out.Int(got, 0);
  Out.String(" expected="); Out.Int(expected, 0); Out.Ln
END Fail;

PROCEDURE Check(label: ARRAY OF CHAR; ok: BOOLEAN);
BEGIN
  IF ~ok THEN
    testResult := 1;
    Out.String("  FAIL: "); Out.String(label); Out.Ln
  END
END Check;

PROCEDURE EndTest;
BEGIN
  INC(totalTests);
  IF testResult = 0 THEN
    INC(passedTests); Out.String("  PASSED"); Out.Ln
  ELSE
    Out.String("  FAILED"); Out.Ln
  END
END EndTest;

(* ── Global declarations ── *)

TYPE
  Point  = RECORD x, y: INTEGER END;
  Named  = RECORD id: INTEGER; tag: CHAR; score: INTEGER END;
  Mixed  = RECORD b1: BYTE; i1: INTEGER; b2: BYTE; i2: INTEGER END;

VAR
  gI1, gI2, gI3 : INTEGER;
  gB1            : BOOLEAN;
  gC1            : CHAR;

  gArr10         : ARRAY 10 OF INTEGER;
  gStr16         : ARRAY 16 OF CHAR;
  gCharArr8      : ARRAY 8 OF CHAR;

  gPt1, gPt2     : Point;
  gNamed         : Named;
  gMixed         : Mixed;

(* ── Test 1: Global INTEGER read/write and zero-init ── *)
PROCEDURE SetGI(a, b, c: INTEGER);
BEGIN gI1 := a; gI2 := b; gI3 := c END SetGI;

PROCEDURE SumGI(): INTEGER;
BEGIN RETURN gI1 + gI2 + gI3 END SumGI;

PROCEDURE TestGlobalIntegers*;
BEGIN
  BeginTest("Global INTEGER variables");
  SetGI(10, 20, 30);
  IF gI1 # 10 THEN Fail("gI1", gI1, 10) END;
  IF gI2 # 20 THEN Fail("gI2", gI2, 20) END;
  IF gI3 # 30 THEN Fail("gI3", gI3, 30) END;
  IF SumGI() # 60 THEN Fail("sum", SumGI(), 60) END;
  SetGI(-1, -2, -3);
  IF SumGI() # -6 THEN Fail("sum neg", SumGI(), -6) END;
  EndTest
END TestGlobalIntegers;

(* ── Test 2: Global BOOLEAN and CHAR ── *)
PROCEDURE TestGlobalScalars*;
BEGIN
  BeginTest("Global BOOLEAN and CHAR");
  gB1 := TRUE;  Check("gB1 TRUE",  gB1 = TRUE);
  gB1 := FALSE; Check("gB1 FALSE", gB1 = FALSE);
  gB1 := ~gB1;  Check("NOT FALSE = TRUE", gB1 = TRUE);
  gC1 := 'Z'; Check("gC1='Z'", gC1 = 'Z');
  gC1 := CHR(ORD(gC1) - 25);  (* 'Z'-25 = 'A' *)
  Check("gC1='A'", gC1 = 'A');
  IF ORD(gC1) # 65 THEN Fail("ORD('A')", ORD(gC1), 65) END;
  EndTest
END TestGlobalScalars;

(* ── Test 3: Global INTEGER array fill and element access ── *)
PROCEDURE FillArr(val: INTEGER);
VAR i: INTEGER;
BEGIN FOR i := 0 TO 9 DO gArr10[i] := val END END FillArr;

PROCEDURE SumArr(): INTEGER;
VAR i, s: INTEGER;
BEGIN s := 0; FOR i := 0 TO 9 DO s := s + gArr10[i] END; RETURN s END SumArr;

PROCEDURE TestGlobalIntArray*;
VAR i: INTEGER;
BEGIN
  BeginTest("Global INTEGER array");
  FOR i := 0 TO 9 DO gArr10[i] := i * i END;
  IF gArr10[0] # 0  THEN Fail("arr[0]", gArr10[0], 0) END;
  IF gArr10[3] # 9  THEN Fail("arr[3]", gArr10[3], 9) END;
  IF gArr10[9] # 81 THEN Fail("arr[9]", gArr10[9], 81) END;
  (* sum 0²+1²+...+9² = 285 *)
  IF SumArr() # 285 THEN Fail("sum squares", SumArr(), 285) END;
  (* fill all with 5 and verify *)
  FillArr(5);
  IF SumArr() # 50 THEN Fail("10*5", SumArr(), 50) END;
  (* modify one element, others unchanged *)
  gArr10[4] := 100;
  IF gArr10[3] # 5   THEN Fail("arr[3] unchanged", gArr10[3], 5) END;
  IF gArr10[4] # 100 THEN Fail("arr[4]", gArr10[4], 100) END;
  IF gArr10[5] # 5   THEN Fail("arr[5] unchanged", gArr10[5], 5) END;
  EndTest
END TestGlobalIntArray;

(* ── Test 4: Global ARRAY OF CHAR -- string literal assignment ── *)
PROCEDURE TestGlobalCharArray*;
BEGIN
  BeginTest("Global ARRAY OF CHAR string assignment");
  gStr16 := "hello";
  Check("str[0]='h'", gStr16[0] = 'h');
  Check("str[4]='o'", gStr16[4] = 'o');
  Check("str[5]=NUL", ORD(gStr16[5]) = 0);
  (* overwrite with shorter string — NUL terminator must be updated *)
  gStr16 := "hi";
  Check("hi[0]='h'", gStr16[0] = 'h');
  Check("hi[1]='i'", gStr16[1] = 'i');
  Check("hi[2]=NUL", ORD(gStr16[2]) = 0);
  (* CHAR element write *)
  gStr16[0] := 'H';
  Check("str[0]='H'", gStr16[0] = 'H');
  Check("str[1]='i' unchanged", gStr16[1] = 'i');
  EndTest
END TestGlobalCharArray;

(* ── Test 5: Global ARRAY OF CHAR with mixed element writes ── *)
PROCEDURE TestGlobalCharElemWrite*;
VAR i: INTEGER;
BEGIN
  BeginTest("Global CHAR array element-by-element write");
  gCharArr8 := "abcdefg";
  Check("orig[0]='a'", gCharArr8[0] = 'a');
  (* uppercase each character *)
  FOR i := 0 TO 6 DO
    gCharArr8[i] := CHR(ORD(gCharArr8[i]) - 32)
  END;
  Check("[0]='A'", gCharArr8[0] = 'A');
  Check("[3]='D'", gCharArr8[3] = 'D');
  Check("[6]='G'", gCharArr8[6] = 'G');
  Check("[7]=NUL", ORD(gCharArr8[7]) = 0);
  EndTest
END TestGlobalCharElemWrite;

(* ── Test 6: Global record -- basic field access ── *)
PROCEDURE TestGlobalRecord*;
BEGIN
  BeginTest("Global record field access");
  gPt1.x := 100; gPt1.y := 200;
  IF gPt1.x # 100 THEN Fail("x", gPt1.x, 100) END;
  IF gPt1.y # 200 THEN Fail("y", gPt1.y, 200) END;
  gPt1.x := gPt1.x + gPt1.y;
  IF gPt1.x # 300 THEN Fail("x+y", gPt1.x, 300) END;
  EndTest
END TestGlobalRecord;

(* ── Test 7: Two global records of same type are independent ── *)
PROCEDURE TestTwoGlobalRecords*;
BEGIN
  BeginTest("Two global records are independent");
  gPt1.x := 1; gPt1.y := 2;
  gPt2.x := 3; gPt2.y := 4;
  IF gPt1.x # 1 THEN Fail("pt1.x", gPt1.x, 1) END;
  IF gPt2.x # 3 THEN Fail("pt2.x", gPt2.x, 3) END;
  gPt2.x := 99;
  IF gPt1.x # 1 THEN Fail("pt1.x unaffected", gPt1.x, 1) END;
  IF gPt2.x # 99 THEN Fail("pt2.x updated", gPt2.x, 99) END;
  EndTest
END TestTwoGlobalRecords;

(* ── Test 8: Global record with CHAR field (byte load regression) ── *)
PROCEDURE TestGlobalRecordChar*;
BEGIN
  BeginTest("Global record with CHAR field (byte isolation)");
  gNamed.id    := 42;
  gNamed.tag   := 'X';
  gNamed.score := 100;
  IF gNamed.id    # 42  THEN Fail("id", gNamed.id, 42) END;
  IF ORD(gNamed.tag) # 88 THEN Fail("tag 'X'=88", ORD(gNamed.tag), 88) END;
  IF gNamed.score # 100  THEN Fail("score", gNamed.score, 100) END;
  (* Modify tag -- id and score must not change *)
  gNamed.tag := 'Y';
  IF gNamed.id    # 42  THEN Fail("id after tag chg", gNamed.id, 42) END;
  IF ORD(gNamed.tag) # 89 THEN Fail("tag 'Y'=89", ORD(gNamed.tag), 89) END;
  IF gNamed.score # 100  THEN Fail("score after tag chg", gNamed.score, 100) END;
  EndTest
END TestGlobalRecordChar;

(* ── Test 9: Global record with alternating BYTE and INTEGER fields ── *)
PROCEDURE TestGlobalMixedRecord*;
BEGIN
  BeginTest("Global BYTE/INTEGER interleaved record");
  gMixed.b1 := 1; gMixed.i1 := 1000;
  gMixed.b2 := 2; gMixed.i2 := 2000;
  Check("b1=1",   ORD(gMixed.b1) = 1);
  IF gMixed.i1 # 1000 THEN Fail("i1", gMixed.i1, 1000) END;
  Check("b2=2",   ORD(gMixed.b2) = 2);
  IF gMixed.i2 # 2000 THEN Fail("i2", gMixed.i2, 2000) END;
  (* Update i1, verify others unchanged *)
  gMixed.i1 := gMixed.i1 + gMixed.i2;
  IF gMixed.i1 # 3000    THEN Fail("i1+i2", gMixed.i1, 3000) END;
  Check("b1 unchanged", ORD(gMixed.b1) = 1);
  Check("b2 unchanged", ORD(gMixed.b2) = 2);
  IF gMixed.i2 # 2000    THEN Fail("i2 unchanged", gMixed.i2, 2000) END;
  EndTest
END TestGlobalMixedRecord;

(* ── Test 10: Cross-procedure sharing via globals ── *)
VAR gAccum: INTEGER;

PROCEDURE ResetAccum;   BEGIN gAccum := 0 END ResetAccum;
PROCEDURE AddToAccum(v: INTEGER); BEGIN gAccum := gAccum + v END AddToAccum;
PROCEDURE GetAccum(): INTEGER; BEGIN RETURN gAccum END GetAccum;

PROCEDURE TestGlobalSharing*;
BEGIN
  BeginTest("Cross-procedure global sharing");
  ResetAccum;
  IF GetAccum() # 0 THEN Fail("initial 0", GetAccum(), 0) END;
  AddToAccum(5); AddToAccum(10); AddToAccum(15);
  IF GetAccum() # 30 THEN Fail("sum 5+10+15", GetAccum(), 30) END;
  AddToAccum(-30);
  IF GetAccum() # 0 THEN Fail("back to 0", GetAccum(), 0) END;
  EndTest
END TestGlobalSharing;

(* ── Test 11: Global array used across multiple procedures ── *)
VAR gCounter: ARRAY 4 OF INTEGER;

PROCEDURE ResetCounters;
VAR i: INTEGER;
BEGIN FOR i := 0 TO 3 DO gCounter[i] := 0 END END ResetCounters;

PROCEDURE BumpCounter(i: INTEGER);
BEGIN
  IF (i >= 0) & (i < 4) THEN INC(gCounter[i]) END
END BumpCounter;

PROCEDURE CounterSum(): INTEGER;
VAR i, s: INTEGER;
BEGIN s := 0; FOR i := 0 TO 3 DO s := s + gCounter[i] END; RETURN s END CounterSum;

PROCEDURE TestGlobalArraySharing*;
VAR i: INTEGER;
BEGIN
  BeginTest("Global array shared across procedures");
  ResetCounters;
  IF CounterSum() # 0 THEN Fail("initial sum", CounterSum(), 0) END;
  FOR i := 0 TO 3 DO BumpCounter(i) END;
  IF CounterSum() # 4 THEN Fail("4 bumps sum", CounterSum(), 4) END;
  BumpCounter(0); BumpCounter(0); BumpCounter(3);
  IF gCounter[0] # 3 THEN Fail("counter[0]", gCounter[0], 3) END;
  IF gCounter[1] # 1 THEN Fail("counter[1]", gCounter[1], 1) END;
  IF gCounter[3] # 2 THEN Fail("counter[3]", gCounter[3], 2) END;
  EndTest
END TestGlobalArraySharing;

PROCEDURE PrintSummary;
BEGIN
  Out.Ln;
  Out.String("========================================"); Out.Ln;
  Out.String("Total tests: "); Out.Int(totalTests, 0); Out.Ln;
  Out.String("Passed: ");      Out.Int(passedTests, 0); Out.Ln;
  Out.String("Failed: ");      Out.Int(totalTests - passedTests, 0); Out.Ln;
  Out.String("========================================"); Out.Ln
END PrintSummary;

PROCEDURE RunAllTests*;
BEGIN
  Out.String("========================================"); Out.Ln;
  Out.String("Starting Globals Tests"); Out.Ln;
  Out.String("========================================"); Out.Ln;
  totalTests := 0; passedTests := 0;

  TestGlobalIntegers;
  TestGlobalScalars;
  TestGlobalIntArray;
  TestGlobalCharArray;
  TestGlobalCharElemWrite;
  TestGlobalRecord;
  TestTwoGlobalRecords;
  TestGlobalRecordChar;
  TestGlobalMixedRecord;
  TestGlobalSharing;
  TestGlobalArraySharing;

  PrintSummary
END RunAllTests;

END TestGlobals.
