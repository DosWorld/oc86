MODULE Out;

PROCEDURE Open*;
BEGIN
END Open;

PROCEDURE Char*(ch: CHAR);
VAR
  r: SYSTEM.Registers;
BEGIN
  r.AX := 0200H;
  r.DX := ORD(ch);
  SYSTEM.Intr(21H, r)
END Char;

PROCEDURE Ln*;
BEGIN
  Char(0DX); Char(0AX)
END Ln;

PROCEDURE String*(s: ARRAY OF CHAR);
VAR i: INTEGER;
BEGIN
  i := 0;
  WHILE s[i] # 0X DO
    Char(s[i]);
    INC(i)
  END
END String;

PROCEDURE Int*(l: LONGINT; w: INTEGER);
VAR
  n: INTEGER;
  neg: BOOLEAN;
  s: ARRAY 12 OF INTEGER;
  r: LONGINT;
BEGIN
  IF l = 0 THEN
    WHILE w > 1 DO Char(" "); DEC(w) END;
    Char("0")
  ELSE
    IF l < 0 THEN
      neg := TRUE;
      DEC(w);
      l := -l
    ELSE
      neg := FALSE
    END;
    n := 0;
    WHILE l > 0 DO
      r := l MOD 10;
      s[n] := INTEGER(r);
      l := l DIV 10;
      INC(n); DEC(w)
    END;
    WHILE w > 0 DO Char(" "); DEC(w) END;
    IF neg THEN Char("-") END;
    WHILE n > 0 DO
      DEC(n);
      Char(CHR(ORD("0") + s[n]))
    END
  END
END Int;

PROCEDURE Hex*(i: INTEGER; w: INTEGER);
VAR
  n, d: INTEGER;
  buf: ARRAY 4 OF CHAR;
BEGIN
  n := 0;
  WHILE n < 4 DO
    d := i MOD 16;
    IF d < 10 THEN buf[n] := CHR(ORD("0") + d)
    ELSE buf[n] := CHR(ORD("A") + d - 10)
    END;
    i := i DIV 16;
    INC(n)
  END;
  WHILE w > 4 DO Char(" "); DEC(w) END;
  WHILE n > 0 DO
    DEC(n);
    Char(buf[n])
  END
END Hex;

PROCEDURE Real*(x: REAL; w: INTEGER);
(* Print a REAL value in simple decimal format.
   Uses sign + integer part (via FLOOR) + 6 fractional digits.
   Limited to values whose integer part fits in INTEGER (-32768..32767). *)
VAR
  neg: BOOLEAN;
  ipart: INTEGER;
  frac: REAL;
  digits: ARRAY 7 OF INTEGER;
  i: INTEGER;
  d: INTEGER;
  needed: INTEGER;
BEGIN
  neg := FALSE;
  IF x < 0.0 THEN
    neg := TRUE;
    x := -x
  END;
  ipart := FLOOR(x);
  frac := x - REAL(ipart);  (* fractional part *)
  (* Collect 6 fractional digits *)
  i := 0;
  WHILE i < 6 DO
    frac := frac * 10.0;
    d := FLOOR(frac);
    digits[i] := d;
    frac := frac - REAL(d);
    INC(i)
  END;
  (* Count chars needed: sign + ipart + '.' + 6 digits *)
  needed := 8;  (* '.', 6 digits, at least 1 for ipart *)
  IF neg THEN INC(needed) END;
  IF ipart >= 10000 THEN INC(needed, 4)
  ELSIF ipart >= 1000 THEN INC(needed, 3)
  ELSIF ipart >= 100 THEN INC(needed, 2)
  ELSIF ipart >= 10 THEN INC(needed)
  END;
  (* Pad with spaces *)
  WHILE w > needed DO Char(" "); DEC(w) END;
  IF neg THEN Char("-") END;
  Int(ipart, 0);
  Char(".");
  i := 0;
  WHILE i < 6 DO
    Char(CHR(ORD("0") + digits[i]));
    INC(i)
  END
END Real;

PROCEDURE LongReal*(x: LONGREAL; w: INTEGER);
(* Print a LONGREAL value — delegates to REAL for now. *)
VAR r: REAL;
BEGIN
  r := REAL(x);
  Real(r, w)
END LongReal;

END Out.
