MODULE TestImplicitCoerce;
IMPORT Out;

(* Implicit INTEGER -> REAL/LONGREAL coercion in binary expressions *)

PROCEDURE Check(name: ARRAY OF CHAR; ok: BOOLEAN);
BEGIN
    IF ok THEN Out.String("PASS: ") ELSE Out.String("FAIL: ") END;
    Out.String(name); Out.Ln
END Check;

PROCEDURE TestArith;
VAR
    n  : INTEGER;
    r  : REAL;
    lr : LONGREAL;
BEGIN
    n := 3;

    (* REAL * INTEGER  — LHS is REAL, RHS is integer var *)
    r := 1.5;
    Check("1.5 * n  = 4.5",  1.5 * n = 4.5);
    Check("r * n    = 4.5",  r * n   = 4.5);

    (* INTEGER * REAL  — LHS is integer var, RHS is REAL *)
    Check("n * 1.5  = 4.5",  n * 1.5 = 4.5);
    Check("n * r    = 4.5",  n * r   = 4.5);

    (* REAL / INTEGER *)
    Check("9.0 / n  = 3.0",  9.0 / n = 3.0);
    Check("r / 1    = 1.5",  r / 1   = 1.5);  (* r=1.5, /1 *)

    (* INTEGER / REAL (promotes LHS) *)
    Check("3 / r    = 2.0",  3 / r   = 2.0);  (* r=1.5, 3/1.5=2 *)

    (* REAL + INTEGER *)
    Check("r + 1    = 2.5",  r + 1   = 2.5);
    Check("r + n    = 4.5",  r + n   = 4.5);

    (* INTEGER + REAL *)
    Check("1 + r    = 2.5",  1 + r   = 2.5);
    Check("n + r    = 4.5",  n + r   = 4.5);

    (* REAL - INTEGER *)
    Check("r - 1    = 0.5",  r - 1   = 0.5);

    (* INTEGER - REAL *)
    Check("2 - r    = 0.5",  2 - r   = 0.5);

    (* integer constant zero — r < 0, r = 0 *)
    r := -0.5;
    Check("r < 0  (r=-0.5)",  r < 0);
    r := 0.0;
    Check("r = 0  (r=0.0)",   r = 0);
    r := 0.5;
    Check("~(r < 0) (r=0.5)", ~(r < 0));
    Check("~(r = 0) (r=0.5)", ~(r = 0));

    (* REAL > integer, REAL >= integer, REAL # integer *)
    r := 3.0;
    Check("r > 2",   r > 2);
    Check("r >= 3",  r >= 3);
    Check("r # 2",   r # 2);

    (* integer relops with REAL on RHS *)
    n := 2;
    Check("n < r    (2 < 3.0)", n < r);
    Check("n # r    (2 # 3.0)", n # r)
END TestArith;

PROCEDURE TestLongreal;
VAR
    n  : INTEGER;
    lr : LONGREAL;
BEGIN
    n := 4;
    lr := 2.5;

    Check("lr * n   = 10.0", lr * n   = 10.0);
    Check("n * lr   = 10.0", n * lr   = 10.0);
    Check("lr + n   = 6.5",  lr + n   = 6.5);
    Check("n + lr   = 6.5",  n + lr   = 6.5);
    Check("lr - n   = -1.5", lr - n   = -1.5);
    Check("lr / 2   = 1.25", lr / 2   = 1.25);
    Check("lr < 0   FALSE",  ~(lr < 0));
    lr := -1.0;
    Check("lr < 0   TRUE",   lr < 0);
    lr := 0.0;
    Check("lr = 0   TRUE",   lr = 0)
END TestLongreal;

PROCEDURE TestLiteral;
(* Constant-integer literal on either side *)
VAR r: REAL;
BEGIN
    r := 2.0;
    Check("2.0 * 3  = 6.0",  2.0 * 3 = 6.0);
    Check("3 * 2.0  = 6.0",  3 * 2.0 = 6.0);
    Check("1.0 + 2  = 3.0",  1.0 + 2 = 3.0);
    Check("2 + 1.0  = 3.0",  2 + 1.0 = 3.0);
    Check("3.0 - 1  = 2.0",  3.0 - 1 = 2.0);
    Check("3 - 1.0  = 2.0",  3 - 1.0 = 2.0);
    Check("6.0 / 2  = 3.0",  6.0 / 2 = 3.0);
    Check("r < 3",           r < 3);
    Check("r = 2",           r = 2);
    Check("r > 1",           r > 1);
    Check("r # 3",           r # 3);
    Check("3 > r",           3 > r);
    Check("1 < r",           1 < r)
END TestLiteral;

BEGIN
    TestArith;
    TestLongreal;
    TestLiteral;
    Out.String("Done."); Out.Ln
END TestImplicitCoerce.
