MODULE TestAssert;
(* Runtime tests for ASSERT built-in.
   ASSERT(TRUE)  must not halt the program.
   ASSERT(FALSE) halts with a non-zero exit code — tested as a separate
   sub-process from test_src.sh; only the TRUE cases live here. *)

IMPORT Out;

VAR
    passed, failed: INTEGER;

PROCEDURE Check(name: ARRAY OF CHAR; cond: BOOLEAN);
BEGIN
    INC(passed);
    IF ~cond THEN
        Out.String("FAIL: "); Out.String(name); Out.Ln;
        INC(failed); DEC(passed)
    END
END Check;

PROCEDURE RunAllTests*;
    VAR x, y: INTEGER;
BEGIN
    passed := 0; failed := 0;

    (* ASSERT(TRUE) must be a no-op — program continues *)
    ASSERT(TRUE);
    Check("ASSERT(TRUE) continues", TRUE);

    (* ASSERT with true arithmetic condition *)
    x := 5; y := 3;
    ASSERT(x > y);
    Check("ASSERT(x>y) continues", TRUE);

    (* ASSERT with explicit error code, true condition *)
    ASSERT(x = 5, 99);
    Check("ASSERT(x=5, 99) continues", TRUE);

    (* Nested ASSERT in expression context *)
    x := 42;
    ASSERT(x # 0);
    Check("ASSERT(x#0) continues", TRUE);

    (* ASSERT after arithmetic *)
    x := 10; y := x * 2;
    ASSERT(y = 20);
    Check("ASSERT(y=20) continues", TRUE);

    Out.String("Failed: "); Out.Int(failed, 0); Out.Ln;
    Out.String("Total:  "); Out.Int(passed + failed, 0); Out.Ln
END RunAllTests;

END TestAssert.
