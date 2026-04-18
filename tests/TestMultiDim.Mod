MODULE TestMultiDim;
(* Tests for multi-dimensional arrays: ARRAY m, n OF T syntax,
   which desugars to nested arrays ARRAY m OF ARRAY n OF T. *)

IMPORT Out;

VAR
  totalTests, passedTests: INTEGER;
  testResult: INTEGER;

PROCEDURE BeginTest(name: ARRAY OF CHAR);
BEGIN
  Out.String("Test: "); Out.String(name); Out.Ln;
  testResult := 0
END BeginTest;

PROCEDURE Check(label: ARRAY OF CHAR; ok: BOOLEAN);
BEGIN
  IF ~ok THEN
    testResult := 1;
    Out.String("  FAIL: "); Out.String(label); Out.Ln
  END
END Check;

PROCEDURE CheckInt(label: ARRAY OF CHAR; actual, expected: INTEGER);
BEGIN
  IF actual # expected THEN
    testResult := 1;
    Out.String("  FAIL: "); Out.String(label);
    Out.String(" got="); Out.Int(actual, 0);
    Out.String(" expected="); Out.Int(expected, 0); Out.Ln
  END
END CheckInt;

PROCEDURE EndTest;
BEGIN
  INC(totalTests);
  IF testResult = 0 THEN
    INC(passedTests); Out.String("  PASSED"); Out.Ln
  ELSE
    Out.String("  FAILED"); Out.Ln
  END;
  Out.Ln
END EndTest;

(* ── Test 1: basic 2D array read/write ── *)
PROCEDURE TestBasic2D*;
VAR
  a: ARRAY 3, 4 OF INTEGER;
  i, j: INTEGER;
BEGIN
  BeginTest("basic 2D array read/write");
  FOR i := 0 TO 2 DO
    FOR j := 0 TO 3 DO
      a[i][j] := i * 10 + j
    END
  END;
  CheckInt("a[0][0]", a[0][0], 0);
  CheckInt("a[0][3]", a[0][3], 3);
  CheckInt("a[1][0]", a[1][0], 10);
  CheckInt("a[2][3]", a[2][3], 23);
  EndTest
END TestBasic2D;

(* ── Test 2: 2D array with variable indices ── *)
PROCEDURE TestVarIndex2D*;
VAR
  m: ARRAY 4, 4 OF INTEGER;
  r, c, sum: INTEGER;
BEGIN
  BeginTest("2D array with variable indices");
  FOR r := 0 TO 3 DO
    FOR c := 0 TO 3 DO
      m[r][c] := r * 4 + c + 1
    END
  END;
  (* sum of first row: 1+2+3+4 = 10 *)
  sum := 0;
  FOR c := 0 TO 3 DO sum := sum + m[0][c] END;
  CheckInt("row0 sum", sum, 10);
  (* sum of main diagonal: 1+6+11+16 = 34 *)
  sum := 0;
  FOR r := 0 TO 3 DO sum := sum + m[r][r] END;
  CheckInt("diagonal sum", sum, 34);
  EndTest
END TestVarIndex2D;

(* ── Test 3: 3D array (desugared to 3 nested levels) ── *)
PROCEDURE TestBasic3D*;
VAR
  b: ARRAY 2, 3, 4 OF INTEGER;
  x, y, z: INTEGER;
BEGIN
  BeginTest("3D array read/write");
  FOR x := 0 TO 1 DO
    FOR y := 0 TO 2 DO
      FOR z := 0 TO 3 DO
        b[x][y][z] := x * 100 + y * 10 + z
      END
    END
  END;
  CheckInt("b[0][0][0]", b[0][0][0], 0);
  CheckInt("b[1][2][3]", b[1][2][3], 123);
  CheckInt("b[0][1][2]", b[0][1][2], 12);
  CheckInt("b[1][0][3]", b[1][0][3], 103);
  EndTest
END TestBasic3D;

(* ── Test 4: global 2D array ── *)
VAR
  gmat: ARRAY 5, 5 OF INTEGER;

PROCEDURE TestGlobal2D*;
VAR
  i, j: INTEGER;
BEGIN
  BeginTest("global 2D array");
  FOR i := 0 TO 4 DO
    FOR j := 0 TO 4 DO
      gmat[i][j] := i * 5 + j
    END
  END;
  CheckInt("gmat[0][0]", gmat[0][0], 0);
  CheckInt("gmat[4][4]", gmat[4][4], 24);
  CheckInt("gmat[2][3]", gmat[2][3], 13);
  (* last element of row 3: 3*5+4 = 19 *)
  CheckInt("gmat[3][4]", gmat[3][4], 19);
  EndTest
END TestGlobal2D;

(* ── Test 5: 2D array — accumulate and overwrite elements ── *)
PROCEDURE TestAccum2D*;
VAR
  a: ARRAY 4, 4 OF INTEGER;
  i, j, total: INTEGER;
BEGIN
  BeginTest("2D array accumulate and overwrite");
  (* fill with 1..16 *)
  FOR i := 0 TO 3 DO
    FOR j := 0 TO 3 DO
      a[i][j] := i * 4 + j + 1
    END
  END;
  (* double every element in-place *)
  FOR i := 0 TO 3 DO
    FOR j := 0 TO 3 DO
      a[i][j] := a[i][j] * 2
    END
  END;
  (* sum should be 2*(1+2+...+16) = 2*136 = 272 *)
  total := 0;
  FOR i := 0 TO 3 DO
    FOR j := 0 TO 3 DO
      total := total + a[i][j]
    END
  END;
  CheckInt("total after doubling", total, 272);
  CheckInt("a[0][0]", a[0][0], 2);
  CheckInt("a[3][3]", a[3][3], 32);
  EndTest
END TestAccum2D;

(* ── Test 6: 2D BYTE array ── *)
PROCEDURE TestByte2D*;
VAR
  b: ARRAY 4, 8 OF BYTE;
  i, j, sum: INTEGER;
BEGIN
  BeginTest("2D BYTE array");
  FOR i := 0 TO 3 DO
    FOR j := 0 TO 7 DO
      b[i][j] := i * 8 + j
    END
  END;
  (* sum of row 1: 8+9+10+11+12+13+14+15 = 92 *)
  sum := 0;
  FOR j := 0 TO 7 DO sum := sum + ORD(b[1][j]) END;
  CheckInt("row1 sum", sum, 92);
  CheckInt("b[3][7]", ORD(b[3][7]), 31);
  EndTest
END TestByte2D;

(* ── Test 7: 2D CHAR array (string table) ── *)
PROCEDURE TestChar2D*;
VAR
  table: ARRAY 3, 8 OF CHAR;
BEGIN
  BeginTest("2D CHAR array");
  table[0][0] := 'A'; table[0][1] := 'B'; table[0][2] := 'C'; table[0][3] := 0X;
  table[1][0] := 'X'; table[1][1] := 'Y'; table[1][2] := 'Z'; table[1][3] := 0X;
  table[2][0] := '1'; table[2][1] := '2'; table[2][2] := '3'; table[2][3] := 0X;
  Check("table[0][0]='A'", table[0][0] = 'A');
  Check("table[1][2]='Z'", table[1][2] = 'Z');
  Check("table[2][0]='1'", table[2][0] = '1');
  Check("table[0][3]=0X",  table[0][3] = 0X);
  EndTest
END TestChar2D;

(* ── Test 8: 2D array size (layout check via indexing) ── *)
PROCEDURE TestLayout2D*;
VAR
  a: ARRAY 3, 4 OF INTEGER;
  i, j: INTEGER;
BEGIN
  BeginTest("2D array layout (row-major)");
  (* Fill with sequential values; verify row-major ordering via sequential scan *)
  FOR i := 0 TO 2 DO
    FOR j := 0 TO 3 DO
      a[i][j] := i * 4 + j
    END
  END;
  (* Row-major: a[1][0] > a[0][3] → 4 > 3 *)
  Check("a[1][0] > a[0][3]", a[1][0] > a[0][3]);
  (* a[0][1] = a[0][0] + 1 *)
  Check("a[0][1] = a[0][0]+1", a[0][1] = a[0][0] + 1);
  CheckInt("a[2][3]", a[2][3], 11);
  EndTest
END TestLayout2D;

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
  Out.String("Starting Multi-Dimensional Array Tests"); Out.Ln;
  Out.String("========================================"); Out.Ln;
  totalTests := 0; passedTests := 0;

  TestBasic2D;
  TestVarIndex2D;
  TestBasic3D;
  TestGlobal2D;
  TestAccum2D;
  TestByte2D;
  TestChar2D;
  TestLayout2D;

  PrintSummary
END RunAllTests;

END TestMultiDim.
