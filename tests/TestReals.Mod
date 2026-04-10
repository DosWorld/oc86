MODULE TestReals;

IMPORT Out;

CONST
  Ok* = 0;
  ErrArithmetic* = 1;
  ErrBoolean* = 2;
  ErrArray* = 3;
  ErrRecord* = 4;
  ErrHeap* = 5;
  ErrConversion* = 6;

  Epsilon = 0.0001;
  MaxReal = 3.40282347E+38;
  MinReal = 1.17549435E-38;

TYPE
  HeapReal = POINTER TO RECORD
    value: REAL
  END;
  RealBlock = POINTER TO ARRAY 50 OF REAL;

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

PROCEDURE AssertReal(actual, expected: REAL; errorCode: INTEGER);
VAR
  diff: REAL;
BEGIN
  diff := actual - expected;
  IF diff < 0.0 THEN diff := -diff END;
  IF diff > Epsilon THEN
    testResult := errorCode;
    Out.String("  FAILED: expected ");
    Out.Real(expected, 10);
    Out.String(", got ");
    Out.Real(actual, 10);
    Out.Ln
  END
END AssertReal;

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

PROCEDURE TestRealArithmetic*;
VAR
  a, b, c: REAL;
BEGIN
  InitTest("REAL arithmetic");

  a := 100.0; b := 200.0; c := a + b;
  AssertReal(c, 300.0, ErrArithmetic);

  a := 500.0; b := 300.0; c := a - b;
  AssertReal(c, 200.0, ErrArithmetic);

  a := 50.0; b := 20.0; c := a * b;
  AssertReal(c, 1000.0, ErrArithmetic);

  a := 100.0; b := 3.0; c := a / b;
  AssertReal(c, 33.333333, ErrArithmetic);

  a := 10.0; b := 20.0; c := 30.0;
  a := (a + b) * c - 50.0;
  AssertReal(a, 850.0, ErrArithmetic);

  ReportTest
END TestRealArithmetic;

PROCEDURE TestRealBoundaries*;
VAR
  a, b: REAL;
BEGIN
  InitTest("REAL boundaries");

  a := MaxReal;
  Out.String("  MaxReal: "); Out.Real(a, 10); Out.Ln;
  Assert(a > 1.0E38, ErrArithmetic);

  a := MinReal;
  Out.String("  MinReal: "); Out.Real(a, 10); Out.Ln;
  Assert(a > 0.0, ErrArithmetic);

  b := a;
  Assert(b > 0.0, ErrArithmetic);

  ReportTest
END TestRealBoundaries;

PROCEDURE TestRealAssignment*;
VAR
  a, b: REAL;
  const1, const2: REAL;
BEGIN
  InitTest("REAL assignment");

  a := 12345.6789;
  b := -9876.54321;
  AssertReal(a, 12345.6789, ErrArithmetic);
  AssertReal(b, -9876.54321, ErrArithmetic);

  const1 := 777.777;
  const2 := 888.888;
  a := const1;
  b := const2;
  AssertReal(a, 777.777, ErrArithmetic);
  AssertReal(b, 888.888, ErrArithmetic);

  a := (const1 + const2) * 2.0;
  AssertReal(a, 3333.33, ErrArithmetic);

  ReportTest
END TestRealAssignment;

PROCEDURE TestRealConversion*;
VAR
  i: INTEGER;
  r: REAL;
BEGIN
  InitTest("REAL <-> INTEGER conversion");

  i := 42;
  r := REAL(i);
  AssertReal(r, 42.0, ErrConversion);

  r := 123.456;
  i := FLOOR(r);
  AssertInt(i, 123, ErrConversion);

  r := -123.456;
  i := FLOOR(r);
  AssertInt(i, -124, ErrConversion);

  i := FLOOR(3.999);
  AssertInt(i, 3, ErrConversion);

  ReportTest
END TestRealConversion;

PROCEDURE TestBooleanOperations*;
VAR
  a, b, c: BOOLEAN;
  x, y, z: REAL;
BEGIN
  InitTest("Boolean operations with REAL");

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
  AssertReal(z, 100.0, ErrBoolean);

  ReportTest
END TestBooleanOperations;

PROCEDURE TestBooleanComparisons*;
VAR
  a, b, c: REAL;
  bool1: BOOLEAN;
BEGIN
  InitTest("REAL comparisons");

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
  RealArray = ARRAY 10 OF REAL;
  MatRow    = ARRAY 5 OF REAL;
  Matrix    = ARRAY 5 OF MatRow;
VAR
  arr: RealArray;
  mat: Matrix;
  i, j: INTEGER;
  sum, step: REAL;
BEGIN
  InitTest("Static arrays of REAL");

  step := 0.0;
  FOR i := 0 TO 9 DO
    arr[i] := step;
    step := step + 1.5
  END;

  AssertReal(arr[0], 0.0, ErrArray);
  AssertReal(arr[5], 7.5, ErrArray);
  AssertReal(arr[9], 13.5, ErrArray);

  sum := 0.0;
  FOR i := 0 TO 9 DO
    sum := sum + arr[i]
  END;
  AssertReal(sum, 67.5, ErrArray);

  FOR i := 0 TO 4 DO
    FOR j := 0 TO 4 DO
      mat[i][j] := REAL(i * 10 + j) + 0.5
    END
  END;

  AssertReal(mat[0][0], 0.5, ErrArray);
  AssertReal(mat[2][3], 23.5, ErrArray);
  AssertReal(mat[4][4], 44.5, ErrArray);

  i := 3; j := 4;
  AssertReal(mat[i][j], 34.5, ErrArray);

  ReportTest
END TestStaticArray;

PROCEDURE TestArrayIndexing*;
TYPE
  RealArray = ARRAY 100 OF REAL;
VAR
  arr: RealArray;
  i: INTEGER;
  step: REAL;
BEGIN
  InitTest("Array indexing with REAL elements");

  step := 0.0;
  FOR i := 0 TO 99 DO
    arr[i] := step;
    step := step + 0.5
  END;
  AssertReal(arr[25], 12.5, ErrArray);
  AssertReal(arr[50], 25.0, ErrArray);
  AssertReal(arr[99], 49.5, ErrArray);

  ReportTest
END TestArrayIndexing;

PROCEDURE TestRecordAccess*;
TYPE
  Point = RECORD
    x, y: REAL
  END;

  Rectangle = RECORD
    topLeft, bottomRight: Point
  END;

VAR
  p: Point;
  r: Rectangle;
BEGIN
  InitTest("Record field access with REAL");

  p.x := 100.5;
  p.y := 200.75;
  AssertReal(p.x, 100.5, ErrRecord);
  AssertReal(p.y, 200.75, ErrRecord);

  r.topLeft.x := 10.25;
  r.topLeft.y := 20.5;
  r.bottomRight.x := 100.0;
  r.bottomRight.y := 200.0;

  AssertReal(r.topLeft.x, 10.25, ErrRecord);
  AssertReal(r.topLeft.y, 20.5, ErrRecord);
  AssertReal(r.bottomRight.x, 100.0, ErrRecord);
  AssertReal(r.bottomRight.y, 200.0, ErrRecord);

  r.topLeft.x := r.bottomRight.x - r.topLeft.x;
  AssertReal(r.topLeft.x, 89.75, ErrRecord);

  ReportTest
END TestRecordAccess;

PROCEDURE TestRecordAlignment*;
TYPE
  Mixed = RECORD
    a: BYTE;
    b: REAL;
    c: BYTE;
    d: REAL
  END;

VAR
  m: Mixed;
BEGIN
  InitTest("Record field alignment with REAL");

  m.a := 1;
  m.b := 1000.5;
  m.c := 2;
  m.d := 2000.75;

  Assert(ORD(m.a) = 1, ErrRecord);
  AssertReal(m.b, 1000.5, ErrRecord);
  Assert(ORD(m.c) = 2, ErrRecord);
  AssertReal(m.d, 2000.75, ErrRecord);

  ReportTest
END TestRecordAlignment;

PROCEDURE TestHeapAllocation*;
VAR
  p: HeapReal;
  q: RealBlock;
  i: INTEGER;
  sum, step: REAL;
BEGIN
  InitTest("Heap allocation for REAL");

  NEW(p);
  IF p = NIL THEN
    Assert(FALSE, ErrHeap)
  ELSE
    p^.value := 42.5;
    AssertReal(p^.value, 42.5, ErrHeap);
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
    AssertReal(sum, 1837.5, ErrHeap);
    DISPOSE(q)
  END;

  ReportTest
END TestHeapAllocation;

PROCEDURE RunAllTests*;
BEGIN
  Out.String("========================================");
  Out.Ln;
  Out.String("Starting REAL Type and Memory Tests");
  Out.Ln;
  Out.String("========================================");
  Out.Ln;

  totalTests := 0;
  passedTests := 0;

  TestRealArithmetic;
  TestRealBoundaries;
  TestRealAssignment;
  TestRealConversion;

  TestBooleanOperations;
  TestBooleanComparisons;

  TestStaticArray;
  TestArrayIndexing;

  TestRecordAccess;
  TestRecordAlignment;

  TestHeapAllocation;

  PrintSummary
END RunAllTests;

END TestReals.
