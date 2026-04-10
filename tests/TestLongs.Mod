MODULE TestLongs;

IMPORT Out;

CONST
  Ok* = 0;
  ErrArithmetic* = 1;
  ErrBoolean* = 2;
  ErrArray* = 3;
  ErrRecord* = 4;
  ErrHeap* = 5;
  ErrShift* = 6;

  (* 7FFFFFFFH = 2147483647; 80000000H = -2147483648 as signed 32-bit *)
  MaxLong = 7FFFFFFFH;
  MinLong = 80000000H;

TYPE
  LongRecord = POINTER TO RECORD
    a, b, c: LONGINT
  END;

  DataBlock = POINTER TO ARRAY 100 OF LONGINT;

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

PROCEDURE AssertLong(actual, expected: LONGINT; errorCode: INTEGER);
BEGIN
  IF actual # expected THEN
    testResult := errorCode;
    Out.String("  FAILED: expected ");
    Out.Int(expected, 0);
    Out.String(", got ");
    Out.Int(actual, 0);
    Out.Ln
  END
END AssertLong;

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

PROCEDURE TestLongArithmetic*;
VAR
  a, b, c: LONGINT;
BEGIN
  InitTest("LONGINT arithmetic");

  a := 100; b := 200; c := a + b;
  AssertLong(c, 300, ErrArithmetic);

  a := 100; b := -200; c := a + b;
  AssertLong(c, -100, ErrArithmetic);

  a := 500; b := 300; c := a - b;
  AssertLong(c, 200, ErrArithmetic);

  a := 500; b := -300; c := a - b;
  AssertLong(c, 800, ErrArithmetic);

  a := 50; b := 20; c := a * b;
  AssertLong(c, 1000, ErrArithmetic);

  a := 50; b := -20; c := a * b;
  AssertLong(c, -1000, ErrArithmetic);

  a := 100; b := 3; c := a DIV b;
  AssertLong(c, 33, ErrArithmetic);

  a := 100; b := -3; c := a DIV b;
  AssertLong(c, -33, ErrArithmetic);

  a := 100; b := 3; c := a MOD b;
  AssertLong(c, 1, ErrArithmetic);

  a := 10; b := 20; c := 30;
  a := (a + b) * c - 50;
  AssertLong(a, 850, ErrArithmetic);

  ReportTest
END TestLongArithmetic;

PROCEDURE TestLongBoundaries*;
VAR
  a, b, c: LONGINT;
  overflow: BOOLEAN;
BEGIN
  InitTest("LONGINT boundaries");

  (* MaxLong = 7FFFFFFFH = 2147483647 *)
  a := MaxLong;
  AssertLong(a, 7FFFFFFFH, ErrArithmetic);

  (* MinLong = 80000000H interpreted as signed = -2147483648 *)
  a := MinLong;
  b := -1;
  c := a - b;  (* -2147483648 - (-1) = -2147483647 *)
  AssertLong(c, -7FFFFFFFH, ErrArithmetic);

  overflow := FALSE;
  a := MaxLong;
  b := 1;
  c := a + b;
  IF (a > 0) & (b > 0) & (c < 0) THEN
    overflow := TRUE
  END;
  Assert(overflow, ErrArithmetic);

  ReportTest
END TestLongBoundaries;

PROCEDURE TestLongAssignment*;
VAR
  a, b: LONGINT;
  const1, const2: LONGINT;
BEGIN
  InitTest("LONGINT assignment");

  (* 75BCD15H = 123456789 in hex *)
  a := 75BCD15H;
  (* 3ADE68B1H = 987654321; negated = -987654321 *)
  b := -3ADE68B1H;
  AssertLong(a, 75BCD15H, ErrArithmetic);
  AssertLong(b, -3ADE68B1H, ErrArithmetic);

  (* 777777 = BDBF1H; 888888 = D9038H *)
  const1 := 0BDBF1H;
  const2 := 0D9038H;
  a := const1;
  b := const2;
  AssertLong(a, 0BDBF1H, ErrArithmetic);
  AssertLong(b, 0D9038H, ErrArithmetic);

  (* (777201 + 888888) * 2 = 3332178 = 32D852H *)
  a := (const1 + const2) * 2;
  AssertLong(a, 32D852H, ErrArithmetic);

  ReportTest
END TestLongAssignment;

PROCEDURE TestShiftLSL*;
VAR
  a, result: LONGINT;
BEGIN
  InitTest("LSL - Logical shift left for LONGINT");

  a := 1; result := SYSTEM.LSL(a, 1); AssertLong(result, 2, ErrShift);
  a := 3; result := SYSTEM.LSL(a, 2); AssertLong(result, 12, ErrShift);
  a := 0FFH; result := SYSTEM.LSL(a, 4); AssertLong(result, 0FF0H, ErrShift);
  a := 1; result := SYSTEM.LSL(a, 8); AssertLong(result, 100H, ErrShift);
  a := 5; result := SYSTEM.LSL(a, 5); AssertLong(result, 0A0H, ErrShift);
  a := MaxLong; result := SYSTEM.LSL(a, 1); AssertLong(result, -2, ErrShift);
  a := 40000000H; result := SYSTEM.LSL(a, 1); AssertLong(result, MinLong, ErrShift);

  ReportTest
END TestShiftLSL;

PROCEDURE TestShiftLSR*;
VAR
  a, result: LONGINT;
BEGIN
  InitTest("LSR - Logical shift right for LONGINT");

  a := 2; result := SYSTEM.LSR(a, 1); AssertLong(result, 1, ErrShift);
  a := 12; result := SYSTEM.LSR(a, 2); AssertLong(result, 3, ErrShift);
  a := 0FF0H; result := SYSTEM.LSR(a, 4); AssertLong(result, 0FFH, ErrShift);
  a := 100H; result := SYSTEM.LSR(a, 8); AssertLong(result, 1, ErrShift);
  a := 0A0H; result := SYSTEM.LSR(a, 5); AssertLong(result, 5, ErrShift);
  a := -2; result := SYSTEM.LSR(a, 1); AssertLong(result, MaxLong, ErrShift);
  a := MinLong; result := SYSTEM.LSR(a, 1); AssertLong(result, 40000000H, ErrShift);

  ReportTest
END TestShiftLSR;

PROCEDURE TestShiftASR*;
VAR
  a, result: LONGINT;
BEGIN
  InitTest("ASR - Arithmetic shift right for LONGINT");

  a := 2; result := SYSTEM.ASR(a, 1); AssertLong(result, 1, ErrShift);
  a := 12; result := SYSTEM.ASR(a, 2); AssertLong(result, 3, ErrShift);
  a := 0FF0H; result := SYSTEM.ASR(a, 4); AssertLong(result, 0FFH, ErrShift);
  a := 100H; result := SYSTEM.ASR(a, 8); AssertLong(result, 1, ErrShift);
  a := 0A0H; result := SYSTEM.ASR(a, 5); AssertLong(result, 5, ErrShift);
  a := -2; result := SYSTEM.ASR(a, 1); AssertLong(result, -1, ErrShift);
  a := -4; result := SYSTEM.ASR(a, 1); AssertLong(result, -2, ErrShift);
  a := -16; result := SYSTEM.ASR(a, 2); AssertLong(result, -4, ErrShift);
  a := -1; result := SYSTEM.ASR(a, 5); AssertLong(result, -1, ErrShift);
  a := MinLong; result := SYSTEM.ASR(a, 1); AssertLong(result, -40000000H, ErrShift);

  ReportTest
END TestShiftASR;

PROCEDURE TestShiftROR*;
VAR
  a, result: LONGINT;
BEGIN
  InitTest("ROR - Rotate right for LONGINT");

  a := 1; result := SYSTEM.ROR(a, 1); AssertLong(result, MinLong, ErrShift);
  a := 2; result := SYSTEM.ROR(a, 1); AssertLong(result, 1, ErrShift);
  a := 4; result := SYSTEM.ROR(a, 1); AssertLong(result, 2, ErrShift);
  a := 8; result := SYSTEM.ROR(a, 1); AssertLong(result, 4, ErrShift);
  (* 40000000H ROR 1 = 20000000H *)
  a := 40000000H; result := SYSTEM.ROR(a, 1); AssertLong(result, 20000000H, ErrShift);
  (* 1 ROR 2 = 40000000H *)
  a := 1; result := SYSTEM.ROR(a, 2); AssertLong(result, 40000000H, ErrShift);
  (* 3 ROR 1 = MinLong OR 1 = MinLong + 1 *)
  a := 3; result := SYSTEM.ROR(a, 1); AssertLong(result, MinLong + 1, ErrShift);

  ReportTest
END TestShiftROR;

PROCEDURE TestShiftCombinations*;
VAR
  a, result: LONGINT;
BEGIN
  InitTest("Shift combinations and chaining for LONGINT");

  a := 1;
  result := SYSTEM.LSL(a, 3);
  result := SYSTEM.LSR(result, 1);
  AssertLong(result, 4, ErrShift);

  a := 100;
  result := SYSTEM.LSL(a, 2);
  result := SYSTEM.ASR(result, 1);
  AssertLong(result, 200, ErrShift);

  a := -100;
  result := SYSTEM.ASR(a, 2);
  result := SYSTEM.LSL(result, 1);
  AssertLong(result, -50, ErrShift);

  a := 1;
  result := SYSTEM.ROR(a, 1);
  result := SYSTEM.LSL(result, 1);
  AssertLong(result, 0, ErrShift);

  a := 0FFH;
  result := SYSTEM.LSL(a, 8);
  result := SYSTEM.LSR(result, 8);
  AssertLong(result, 0FFH, ErrShift);

  (* 75BCD15H = 123456789; LSL 4 then LSR 4 should recover it *)
  a := 75BCD15H;
  result := SYSTEM.LSL(a, 4);
  result := SYSTEM.LSR(result, 4);
  AssertLong(result, 75BCD15H, ErrShift);

  ReportTest
END TestShiftCombinations;

PROCEDURE TestShiftWithVariables*;
VAR
  value, shift: LONGINT;
  result: LONGINT;
BEGIN
  InitTest("Shifts with variable shift counts for LONGINT");

  value := 1; shift := 5;
  result := SYSTEM.LSL(value, shift);
  AssertLong(result, 20H, ErrShift);

  value := 80H; shift := 3;
  result := SYSTEM.LSR(value, shift);
  AssertLong(result, 10H, ErrShift);

  value := -40H; shift := 2;
  result := SYSTEM.ASR(value, shift);
  AssertLong(result, -10H, ErrShift);

  value := 1; shift := 31;
  result := SYSTEM.LSL(value, shift);
  AssertLong(result, MinLong, ErrShift);

  value := MinLong; shift := 1;
  result := SYSTEM.LSR(value, shift);
  AssertLong(result, 40000000H, ErrShift);

  ReportTest
END TestShiftWithVariables;

PROCEDURE TestShiftBoundaries*;
VAR
  a, result: LONGINT;
BEGIN
  InitTest("Shift boundary conditions for LONGINT");

  a := 75BCD15H;
  result := SYSTEM.LSL(a, 0); AssertLong(result, 75BCD15H, ErrShift);
  result := SYSTEM.LSR(a, 0); AssertLong(result, 75BCD15H, ErrShift);
  result := SYSTEM.ASR(a, 0); AssertLong(result, 75BCD15H, ErrShift);
  result := SYSTEM.ROR(a, 0); AssertLong(result, 75BCD15H, ErrShift);

  a := 1;
  result := SYSTEM.LSL(a, 31); AssertLong(result, MinLong, ErrShift);
  result := SYSTEM.LSL(a, 32); AssertLong(result, 1, ErrShift);  (* x86: shift count & 31 = 0, no-op *)

  a := -1;
  result := SYSTEM.LSR(a, 1); AssertLong(result, MaxLong, ErrShift);
  result := SYSTEM.ASR(a, 32); AssertLong(result, -1, ErrShift);

  a := 1;
  result := SYSTEM.ROR(a, 32); AssertLong(result, 1, ErrShift);

  ReportTest
END TestShiftBoundaries;

PROCEDURE TestBooleanOperations*;
VAR
  a, b, c: BOOLEAN;
  x, y, z: LONGINT;
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
  IF (x < y) & (x > 0) THEN
    z := 100
  END;
  AssertLong(z, 100, ErrBoolean);

  ReportTest
END TestBooleanOperations;

PROCEDURE TestBooleanComparisons*;
VAR
  a, b, c: LONGINT;
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
  LongArray = ARRAY 10 OF LONGINT;
VAR
  arr: LongArray;
  i: INTEGER;
  sum: LONGINT;
BEGIN
  InitTest("Static arrays of LONGINT");

  FOR i := 0 TO 9 DO
    arr[i] := i * i
  END;

  AssertLong(arr[0], 0, ErrArray);
  AssertLong(arr[5], 25, ErrArray);
  AssertLong(arr[9], 81, ErrArray);

  sum := 0;
  FOR i := 0 TO 9 DO
    sum := sum + arr[i]
  END;
  AssertLong(sum, 285, ErrArray);

  ReportTest
END TestStaticArray;

PROCEDURE TestArrayIndexing*;
TYPE
  LongArray = ARRAY 100 OF LONGINT;
VAR
  arr: LongArray;
  i: INTEGER;
BEGIN
  InitTest("Array indexing with LONGINT elements");

  FOR i := 0 TO 99 DO
    arr[i] := LONGINT(i) * 1000
  END;
  AssertLong(arr[25], 61A8H, ErrArray);   (* 25000 = 61A8H *)
  AssertLong(arr[50], 0C350H, ErrArray);  (* 50000 = C350H *)
  AssertLong(arr[99], 182B8H, ErrArray);  (* 99000 = 182B8H *)

  ReportTest
END TestArrayIndexing;

PROCEDURE TestRecordAccess*;
TYPE
  Point = RECORD
    x, y: LONGINT
  END;

  Rectangle = RECORD
    topLeft, bottomRight: Point
  END;

VAR
  p: Point;
  r: Rectangle;
BEGIN
  InitTest("Record field access with LONGINT");

  p.x := 186A0H;   (* 100000 *)
  p.y := 30D40H;   (* 200000 -- actually 30D40H = 200000? 200000 = 30D40H yes *)
  AssertLong(p.x, 186A0H, ErrRecord);
  AssertLong(p.y, 30D40H, ErrRecord);

  r.topLeft.x := 2710H;     (* 10000 *)
  r.topLeft.y := 4E20H;     (* 20000 *)
  r.bottomRight.x := 186A0H; (* 100000 *)
  r.bottomRight.y := 30D40H; (* 200000 *)

  AssertLong(r.topLeft.x, 2710H, ErrRecord);
  AssertLong(r.topLeft.y, 4E20H, ErrRecord);
  AssertLong(r.bottomRight.x, 186A0H, ErrRecord);
  AssertLong(r.bottomRight.y, 30D40H, ErrRecord);

  r.topLeft.x := r.bottomRight.x - r.topLeft.x;
  AssertLong(r.topLeft.x, 15F90H, ErrRecord);  (* 90000 = 15F90H *)

  ReportTest
END TestRecordAccess;

PROCEDURE TestRecordAlignment*;
TYPE
  Mixed = RECORD
    a: BYTE;
    b: LONGINT;
    c: BYTE;
    d: LONGINT
  END;

VAR
  m: Mixed;
BEGIN
  InitTest("Record field alignment with LONGINT");

  m.a := 1X;
  m.b := 0F4240H;   (* 1000000 *)
  m.c := 2X;
  m.d := 1E8480H;   (* 2000000 *)

  Assert(m.a = 1X, ErrRecord);
  AssertLong(m.b, 0F4240H, ErrRecord);
  Assert(m.c = 2X, ErrRecord);
  AssertLong(m.d, 1E8480H, ErrRecord);

  ReportTest
END TestRecordAlignment;

PROCEDURE TestHeapAllocation*;
VAR
  p: LongRecord;
  q: DataBlock;
  i: INTEGER;
  sum: LONGINT;
BEGIN
  InitTest("Heap allocation for LONGINT");

  NEW(p);
  IF p = NIL THEN
    Assert(FALSE, ErrHeap)
  ELSE
    p^.a := 67932H;  (* 424242 = 67932H *)
    p^.b := -1;
    p^.c := MaxLong;
    AssertLong(p^.a, 67932H, ErrHeap);
    AssertLong(p^.b, -1, ErrHeap);
    AssertLong(p^.c, MaxLong, ErrHeap);
    DISPOSE(p)
  END;

  NEW(q);
  IF q = NIL THEN
    Assert(FALSE, ErrHeap)
  ELSE
    FOR i := 0 TO 99 DO
      q^[i] := LONGINT(i) * 1000
    END;

    sum := 0;
    FOR i := 0 TO 99 DO
      sum := sum + q^[i]
    END;
    AssertLong(sum, 4B87F0H, ErrHeap);  (* 4950000 = 4B87F0H *)

    DISPOSE(q)
  END;

  ReportTest
END TestHeapAllocation;

PROCEDURE TestHeapMultiple*;
VAR
  p1, p2, p3: LongRecord;
  i: INTEGER;
BEGIN
  InitTest("Multiple heap allocations");

  NEW(p1); NEW(p2); NEW(p3);
  IF (p1 = NIL) OR (p2 = NIL) OR (p3 = NIL) THEN
    Assert(FALSE, ErrHeap)
  ELSE
    p1^.a := 1; p1^.b := 2; p1^.c := 3;
    p2^.a := 10; p2^.b := 20; p2^.c := 30;
    p3^.a := 100; p3^.b := 200; p3^.c := 300;
    AssertLong(p1^.a + p2^.a + p3^.a, 111, ErrHeap);
    AssertLong(p1^.b + p2^.b + p3^.b, 222, ErrHeap);
    AssertLong(p1^.c + p2^.c + p3^.c, 333, ErrHeap);
    DISPOSE(p1); DISPOSE(p2); DISPOSE(p3)
  END;

  ReportTest
END TestHeapMultiple;

PROCEDURE RunAllTests*;
BEGIN
  Out.String("========================================");
  Out.Ln;
  Out.String("Starting LONGINT Type and Memory Tests");
  Out.Ln;
  Out.String("========================================");
  Out.Ln;

  totalTests := 0;
  passedTests := 0;

  TestLongArithmetic;
  TestLongBoundaries;
  TestLongAssignment;

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
  TestHeapMultiple;

  PrintSummary
END RunAllTests;

END TestLongs.
