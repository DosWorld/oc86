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
  VAR regs: SYSTEM.Registers; buf: CHAR;
BEGIN
  buf := 0X;
  regs.AX := 3F00H;
  regs.BX := r.handle;
  regs.CX := 1;
  regs.DS := SYSTEM.SEG(buf);
  regs.DX := SYSTEM.OFS(buf);
  regs.Flags := 0;
  SYSTEM.Intr(21H, regs);
  IF (SYSTEM.AND(regs.Flags, SYSTEM.FCarry) # 0) OR (regs.AX = 0) THEN
    r.eof := TRUE;
    ch := 0X
  ELSE
    ch := buf;
    INC(r.pos)
  END
END ReadChar;

PROCEDURE WriteChar*(VAR r: Rider; ch: CHAR);
  VAR regs: SYSTEM.Registers;
BEGIN
  regs.AX := 4000H;
  regs.BX := r.handle;
  regs.CX := 1;
  regs.DS := SYSTEM.SEG(ch);
  regs.DX := SYSTEM.OFS(ch);
  regs.Flags := 0;
  SYSTEM.Intr(21H, regs);
  IF SYSTEM.AND(regs.Flags, SYSTEM.FCarry) = 0 THEN
    INC(r.pos)
  END
END WriteChar;

PROCEDURE WriteStr*(VAR r: Rider; s: ARRAY OF CHAR);
  VAR i: INTEGER;
BEGIN
  i := 0;
  WHILE (i < LEN(s)) & (s[i] # 0X) DO
    WriteChar(r, s[i]);
    INC(i)
  END
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
