MODULE TestForward;
IMPORT Out;

VAR failed: INTEGER;

PROCEDURE Fail(n: INTEGER);
BEGIN Out.String("Failed test "); Out.Int(n, 0); Out.Ln; INC(failed) END Fail;

(* Mutual recursion via forward declarations *)
PROCEDURE Even(n: INTEGER): BOOLEAN; FORWARD;
PROCEDURE Odd(n: INTEGER): BOOLEAN; FORWARD;

PROCEDURE Even;
BEGIN
  IF n = 0 THEN RETURN TRUE END;
  RETURN Odd(n - 1)
END Even;

PROCEDURE Odd;
BEGIN
  IF n = 0 THEN RETURN FALSE END;
  RETURN Even(n - 1)
END Odd;

(* Forward with FAR modifier *)
PROCEDURE Compute(a, b: INTEGER): INTEGER; FAR; FORWARD;

PROCEDURE Compute;
BEGIN
  RETURN a * b + a
END Compute;

(* Forward with no modifier — called before definition *)
PROCEDURE Helper(x: INTEGER): INTEGER; FORWARD;

PROCEDURE Helper;
BEGIN
  RETURN x + 1
END Helper;

PROCEDURE RunAllTests*;
VAR r: INTEGER;
BEGIN
  failed := 0;

  (* Test 1: mutual recursion via forward — Even(4) = TRUE *)
  IF ~Even(4) THEN Fail(1) END;

  (* Test 2: mutual recursion via forward — Even(3) = FALSE *)
  IF Even(3) THEN Fail(2) END;

  (* Test 3: Odd(5) = TRUE *)
  IF ~Odd(5) THEN Fail(3) END;

  (* Test 4: Odd(6) = FALSE *)
  IF Odd(6) THEN Fail(4) END;

  (* Test 5: Even(0) = TRUE *)
  IF ~Even(0) THEN Fail(5) END;

  (* Test 6: Odd(0) = FALSE *)
  IF Odd(0) THEN Fail(6) END;

  (* Test 7: forward FAR proc — Compute(3, 4) = 3*4 + 3 = 15 *)
  r := Compute(3, 4);
  IF r # 15 THEN Fail(7) END;

  (* Test 8: forward proc called before definition — Helper(10) = 11 *)
  r := Helper(10);
  IF r # 11 THEN Fail(8) END;

  Out.String("Failed: "); Out.Int(failed, 0); Out.Ln
END RunAllTests;

END TestForward.
