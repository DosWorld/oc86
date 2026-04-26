MODULE Files;

CONST
  BUFSIZE = 8192;

TYPE
  FileBuf = ARRAY BUFSIZE OF CHAR;
  File* = RECORD
    handle  : INTEGER;
    buf     : POINTER TO FileBuf;
    bufTag  : LONGINT;
    bufSize : INTEGER;
    dirty   : BOOLEAN;
    pos     : LONGINT;
    size    : LONGINT
  END;

PROCEDURE DOSSeek(handle: INTEGER; pos: LONGINT);
  VAR r: SYSTEM.Registers;
BEGIN
  r.AX := 4200H;
  r.BX := handle;
  r.CX := INTEGER(SYSTEM.LSR(pos, 16));
  r.DX := INTEGER(SYSTEM.AND(pos, 0FFFFH));
  r.Flags := 0;
  SYSTEM.Intr(21H, r)
END DOSSeek;

PROCEDURE DOSRead(handle: INTEGER; VAR buf; len: INTEGER): INTEGER;
  VAR r: SYSTEM.Registers; p: POINTER TO CHAR;
BEGIN
  p := buf;
  r.AX := 3F00H;
  r.BX := handle;
  r.CX := len;
  r.DS := SYSTEM.SEG(p^);
  r.DX := SYSTEM.OFS(p^);
  r.Flags := 0;
  SYSTEM.Intr(21H, r);
  IF SYSTEM.AND(r.Flags, SYSTEM.FCarry) = 0 THEN RETURN r.AX END;
  RETURN 0
END DOSRead;

PROCEDURE DOSWrite(handle: INTEGER; VAR buf; len: INTEGER): INTEGER;
  VAR r: SYSTEM.Registers; p: POINTER TO CHAR;
BEGIN
  p := buf;
  r.AX := 4000H;
  r.BX := handle;
  r.CX := len;
  r.DS := SYSTEM.SEG(p^);
  r.DX := SYSTEM.OFS(p^);
  r.Flags := 0;
  SYSTEM.Intr(21H, r);
  IF SYSTEM.AND(r.Flags, SYSTEM.FCarry) = 0 THEN RETURN r.AX END;
  RETURN 0
END DOSWrite;

PROCEDURE DOSClose(handle: INTEGER);
  VAR r: SYSTEM.Registers;
BEGIN
  r.AX := 3E00H;
  r.BX := handle;
  SYSTEM.Intr(21H, r)
END DOSClose;

PROCEDURE DOSGetFileSize(handle: INTEGER): LONGINT;
  VAR r: SYSTEM.Registers; cur, sz: LONGINT;
BEGIN
  r.AX := 4201H;
  r.BX := handle;
  r.CX := 0;
  r.DX := 0;
  r.Flags := 0;
  SYSTEM.Intr(21H, r);
  IF SYSTEM.AND(r.Flags, SYSTEM.FCarry) # 0 THEN RETURN -1 END;
  cur := SYSTEM.AND(LONGINT(r.AX), LONGINT(0FFFFH));
  cur := SYSTEM.IOR(cur, SYSTEM.LSL(LONGINT(r.DX), 16));
  r.AX := 4202H;
  r.BX := handle;
  r.CX := 0;
  r.DX := 0;
  SYSTEM.Intr(21H, r);
  IF SYSTEM.AND(r.Flags, SYSTEM.FCarry) # 0 THEN RETURN -1 END;
  sz := SYSTEM.AND(LONGINT(r.AX), LONGINT(0FFFFH));
  sz := SYSTEM.IOR(sz, SYSTEM.LSL(LONGINT(r.DX), 16));
  DOSSeek(handle, cur);
  RETURN sz
END DOSGetFileSize;

PROCEDURE BufFlush(VAR F: File);
BEGIN
  IF F.dirty & (F.bufTag # -1) THEN
    DOSSeek(F.handle, F.bufTag);
    DOSWrite(F.handle, F.buf^, F.bufSize);
    F.dirty := FALSE
  END
END BufFlush;

PROCEDURE BufFetch(VAR F: File; tag: LONGINT);
BEGIN
  IF tag = F.bufTag THEN RETURN END;
  BufFlush(F);
  F.bufTag := tag;
  F.bufSize := DOSRead(F.handle, F.buf^, BUFSIZE);
  F.dirty := FALSE
END BufFetch;

PROCEDURE Seek*(VAR F: File; pos: LONGINT);
BEGIN
  IF pos < 0 THEN pos := 0 END;
  IF pos > F.size THEN pos := F.size END;
  F.pos := pos
END Seek;

PROCEDURE BlockRead*(VAR F: File; VAR x; len: INTEGER): INTEGER;
  VAR total, chunk, off: INTEGER; tag: LONGINT; dest: POINTER TO ARRAY OF CHAR;
BEGIN
  IF F.pos + len > F.size THEN len := F.size - F.pos END;
  IF len <= 0 THEN RETURN 0 END;
  total := 0;
  dest := x;
  WHILE total < len DO
    tag := F.pos - F.pos MOD BUFSIZE;
    off := INTEGER(F.pos MOD BUFSIZE);
    BufFetch(F, tag);
    IF off >= F.bufSize THEN RETURN total END;
    chunk := F.bufSize - off;
    IF chunk > len - total THEN chunk := len - total END;
    SYSTEM.MOVE(F.buf^[off], dest^[total], chunk);
    INC(F.pos, chunk);
    INC(total, chunk)
  END;
  RETURN total
END BlockRead;

PROCEDURE BlockWrite*(VAR F: File; VAR x; len: INTEGER): INTEGER;
  VAR total, chunk, off: INTEGER; tag: LONGINT; src: POINTER TO ARRAY OF CHAR;
BEGIN
  total := 0;
  src := x;
  WHILE total < len DO
    tag := F.pos - F.pos MOD BUFSIZE;
    off := INTEGER(F.pos MOD BUFSIZE);
    BufFetch(F, tag);
    chunk := BUFSIZE - off;
    IF chunk > len - total THEN chunk := len - total END;
    SYSTEM.MOVE(src^[total], F.buf^[off], chunk);
    F.dirty := TRUE;
    IF off + chunk > F.bufSize THEN F.bufSize := off + chunk END;
    INC(F.pos, chunk);
    INC(total, chunk);
    IF F.pos > F.size THEN F.size := F.pos END
  END;
  RETURN total
END BlockWrite;

PROCEDURE Position*(VAR F: File): LONGINT;
BEGIN
  RETURN F.pos
END Position;

PROCEDURE Size*(VAR F: File): LONGINT;
BEGIN
  RETURN F.size
END Size;

PROCEDURE Truncate*(VAR F: File);
BEGIN
  BufFlush(F);
  IF F.bufTag # -1 THEN
    IF F.pos > F.bufTag THEN
      F.bufSize := INTEGER(F.pos - F.bufTag)
    ELSE
      F.bufSize := 0
    END
  END;
  F.size := F.pos;
  DOSSeek(F.handle, F.pos);
  DOSWrite(F.handle, F.buf^, 0)
END Truncate;

PROCEDURE Reset*(VAR F: File; name: ARRAY OF CHAR): BOOLEAN;
  VAR r: SYSTEM.Registers;
BEGIN
  F.handle := -1;
  F.buf := NIL;
  r.Flags := SYSTEM.FCarry;
  r.AX := 716CH;
  r.BX := 2;
  r.CX := 0;
  r.DX := 1;
  r.DS := SYSTEM.SEG(name);
  r.SI := SYSTEM.OFS(name); r.DI := 1;
  SYSTEM.Intr(21H, r);
  IF SYSTEM.AND(r.Flags, SYSTEM.FCarry) # 0 THEN
    r.Flags := 0;
    r.AX := 3D02H;
    r.DS := SYSTEM.SEG(name);
    r.DX := SYSTEM.OFS(name);
    SYSTEM.Intr(21H, r)
  END;
  IF SYSTEM.AND(r.Flags, SYSTEM.FCarry) = 0 THEN
    F.handle := r.AX
  END;
  IF F.handle = -1 THEN
    F.pos := 0; F.size := -1; RETURN FALSE
  END;
  NEW(F.buf);
  F.bufTag := -1;
  F.bufSize := 0;
  F.dirty := FALSE;
  F.pos := 0;
  F.size := DOSGetFileSize(F.handle);
  RETURN TRUE
END Reset;

PROCEDURE ReWrite*(VAR F: File; name: ARRAY OF CHAR): BOOLEAN;
  VAR r: SYSTEM.Registers;
BEGIN
  F.handle := -1;
  F.buf := NIL;
  r.AX := 716CH;
  r.BX := 1;
  r.CX := 0;
  r.DX := 12H;
  r.DS := SYSTEM.SEG(name);
  r.SI := SYSTEM.OFS(name);
  r.Flags := SYSTEM.FCarry;
  SYSTEM.Intr(21H, r);
  IF SYSTEM.AND(r.Flags, SYSTEM.FCarry) # 0 THEN
    r.AX := 3C00H;
    r.CX := 0;
    r.DS := SYSTEM.SEG(name);
    r.DX := SYSTEM.OFS(name);
    SYSTEM.Intr(21H, r)
  END;
  IF SYSTEM.AND(r.Flags, SYSTEM.FCarry) = 0 THEN
    F.handle := r.AX
  END;
  IF F.handle = -1 THEN
    F.pos := 0; F.size := -1; RETURN FALSE
  END;
  NEW(F.buf);
  F.bufTag := -1;
  F.bufSize := 0;
  F.dirty := FALSE;
  F.pos := 0;
  F.size := 0;
  Truncate(F);
  RETURN TRUE
END ReWrite;

PROCEDURE Close*(VAR F: File);
BEGIN
  IF F.handle # -1 THEN
    BufFlush(F);
    DOSClose(F.handle);
    F.handle := -1
  END;
  IF F.buf # NIL THEN
    DISPOSE(F.buf);
    F.buf := NIL
  END
END Close;

PROCEDURE Delete*(FileName: ARRAY OF CHAR): BOOLEAN;
  VAR r: SYSTEM.Registers;
BEGIN
  r.AX := 7141H;
  r.CX := 0;
  r.SI := 0;
  r.DS := SYSTEM.SEG(FileName);
  r.DX := SYSTEM.OFS(FileName);
  r.Flags := SYSTEM.FCarry;
  SYSTEM.Intr(21H, r);
  IF SYSTEM.AND(r.Flags, SYSTEM.FCarry) # 0 THEN
    r.AX := 4100H;
    r.DS := SYSTEM.SEG(FileName);
    r.DX := SYSTEM.OFS(FileName);
    SYSTEM.Intr(21H, r)
  END;
  RETURN SYSTEM.AND(r.Flags, SYSTEM.FCarry) = 0
END Delete;

PROCEDURE Rename*(OldName, NewName: ARRAY OF CHAR): BOOLEAN;
  VAR r: SYSTEM.Registers;
BEGIN
  r.AX := 7156H;
  r.DS := SYSTEM.SEG(OldName);
  r.DX := SYSTEM.OFS(OldName);
  r.ES := SYSTEM.SEG(NewName);
  r.DI := SYSTEM.OFS(NewName);
  r.Flags := SYSTEM.FCarry;
  SYSTEM.Intr(21H, r);
  IF SYSTEM.AND(r.Flags, SYSTEM.FCarry) # 0 THEN
    r.AX := 5600H;
    r.DS := SYSTEM.SEG(OldName);
    r.DX := SYSTEM.OFS(OldName);
    r.ES := SYSTEM.SEG(NewName);
    r.DI := SYSTEM.OFS(NewName);
    SYSTEM.Intr(21H, r)
  END;
  RETURN SYSTEM.AND(r.Flags, SYSTEM.FCarry) = 0
END Rename;

PROCEDURE FileExists*(FileName: ARRAY OF CHAR): BOOLEAN;
  VAR r: SYSTEM.Registers; buf: ARRAY 320 OF CHAR; oldES, oldBX: INTEGER; ok: BOOLEAN;
BEGIN
  r.AX := 714EH;
  r.CX := 0;
  r.DS := SYSTEM.SEG(FileName);
  r.DX := SYSTEM.OFS(FileName);
  r.ES := SYSTEM.SEG(buf);
  r.DI := SYSTEM.OFS(buf);
  r.Flags := SYSTEM.FCarry;
  SYSTEM.Intr(21H, r);

  IF SYSTEM.AND(r.Flags, SYSTEM.FCarry) = 0 THEN RETURN TRUE END;
  IF r.AX # 7100H THEN RETURN FALSE END;

  r.AX := 2F00H;
  SYSTEM.Intr(21H, r);
  oldES := r.ES;
  oldBX := r.BX;

  r.AX := 1A00H;
  r.DS := SYSTEM.SEG(buf);
  r.DX := SYSTEM.OFS(buf);
  SYSTEM.Intr(21H, r);

  r.AX := 4E00H;
  r.CX := 0;
  r.DS := SYSTEM.SEG(FileName);
  r.DX := SYSTEM.OFS(FileName);
  SYSTEM.Intr(21H, r);

  ok := SYSTEM.AND(r.Flags, SYSTEM.FCarry) = 0;

  r.AX := 1A00H;
  r.DS := oldES;
  r.DX := oldBX;
  SYSTEM.Intr(21H, r);

  RETURN ok
END FileExists;

END Files.