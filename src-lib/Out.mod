(*$R-*)
MODULE Out;

CONST
  STDOUT = 1;

PROCEDURE Open*;
BEGIN
END Open;

PROCEDURE Char*(ch: CHAR);
BEGIN
  SYSTEM.DOSCHAR(ch)
END Char;

PROCEDURE Ln*;
BEGIN
  Char(0DX); Char(0AX)
END Ln;

PROCEDURE String*(s: ARRAY OF CHAR);
BEGIN
  SYSTEM.BLOCKWRITE(STDOUT, s, SYSTEM.LENGTH(s))
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

END Out.
