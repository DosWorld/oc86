MODULE TestIsInstance;
IMPORT Out;
TYPE Animal = RECORD END;
TYPE Dog = RECORD (Animal) END;
VAR pd: POINTER TO Dog;
    pa: POINTER TO Animal;
    pass, fail: INTEGER;

PROCEDURE Check(name: ARRAY OF CHAR; got, expected: INTEGER);
BEGIN
  IF got = expected THEN
    Out.String("  PASS "); Out.String(name); Out.Ln; INC(pass)
  ELSE
    Out.String("  FAIL "); Out.String(name); Out.Ln; INC(fail)
  END
END Check;

PROCEDURE RunAllTests*;
VAR x: INTEGER;
BEGIN
  pass := 0; fail := 0;
  Out.String("IsTest"); Out.Ln;
  (* compile-time fold: pd IS Animal -> true *)
  IF pd IS Animal THEN x := 1 ELSE x := 0 END;
  Check("pd IS Animal", x, 1);
  (* runtime: NEW pd; pa := pd; pa IS Dog -> true *)
  NEW(pd);
  pa := pd;
  IF pa IS Dog THEN x := 1 ELSE x := 0 END;
  Check("pa IS Dog", x, 1);
  Out.String("Failed: "); Out.Int(fail, 0); Out.Ln
END RunAllTests;

END TestIsInstance.
