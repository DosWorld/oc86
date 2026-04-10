MODULE TestTypes;

IMPORT Out;

CONST
  Ok* = 0;
  ErrArithmetic* = 1;
  ErrBoolean* = 2;
  ErrArray* = 3;
  ErrRecord* = 4;
  ErrHeap* = 5;
  ErrShift* = 6;
  
  MaxInt = 7FFFH;
  MinInt = -32768;

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

PROCEDURE TestIntegerArithmetic*;
VAR
  a, b, c: INTEGER;
BEGIN
  InitTest("Integer arithmetic");
  
  a := 100; b := 200; c := a + b;
  AssertInt(c, 300, ErrArithmetic);

  a := 100; b := -200; c := a + b;
  AssertInt(c, -100, ErrArithmetic);
  
  a := 500; b := 300; c := a - b;
  AssertInt(c, 200, ErrArithmetic);
  
  a := 50; b := 20; c := a * b;
  AssertInt(c, 1000, ErrArithmetic);

  a := -50; b := 20; c := a * b;
  AssertInt(c, -1000, ErrArithmetic);
  
  a := 100; b := 3; c := a DIV b;
  AssertInt(c, 33, ErrArithmetic);

  a := -100; b := 3; c := a DIV b;
  AssertInt(c, -33, ErrArithmetic);
  
  a := 100; b := 3; c := a MOD b;
  AssertInt(c, 1, ErrArithmetic);
  
  a := 10; b := 20; c := 30;
  a := (a + b) * c - 50;
  AssertInt(a, 850, ErrArithmetic);
  
  ReportTest
END TestIntegerArithmetic;

PROCEDURE TestIntegerBoundaries*;
VAR
  a, b, c: INTEGER;
  overflow: BOOLEAN;
BEGIN
  InitTest("Integer boundaries");
  
  a := MaxInt;
  AssertInt(a, 32767, ErrArithmetic);
  
  a := MinInt;
  AssertInt(a, -32768, ErrArithmetic);
  
  overflow := FALSE;
  a := MaxInt;
  b := 1;
  c := a + b;
  IF (a > 0) & (b > 0) & (c < 0) THEN
    overflow := TRUE
  END;
  Assert(overflow, ErrArithmetic);
  
  ReportTest
END TestIntegerBoundaries;

PROCEDURE TestIntegerAssignment*;
VAR
  a, b: INTEGER;
  const1, const2: INTEGER;
BEGIN
  InitTest("Integer assignment");
  
  a := 12345;
  b := -12345;
  AssertInt(a, 12345, ErrArithmetic);
  AssertInt(b, -12345, ErrArithmetic);
  
  const1 := 777;
  const2 := 888;
  a := const1;
  b := const2;
  AssertInt(a, 777, ErrArithmetic);
  AssertInt(b, 888, ErrArithmetic);
  
  a := (const1 + const2) * 2;
  AssertInt(a, 3330, ErrArithmetic);
  
  ReportTest
END TestIntegerAssignment;

PROCEDURE TestShiftLSL*;
VAR
  a, result: INTEGER;
BEGIN
  InitTest("LSL - Logical shift left");
  
  a := 1; result := SYSTEM.LSL(a, 1); AssertInt(result, 2, ErrShift);
  a := 3; result := SYSTEM.LSL(a, 2); AssertInt(result, 12, ErrShift);
  a := 255; result := SYSTEM.LSL(a, 4); AssertInt(result, 4080, ErrShift);
  a := 1; result := SYSTEM.LSL(a, 8); AssertInt(result, 256, ErrShift);
  a := 5; result := SYSTEM.LSL(a, 5); AssertInt(result, 160, ErrShift);
  a := 32767; result := SYSTEM.LSL(a, 1); AssertInt(result, -2, ErrShift);
  a := 16384; result := SYSTEM.LSL(a, 1); AssertInt(result, -32768, ErrShift);
  
  ReportTest
END TestShiftLSL;

PROCEDURE TestShiftLSR*;
VAR
  a, result: INTEGER;
BEGIN
  InitTest("LSR - Logical shift right");
  
  a := 2; result := SYSTEM.LSR(a, 1); AssertInt(result, 1, ErrShift);
  a := 12; result := SYSTEM.LSR(a, 2); AssertInt(result, 3, ErrShift);
  a := 4080; result := SYSTEM.LSR(a, 4); AssertInt(result, 255, ErrShift);
  a := 256; result := SYSTEM.LSR(a, 8); AssertInt(result, 1, ErrShift);
  a := 160; result := SYSTEM.LSR(a, 5); AssertInt(result, 5, ErrShift);
  a := -2; result := SYSTEM.LSR(a, 1); AssertInt(result, 32767, ErrShift);
  a := -32768; result := SYSTEM.LSR(a, 1); AssertInt(result, 16384, ErrShift);
  
  ReportTest
END TestShiftLSR;

PROCEDURE TestShiftASR*;
VAR
  a, result: INTEGER;
BEGIN
  InitTest("ASR - Arithmetic shift right");
  
  a := 2; result := SYSTEM.ASR(a, 1); AssertInt(result, 1, ErrShift);
  a := 12; result := SYSTEM.ASR(a, 2); AssertInt(result, 3, ErrShift);
  a := 4080; result := SYSTEM.ASR(a, 4); AssertInt(result, 255, ErrShift);
  a := 256; result := SYSTEM.ASR(a, 8); AssertInt(result, 1, ErrShift);
  a := 160; result := SYSTEM.ASR(a, 5); AssertInt(result, 5, ErrShift);
  a := -2; result := SYSTEM.ASR(a, 1); AssertInt(result, -1, ErrShift);
  a := -4; result := SYSTEM.ASR(a, 1); AssertInt(result, -2, ErrShift);
  a := -16; result := SYSTEM.ASR(a, 2); AssertInt(result, -4, ErrShift);
  a := -1; result := SYSTEM.ASR(a, 5); AssertInt(result, -1, ErrShift);
  a := -32768; result := SYSTEM.ASR(a, 1); AssertInt(result, -16384, ErrShift);
  
  ReportTest
END TestShiftASR;

PROCEDURE TestShiftROR*;
VAR
  a, result: INTEGER;
BEGIN
  InitTest("ROR - Rotate right");
  
  a := 1; result := SYSTEM.ROR(a, 1); AssertInt(result, -32768, ErrShift);
  a := 2; result := SYSTEM.ROR(a, 1); AssertInt(result, 1, ErrShift);
  a := 4; result := SYSTEM.ROR(a, 1); AssertInt(result, 2, ErrShift);
  a := 8; result := SYSTEM.ROR(a, 1); AssertInt(result, 4, ErrShift);
  a := -32768; result := SYSTEM.ROR(a, 1); AssertInt(result, 16384, ErrShift);
  a := 1; result := SYSTEM.ROR(a, 2); AssertInt(result, 16384, ErrShift);
  a := 3; result := SYSTEM.ROR(a, 1); AssertInt(result, -32767, ErrShift);
  a := -1; result := SYSTEM.ROR(a, 8); AssertInt(result, -1, ErrShift);
  
  ReportTest
END TestShiftROR;

PROCEDURE TestShiftCombinations*;
VAR
  a, result: INTEGER;
BEGIN
  InitTest("Shift combinations and chaining");
  
  a := 1;
  result := SYSTEM.LSL(a, 3);
  result := SYSTEM.LSR(result, 1);
  AssertInt(result, 4, ErrShift);
  
  a := 100;
  result := SYSTEM.LSL(a, 2);
  result := SYSTEM.ASR(result, 1);
  AssertInt(result, 200, ErrShift);
  
  a := -100;
  result := SYSTEM.ASR(a, 2);
  result := SYSTEM.LSL(result, 1);
  AssertInt(result, -50, ErrShift);
  
  a := 1;
  result := SYSTEM.ROR(a, 1);
  result := SYSTEM.LSL(result, 1);
  AssertInt(result, 0, ErrShift);
  
  a := 255;
  result := SYSTEM.LSL(a, 8);
  result := SYSTEM.LSR(result, 8);
  AssertInt(result, 255, ErrShift);
  
  a := 12345;
  result := SYSTEM.LSL(a, 4);
  result := SYSTEM.LSR(result, 4);
  AssertInt(result, 57, ErrShift);
  
  ReportTest
END TestShiftCombinations;

PROCEDURE TestShiftWithVariables*;
VAR
  value, shift, result: INTEGER;
BEGIN
  InitTest("Shifts with variable shift counts");
  
  value := 1; shift := 5;
  result := SYSTEM.LSL(value, shift);
  AssertInt(result, 32, ErrShift);
  
  value := 128; shift := 3;
  result := SYSTEM.LSR(value, shift);
  AssertInt(result, 16, ErrShift);
  
  value := -64; shift := 2;
  result := SYSTEM.ASR(value, shift);
  AssertInt(result, -16, ErrShift);
  
  value := 1; shift := 15;
  result := SYSTEM.LSL(value, shift);
  AssertInt(result, -32768, ErrShift);
  
  value := -32768; shift := 1;
  result := SYSTEM.LSR(value, shift);
  AssertInt(result, 16384, ErrShift);
  
  ReportTest
END TestShiftWithVariables;

PROCEDURE TestShiftBoundaries*;
VAR
  a, result: INTEGER;
BEGIN
  InitTest("Shift boundary conditions");
  
  a := 12345;
  result := SYSTEM.LSL(a, 0); AssertInt(result, 12345, ErrShift);
  result := SYSTEM.LSR(a, 0); AssertInt(result, 12345, ErrShift);
  result := SYSTEM.ASR(a, 0); AssertInt(result, 12345, ErrShift);
  result := SYSTEM.ROR(a, 0); AssertInt(result, 12345, ErrShift);
  
  a := 1;
  result := SYSTEM.LSL(a, 15); AssertInt(result, -32768, ErrShift);
  result := SYSTEM.LSL(a, 16); AssertInt(result, 0, ErrShift);
  
  a := -1;
  result := SYSTEM.LSR(a, 1); AssertInt(result, 32767, ErrShift);
  result := SYSTEM.ASR(a, 16); AssertInt(result, -1, ErrShift);
  
  a := 1;
  result := SYSTEM.ROR(a, 16); AssertInt(result, 1, ErrShift);
  
  ReportTest
END TestShiftBoundaries;

PROCEDURE TestBooleanOperations*;
VAR
  a, b, c: BOOLEAN;
  x, y, z: INTEGER;
BEGIN
  InitTest("Boolean operations");
  
  a := TRUE; b := FALSE;
  
  c := a & b; AssertBool(c, FALSE, ErrBoolean);
  c := a & a; AssertBool(c, TRUE, ErrBoolean);
  c := b & b; AssertBool(c, FALSE, ErrBoolean);
  
  c := a OR b; AssertBool(c, TRUE, ErrBoolean);
  c := b OR b; AssertBool(c, FALSE, ErrBoolean);
  c := a OR a; AssertBool(c, TRUE, ErrBoolean);
  
  c := ~a; AssertBool(c, FALSE, ErrBoolean);
  c := ~b; AssertBool(c, TRUE, ErrBoolean);
  
  a := TRUE; b := FALSE;
  c := (a & ~b) OR (~a & b);
  AssertBool(c, TRUE, ErrBoolean);
  
  x := 10; y := 20; z := 0;
  IF (x < y) & (x > 0) THEN z := 100 END;
  AssertInt(z, 100, ErrBoolean);
  
  ReportTest
END TestBooleanOperations;

PROCEDURE TestBooleanComparisons*;
VAR
  a, b, c: INTEGER;
  bool1: BOOLEAN;
BEGIN
  InitTest("Boolean comparisons");
  
  a := 10; b := 20; c := 10;
  
  bool1 := (a = c); AssertBool(bool1, TRUE, ErrBoolean);
  bool1 := (a = b); AssertBool(bool1, FALSE, ErrBoolean);
  bool1 := (a # b); AssertBool(bool1, TRUE, ErrBoolean);
  
  bool1 := (a < b); AssertBool(bool1, TRUE, ErrBoolean);
  bool1 := (b < a); AssertBool(bool1, FALSE, ErrBoolean);
  bool1 := (a <= c); AssertBool(bool1, TRUE, ErrBoolean);
  bool1 := (b > a); AssertBool(bool1, TRUE, ErrBoolean);
  
  ReportTest
END TestBooleanComparisons;

PROCEDURE TestStaticArray*;
TYPE
  IntArray = ARRAY 10 OF INTEGER;
  Matrix = ARRAY 25 OF INTEGER;
VAR
  arr: IntArray;
  mat: Matrix;
  i, j, sum: INTEGER;
BEGIN
  InitTest("Static arrays");

  FOR i := 0 TO 9 DO arr[i] := i * i END;

  AssertInt(arr[0], 0, ErrArray);
  AssertInt(arr[5], 25, ErrArray);
  AssertInt(arr[9], 81, ErrArray);

  sum := 0;
  FOR i := 0 TO 9 DO sum := sum + arr[i] END;
  AssertInt(sum, 285, ErrArray);

  FOR i := 0 TO 4 DO
    FOR j := 0 TO 4 DO mat[i * 5 + j] := i * 10 + j END
  END;

  AssertInt(mat[0 * 5 + 0], 0, ErrArray);
  AssertInt(mat[2 * 5 + 3], 23, ErrArray);
  AssertInt(mat[4 * 5 + 4], 44, ErrArray);

  i := 3; j := 4;
  AssertInt(mat[i * 5 + j], 34, ErrArray);

  ReportTest
END TestStaticArray;

PROCEDURE TestArrayIndexing*;
TYPE
  ByteArray = ARRAY 100 OF BYTE;
  WordArray = ARRAY 100 OF INTEGER;
VAR
  bytes: ByteArray;
  words: WordArray;
  i: INTEGER;
BEGIN
  InitTest("Array indexing (16-bit offsets)");
  
  FOR i := 0 TO 99 DO bytes[i] := CHR(i MOD 256) END;
  Assert(ORD(bytes[50]) = 50, ErrArray);
  
  FOR i := 0 TO 99 DO words[i] := i * 100 END;
  AssertInt(words[25], 2500, ErrArray);
  AssertInt(words[50], 5000, ErrArray);
  
  ReportTest
END TestArrayIndexing;

PROCEDURE TestRecordAccess*;
TYPE
  Point = RECORD x, y: INTEGER END;
  Rectangle = RECORD topLeft, bottomRight: Point END;
VAR
  p: Point;
  r: Rectangle;
BEGIN
  InitTest("Record field access");
  
  p.x := 100; p.y := 200;
  AssertInt(p.x, 100, ErrRecord);
  AssertInt(p.y, 200, ErrRecord);
  
  r.topLeft.x := 10; r.topLeft.y := 20;
  r.bottomRight.x := 100; r.bottomRight.y := 200;
  
  AssertInt(r.topLeft.x, 10, ErrRecord);
  AssertInt(r.topLeft.y, 20, ErrRecord);
  AssertInt(r.bottomRight.x, 100, ErrRecord);
  AssertInt(r.bottomRight.y, 200, ErrRecord);
  
  r.topLeft.x := r.bottomRight.x - r.topLeft.x;
  AssertInt(r.topLeft.x, 90, ErrRecord);
  
  ReportTest
END TestRecordAccess;

PROCEDURE TestRecordAlignment*;
TYPE
  Mixed = RECORD
    a: BYTE;
    b: INTEGER;
    c: BYTE;
    d: INTEGER
  END;
VAR
  m: Mixed;
BEGIN
  InitTest("Record field alignment (offsets)");
  
  m.a := 1;
  m.b := 1000;
  m.c := 2;
  m.d := 2000;
  
  Assert(ORD(m.a) = 1, ErrRecord);
  AssertInt(m.b, 1000, ErrRecord);
  Assert(ORD(m.c) = 2, ErrRecord);
  AssertInt(m.d, 2000, ErrRecord);
  
  ReportTest
END TestRecordAlignment;

TYPE
  HeapRec = POINTER TO RECORD
    value: INTEGER;
    extra: INTEGER
  END;
  DataBlock = POINTER TO ARRAY 100 OF INTEGER;
  LargeArray = POINTER TO ARRAY 50 OF INTEGER;

PROCEDURE TestHeapAllocation*;
VAR
  p: HeapRec;
  q: DataBlock;
  i, sum: INTEGER;
BEGIN
  InitTest("Heap allocation (NEW/DISPOSE)");

  NEW(p);
  IF p = NIL THEN
    Assert(FALSE, ErrHeap)
  ELSE
    p^.value := 42;
    p^.extra := 0;
    AssertInt(p^.value, 42, ErrHeap);
    DISPOSE(p)
  END;

  NEW(q);
  IF q = NIL THEN
    Assert(FALSE, ErrHeap)
  ELSE
    FOR i := 0 TO 49 DO q^[i] := i * 2 END;
    sum := 0;
    FOR i := 0 TO 49 DO sum := sum + q^[i] END;
    AssertInt(sum, 2450, ErrHeap);
    DISPOSE(q)
  END;

  ReportTest
END TestHeapAllocation;

PROCEDURE TestHeapMultiAlloc*;
VAR
  p1, p2, p3: HeapRec;
BEGIN
  InitTest("Multiple heap allocations");

  NEW(p1); NEW(p2); NEW(p3);
  IF (p1 = NIL) OR (p2 = NIL) OR (p3 = NIL) THEN
    Assert(FALSE, ErrHeap)
  ELSE
    p1^.value := 100;
    p2^.value := 200;
    p3^.value := 300;
    AssertInt(p1^.value, 100, ErrHeap);
    AssertInt(p2^.value, 200, ErrHeap);
    AssertInt(p3^.value, 300, ErrHeap);
    DISPOSE(p1); DISPOSE(p2); DISPOSE(p3)
  END;

  ReportTest
END TestHeapMultiAlloc;

PROCEDURE TestFarPointers*;
VAR
  p: LargeArray;
BEGIN
  InitTest("Far pointers (SEG/OFS)");
  
  NEW(p);
  IF p = NIL THEN
    Assert(FALSE, ErrHeap)
  ELSE
    p^[5] := 12345;
    AssertInt(p^[5], 12345, ErrHeap);
    DISPOSE(p)
  END;
  
  ReportTest
END TestFarPointers;

PROCEDURE RunAllTests*;
BEGIN
  Out.String("========================================");
  Out.Ln;
  Out.String("Starting Type and Memory Tests");
  Out.Ln;
  Out.String("========================================");
  Out.Ln;
  
  totalTests := 0;
  passedTests := 0;
  
  TestIntegerArithmetic;
  TestIntegerBoundaries;
  TestIntegerAssignment;
  
  TestShiftLSL;
  TestShiftLSR;
  TestShiftASR;
  TestShiftROR;
  TestShiftCombinations;
  TestShiftWithVariables;
  TestShiftBoundaries;
  
  TestBooleanOperations;
  TestBooleanComparisons;
  
  TestStaticArray;
  TestArrayIndexing;
  
  TestRecordAccess;
  TestRecordAlignment;
  
  TestHeapAllocation;
  TestHeapMultiAlloc;
  TestFarPointers;
  
  PrintSummary
END RunAllTests;

END TestTypes.