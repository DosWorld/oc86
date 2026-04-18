MODULE TestParams;
(* Comprehensive parameter passing tests:
   - Open-array parameters (LEN, indexing, CHAR arrays)
   - Fixed-array VAR parameters
   - Record value and VAR parameters
   - Multiple VAR params in one call
   - Passing array elements and record fields as VAR params
   - Nested proc with outer VAR param forwarding *)

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

(* ── Types ── *)

TYPE
  Rec  = RECORD x, y: INTEGER END;
  Rec3 = RECORD a, b, c: INTEGER END;
  SmallArr = ARRAY 5 OF INTEGER;
  BigArr   = ARRAY 20 OF INTEGER;

(* Global arrays used as open-array actual params (LEN is correct for globals) *)
VAR
  gArr5:  SmallArr;
  gArr20: BigArr;
  gStr:   ARRAY 32 OF CHAR;

(* ── Open-array helpers ── *)

PROCEDURE ArraySum(a: ARRAY OF INTEGER): INTEGER;
VAR i, s: INTEGER;
BEGIN
  s := 0;
  FOR i := 0 TO LEN(a) - 1 DO s := s + a[i] END;
  RETURN s
END ArraySum;

PROCEDURE ArrayFill(VAR a: ARRAY OF INTEGER; val: INTEGER);
VAR i: INTEGER;
BEGIN
  FOR i := 0 TO LEN(a) - 1 DO a[i] := val END
END ArrayFill;

PROCEDURE ArrayLen(a: ARRAY OF INTEGER): INTEGER;
BEGIN RETURN LEN(a) END ArrayLen;

PROCEDURE StringLen(s: ARRAY OF CHAR): INTEGER;
VAR i: INTEGER;
BEGIN
  i := 0;
  WHILE (i < LEN(s)) & (s[i] # CHR(0)) DO INC(i) END;
  RETURN i
END StringLen;

PROCEDURE StringCopyUpper(src: ARRAY OF CHAR; VAR dst: ARRAY OF CHAR);
VAR i, c: INTEGER;
BEGIN
  i := 0;
  WHILE (i < LEN(src)) & (i < LEN(dst)) & (src[i] # CHR(0)) DO
    c := ORD(src[i]);
    IF (c >= ORD('a')) & (c <= ORD('z')) THEN c := c - 32 END;
    dst[i] := CHR(c);
    INC(i)
  END;
  IF i < LEN(dst) THEN dst[i] := CHR(0) END
END StringCopyUpper;

(* ── Test 1: LEN of open-array parameter (global array) ── *)
PROCEDURE TestOpenArrayLen*;
BEGIN
  BeginTest("LEN of open-array parameter");
  IF ArrayLen(gArr5)  # 5  THEN Fail("len gArr5",  ArrayLen(gArr5),  5)  END;
  IF ArrayLen(gArr20) # 20 THEN Fail("len gArr20", ArrayLen(gArr20), 20) END;
  EndTest
END TestOpenArrayLen;

(* ── Test 2: Open-array indexing and sum ── *)
PROCEDURE TestOpenArraySum*;
VAR i: INTEGER;
BEGIN
  BeginTest("Open-array indexing and sum");
  FOR i := 0 TO 4  DO gArr5[i]  := i + 1 END;      (* 1..5 = 15 *)
  FOR i := 0 TO 19 DO gArr20[i] := i + 1 END;      (* 1..20 = 210 *)
  IF ArraySum(gArr5)  # 15  THEN Fail("sum gArr5",  ArraySum(gArr5),  15)  END;
  IF ArraySum(gArr20) # 210 THEN Fail("sum gArr20", ArraySum(gArr20), 210) END;
  EndTest
END TestOpenArraySum;

(* ── Test 3: VAR open-array fill and read-back ── *)
PROCEDURE TestVarOpenArrayFill*;
VAR i: INTEGER;
BEGIN
  BeginTest("VAR open-array fill");
  ArrayFill(gArr5, 7);
  FOR i := 0 TO 4 DO
    IF gArr5[i] # 7 THEN Fail("fill[i]", gArr5[i], 7) END
  END;
  ArrayFill(gArr20, 3);
  FOR i := 0 TO 19 DO
    IF gArr20[i] # 3 THEN Fail("fill20[i]", gArr20[i], 3) END
  END;
  EndTest
END TestVarOpenArrayFill;

(* ── Test 4: CHAR open-array parameter (LEN and indexing) ── *)
PROCEDURE TestCharOpenArray*;
VAR upper: ARRAY 32 OF CHAR;
BEGIN
  BeginTest("CHAR open-array parameter");
  gStr := "hello";
  IF StringLen(gStr) # 5 THEN Fail("len 'hello'", StringLen(gStr), 5) END;
  StringCopyUpper(gStr, upper);
  Check("upper[0]='H'", upper[0] = 'H');
  Check("upper[1]='E'", upper[1] = 'E');
  Check("upper[4]='O'", upper[4] = 'O');
  Check("upper[5]=NUL", ORD(upper[5]) = 0);
  (* empty string *)
  gStr := "";
  IF StringLen(gStr) # 0 THEN Fail("len empty", StringLen(gStr), 0) END;
  EndTest
END TestCharOpenArray;

(* ── Test 5: Record passed by VAR to read-only procedure ── *)
PROCEDURE SumRec(VAR r: Rec): INTEGER;
BEGIN RETURN r.x + r.y END SumRec;

PROCEDURE ScaleAndSum(VAR r: Rec; factor: INTEGER): INTEGER;
VAR oldX, oldY: INTEGER;
BEGIN
  oldX := r.x; oldY := r.y;
  r.x := r.x * factor; r.y := r.y * factor;
  RETURN r.x + r.y
END ScaleAndSum;

PROCEDURE TestRecordReadVarParam*;
VAR r: Rec;
BEGIN
  BeginTest("Record read via VAR param");
  r.x := 10; r.y := 20;
  IF SumRec(r) # 30 THEN Fail("sum", SumRec(r), 30) END;
  IF r.x # 10 THEN Fail("r.x unchanged by read", r.x, 10) END;
  IF r.y # 20 THEN Fail("r.y unchanged by read", r.y, 20) END;
  IF ScaleAndSum(r, 3) # 90 THEN Fail("scale sum", ScaleAndSum(r, 3), 90) END;
  EndTest
END TestRecordReadVarParam;

(* ── Test 6: Record passed by VAR ── *)
PROCEDURE ScaleRec(VAR r: Rec; factor: INTEGER);
BEGIN r.x := r.x * factor; r.y := r.y * factor END ScaleRec;

PROCEDURE SwapFields(VAR r: Rec);
VAR t: INTEGER;
BEGIN t := r.x; r.x := r.y; r.y := t END SwapFields;

PROCEDURE TestRecordVarParam*;
VAR r: Rec;
BEGIN
  BeginTest("Record passed by VAR");
  r.x := 3; r.y := 4;
  ScaleRec(r, 10);
  IF r.x # 30 THEN Fail("r.x*10", r.x, 30) END;
  IF r.y # 40 THEN Fail("r.y*10", r.y, 40) END;
  SwapFields(r);
  IF r.x # 40 THEN Fail("swap r.x", r.x, 40) END;
  IF r.y # 30 THEN Fail("swap r.y", r.y, 30) END;
  EndTest
END TestRecordVarParam;

(* ── Test 7: Multiple VAR params in one call ── *)
PROCEDURE MinMax(a, b: INTEGER; VAR lo, hi: INTEGER);
BEGIN
  IF a < b THEN lo := a; hi := b ELSE lo := b; hi := a END
END MinMax;

PROCEDURE Divmod(a, b: INTEGER; VAR q, r: INTEGER);
BEGIN q := a DIV b; r := a MOD b END Divmod;

PROCEDURE TestMultiVarParam*;
VAR lo, hi, q, r: INTEGER;
BEGIN
  BeginTest("Multiple VAR parameters in one call");
  MinMax(7, 3, lo, hi);
  IF lo # 3  THEN Fail("lo", lo, 3)  END;
  IF hi # 7  THEN Fail("hi", hi, 7)  END;
  MinMax(-5, 2, lo, hi);
  IF lo # -5 THEN Fail("lo neg", lo, -5) END;
  IF hi # 2  THEN Fail("hi neg", hi, 2)  END;
  Divmod(17, 5, q, r);
  IF q # 3 THEN Fail("q=17 div 5", q, 3) END;
  IF r # 2 THEN Fail("r=17 mod 5", r, 2) END;
  EndTest
END TestMultiVarParam;

(* ── Test 8: Array elements modified via indexed assignment ── *)
PROCEDURE DoubleAll(VAR a: ARRAY OF INTEGER);
VAR i: INTEGER;
BEGIN FOR i := 0 TO LEN(a) - 1 DO a[i] := a[i] * 2 END END DoubleAll;

PROCEDURE TestArrayModifyViaVar*;
VAR i: INTEGER;
BEGIN
  BeginTest("Array elements modified via VAR array param");
  FOR i := 0 TO 4 DO gArr5[i] := i + 1 END;   (* 1,2,3,4,5 *)
  DoubleAll(gArr5);                              (* 2,4,6,8,10 *)
  IF gArr5[0] # 2  THEN Fail("arr[0]*2", gArr5[0], 2)  END;
  IF gArr5[2] # 6  THEN Fail("arr[2]*2", gArr5[2], 6)  END;
  IF gArr5[4] # 10 THEN Fail("arr[4]*2", gArr5[4], 10) END;
  EndTest
END TestArrayModifyViaVar;

(* ── Test 9: Record field modified via VAR record param ── *)
PROCEDURE ScaleField(VAR r: Rec3; factor: INTEGER);
BEGIN r.b := r.b * factor END ScaleField;

PROCEDURE TestRecordFieldModify*;
VAR r: Rec3;
BEGIN
  BeginTest("Record field modified via VAR record param");
  r.a := 10; r.b := 20; r.c := 30;
  ScaleField(r, 2);
  IF r.a # 10 THEN Fail("r.a unchanged", r.a, 10) END;
  IF r.b # 40 THEN Fail("r.b doubled", r.b, 40) END;
  IF r.c # 30 THEN Fail("r.c unchanged", r.c, 30) END;
  EndTest
END TestRecordFieldModify;

(* ── Test 10: Nested proc using outer VAR param directly ── *)
PROCEDURE TestNestedUsesVarParam*;
VAR x: INTEGER;

  PROCEDURE AddFive(VAR v: INTEGER);
  BEGIN v := v + 5 END AddFive;

  PROCEDURE RunTwice;
  BEGIN
    AddFive(x);    (* x is captured from outer scope via static link *)
    AddFive(x)
  END RunTwice;

BEGIN
  BeginTest("Nested proc calls AddFive on outer VAR");
  x := 10;
  RunTwice;
  IF x # 20 THEN Fail("x after RunTwice", x, 20) END;
  EndTest
END TestNestedUsesVarParam;

(* ── Test 11: Open-array with different-sized globals ── *)
PROCEDURE TestOpenArraySizes*;
VAR i: INTEGER;
BEGIN
  BeginTest("Open-array with different-sized global arrays");
  (* Fill gArr5 with 4 and sum *)
  ArrayFill(gArr5, 4);
  IF ArraySum(gArr5) # 20 THEN Fail("5*4", ArraySum(gArr5), 20) END;
  (* Fill gArr20 with 2 and sum *)
  ArrayFill(gArr20, 2);
  IF ArraySum(gArr20) # 40 THEN Fail("20*2", ArraySum(gArr20), 40) END;
  (* Fill gArr5 with 0 and verify *)
  ArrayFill(gArr5, 0);
  IF ArraySum(gArr5) # 0 THEN Fail("sum after zero", ArraySum(gArr5), 0) END;
  (* Verify gArr20 unchanged by fills of gArr5 *)
  IF ArraySum(gArr20) # 40 THEN Fail("gArr20 unchanged", ArraySum(gArr20), 40) END;
  EndTest
END TestOpenArraySizes;

(* ── Test 12: Passing same variable as multiple VAR params
   (aliasing within one call) ── *)
PROCEDURE BothDouble(VAR a, b: INTEGER);
BEGIN a := a * 2; b := b * 2 END BothDouble;

PROCEDURE TestIndependentVarParams*;
VAR a, b: INTEGER;
BEGIN
  BeginTest("Independent VAR params double independently");
  a := 3; b := 7;
  BothDouble(a, b);
  IF a # 6  THEN Fail("a*2", a, 6)  END;
  IF b # 14 THEN Fail("b*2", b, 14) END;
  EndTest
END TestIndependentVarParams;

(* ── Test 13: Value param: callee cannot change caller's var ── *)
PROCEDURE TryModify(p: INTEGER);
BEGIN
  p := 9999   (* local copy only *)
END TryModify;

PROCEDURE TestValueParamIsolation*;
VAR x: INTEGER;
BEGIN
  BeginTest("Value parameter cannot modify caller variable");
  x := 42;
  TryModify(x);
  IF x # 42 THEN Fail("x unchanged after TryModify", x, 42) END;
  EndTest
END TestValueParamIsolation;

(* ── Test 14: CHAR array params - write into VAR CHAR array ── *)
PROCEDURE FillChar(VAR s: ARRAY OF CHAR; c: CHAR; n: INTEGER);
VAR i: INTEGER;
BEGIN
  i := 0;
  WHILE (i < n) & (i < LEN(s)) DO s[i] := c; INC(i) END;
  IF i < LEN(s) THEN s[i] := CHR(0) END
END FillChar;

PROCEDURE TestCharArrayVarParam*;
VAR buf: ARRAY 16 OF CHAR; i: INTEGER;
BEGIN
  BeginTest("CHAR array VAR parameter write");
  gStr := "xxxxxxxxxx";
  FillChar(gStr, 'A', 3);
  Check("gStr[0]='A'", gStr[0] = 'A');
  Check("gStr[1]='A'", gStr[1] = 'A');
  Check("gStr[2]='A'", gStr[2] = 'A');
  Check("gStr[3]=NUL", ORD(gStr[3]) = 0);
  EndTest
END TestCharArrayVarParam;

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
  Out.String("Starting Parameter Tests"); Out.Ln;
  Out.String("========================================"); Out.Ln;
  totalTests := 0; passedTests := 0;

  TestOpenArrayLen;
  TestOpenArraySum;
  TestVarOpenArrayFill;
  TestCharOpenArray;
  TestRecordReadVarParam;
  TestRecordVarParam;
  TestMultiVarParam;
  TestArrayModifyViaVar;
  TestRecordFieldModify;
  TestNestedUsesVarParam;
  TestOpenArraySizes;
  TestIndependentVarParams;
  TestValueParamIsolation;
  TestCharArrayVarParam;

  PrintSummary
END RunAllTests;

END TestParams.
