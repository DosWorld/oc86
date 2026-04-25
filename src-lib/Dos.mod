MODULE Dos;

CONST

ReadOnly*  = 01H;
Hidden*    = 02H;
SysFile*   = 04H;
VolumeID*  = 08H;
Directory* = 10H;
Archive*   = 20H;
AnyFile*   = 3FH;

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
    argBuf[0] := 0X;
    argOfs[0] := 0;
    argCount  := 1;
    ai        := 1;
    bi        := 1;    (* next free slot in argBuf; slot 0 = empty argv[0] *)

    psp := GetPsp();

    len := ORD(ReadByte(psp, 80H));

    IF len = 0 THEN RETURN END;

    pos     := 0;
    inQuote := FALSE;
    inArg   := FALSE;

    WHILE (pos < len) & (ai < 32) DO
        ch := ReadByte(psp, 81H + pos);
        INC(pos);

        IF ch = 22X THEN                        (* double-quote *)
            IF inQuote THEN
                inQuote := FALSE
            ELSE
                IF ~inArg THEN
                    argOfs[ai] := bi;
                    inArg := TRUE
                END;
                inQuote := TRUE
            END
        ELSIF (ch = ' ') OR (ch = 9X) THEN     (* space or tab *)
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

PROCEDURE ARGCOUNT*: INTEGER;
BEGIN
    RETURN argCount
END ARGCOUNT;

PROCEDURE ARG*(i: INTEGER; VAR p: ARRAY OF CHAR);
VAR
    src : INTEGER;
    dst : INTEGER;
    ch  : CHAR;
BEGIN
    dst := 0;
    IF (i >= 0) & (i < argCount) THEN
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
END ARG;

PROCEDURE Up(c: CHAR): CHAR; NEAR;
BEGIN
    IF (c >= 'a') & (c <= 'z') THEN
        c := CHR(ORD(c) - 32)
    END;
    RETURN c
END Up;

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
    (* PSP offset 2Ch holds the environment segment (word, lo then hi byte) *)
    envseg := ORD(ReadByte(GetPsp(), 2CH)) + ORD(ReadByte(GetPsp(), 2DH)) * 256;
    IF envseg = 0 THEN RETURN END;

    ofs := 0;
    WHILE ReadByte(envseg, ofs) # 0X DO
        (* compare env var name with envvar (case-insensitive) *)
        ei := 0;
        match := TRUE;
        ch := ReadByte(envseg, ofs);
        WHILE (ch # '=') & (ch # 0X) DO
            IF (ei < LEN(envvar) - 1) & match THEN
                match := Up(ch) = Up(envvar[ei]);
                INC(ei)
            ELSE
                match := FALSE
            END;
            INC(ofs);
            ch := ReadByte(envseg, ofs)
        END;
        (* match only if name lengths also equal *)
        IF match & (envvar[ei] = 0X) & (ch = '=') THEN
            INC(ofs); (* skip '=' *)
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
        (* skip to next entry: find end of current entry *)
        WHILE ReadByte(envseg, ofs) # 0X DO INC(ofs) END;
        INC(ofs) (* skip the NUL terminator *)
    END
END GetEnv;

BEGIN
    ArgInit;
END Dos.
