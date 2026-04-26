(*$R+*)
MODULE TestBounds;
(* Runtime tests for array bounds checking ($R+ mode).
   In-bounds accesses must work correctly.
   Out-of-bounds accesses halt with a non-zero exit code — those cases
   are tested separately from test_src.sh as sub-processes. *)

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

PROCEDURE SumArray(a: ARRAY OF INTEGER): INTEGER;
    VAR i, s: INTEGER;
BEGIN
    s := 0;
    FOR i := 0 TO LEN(a) - 1 DO s := s + a[i] END;
    RETURN s
END SumArray;

PROCEDURE RunAllTests*;
    VAR
        a: ARRAY 5 OF INTEGER;
        i, s: INTEGER;
BEGIN
    passed := 0; failed := 0;

    (* in-bounds fixed array read/write *)
    FOR i := 0 TO 4 DO a[i] := i * 10 END;
    Check("a[0]=0",  a[0] = 0);
    Check("a[4]=40", a[4] = 40);

    (* sum via open-array param *)
    s := SumArray(a);
    Check("SumArray=100", s = 100);

    (* boundary values are accessible *)
    a[0] := 999;
    Check("a[0] write/read", a[0] = 999);
    a[4] := -1;
    Check("a[4] write/read", a[4] = -1);

    (* variable index at boundary *)
    i := 0; Check("a[i=0]", a[i] = 999);
    i := 4; Check("a[i=4]", a[i] = -1);

    Out.String("Failed: "); Out.Int(failed, 0); Out.Ln;
    Out.String("Total:  "); Out.Int(passed + failed, 0); Out.Ln
END RunAllTests;

END TestBounds.
