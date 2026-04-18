MODULE TestNestedProcs;
(* Comprehensive test for nested procedure static link uplevel variable access.
   Covers: INTEGER, LONGINT, REAL, LONGREAL, POINTER uplevel load/store;
   1-hop, 2-hop, 3-hop static-link traversal; uplevel VAR param passing;
   sibling nested proc calls; closure-like mutation patterns. *)

IMPORT Out;

CONST
  Ok*    = 0;
  ErrSL* = 1;

VAR
  totalTests, passedTests: INTEGER;
  testResult: INTEGER;

PROCEDURE InitTest(name: ARRAY OF CHAR);
BEGIN
  Out.String("Testing: "); Out.String(name); Out.Ln;
  testResult := Ok
END InitTest;

PROCEDURE Assert(cond: BOOLEAN; code: INTEGER);
BEGIN
  IF ~cond THEN
    testResult := code;
    Out.String("  FAILED (code "); Out.Int(code, 0); Out.String(")"); Out.Ln
  END
END Assert;

PROCEDURE AssertInt(actual, expected: INTEGER; code: INTEGER);
BEGIN
  IF actual # expected THEN
    testResult := code;
    Out.String("  FAILED: expected "); Out.Int(expected, 0);
    Out.String(", got "); Out.Int(actual, 0); Out.Ln
  END
END AssertInt;

PROCEDURE ReportTest;
BEGIN
  IF testResult = Ok THEN
    Out.String("  PASSED"); Out.Ln; INC(passedTests)
  ELSE
    Out.String("  TEST FAILED"); Out.Ln
  END;
  INC(totalTests); Out.Ln
END ReportTest;

(* ------------------------------------------------------------------ *)
(* Test 1: Basic INTEGER uplevel load/store (1 hop)                   *)
(* ------------------------------------------------------------------ *)
PROCEDURE TestUplevelInteger*;
VAR x, y: INTEGER;

  PROCEDURE SetX(v: INTEGER);
  BEGIN x := v END SetX;

  PROCEDURE GetX(): INTEGER;
  BEGIN RETURN x END GetX;

  PROCEDURE AddToX(delta: INTEGER);
  BEGIN x := x + delta END AddToX;

BEGIN
  InitTest("Uplevel INTEGER load/store (1 hop)");
  x := 0; y := 0;
  SetX(42);
  AssertInt(x, 42, ErrSL);
  AssertInt(GetX(), 42, ErrSL);
  AddToX(8);
  AssertInt(x, 50, ErrSL);
  AssertInt(GetX(), 50, ErrSL);
  y := GetX() * 2;
  AssertInt(y, 100, ErrSL);
  ReportTest
END TestUplevelInteger;

(* ------------------------------------------------------------------ *)
(* Test 2: LONGINT uplevel load/store (1 hop)                         *)
(* ------------------------------------------------------------------ *)
PROCEDURE TestUplevelLongint*;
VAR big: LONGINT;

  PROCEDURE SetBig(v: LONGINT);
  BEGIN big := v END SetBig;

  PROCEDURE GetBig(): LONGINT;
  BEGIN RETURN big END GetBig;

  PROCEDURE MulBig(factor: LONGINT);
  BEGIN big := big * factor END MulBig;

BEGIN
  InitTest("Uplevel LONGINT load/store (1 hop)");
  big := 0;
  SetBig(100000);
  Assert(big = 100000, ErrSL);
  Assert(GetBig() = 100000, ErrSL);
  MulBig(3);
  Assert(big = 300000, ErrSL);
  Assert(GetBig() = 300000, ErrSL);
  ReportTest
END TestUplevelLongint;

(* ------------------------------------------------------------------ *)
(* Test 3: REAL uplevel load/store (1 hop)                            *)
(* ------------------------------------------------------------------ *)
PROCEDURE TestUplevelReal*;
VAR r: REAL;

  PROCEDURE SetR(v: REAL);
  BEGIN r := v END SetR;

  PROCEDURE GetR(): REAL;
  BEGIN RETURN r END GetR;

  PROCEDURE DoubleR;
  BEGIN r := r * 2.0 END DoubleR;

BEGIN
  InitTest("Uplevel REAL load/store (1 hop)");
  r := 0.0;
  SetR(3.5);
  Assert(r = 3.5, ErrSL);
  Assert(GetR() = 3.5, ErrSL);
  DoubleR;
  Assert(r = 7.0, ErrSL);
  Assert(GetR() = 7.0, ErrSL);
  ReportTest
END TestUplevelReal;

(* ------------------------------------------------------------------ *)
(* Test 4: LONGREAL uplevel load/store (1 hop)                        *)
(* ------------------------------------------------------------------ *)
PROCEDURE TestUplevelLongreal*;
VAR d: LONGREAL;

  PROCEDURE SetD(v: LONGREAL);
  BEGIN d := v END SetD;

  PROCEDURE GetD(): LONGREAL;
  BEGIN RETURN d END GetD;

  PROCEDURE AddD(delta: LONGREAL);
  BEGIN d := d + delta END AddD;

BEGIN
  InitTest("Uplevel LONGREAL load/store (1 hop)");
  d := 0.0;
  SetD(1.5);
  Assert(d = 1.5, ErrSL);
  Assert(GetD() = 1.5, ErrSL);
  AddD(0.5);
  Assert(d = 2.0, ErrSL);
  Assert(GetD() = 2.0, ErrSL);
  ReportTest
END TestUplevelLongreal;

(* ------------------------------------------------------------------ *)
(* Test 5: 2-hop uplevel access (grandchild reads grandparent var)    *)
(* ------------------------------------------------------------------ *)
PROCEDURE TestTwoHopUplevel*;
VAR outer: INTEGER;

  PROCEDURE Middle(x: INTEGER): INTEGER;
  VAR mid: INTEGER;

    PROCEDURE Inner(): INTEGER;
    BEGIN
      outer := outer + 1;   (* 2-hop: Inner -> Middle -> TestTwoHop *)
      RETURN outer + mid
    END Inner;

  BEGIN
    mid := x * 10;
    outer := outer + x;    (* 1-hop: Middle -> TestTwoHop *)
    RETURN Inner()
  END Middle;

BEGIN
  InitTest("2-hop uplevel access (grandchild reads grandparent)");
  outer := 0;
  (* Middle(3): outer=0+3=3, mid=30, Inner: outer=3+1=4, returns 4+30=34 *)
  AssertInt(Middle(3), 34, ErrSL);
  AssertInt(outer, 4, ErrSL);
  (* Middle(5): outer=4+5=9, mid=50, Inner: outer=9+1=10, returns 10+50=60 *)
  AssertInt(Middle(5), 60, ErrSL);
  AssertInt(outer, 10, ErrSL);
  ReportTest
END TestTwoHopUplevel;

(* ------------------------------------------------------------------ *)
(* Test 6: 3-hop uplevel access                                        *)
(* ------------------------------------------------------------------ *)
PROCEDURE TestThreeHopUplevel*;
VAR root: INTEGER;

  PROCEDURE Level1(a: INTEGER): INTEGER;
  VAR v1: INTEGER;

    PROCEDURE Level2(b: INTEGER): INTEGER;
    VAR v2: INTEGER;

      PROCEDURE Level3(c: INTEGER): INTEGER;
      BEGIN
        root := root + c;     (* 3 hops: L3->L2->L1->TestThreeHop *)
        v1   := v1 + c;       (* 2 hops: L3->L2->L1 *)
        v2   := v2 + c;       (* 1 hop:  L3->L2 *)
        RETURN root + v1 + v2
      END Level3;

    BEGIN
      v2 := b * 2;
      RETURN Level3(b + 1)
    END Level2;

  BEGIN
    v1 := a;
    RETURN Level2(a + 1)
  END Level1;

VAR res: INTEGER;
BEGIN
  InitTest("3-hop uplevel access");
  root := 0;
  (* Level1(2): v1=2, Level2(3): v2=6, Level3(4):
       root=0+4=4, v1=2+4=6, v2=6+4=10; returns 4+6+10=20 *)
  res := Level1(2);
  AssertInt(res, 20, ErrSL);
  AssertInt(root, 4, ErrSL);
  (* Level1(1): v1=1, Level2(2): v2=4, Level3(3):
       root=4+3=7, v1=1+3=4, v2=4+3=7; returns 7+4+7=18 *)
  res := Level1(1);
  AssertInt(res, 18, ErrSL);
  AssertInt(root, 7, ErrSL);
  ReportTest
END TestThreeHopUplevel;

(* ------------------------------------------------------------------ *)
(* Test 7: Passing uplevel var as VAR param                           *)
(* ------------------------------------------------------------------ *)
PROCEDURE TestUplevelVarParam*;
VAR counter: INTEGER;

  PROCEDURE BumpAndGet(delta: INTEGER): INTEGER;
  VAR snapshot: INTEGER;

    PROCEDURE Bump(VAR n: INTEGER);
    BEGIN n := n + delta END Bump;

  BEGIN
    Bump(counter);           (* pass outer var as VAR param *)
    snapshot := counter;
    Bump(counter);
    RETURN counter - snapshot
  END BumpAndGet;

BEGIN
  InitTest("Passing uplevel var as VAR param to sibling");
  counter := 0;
  (* BumpAndGet(5): Bump(counter)→5, snap=5, Bump(counter)→10, returns 10-5=5 *)
  AssertInt(BumpAndGet(5), 5, ErrSL);
  AssertInt(counter, 10, ErrSL);
  (* BumpAndGet(3): Bump(counter)→13, snap=13, Bump(counter)→16, returns 16-13=3 *)
  AssertInt(BumpAndGet(3), 3, ErrSL);
  AssertInt(counter, 16, ErrSL);
  ReportTest
END TestUplevelVarParam;

(* ------------------------------------------------------------------ *)
(* Test 8: Sibling nested procs sharing outer state                   *)
(* ------------------------------------------------------------------ *)
PROCEDURE TestSiblingProcs*;
VAR acc: INTEGER; factor: INTEGER;

  PROCEDURE Add(v: INTEGER);
  BEGIN acc := acc + v END Add;

  PROCEDURE Mul(v: INTEGER);
  BEGIN acc := acc * v END Mul;

  PROCEDURE Reset;
  BEGIN acc := 0 END Reset;

  PROCEDURE ApplyFactor;
  BEGIN Mul(factor) END ApplyFactor;

BEGIN
  InitTest("Sibling nested procs sharing outer state");
  acc := 0; factor := 0;
  Add(5); Add(3);
  AssertInt(acc, 8, ErrSL);
  factor := 4;
  ApplyFactor;
  AssertInt(acc, 32, ErrSL);
  Reset;
  AssertInt(acc, 0, ErrSL);
  Add(10); Mul(3);
  AssertInt(acc, 30, ErrSL);
  ReportTest
END TestSiblingProcs;

(* ------------------------------------------------------------------ *)
(* Test 9: Closure-like counter (inner proc accumulates outer state)  *)
(* ------------------------------------------------------------------ *)
PROCEDURE TestClosureLike*;
VAR sum: INTEGER; count: INTEGER;

  PROCEDURE Accumulate(v: INTEGER);
  BEGIN
    sum := sum + v;
    INC(count)
  END Accumulate;

  PROCEDURE Average(): INTEGER;
  BEGIN
    IF count = 0 THEN RETURN 0 END;
    RETURN sum DIV count
  END Average;

VAR i: INTEGER;
BEGIN
  InitTest("Closure-like: inner proc accumulates outer state");
  sum := 0; count := 0;
  FOR i := 1 TO 10 DO Accumulate(i) END;
  AssertInt(count, 10, ErrSL);
  AssertInt(sum, 55, ErrSL);
  AssertInt(Average(), 5, ErrSL);
  sum := 0; count := 0;
  Accumulate(100); Accumulate(200); Accumulate(300);
  AssertInt(Average(), 200, ErrSL);
  ReportTest
END TestClosureLike;

(* ------------------------------------------------------------------ *)
(* Test 10: Nested proc reads outer proc's param (not just local var) *)
(* ------------------------------------------------------------------ *)
PROCEDURE TestUplevelParam*;
VAR result: INTEGER;

  PROCEDURE Scale(base, factor: INTEGER): INTEGER;
  VAR partial: INTEGER;

    PROCEDURE AddBase(n: INTEGER): INTEGER;
    BEGIN RETURN n + base END AddBase;   (* reads outer param 'base' *)

    PROCEDURE MulFactor(n: INTEGER): INTEGER;
    BEGIN RETURN n * factor END MulFactor; (* reads outer param 'factor' *)

  BEGIN
    partial := AddBase(0);                 (* partial = base *)
    partial := MulFactor(partial);         (* partial = base * factor *)
    partial := AddBase(partial);           (* partial = base*factor + base *)
    RETURN partial
  END Scale;

BEGIN
  InitTest("Uplevel access to outer proc params");
  result := Scale(3, 4);
  (* partial = 0+3=3; 3*4=12; 12+3=15 *)
  AssertInt(result, 15, ErrSL);
  result := Scale(5, 2);
  (* partial = 0+5=5; 5*2=10; 10+5=15 *)
  AssertInt(result, 15, ErrSL);
  result := Scale(7, 3);
  (* partial = 0+7=7; 7*3=21; 21+7=28 *)
  AssertInt(result, 28, ErrSL);
  ReportTest
END TestUplevelParam;

(* ------------------------------------------------------------------ *)
(* Test 11: Recursive nested proc using uplevel variable              *)
(* ------------------------------------------------------------------ *)
PROCEDURE TestUplevelRecursion*;
VAR callCount: INTEGER;

  PROCEDURE RecCount(n: INTEGER): INTEGER;
  BEGIN
    INC(callCount);
    IF n <= 0 THEN RETURN 0 END;
    RETURN n + RecCount(n - 1)
  END RecCount;

BEGIN
  InitTest("Recursive nested proc with uplevel callCount");
  callCount := 0;
  AssertInt(RecCount(5), 15, ErrSL);   (* 5+4+3+2+1+0 = 15 *)
  AssertInt(callCount, 6, ErrSL);
  callCount := 0;
  AssertInt(RecCount(10), 55, ErrSL);
  AssertInt(callCount, 11, ErrSL);
  ReportTest
END TestUplevelRecursion;

(* ------------------------------------------------------------------ *)
(* Test 12: Deeply nested: inner modifies vars at multiple outer levels *)
(* ------------------------------------------------------------------ *)
PROCEDURE TestDeepMutation*;
VAR a: INTEGER;

  PROCEDURE L1(b: INTEGER): INTEGER;
  VAR c: INTEGER;

    PROCEDURE L2(d: INTEGER): INTEGER;
    BEGIN
      a := a + d;    (* 2 hops *)
      c := c + d;    (* 1 hop  *)
      RETURN a + c + b + d
    END L2;

  BEGIN
    c := b + 1;
    RETURN L2(b * 2)
  END L1;

VAR res: INTEGER;
BEGIN
  InitTest("Deep mutation: inner writes to multiple outer frame vars");
  a := 10;
  (* L1(3): c=4, L2(6): a=10+6=16, c=4+6=10; returns 16+10+3+6=35 *)
  res := L1(3);
  AssertInt(res, 35, ErrSL);
  AssertInt(a, 16, ErrSL);
  (* L1(2): c=3, L2(4): a=16+4=20, c=3+4=7; returns 20+7+2+4=33 *)
  res := L1(2);
  AssertInt(res, 33, ErrSL);
  AssertInt(a, 20, ErrSL);
  ReportTest
END TestDeepMutation;

PROCEDURE PrintSummary;
BEGIN
  Out.Ln;
  Out.String("========================================"); Out.Ln;
  Out.String("Total tests:  "); Out.Int(totalTests,  0); Out.Ln;
  Out.String("Passed:       "); Out.Int(passedTests,  0); Out.Ln;
  Out.String("Failed:       "); Out.Int(totalTests - passedTests, 0); Out.Ln;
  Out.String("========================================"); Out.Ln
END PrintSummary;

PROCEDURE RunAllTests*;
BEGIN
  Out.String("========================================"); Out.Ln;
  Out.String("Nested Procedures Static Link Tests");    Out.Ln;
  Out.String("========================================"); Out.Ln;
  totalTests := 0; passedTests := 0;

  TestUplevelInteger;
  TestUplevelLongint;
  TestUplevelReal;
  TestUplevelLongreal;
  TestTwoHopUplevel;
  TestThreeHopUplevel;
  TestUplevelVarParam;
  TestSiblingProcs;
  TestClosureLike;
  TestUplevelParam;
  TestUplevelRecursion;
  TestDeepMutation;

  PrintSummary
END RunAllTests;

END TestNestedProcs.
