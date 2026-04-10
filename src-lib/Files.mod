MODULE Files;

TYPE

FileDesc* = RECORD
    h    : INTEGER;
    res* : INTEGER
END;

File* = POINTER TO FileDesc;

Rider* = RECORD
    f*: File;
    pos*: LONGINT;
    res*: LONGINT;
    eof*: BOOLEAN
END;

PROCEDURE Old*(name: ARRAY OF CHAR): File;
  VAR r: SYSTEM.Registers; f: File;
BEGIN
  r.Flags := SYSTEM.FCarry;
  r.AX := 716CH;
  r.BX := 2;
  r.CX := 0;
  r.DX := 1;
  r.DS := SYSTEM.SEG(name);
  r.SI := SYSTEM.OFS(name);
  r.DI := 1;
  SYSTEM.Intr(21H, r);
  IF (r.Flags & SYSTEM.FCarry) # 0  THEN
    r.Flags := 0;
    r.AX := 3D02H;
    r.DS := SYSTEM.SEG(name);
    r.DX := SYSTEM.OFS(name);
    SYSTEM.Intr(21H, r)
  END;
  IF (r.Flags & SYSTEM.FCarry) # 0 THEN
    f := NIL
  ELSE
    NEW(f);
    f.h := r.AX;
    f.res := 0
  END;
  RETURN f
END Old;

PROCEDURE New*(name: ARRAY OF CHAR): File;
  VAR f: File;
BEGIN
  NEW(f);
  f.h := -1;
  f.res := 0;
  RETURN f
END New;

PROCEDURE Register*(f: File; name: ARRAY OF CHAR);
  VAR r: SYSTEM.Registers;
BEGIN
  IF (f # NIL) & (f.h = -1) THEN
    r.Flags := SYSTEM.FCarry;
    r.AX := 716CH;
    r.BX := 2;
    r.CX := 0;
    r.DX := 12H;
    r.DS := SYSTEM.SEG(name);
    r.SI := SYSTEM.OFS(name);
    r.DI := 1;
    SYSTEM.Intr(21H, r);
    IF (r.Flags & SYSTEM.FCarry) # 0 THEN
      r.Flags := 0;
      r.AX := 3C00H;
      r.CX := 0;
      r.DS := SYSTEM.SEG(name);
      r.DX := SYSTEM.OFS(name);
      SYSTEM.Intr(21H, r)
    END;
    IF (r.Flags & SYSTEM.FCarry) = 0 THEN
      f.h := r.AX
    END
  END
END Register;

PROCEDURE Close*(f: File);
  VAR r: SYSTEM.Registers;
BEGIN
  IF (f # NIL) & (f.h # -1) THEN
    r.AX := 3E00H;
    r.BX := f.h;
    SYSTEM.Intr(21H, r);
    f.h := -1
  END
END Close;

PROCEDURE Set*(VAR r: Rider; f: File; pos: LONGINT);
  VAR regs: SYSTEM.Registers;
BEGIN
  r.f := f;
  r.pos := pos;
  r.eof := FALSE;
  r.res := 0;
  IF (f # NIL) & (f.h # -1) THEN
    regs.AX := 4200H;
    regs.BX := f.h;
    regs.CX := INTEGER(pos DIV 10000H);
    regs.DX := INTEGER(pos MOD 10000H);
    regs.Flags := 0;
    SYSTEM.Intr(21H, regs);
    IF ODD(regs.Flags) THEN
      r.res := regs.AX
    ELSE
      r.pos := LONGINT(regs.DX) * 10000H + LONGINT(regs.AX)
    END
  END
END Set;

PROCEDURE Read*(VAR r: Rider; VAR x: BYTE);
  VAR regs: SYSTEM.Registers;
BEGIN
  IF (r.f # NIL) & (r.f.h # -1) THEN
    regs.AX := 3F00H;
    regs.BX := r.f.h;
    regs.CX := 1;
    regs.DS := SYSTEM.SEG(x);
    regs.DX := SYSTEM.OFS(x);
    regs.Flags := 0;
    SYSTEM.Intr(21H, regs);
    IF ((regs.Flags & SYSTEM.FCarry) # 0) OR (regs.AX = 0) THEN
      r.eof := TRUE;
      x := 0X
    ELSE
      r.pos := r.pos + 1
    END
  END
END Read;

PROCEDURE Write*(VAR r: Rider; x: BYTE);
  VAR regs: SYSTEM.Registers;
BEGIN
  IF (r.f # NIL) & (r.f.h # -1) THEN
    regs.AX := 4000H;
    regs.BX := r.f.h;
    regs.CX := 1;
    regs.DS := SYSTEM.SEG(x);
    regs.DX := SYSTEM.OFS(x);
    regs.Flags := 0;
    SYSTEM.Intr(21H, regs);
    IF (regs.Flags & SYSTEM.FCarry) = 0 THEN
      r.pos := r.pos + 1
    END
  END
END Write;

PROCEDURE Pos*(VAR r: Rider): LONGINT;
BEGIN
  RETURN r.pos
END Pos;

END Files.
