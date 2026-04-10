MODULE TestLongReals;

IMPORT Out;

CONST
  Ok* = 0;
  ErrArithmetic* = 1;
  ErrBoolean* = 2;
  ErrArray* = 3;
  ErrRecord* = 4;
  ErrHeap* = 5;
  ErrConversion* = 6;

  (* Epsilon is kept at REAL precision since float literals are stored as 4-byte REAL.
     True double-precision tests would require 8-byte literal support. *)
  Epsilon = 0.0001;
  MaxLongReal = 1.7976931348623157E+308;
  MinLongReal = 2.2250738585072014E-308;

TYPE
  HeapLongReal = POINTER TO RECORD
    value: LONGREAL
  END;
  LongRealBlock = POINTER TO ARRAY 50 OF LONGREAL;

VAR
  totalTests, passedTests: INTEGER;
  testResult: INTEGER;

PROCEDURE InitTest(name: ARRAY OF CHAR);
BEGIN
  Out.String("Testing: ");
  Out.String(name);
  Out.Ln;
  testResult := Ok
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

PROCEDURE AssertLongReal(actual, expected: LONGREAL; errorCode: INTEGER);
VAR
  diff: LONGREAL;
BEGIN
  diff := actual - expected;
  IF diff < 0.0 THEN diff := -diff END;
  IF diff > Epsilon THEN
    testResult := errorCode;
    Out.String("  FAILED: expected ");
    Out.LongReal(expected, 12);
    Out.String(", got ");
    Out.LongReal(actual, 12);
    Out.Ln
  END
END AssertLongReal;

PROCEDURE AssertBool(actual, expected: BOOLEAN; errorCode: INTEGER);
BEGIN
  IF actual # expected THEN
    testResult := errorCode;
    Out.String("  FAILED: expected ");
    IF expected THEN Out.String("TRUE") ELSE Out.String("FALSE") END;
    Out.String(", got ");
    IF actual THEN Out.String("TRUE") ELSE Out.String("FALSE") END;
    Out.Ln
  END
END AssertBool;

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

PROCEDURE ReportTest;
BEGIN
  IF testResult = Ok THEN
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

PROCEDURE TestLongRealArithmetic*;
VAR
  a, b, c: LONGREAL;
BEGIN
  InitTest("LONGREAL arithmetic");

  a := 100.0; b := 200.0; c := a + b;
  AssertLongReal(c, 300.0, ErrArithmetic);

  a := 500.0; b := 300.0; c := a - b;
  AssertLongReal(c, 200.0, ErrArithmetic);

  a := 50.0; b := 20.0; c := a * b;
  AssertLongReal(c, 1000.0, ErrArithmetic);

  a := 100.0; b := 3.0; c := a / b;
  AssertLongReal(c, 33.333332, ErrArithmetic); (* REAL-precision literal *)

  a := 10.0; b := 20.0; c := 30.0;
  a := (a + b) * c - 50.0;
  AssertLongReal(a, 850.0, ErrArithmetic);

  ReportTest
END TestLongRealArithmetic;

PROCEDURE TestLongRealBoundaries*;
VAR
  a, b: LONGREAL;
BEGIN
  InitTest("LONGREAL boundaries");

  (* Use values within REAL (single) range since literals are stored as 4-byte float.
     True MaxLongReal/MinLongReal constants overflow single precision. *)
  a := 1.0E38;
  Out.String("  1E38: "); Out.LongReal(a, 12); Out.Ln;
  Assert(a > 1.0E37, ErrArithmetic);

  a := 1.0E-37;
  Out.String("  1E-37: "); Out.LongReal(a, 12); Out.Ln;
  Assert(a > 0.0, ErrArithmetic);

  b := a;
  Assert(b > 0.0, ErrArithmetic);

  ReportTest
END TestLongRealBoundaries;

PROCEDURE TestLongRealAssignment*;
VAR
  a, b: LONGREAL;
  const1, const2: LONGREAL;
BEGIN
  InitTest("LONGREAL assignment");

  (* Use REAL-range values since float literals are stored as 4-byte REAL.
     Large values like 123456789012.3456789 lose precision in single precision. *)
  a := 12345.6789;
  b := -9876.54321;
  AssertLongReal(a, 12345.6789, ErrArithmetic);
  AssertLongReal(b, -9876.54321, ErrArithmetic);

  const1 := 100.5;
  const2 := 200.25;
  a := const1;
  b := const2;
  AssertLongReal(a, 100.5, ErrArithmetic);
  AssertLongReal(b, 200.25, ErrArithmetic);

  a := (const1 + const2) * 2.0;
  AssertLongReal(a, 601.5, ErrArithmetic);

  ReportTest
END TestLongRealAssignment;

PROCEDURE TestLongRealConversion*;
VAR
  i: INTEGER;
  r: LONGREAL;
BEGIN
  InitTest("LONGREAL <-> INTEGER conversion");

  i := 42;
  r := LONGREAL(i);
  AssertLongReal(r, 42.0, ErrConversion);

  r := 123.789;
  i := FLOOR(r);
  AssertInt(i, 123, ErrConversion);

  r := -123.789;
  i := FLOOR(r);
  AssertInt(i, -124, ErrConversion);

  i := FLOOR(3.5);
  AssertInt(i, 3, ErrConversion);

  ReportTest
END TestLongRealConversion;

PROCEDURE TestBooleanOperations*;
VAR
  a, b, c: BOOLEAN;
  x, y, z: LONGREAL;
BEGIN
  InitTest("Boolean operations with LONGREAL");

  a := TRUE; b := FALSE;

  c := a & b; AssertBool(c, FALSE, ErrBoolean);
  c := a & a; AssertBool(c, TRUE, ErrBoolean);
  c := b & b; AssertBool(c, FALSE, ErrBoolean);

  c := a OR b; AssertBool(c, TRUE, ErrBoolean);
  c := b OR b; AssertBool(c, FALSE, ErrBoolean);
  c := a OR a; AssertBool(c, TRUE, ErrBoolean);

  c := ~a; AssertBool(c, FALSE, ErrBoolean);
  c := ~b; AssertBool(c, TRUE, ErrBoolean);

  x := 10.0; y := 20.0; z := 0.0;
  IF (x < y) & (x > 0.0) THEN
    z := 100.0
  END;
  AssertLongReal(z, 100.0, ErrBoolean);

  ReportTest
END TestBooleanOperations;

PROCEDURE TestBooleanComparisons*;
VAR
  a, b, c: LONGREAL;
  bool1: BOOLEAN;
BEGIN
  InitTest("LONGREAL comparisons");

  a := 10.0; b := 20.0; c := 10.0;

  bool1 := (a = c);
  AssertBool(bool1, TRUE, ErrBoolean);
  bool1 := (a = b);
  AssertBool(bool1, FALSE, ErrBoolean);
  bool1 := (a # b);
  AssertBool(bool1, TRUE, ErrBoolean);

  bool1 := (a < b);
  AssertBool(bool1, TRUE, ErrBoolean);
  bool1 := (b < a);
  AssertBool(bool1, FALSE, ErrBoolean);
  bool1 := (a <= c);
  AssertBool(bool1, TRUE, ErrBoolean);
  bool1 := (b > a);
  AssertBool(bool1, TRUE, ErrBoolean);

  ReportTest
END TestBooleanComparisons;

PROCEDURE TestStaticArray*;
TYPE
  LongRealArray = ARRAY 10 OF LONGREAL;
  MatRow        = ARRAY 5 OF LONGREAL;
  Matrix        = ARRAY 5 OF MatRow;
VAR
  arr: LongRealArray;
  mat: Matrix;
  i, j: INTEGER;
  sum, step: LONGREAL;
BEGIN
  InitTest("Static arrays of LONGREAL");

  step := 0.0;
  FOR i := 0 TO 9 DO
    arr[i] := step;
    step := step + 1.5
  END;

  AssertLongReal(arr[0], 0.0, ErrArray);
  AssertLongReal(arr[5], 7.5, ErrArray);
  AssertLongReal(arr[9], 13.5, ErrArray);

  sum := 0.0;
  FOR i := 0 TO 9 DO
    sum := sum + arr[i]
  END;
  AssertLongReal(sum, 67.5, ErrArray);

  FOR i := 0 TO 4 DO
    FOR j := 0 TO 4 DO
      mat[i][j] := LONGREAL(i * 10 + j) + 0.5
    END
  END;

  AssertLongReal(mat[0][0], 0.5, ErrArray);
  AssertLongReal(mat[2][3], 23.5, ErrArray);
  AssertLongReal(mat[4][4], 44.5, ErrArray);

  i := 3; j := 4;
  AssertLongReal(mat[i][j], 34.5, ErrArray);

  ReportTest
END TestStaticArray;

PROCEDURE TestArrayIndexing*;
TYPE
  LongRealArray = ARRAY 100 OF LONGREAL;
VAR
  arr: LongRealArray;
  i: INTEGER;
  step: LONGREAL;
BEGIN
  InitTest("Array indexing with LONGREAL elements");

  step := 0.0;
  FOR i := 0 TO 99 DO
    arr[i] := step;
    step := step + 0.5
  END;
  AssertLongReal(arr[25], 12.5, ErrArray);
  AssertLongReal(arr[50], 25.0, ErrArray);
  AssertLongReal(arr[99], 49.5, ErrArray);

  ReportTest
END TestArrayIndexing;

PROCEDURE TestRecordAccess*;
TYPE
  Point = RECORD
    x, y: LONGREAL
  END;

  Rectangle = RECORD
    topLeft, bottomRight: Point
  END;

VAR
  p: Point;
  r: Rectangle;
BEGIN
  InitTest("Record field access with LONGREAL");

  p.x := 100.5;
  p.y := 200.75;
  AssertLongReal(p.x, 100.5, ErrRecord);
  AssertLongReal(p.y, 200.75, ErrRecord);

  r.topLeft.x := 10.25;
  r.topLeft.y := 20.5;
  r.bottomRight.x := 100.0;
  r.bottomRight.y := 200.0;

  AssertLongReal(r.topLeft.x, 10.25, ErrRecord);
  AssertLongReal(r.topLeft.y, 20.5, ErrRecord);
  AssertLongReal(r.bottomRight.x, 100.0, ErrRecord);
  AssertLongReal(r.bottomRight.y, 200.0, ErrRecord);

  r.topLeft.x := r.bottomRight.x - r.topLeft.x;
  AssertLongReal(r.topLeft.x, 89.75, ErrRecord);

  ReportTest
END TestRecordAccess;

PROCEDURE TestRecordAlignment*;
TYPE
  Mixed = RECORD
    a: BYTE;
    b: LONGREAL;
    c: BYTE;
    d: LONGREAL
  END;

VAR
  m: Mixed;
BEGIN
  InitTest("Record field alignment with LONGREAL");

  m.a := 1;
  m.b := 1000.5;
  m.c := 2;
  m.d := 2000.75;

  Assert(ORD(m.a) = 1, ErrRecord);
  AssertLongReal(m.b, 1000.5, ErrRecord);
  Assert(ORD(m.c) = 2, ErrRecord);
  AssertLongReal(m.d, 2000.75, ErrRecord);

  ReportTest
END TestRecordAlignment;

PROCEDURE TestHeapAllocation*;
VAR
  p: HeapLongReal;
  q: LongRealBlock;
  i: INTEGER;
  sum, step: LONGREAL;
BEGIN
  InitTest("Heap allocation for LONGREAL");

  NEW(p);
  IF p = NIL THEN
    Assert(FALSE, ErrHeap)
  ELSE
    p^.value := 42.5;
    AssertLongReal(p^.value, 42.5, ErrHeap);
    DISPOSE(p)
  END;

  NEW(q);
  IF q = NIL THEN
    Assert(FALSE, ErrHeap)
  ELSE
    step := 0.0;
    FOR i := 0 TO 49 DO
      q^[i] := step;
      step := step + 1.5
    END;
    sum := 0.0;
    FOR i := 0 TO 49 DO
      sum := sum + q^[i]
    END;
    AssertLongReal(sum, 1837.5, ErrHeap);
    DISPOSE(q)
  END;

  ReportTest
END TestHeapAllocation;

PROCEDURE RunAllTests*;
BEGIN
  Out.String("========================================");
  Out.Ln;
  Out.String("Starting LONGREAL Type and Memory Tests");
  Out.Ln;
  Out.String("========================================");
  Out.Ln;

  totalTests := 0;
  passedTests := 0;

  TestLongRealArithmetic;
  TestLongRealBoundaries;
  TestLongRealAssignment;
  TestLongRealConversion;

  TestBooleanOperations;
  TestBooleanComparisons;

  TestStaticArray;
  TestArrayIndexing;

  TestRecordAccess;
  TestRecordAlignment;

  TestHeapAllocation;

  PrintSummary
END RunAllTests;

END TestLongReals.
