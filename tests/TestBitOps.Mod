MODULE TestBitOps;

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

PROCEDURE AssertInt(actual, expected: INTEGER; errorCode: INTEGER);
BEGIN
  IF actual # expected THEN
    testResult := errorCode;
    Out.String("  FAILED: expected ");
    Out.Int(expected, 0);
    Out.String(", got ");
    Out.Int(actual, 0);
    Out.Ln
  END
END AssertInt;

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

(* Test 1: SYSTEM.AND on INTEGER *)
PROCEDURE TestAndInt*;
VAR a, b, r: INTEGER;
BEGIN
  InitTest("SYSTEM.AND on INTEGER");
  a := 0FFH; b := 0F0H; r := SYSTEM.AND(a, b);
  AssertInt(r, 0F0H, 1);
  a := 55H;  b := 0AAH; r := SYSTEM.AND(a, b);
  AssertInt(r, 0, 2);
  a := 0FFFFH; b := 1234H; r := SYSTEM.AND(a, b);
  AssertInt(r, 1234H, 3);
  a := 0;    b := 0FFFFH; r := SYSTEM.AND(a, b);
  AssertInt(r, 0, 4);
  ReportTest
END TestAndInt;

(* Test 2: SYSTEM.IOR on INTEGER *)
PROCEDURE TestIorInt*;
VAR a, b, r: INTEGER;
BEGIN
  InitTest("SYSTEM.IOR on INTEGER");
  a := 0F0H; b := 0FH;   r := SYSTEM.IOR(a, b);
  AssertInt(r, 0FFH, 1);
  a := 55H;  b := 0AAH;  r := SYSTEM.IOR(a, b);
  AssertInt(r, 0FFH, 2);
  a := 0;    b := 1234H; r := SYSTEM.IOR(a, b);
  AssertInt(r, 1234H, 3);
  a := 0FFFFH; b := 0;   r := SYSTEM.IOR(a, b);
  AssertInt(r, 0FFFFH, 4);
  ReportTest
END TestIorInt;

(* Test 3: SYSTEM.XOR on INTEGER *)
PROCEDURE TestXorInt*;
VAR a, b, r: INTEGER;
BEGIN
  InitTest("SYSTEM.XOR on INTEGER");
  a := 0FFH;   b := 0F0H;   r := SYSTEM.XOR(a, b);
  AssertInt(r, 0FH, 1);
  a := 0FFFFH; b := 0FFFFH; r := SYSTEM.XOR(a, b);
  AssertInt(r, 0, 2);
  a := 0;      b := 5A5AH;  r := SYSTEM.XOR(a, b);
  AssertInt(r, 5A5AH, 3);
  a := 55H;    b := 0AAH;   r := SYSTEM.XOR(a, b);
  AssertInt(r, 0FFH, 4);
  ReportTest
END TestXorInt;

(* Test 4: SYSTEM.AND on BYTE operands (result is INTEGER) *)
PROCEDURE TestAndByte*;
VAR a, b: BYTE; r: INTEGER;
BEGIN
  InitTest("SYSTEM.AND on BYTE");
  a := BYTE(0FFH); b := BYTE(0F0H); r := SYSTEM.AND(a, b);
  AssertInt(r, 0F0H, 1);
  a := BYTE(55H);  b := BYTE(0AAH); r := SYSTEM.AND(a, b);
  AssertInt(r, 0, 2);
  ReportTest
END TestAndByte;

(* Test 5: SYSTEM.IOR on BYTE operands *)
PROCEDURE TestIorByte*;
VAR a, b: BYTE; r: INTEGER;
BEGIN
  InitTest("SYSTEM.IOR on BYTE");
  a := BYTE(0F0H); b := BYTE(0FH);  r := SYSTEM.IOR(a, b);
  AssertInt(r, 0FFH, 1);
  a := BYTE(55H);  b := BYTE(0AAH); r := SYSTEM.IOR(a, b);
  AssertInt(r, 0FFH, 2);
  ReportTest
END TestIorByte;

(* Test 6: SYSTEM.XOR on BYTE operands *)
PROCEDURE TestXorByte*;
VAR a, b: BYTE; r: INTEGER;
BEGIN
  InitTest("SYSTEM.XOR on BYTE");
  a := BYTE(0FFH); b := BYTE(0F0H); r := SYSTEM.XOR(a, b);
  AssertInt(r, 0FH, 1);
  a := BYTE(0AAH); b := BYTE(0AAH); r := SYSTEM.XOR(a, b);
  AssertInt(r, 0, 2);
  ReportTest
END TestXorByte;

(* Test 7: SYSTEM.AND on LONGINT *)
PROCEDURE TestAndLong*;
VAR a, b, r: LONGINT;
BEGIN
  InitTest("SYSTEM.AND on LONGINT");
  a := LONGINT(0FFFF00FFH); b := LONGINT(0FFFF0F0FH);
  r := SYSTEM.AND(a, b);
  AssertLong(r, LONGINT(0FFFF000FH), 1);
  a := LONGINT(7FFFFFFFH); b := LONGINT(80000000H);
  r := SYSTEM.AND(a, b);
  AssertLong(r, LONGINT(0), 2);
  a := LONGINT(-1); b := LONGINT(12345678H);
  r := SYSTEM.AND(a, b);
  AssertLong(r, LONGINT(12345678H), 3);
  ReportTest
END TestAndLong;

(* Test 8: SYSTEM.IOR on LONGINT *)
PROCEDURE TestIorLong*;
VAR a, b, r: LONGINT;
BEGIN
  InitTest("SYSTEM.IOR on LONGINT");
  a := LONGINT(7FFFFFFFH); b := LONGINT(80000000H);
  r := SYSTEM.IOR(a, b);
  AssertLong(r, LONGINT(-1), 1);
  a := LONGINT(0); b := LONGINT(12345678H);
  r := SYSTEM.IOR(a, b);
  AssertLong(r, LONGINT(12345678H), 2);
  a := LONGINT(0F0F0F0F0H); b := LONGINT(0F0F0F0FH);
  r := SYSTEM.IOR(a, b);
  AssertLong(r, LONGINT(-1), 3);
  ReportTest
END TestIorLong;

(* Test 9: SYSTEM.XOR on LONGINT *)
PROCEDURE TestXorLong*;
VAR a, b, r: LONGINT;
BEGIN
  InitTest("SYSTEM.XOR on LONGINT");
  a := LONGINT(-1); b := LONGINT(-1);
  r := SYSTEM.XOR(a, b);
  AssertLong(r, LONGINT(0), 1);
  a := LONGINT(0); b := LONGINT(12345678H);
  r := SYSTEM.XOR(a, b);
  AssertLong(r, LONGINT(12345678H), 2);
  a := LONGINT(0AAAAAAAH); b := LONGINT(55555555H);
  r := SYSTEM.XOR(a, b);
  AssertLong(r, LONGINT(5FFFFFFFH), 3);
  ReportTest
END TestXorLong;

(* Test 10: SYSTEM.AND with literal constants *)
PROCEDURE TestAndConst*;
VAR r: INTEGER;
BEGIN
  InitTest("SYSTEM.AND with constants");
  r := SYSTEM.AND(0FFFFH, 0F0F0H);
  AssertInt(r, 0F0F0H, 1);
  r := SYSTEM.AND(0ABH, 0FH);
  AssertInt(r, 0BH, 2);
  ReportTest
END TestAndConst;

PROCEDURE RunAllTests*;
BEGIN
  totalTests := 0;
  passedTests := 0;
  TestAndInt;
  TestIorInt;
  TestXorInt;
  TestAndByte;
  TestIorByte;
  TestXorByte;
  TestAndLong;
  TestIorLong;
  TestXorLong;
  TestAndConst;
  PrintSummary
END RunAllTests;

BEGIN
END TestBitOps.
