MODULE TestTypeTag;
(* Runtime tests for IS type-tag type test.
   Tests compile-time folds and runtime tag checks. *)

IMPORT Out;

TYPE
  Animal   = RECORD legs: INTEGER END;
  Dog      = RECORD (Animal) tricks: INTEGER END;
  Cat      = RECORD (Animal) indoor: BOOLEAN END;
  PAnimal  = POINTER TO Animal;
  PDog     = POINTER TO Dog;
  PCat     = POINTER TO Cat;

VAR
  passed, failed: INTEGER;

PROCEDURE Check(name: ARRAY OF CHAR; cond: BOOLEAN);
BEGIN
  IF cond THEN INC(passed)
  ELSE
    Out.String("FAIL: "); Out.String(name); Out.Ln;
    INC(failed)
  END
END Check;

PROCEDURE RunAllTests*;
VAR
  pd: PDog;
  pc: PCat;
  pa: PAnimal;
  b:  BOOLEAN;
BEGIN
  passed := 0; failed := 0;

  (* Compile-time: static type already extends RHS → TRUE *)
  NEW(pd); pd^.legs := 4; pd^.tricks := 3;
  b := pd IS Animal;
  Check("PDog IS Animal (TRUE)",  b);

  b := pd IS Dog;
  Check("PDog IS Dog (TRUE)", b);

  (* Runtime: pa → Dog IS Dog → TRUE *)
  pa := pd;
  b := pa IS Dog;
  Check("pa→Dog IS Dog (TRUE)", b);

  (* Runtime: pa → Dog IS Cat → FALSE *)
  b := pa IS Cat;
  Check("pa→Dog IS Cat (FALSE)", ~b);

  (* Runtime: pa → new Animal IS Dog → FALSE *)
  NEW(pa);
  pa^.legs := 2;
  b := pa IS Dog;
  Check("pa→Animal IS Dog (FALSE)", ~b);

  (* Runtime: pa → new Cat IS Cat → TRUE *)
  NEW(pc); pc^.legs := 4; pc^.indoor := TRUE;
  pa := pc;
  b := pa IS Cat;
  Check("pa→Cat IS Cat (TRUE)", b);

  (* Runtime: pa → Cat IS Dog → FALSE *)
  b := pa IS Dog;
  Check("pa→Cat IS Dog (FALSE)", ~b);

  Out.String("Failed: "); Out.Int(failed, 0); Out.Ln;
  Out.String("Total:  "); Out.Int(passed + failed, 0); Out.Ln
END RunAllTests;

END TestTypeTag.
