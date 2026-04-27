MODULE ImportConstB;
IMPORT I = ImportConstA, Out;
CONST
  C = I.N + 1;
  D = I.N;
  E = D * 2;
VAR pass, fail: INTEGER;

PROCEDURE Check(name: ARRAY OF CHAR; got, expected: INTEGER);
BEGIN
  IF got = expected THEN
    Out.String("  PASS "); Out.String(name); Out.Ln;
    INC(pass)
  ELSE
    Out.String("  FAIL "); Out.String(name); Out.Ln;
    INC(fail)
  END
END Check;

PROCEDURE RunAllTests*;
VAR x: INTEGER;
    arr: ARRAY I.N OF INTEGER;
    arr2: ARRAY D, E OF INTEGER;
BEGIN
  pass := 0; fail := 0;
  Out.String("ImportConst"); Out.Ln;
  (* use imported const as array dim and in CASE label *)
  x := 0;
  CASE I.N OF
    1: x := 1;
    2: x := 2;
    I.N-1: x := 3;
    I.N: x := 4;
  ELSE x := 99;
  END;
  Check("CASE I.N", x, 4);
  Check("C=5", C, 5);
  (* use D/E in multi-dim array and nested const expression E = D*2 *)
  arr2[D-1][E-1] := 123;
  Check("arr2[D-1][E-1]", arr2[D-1][E-1], 123);
  Check("D=4", D, 4);
  Check("E=8", E, 8);
  Out.String("Failed: "); Out.Int(fail, 0); Out.Ln
END RunAllTests;

BEGIN
  RunAllTests
END ImportConstB.
