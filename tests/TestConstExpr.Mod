MODULE TestConstExpr;
IMPORT Out;

CONST
  A = 10;
  B = 3;
  Sum    = A + B;          (* 13 *)
  Diff   = A - B;          (* 7 *)
  Prod   = A * B;          (* 30 *)
  Quot   = A DIV B;        (* 3 — floor(10/3) *)
  Rem    = A MOD B;        (* 1 — 10 - 3*3 *)
  Neg    = -A;             (* -10 *)
  NegDiv = -7 DIV 2;       (* -3 — Oberon: -(7 DIV 2) = -3 *)
  NegMod = -7 MOD 2;       (* -1 — Oberon: -(7 MOD 2) = -1 *)
  FlrDiv = (-7) DIV 2;     (* -4 — floor(-7/2) = -4 *)
  FlrMod = (-7) MOD 2;     (* 1  — (-7) - 2*(-4) = 1 *)
  Paren  = (A + B) * 2;    (* 26 *)
  Chain  = A + B * 2;      (* 16 — operator precedence: B*2=6, A+6=16 *)
  Big    = 1000 * 200;     (* 200000 — exceeds 16-bit, becomes LONGINT *)
  Zero   = A - A;          (* 0 *)
  MaxDiv = 32767 DIV 1;    (* 32767 *)

VAR pass, fail: INTEGER;

PROCEDURE Check(name: ARRAY OF CHAR; got, expected: INTEGER);
BEGIN
  IF got = expected THEN
    Out.String("  PASS "); Out.String(name); Out.Ln;
    INC(pass)
  ELSE
    Out.String("  FAIL "); Out.String(name);
    Out.String(" expected="); Out.Int(expected, 0);
    Out.String(" got="); Out.Int(got, 0); Out.Ln;
    INC(fail)
  END
END Check;

PROCEDURE CheckL(name: ARRAY OF CHAR; got, expected: LONGINT);
BEGIN
  IF got = expected THEN
    Out.String("  PASS "); Out.String(name); Out.Ln;
    INC(pass)
  ELSE
    Out.String("  FAIL "); Out.String(name); Out.Ln;
    INC(fail)
  END
END CheckL;

PROCEDURE RunAllTests*;
BEGIN
  pass := 0; fail := 0;
  Out.String("TestConstExpr"); Out.Ln;
  Check("A+B=13",       Sum,    13);
  Check("A-B=7",        Diff,   7);
  Check("A*B=30",       Prod,   30);
  Check("A DIV B=3",    Quot,   3);
  Check("A MOD B=1",    Rem,    1);
  Check("-A=-10",         Neg,    -10);
  Check("-7 DIV 2=-3",   NegDiv, -3);
  Check("-7 MOD 2=-1",   NegMod, -1);
  Check("(-7) DIV 2=-4", FlrDiv, -4);
  Check("(-7) MOD 2=1",  FlrMod, 1);
  Check("(A+B)*2=26",   Paren,  26);
  Check("A+B*2=16",     Chain,  16);
  Check("A-A=0",        Zero,   0);
  Check("32767 DIV 1",  MaxDiv, 32767);
  CheckL("1000*200=200000", Big, 200000);
  Out.String("Failed: "); Out.Int(fail, 0); Out.Ln
END RunAllTests;

END TestConstExpr.
