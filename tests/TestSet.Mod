MODULE TestSet;

IMPORT Out;

VAR
  totalTests, passedTests: INTEGER;

PROCEDURE Assert(cond: BOOLEAN; msg: ARRAY OF CHAR);
BEGIN
  INC(totalTests);
  IF cond THEN
    INC(passedTests)
  ELSE
    Out.String("  FAILED: "); Out.String(msg); Out.Ln
  END
END Assert;

PROCEDURE AssertInt(actual, expected: INTEGER; msg: ARRAY OF CHAR);
BEGIN
  INC(totalTests);
  IF actual = expected THEN
    INC(passedTests)
  ELSE
    Out.String("  FAILED: "); Out.String(msg);
    Out.String(" expected "); Out.Int(expected, 0);
    Out.String(" got "); Out.Int(actual, 0); Out.Ln
  END
END AssertInt;

PROCEDURE TestLiterals;
VAR s: SET;
BEGIN
  Out.String("Testing: SET literals"); Out.Ln;
  s := {};
  AssertInt(ORD(s = {}), 1, "empty set = {}");
  s := {0};
  AssertInt(ORD(0 IN s), 1, "{0}: bit 0 set");
  AssertInt(ORD(1 IN s), 0, "{0}: bit 1 not set");
  s := {3, 5, 7};
  AssertInt(ORD(3 IN s), 1, "{3,5,7}: bit 3 set");
  AssertInt(ORD(5 IN s), 1, "{3,5,7}: bit 5 set");
  AssertInt(ORD(7 IN s), 1, "{3,5,7}: bit 7 set");
  AssertInt(ORD(4 IN s), 0, "{3,5,7}: bit 4 not set");
  s := {0..7};
  AssertInt(ORD(0 IN s), 1, "{0..7}: bit 0 set");
  AssertInt(ORD(7 IN s), 1, "{0..7}: bit 7 set");
  AssertInt(ORD(8 IN s), 0, "{0..7}: bit 8 not set")
END TestLiterals;

PROCEDURE TestOperators;
VAR a, b, c: SET;
BEGIN
  Out.String("Testing: SET operators"); Out.Ln;
  a := {1, 3, 5};
  b := {3, 5, 7};
  (* union + *)
  c := a + b;
  AssertInt(ORD(1 IN c), 1, "union: bit 1");
  AssertInt(ORD(3 IN c), 1, "union: bit 3");
  AssertInt(ORD(5 IN c), 1, "union: bit 5");
  AssertInt(ORD(7 IN c), 1, "union: bit 7");
  AssertInt(ORD(2 IN c), 0, "union: bit 2 not set");
  (* intersection * *)
  c := a * b;
  AssertInt(ORD(3 IN c), 1, "intersect: bit 3");
  AssertInt(ORD(5 IN c), 1, "intersect: bit 5");
  AssertInt(ORD(1 IN c), 0, "intersect: bit 1 not set");
  AssertInt(ORD(7 IN c), 0, "intersect: bit 7 not set");
  (* difference - *)
  c := a - b;
  AssertInt(ORD(1 IN c), 1, "diff: bit 1");
  AssertInt(ORD(3 IN c), 0, "diff: bit 3 removed");
  AssertInt(ORD(5 IN c), 0, "diff: bit 5 removed");
  (* symmetric difference / *)
  c := a / b;
  AssertInt(ORD(1 IN c), 1, "symdiff: bit 1");
  AssertInt(ORD(7 IN c), 1, "symdiff: bit 7");
  AssertInt(ORD(3 IN c), 0, "symdiff: bit 3 cancelled");
  AssertInt(ORD(5 IN c), 0, "symdiff: bit 5 cancelled")
END TestOperators;

PROCEDURE TestMembership;
VAR s: SET; i: INTEGER;
BEGIN
  Out.String("Testing: IN operator"); Out.Ln;
  s := {2, 4, 6, 8};
  i := 4;
  AssertInt(ORD(i IN s), 1, "IN: var index 4 in {2,4,6,8}");
  i := 3;
  AssertInt(ORD(i IN s), 0, "IN: var index 3 not in {2,4,6,8}");
  AssertInt(ORD(0 IN s), 0, "IN: literal 0 not in set");
  AssertInt(ORD(8 IN s), 1, "IN: literal 8 in set")
END TestMembership;

PROCEDURE RunAllTests*;
BEGIN
  totalTests := 0; passedTests := 0;
  TestLiterals;
  TestOperators;
  TestMembership;
  Out.String("Passed: "); Out.Int(passedTests, 0); Out.Ln;
  Out.String("Failed: "); Out.Int(totalTests - passedTests, 0); Out.Ln
END RunAllTests;

END TestSet.
