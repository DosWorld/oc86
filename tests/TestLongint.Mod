MODULE TestLongint;

IMPORT Out;

VAR
  totalTests, passedTests: INTEGER;
  testResult: INTEGER;

PROCEDURE InitTest(name: ARRAY OF CHAR);
BEGIN
  Out.String("Testing: ");
  Out.String(name);
  Out.Ln;
  testResult := 0
END InitTest;

PROCEDURE Assert(condition: BOOLEAN; errorCode: INTEGER);
BEGIN
  IF ~condition THEN
    testResult := errorCode;
    Out.String("  FAILED (error code: ");
    Out.Int(errorCode, 0);
    Out.String(")");
    Out.Ln
  END
END Assert;

PROCEDURE AssertLong(actual, expected: LONGINT; errorCode: INTEGER);
BEGIN
  IF actual # expected THEN
    testResult := errorCode;
    Out.String("  FAILED: expected ");
    Out.Int(INTEGER(expected), 0);
    Out.String(", got ");
    Out.Int(INTEGER(actual), 0);
    Out.Ln
  END
END AssertLong;

PROCEDURE ReportTest;
BEGIN
  IF testResult = 0 THEN
    Out.String("  PASSED");
    Out.Ln;
    INC(passedTests)
  ELSE
    Out.String("  TEST FAILED");
    Out.Ln
  END;
  INC(totalTests);
  Out.Ln
END ReportTest;

PROCEDURE PrintSummary;
BEGIN
  Out.Ln;
  Out.String("========================================");
  Out.Ln;
  Out.String("Total tests: ");
  Out.Int(totalTests, 0);
  Out.Ln;
  Out.String("Passed: ");
  Out.Int(passedTests, 0);
  Out.Ln;
  Out.String("Failed: ");
  Out.Int(totalTests - passedTests, 0);
  Out.Ln;
  Out.String("========================================");
  Out.Ln
END PrintSummary;

(* Test 1: Basic LONGINT arithmetic — add and subtract small values *)
PROCEDURE TestBasicArith*;
VAR
  a, b, c: LONGINT;
BEGIN
  InitTest("LONGINT basic arithmetic");
  a := LONGINT(10);
  b := LONGINT(20);
  c := a + b;
  AssertLong(c, LONGINT(30), 1);
  c := b - a;
  AssertLong(c, LONGINT(10), 2);
  c := a + b + LONGINT(5);
  AssertLong(c, LONGINT(35), 3);
  ReportTest
END TestBasicArith;

(* Test 2: LONGINT multiply via SYSTEM_S32MUL *)
PROCEDURE TestMultiply*;
VAR
  a, b, c: LONGINT;
BEGIN
  InitTest("LONGINT multiply");
  a := LONGINT(7);
  b := LONGINT(6);
  c := a * b;
  AssertLong(c, LONGINT(42), 1);
  a := LONGINT(100);
  b := LONGINT(300);
  c := a * b;
  AssertLong(c, LONGINT(30000), 2);
  ReportTest
END TestMultiply;

(* Test 3: LONGINT division and mod via SYSTEM_S32DIV, SYSTEM_S32MOD *)
PROCEDURE TestDivMod*;
VAR
  a, b, c: LONGINT;
BEGIN
  InitTest("LONGINT DIV and MOD");
  a := LONGINT(100);
  b := LONGINT(7);
  c := a DIV b;
  AssertLong(c, LONGINT(14), 1);
  c := a MOD b;
  AssertLong(c, LONGINT(2), 2);
  a := LONGINT(42);
  b := LONGINT(6);
  c := a DIV b;
  AssertLong(c, LONGINT(7), 3);
  c := a MOD b;
  AssertLong(c, LONGINT(0), 4);
  ReportTest
END TestDivMod;

(* Test 4: LONGINT negation *)
PROCEDURE TestNegate*;
VAR
  a, b: LONGINT;
BEGIN
  InitTest("LONGINT negation");
  a := LONGINT(42);
  b := -a;
  AssertLong(b, LONGINT(-42), 1);
  b := -LONGINT(1);
  AssertLong(b, LONGINT(-1), 2);
  ReportTest
END TestNegate;

(* Test 5: LONGINT comparison *)
PROCEDURE TestCompare*;
VAR
  a, b: LONGINT;
BEGIN
  InitTest("LONGINT comparison");
  a := LONGINT(10);
  b := LONGINT(20);
  Assert(a < b, 1);
  Assert(b > a, 2);
  Assert(a # b, 3);
  a := LONGINT(42);
  b := LONGINT(42);
  Assert(a = b, 4);
  Assert(a <= b, 5);
  Assert(a >= b, 6);
  ReportTest
END TestCompare;

(* Test 6: INTEGER to LONGINT cast *)
PROCEDURE TestCastToLong*;
VAR
  i: INTEGER;
  l: LONGINT;
BEGIN
  InitTest("INTEGER to LONGINT cast");
  i := 1000;
  l := LONGINT(i);
  AssertLong(l, LONGINT(1000), 1);
  i := -500;
  l := LONGINT(i);
  AssertLong(l, LONGINT(-500), 2);
  ReportTest
END TestCastToLong;

(* Test 7: LONGINT to INTEGER cast (truncate) *)
PROCEDURE TestCastToInt*;
VAR
  l: LONGINT;
  i: INTEGER;
BEGIN
  InitTest("LONGINT to INTEGER cast");
  l := LONGINT(12345);
  i := INTEGER(l);
  Assert(i = 12345, 1);
  l := LONGINT(-999);
  i := INTEGER(l);
  Assert(i = -999, 2);
  ReportTest
END TestCastToInt;

(* Test 8: LONGINT values as VAR params *)
PROCEDURE Double(VAR x: LONGINT);
BEGIN
  x := x + x
END Double;

PROCEDURE TestVarParam*;
VAR
  a: LONGINT;
BEGIN
  InitTest("LONGINT VAR parameter");
  a := LONGINT(21);
  Double(a);
  AssertLong(a, LONGINT(42), 1);
  ReportTest
END TestVarParam;

(* Test 9: LONGINT in a loop *)
PROCEDURE TestLoop*;
VAR
  sum: LONGINT;
  i: INTEGER;
BEGIN
  InitTest("LONGINT in loop");
  sum := LONGINT(0);
  FOR i := 1 TO 10 DO
    sum := sum + LONGINT(i)
  END;
  AssertLong(sum, LONGINT(55), 1);
  ReportTest
END TestLoop;

(* Test 10: LONGINT negative multiply *)
PROCEDURE TestNegMul*;
VAR
  a, b, c: LONGINT;
BEGIN
  InitTest("LONGINT negative multiply");
  a := LONGINT(-3);
  b := LONGINT(7);
  c := a * b;
  AssertLong(c, LONGINT(-21), 1);
  a := LONGINT(-4);
  b := LONGINT(-5);
  c := a * b;
  AssertLong(c, LONGINT(20), 2);
  ReportTest
END TestNegMul;

PROCEDURE RunAllTests*;
BEGIN
  totalTests := 0;
  passedTests := 0;
  TestBasicArith;
  TestMultiply;
  TestDivMod;
  TestNegate;
  TestCompare;
  TestCastToLong;
  TestCastToInt;
  TestVarParam;
  TestLoop;
  TestNegMul;
  PrintSummary
END RunAllTests;

BEGIN
END TestLongint.
