MODULE TestSynonyms;
IMPORT Out;

(* Verify FLOAT(x) = REAL(x) and ENTIER(x) = FLOOR(x) *)

VAR
    ri  : INTEGER;
    rl  : LONGINT;
    rr  : REAL;
    rrl : REAL;

BEGIN
    (* FLOAT(INTEGER) -- same result as REAL(INTEGER) *)
    rr  := REAL(3);
    rrl := FLOAT(3);
    IF rr = rrl THEN Out.String("PASS: FLOAT(INTEGER) = REAL(INTEGER)") ELSE Out.String("FAIL: FLOAT(INTEGER) != REAL(INTEGER)") END;
    Out.Ln;

    (* FLOAT(LONGINT) *)
    rr  := REAL(100000);
    rrl := FLOAT(100000);
    IF rr = rrl THEN Out.String("PASS: FLOAT(LONGINT) = REAL(LONGINT)") ELSE Out.String("FAIL: FLOAT(LONGINT) != REAL(LONGINT)") END;
    Out.Ln;

    (* ENTIER truncates toward -infinity (same as FLOOR) *)
    ri := FLOOR(3.7);
    IF ri = 3 THEN Out.String("PASS: FLOOR(3.7) = 3") ELSE Out.String("FAIL: FLOOR(3.7)") END;
    Out.Ln;

    ri := ENTIER(3.7);
    IF ri = 3 THEN Out.String("PASS: ENTIER(3.7) = 3") ELSE Out.String("FAIL: ENTIER(3.7)") END;
    Out.Ln;

    ri := ENTIER(-2.1);
    IF ri = -3 THEN Out.String("PASS: ENTIER(-2.1) = -3") ELSE Out.String("FAIL: ENTIER(-2.1)") END;
    Out.Ln;

    ri := FLOOR(-2.1);
    IF ri = -3 THEN Out.String("PASS: FLOOR(-2.1) = -3") ELSE Out.String("FAIL: FLOOR(-2.1)") END;
    Out.Ln;

    (* ENTIER and FLOOR produce identical results *)
    rr := 7.9;
    IF ENTIER(rr) = FLOOR(rr) THEN Out.String("PASS: ENTIER(x) = FLOOR(x) for x=7.9") ELSE Out.String("FAIL: ENTIER != FLOOR") END;
    Out.Ln;

    rr := -0.5;
    IF ENTIER(rr) = FLOOR(rr) THEN Out.String("PASS: ENTIER(x) = FLOOR(x) for x=-0.5") ELSE Out.String("FAIL: ENTIER != FLOOR") END;
    Out.Ln;

    (* FLOAT and REAL produce identical results for a variable *)
    rl := 42;
    IF FLOAT(rl) = REAL(rl) THEN Out.String("PASS: FLOAT(LONGINT var) = REAL(LONGINT var)") ELSE Out.String("FAIL: FLOAT(LONGINT var) != REAL") END;
    Out.Ln;

    Out.String("Done."); Out.Ln
END TestSynonyms.
