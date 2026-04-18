MODULE TestSystem;
(* Runtime tests for SYSTEM module: constants, Registers struct,
   SYSTEM.Intr (INT 21h), and NEW/DISPOSE with SYSTEM.ADDRESS. *)

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
    VAR
        r:    SYSTEM.Registers;
        addr: SYSTEM.ADDRESS;
        n:    INTEGER;
        x:    INTEGER;
BEGIN
    passed := 0; failed := 0;

    (* SYSTEM constants *)
    x := SYSTEM.FZero;
    Check("SYSTEM.FZero = 64",  x = 64);
    x := SYSTEM.FSign;
    Check("SYSTEM.FSign = 128", x = 128);
    x := SYSTEM.FCarry;
    Check("SYSTEM.FCarry = 1",  x = 1);

    (* SYSTEM.Registers struct: field assignment round-trips *)
    r.AX := 42;
    Check("Registers.AX assignment", r.AX = 42);
    r.BX := 1000;
    Check("Registers.BX assignment", r.BX = 1000);
    r.AX := 0;
    Check("Registers.AX zero", r.AX = 0);

    (* SYSTEM.Intr: INT 21h AH=02 (write char to stdout) — writes '*' *)
    (* Verifies Intr executes without crashing; output is a visible side-effect *)
    r.AX := 0200H;  (* AH=02 write char *)
    r.DX := 42;     (* DL='*' *)
    SYSTEM.Intr(21H, r);
    Out.Ln;
    Check("SYSTEM.Intr no crash", TRUE);

    (* NEW(addr, n) / DISPOSE(addr) with SYSTEM.ADDRESS *)
    n := 8;
    NEW(addr, n);
    Check("NEW(addr,8) non-NIL", addr # NIL);
    DISPOSE(addr);
    Check("DISPOSE(addr) no crash", TRUE);

    (* Multiple NEW/DISPOSE cycles — heap remains functional *)
    n := 4;
    NEW(addr, n); Check("NEW addr cycle 1", addr # NIL); DISPOSE(addr);
    NEW(addr, n); Check("NEW addr cycle 2", addr # NIL); DISPOSE(addr);

    Out.String("Failed: "); Out.Int(failed, 0); Out.Ln;
    Out.String("Total:  "); Out.Int(passed + failed, 0); Out.Ln
END RunAllTests;

END TestSystem.
