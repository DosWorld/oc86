(*$R-*)
MODULE Dos;

CONST

ReadOnly*  = 01H;
Hidden*    = 02H;
SysFile*   = 04H;
VolumeID*  = 08H;
Directory* = 10H;
Archive*   = 20H;
AnyFile*   = 3FH;

STDIN*  = 0;
STDOUT* = 1;
STDERR* = 2;
STDPRN* = 4;
STDAUX* = 5;


TYPE
Registers* = SYSTEM.Registers;

DateTime* = RECORD
    Year*, Month*, Day*, Hour*, Min*, Sec*: INTEGER;
END;

PROCEDURE Intr*(IntNo: BYTE; VAR Regs: Registers);
BEGIN
    SYSTEM.Intr(IntNo, Regs);
END Intr;

PROCEDURE MsDos*(VAR Regs: Registers);
BEGIN
    SYSTEM.Intr(21H, Regs)
END MsDos;

VAR
    argBuf   : ARRAY 200 OF CHAR;
    argOfs   : ARRAY 32 OF INTEGER;
    argCount : INTEGER;

PROCEDURE ReadByte(seg, ofs: INTEGER): CHAR;
VAR
    p : POINTER TO CHAR;
BEGIN
    p := SYSTEM.PTR(seg, ofs);
    RETURN p^
END ReadByte;

PROCEDURE GetPsp*:INTEGER;
VAR
    regs    : SYSTEM.Registers;
BEGIN
    regs.AX := 6200H;
    SYSTEM.Intr(21H, regs);
    RETURN regs.BX;
END GetPsp;

PROCEDURE ArgInit;
VAR
    psp     : INTEGER;
    len     : INTEGER;
    pos     : INTEGER;
    ai      : INTEGER;
    bi      : INTEGER;
    ch      : CHAR;
    inQuote : BOOLEAN;
    inArg   : BOOLEAN;
BEGIN
    psp  := GetPsp();
    len  := ORD(ReadByte(psp, 80H));
    IF len = 0 THEN
        argCount := 0;
        RETURN
    END;

    pos     := 0;
    inQuote := FALSE;
    inArg   := FALSE;
    ai      := 1;
    bi      := 1;
    argCount := 0;

    WHILE (pos < len) & (ai < 32) DO
        ch := ReadByte(psp, 81H + pos);
        INC(pos);

        IF ch = 22X THEN
            IF inQuote THEN
                inQuote := FALSE
            ELSE
                IF ~inArg THEN
                    argOfs[ai] := bi;
                    inArg := TRUE
                END;
                inQuote := TRUE
            END
        ELSIF (ch = ' ') OR (ch = 9X) THEN
            IF inQuote THEN
                IF bi < 199 THEN
                    argBuf[bi] := ch; INC(bi)
                END
            ELSIF inArg THEN
                IF bi < 199 THEN
                    argBuf[bi] := 0X; INC(bi)
                END;
                INC(ai); INC(argCount);
                inArg := FALSE
            END
        ELSE
            IF ~inArg THEN
                argOfs[ai] := bi;
                inArg := TRUE
            END;
            IF bi < 199 THEN
                argBuf[bi] := ch; INC(bi)
            END
        END
    END;

    IF inArg & (ai < 32) THEN
        IF bi < 199 THEN
            argBuf[bi] := 0X
        END;
        INC(argCount)
    END
END ArgInit;

PROCEDURE ParamCount*: INTEGER;
BEGIN
    RETURN argCount
END ParamCount;

PROCEDURE ParamStr*(i: INTEGER; VAR p: ARRAY OF CHAR);
    VAR src, dst : INTEGER; ch : CHAR;
    PROCEDURE GetExeName;
        VAR envseg, ofs : INTEGER; wcount : INTEGER; ch2 : CHAR;
    BEGIN
        dst := 0;
        envseg := ORD(ReadByte(GetPsp(), 2CH)) + ORD(ReadByte(GetPsp(), 2DH)) * 256;
        IF envseg = 0 THEN
            p[0] := 0X;
            RETURN
        END;
        ofs := 0;
        WHILE ReadByte(envseg, ofs) # 0X DO
            WHILE ReadByte(envseg, ofs) # 0X DO INC(ofs) END;
            INC(ofs)
        END;
        INC(ofs);
        wcount := ORD(ReadByte(envseg, ofs));
        INC(ofs);
        wcount := wcount + ORD(ReadByte(envseg, ofs)) * 256;
        INC(ofs, 2);
        ch := ReadByte(envseg, ofs);
        WHILE (ch # 0X) & (dst < LEN(p) - 1) DO
            p[dst] := ch; INC(dst); INC(ofs);
            ch := ReadByte(envseg, ofs)
        END;
        p[dst] := 0X
    END GetExeName;
BEGIN
    IF i = 0 THEN
        GetExeName
    ELSE
        dst := 0;
        IF (i >= 1) & (i <= argCount) THEN
            src := argOfs[i];
            ch := argBuf[src];
            WHILE ch # 0X DO
                IF dst < LEN(p) - 1 THEN
                    p[dst] := ch;
                    INC(dst)
                END;
                INC(src);
                ch := argBuf[src]
            END
        END;
        p[dst] := 0X
    END
END ParamStr;

PROCEDURE GetEnv*(envvar: ARRAY OF CHAR; VAR dst: ARRAY OF CHAR);
VAR
    envseg : INTEGER;
    ofs    : INTEGER;
    ei     : INTEGER;
    di     : INTEGER;
    ch     : CHAR;
    match  : BOOLEAN;
BEGIN
    dst[0] := 0X;
    envseg := GetPsp();
    envseg := ORD(ReadByte(envseg, 2CH)) + ORD(ReadByte(envseg, 2DH)) * 256;
    IF envseg = 0 THEN RETURN END;

    ofs := 0;
    WHILE ReadByte(envseg, ofs) # 0X DO
        ei := 0;
        match := TRUE;
        ch := ReadByte(envseg, ofs);
        WHILE (ch # '=') & (ch # 0X) DO
            IF (ei < LEN(envvar) - 1) & match THEN
                match := SYSTEM.UCASE(ch) = SYSTEM.UCASE(envvar[ei]);
                INC(ei)
            ELSE
                match := FALSE
            END;
            INC(ofs);
            ch := ReadByte(envseg, ofs)
        END;
        IF match & (envvar[ei] = 0X) & (ch = '=') THEN
            INC(ofs);
            di := 0;
            ch := ReadByte(envseg, ofs);
            WHILE ch # 0X DO
                IF di < LEN(dst) - 1 THEN
                    dst[di] := ch; INC(di)
                END;
                INC(ofs);
                ch := ReadByte(envseg, ofs)
            END;
            dst[di] := 0X;
            RETURN
        END;
        WHILE ReadByte(envseg, ofs) # 0X DO INC(ofs) END;
        INC(ofs)
    END
END GetEnv;

PROCEDURE GetDTA*: SYSTEM.ADDRESS;
  VAR r: SYSTEM.Registers;
BEGIN
  r.AX := 2F00H;
  SYSTEM.Intr(21H, r);
  RETURN SYSTEM.PTR(r.ES, r.BX)
END GetDTA;

PROCEDURE SetDTA*(dta : SYSTEM.ADDRESS);
  VAR r: SYSTEM.Registers; p : POINTER TO CHAR;
BEGIN
  p := dta;
  r.AX := 1A00H;
  r.DS := SYSTEM.SEG(p^);
  r.DX := SYSTEM.OFS(p^);
  SYSTEM.Intr(21H, r)
END SetDTA;

PROCEDURE GetDate*(VAR Year,Month,Day,DayOfWeek: INTEGER);
  VAR r: SYSTEM.Registers;
BEGIN
  r.AX := 2A00H;
  SYSTEM.Intr(21H, r);
  Year := r.CX;
  Month := ORD(SYSTEM.LSR(r.DX, 8));
  Day := r.DX MOD 256;
  DayOfWeek := r.AX MOD 256
END GetDate;

PROCEDURE SetDate*(Year,Month,Day: INTEGER);
  VAR r: SYSTEM.Registers;
BEGIN
  r.AX := 2B00H;
  r.CX := Year;
  r.DX := Day + SYSTEM.LSL(Month, 8);
  SYSTEM.Intr(21H, r)
END SetDate;

PROCEDURE GetTime*(VAR Hour,Minute,Second,Sec100: INTEGER);
  VAR r: SYSTEM.Registers;
BEGIN
  r.AX := 2C00H;
  SYSTEM.Intr(21H, r);
  Hour := ORD(SYSTEM.LSR(r.CX, 8));
  Minute := r.CX MOD 256;
  Second := ORD(SYSTEM.LSR(r.DX, 8));
  Sec100 := r.DX MOD 256
END GetTime;

PROCEDURE SetTime*(Hour,Minute,Second,Sec100: INTEGER);
  VAR r: SYSTEM.Registers;
BEGIN
  r.AX := 2D00H;
  r.CX := Minute + SYSTEM.LSL(Hour, 8);
  r.DX := Sec100 + SYSTEM.LSL(Second, 8);
  SYSTEM.Intr(21H, r)
END SetTime;

PROCEDURE GetCBreak*(VAR Break: BOOLEAN);
  VAR r: SYSTEM.Registers;
BEGIN
  r.AX := 3300H;
  SYSTEM.Intr(21H, r);
  Break := (r.DX MOD 256) # 0
END GetCBreak;

PROCEDURE SetCBreak*(Break: BOOLEAN);
  VAR r: SYSTEM.Registers;
BEGIN
  r.AX := 3301H;
  IF Break THEN r.DX := 1 ELSE r.DX := 0 END;
  SYSTEM.Intr(21H, r)
END SetCBreak;

PROCEDURE GetVerify*(VAR Verify: BOOLEAN);
  VAR r: SYSTEM.Registers;
BEGIN
  r.AX := 5400H;
  SYSTEM.Intr(21H, r);
  Verify := (r.AX MOD 256) # 0
END GetVerify;

PROCEDURE SetVerify*(Verify: BOOLEAN);
  VAR r: SYSTEM.Registers;
BEGIN
  r.AX := 2E00H;
  IF Verify THEN r.DX := 1 ELSE r.DX := 0 END;
  SYSTEM.Intr(21H, r)
END SetVerify;

PROCEDURE DiskFree*(Drive: BYTE): LONGINT;
  VAR r: SYSTEM.Registers;
BEGIN
  r.AX := 3600H;
  r.DX := Drive;
  SYSTEM.Intr(21H, r);
  IF r.AX = 0FFFFH THEN RETURN -1 END;
  RETURN LONGINT(r.AX) * LONGINT(r.BX) * LONGINT(r.CX)
END DiskFree;

PROCEDURE DiskSize*(Drive: BYTE): LONGINT;
  VAR r: SYSTEM.Registers;
BEGIN
  r.AX := 3600H;
  r.DX := Drive;
  SYSTEM.Intr(21H, r);
  IF r.AX = 0FFFFH THEN RETURN -1 END;
  RETURN LONGINT(r.AX) * LONGINT(r.CX) * LONGINT(r.DX)
END DiskSize;

PROCEDURE GetIntVec*(IntNo: BYTE; VAR Vector: SYSTEM.ADDRESS);
  VAR r: SYSTEM.Registers;
BEGIN
  r.AX := 3500H + IntNo;
  SYSTEM.Intr(21H, r);
  Vector := SYSTEM.PTR(r.ES, r.BX)
END GetIntVec;

PROCEDURE SetIntVec*(IntNo: BYTE; Vector: SYSTEM.ADDRESS);
  VAR r: SYSTEM.Registers; p: POINTER TO CHAR;
BEGIN
  p := Vector;
  r.AX := 2500H + IntNo;
  r.DS := SYSTEM.SEG(p^);
  r.DX := SYSTEM.OFS(p^);
  SYSTEM.Intr(21H, r)
END SetIntVec;

PROCEDURE DosExitCode*: INTEGER;
  VAR r: SYSTEM.Registers;
BEGIN
  r.AX := 4D00H;
  SYSTEM.Intr(21H, r);
  RETURN r.AX
END DosExitCode;

PROCEDURE FExpand*(Path: ARRAY OF CHAR; VAR Dest : ARRAY OF CHAR);
  VAR r: SYSTEM.Registers; p, d: POINTER TO CHAR; i: INTEGER;
BEGIN
  p := SYSTEM.ADR(Path);
  d := SYSTEM.ADR(Dest);
  r.AX := 6000H;
  r.DS := SYSTEM.SEG(p^);
  r.SI := SYSTEM.OFS(p^);
  r.ES := SYSTEM.SEG(d^);
  r.DI := SYSTEM.OFS(d^);
  r.Flags := 0;
  SYSTEM.Intr(21H, r);
  IF SYSTEM.AND(r.Flags, SYSTEM.FCarry) # 0 THEN
    i := 0;
    WHILE (i < LEN(Dest)) & (Path[i] # 0X) DO
      Dest[i] := Path[i]; INC(i)
    END;
    IF i < LEN(Dest) THEN Dest[i] := 0X END
  END
END FExpand;

PROCEDURE FSplit*(Path: ARRAY OF CHAR; VAR Dir, Name, Ext: ARRAY OF CHAR);
  VAR i, j, len, dirEnd, nameStart, extStart : INTEGER;
BEGIN
  len := 0; WHILE (len < LEN(Path)) & (Path[len] # 0X) DO INC(len) END;
  dirEnd := -1;
  i := 0;
  WHILE i < len DO
    IF (Path[i] = '\') OR (Path[i] = ':') THEN dirEnd := i END;
    INC(i)
  END;
  nameStart := dirEnd + 1;
  i := nameStart;
  WHILE (i < len) & (Path[i] # '.') DO INC(i) END;
  extStart := i;
  IF dirEnd < 0 THEN
    Dir[0] := 0X
  ELSE
    j := 0;
    i := 0;
    WHILE (i <= dirEnd) & (j < LEN(Dir)-1) DO
      Dir[j] := Path[i]; INC(i); INC(j)
    END;
    Dir[j] := 0X
  END;
  j := 0;
  i := nameStart;
  WHILE (i < extStart) & (j < LEN(Name)-1) DO
    Name[j] := Path[i]; INC(i); INC(j)
  END;
  Name[j] := 0X;
  IF extStart < len THEN
    j := 0;
    i := extStart;
    WHILE (i < len) & (j < LEN(Ext)-1) DO
      Ext[j] := Path[i]; INC(i); INC(j)
    END;
    Ext[j] := 0X
  ELSE
    Ext[0] := 0X
  END
END FSplit;

BEGIN
    ArgInit;
END Dos.