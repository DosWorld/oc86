MODULE IO;

CONST
  STDIN*  = 0;
  STDOUT* = 1;
  STDERR* = 2;

TYPE
  Rider* = RECORD
    handle*: INTEGER;
    pos*:    LONGINT;
    eof*:    BOOLEAN
  END;

PROCEDURE OpenHandle*(VAR r: Rider; h: INTEGER);
BEGIN
  r.handle := h;
  r.pos    := 0;
  r.eof    := FALSE
END OpenHandle;

PROCEDURE StdOut*(VAR r: Rider);
BEGIN
  OpenHandle(r, STDOUT)
END StdOut;

PROCEDURE StdErr*(VAR r: Rider);
BEGIN
  OpenHandle(r, STDERR)
END StdErr;

PROCEDURE ReadChar*(VAR r: Rider; VAR ch: CHAR);
BEGIN
  IF SYSTEM.BLOCKREAD(r.handle, ch, 1) = 0 THEN
    r.eof := TRUE;
    ch := 0X
  ELSE
    INC(r.pos)
  END
END ReadChar;

PROCEDURE WriteChar*(VAR r: Rider; ch: CHAR);
BEGIN
  IF SYSTEM.BLOCKWRITE(r.handle, ch, 1) = 1 THEN INC(r.pos) END
END WriteChar;

PROCEDURE WriteStr*(VAR r: Rider; s: ARRAY OF CHAR);
BEGIN
  INC(r.pos, SYSTEM.BLOCKWRITE(r.handle, s, SYSTEM.LENGTH(s)))
END WriteStr;

PROCEDURE WriteInt*(VAR r: Rider; n: LONGINT);
  VAR buf: ARRAY 12 OF CHAR;
      i, k: INTEGER;
      neg: BOOLEAN;
      rem: LONGINT;
BEGIN
  IF n = 0 THEN
    WriteChar(r, "0");
    RETURN
  END;
  neg := n < 0;
  IF neg THEN n := -n END;
  i := 0;
  WHILE n > 0 DO
    rem := n MOD 10;
    buf[i] := CHR(ORD("0") + INTEGER(rem));
    n := n DIV 10;
    INC(i)
  END;
  IF neg THEN WriteChar(r, "-") END;
  k := i;
  WHILE k > 0 DO
    DEC(k);
    WriteChar(r, buf[k])
  END
END WriteInt;

END IO.
