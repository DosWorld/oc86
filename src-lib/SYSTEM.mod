(*$R-*)
MODULE SYSTEM;

CONST
FCarry*     = 1;
FParity*    = 4;
FAuxiliary* = 16;
FZero*      = 64;
FSign*      = 128;
FOverflow*  = 2048;

TYPE
ADDRESS* = ADDRESS;

Registers* = RECORD
  AX*, BX*, CX*, DX*, SI*, DI*, DS*, ES*, Flags* : INTEGER
END;

PROCEDURE CallFar*(ProcPtr: ADDRESS; VAR Regs:Registers);EXTERNAL;
PROCEDURE Intr*(IntNo: BYTE;VAR Regs : Registers);EXTERNAL;

PROCEDURE Halt*(ErrCode: BYTE);EXTERNAL;

PROCEDURE Alloc*(sizeInBytes : INTEGER) : ADDRESS;EXTERNAL;
PROCEDURE Free*(p : ADDRESS) : ADDRESS;EXTERNAL;

PROCEDURE S32MUL*(a, b :LONGINT):LONGINT;EXTERNAL;
PROCEDURE S32DIV*(a, b :LONGINT):LONGINT;EXTERNAL;
PROCEDURE S32MOD*(a, b :LONGINT):LONGINT;EXTERNAL;

(* ADR(VAR v): ADDRESS
   Returns the full far address (segment:offset) of variable v.
   Caller pushes v's far address: segment first (deepest), then offset.
   Stack on entry (POP order): offset, segment.
   POP AX=offset; POP DX=segment. Result: DX:AX = far pointer. SP balanced.
   Note: v must be a variable (local, global, VAR param, array element, pointer deref).
         Procedure names are not supported (use a procedure variable instead). *)
PROCEDURE ADR*(VAR v): ADDRESS;
  INLINE(058H, 05AH);

(* SEG(VAR v): INTEGER
   Returns the segment of variable v (SS for locals/params, DS for globals, heap seg for p^).
   Stack on entry (POP order): offset, segment.
   POP AX (=offset, discard); POP AX (=segment). Result: AX = segment. SP balanced. *)
PROCEDURE SEG*(VAR v): INTEGER;
  INLINE(058H, 058H);

(* OFS(VAR v): INTEGER
   Returns the offset of variable v within its segment.
   Stack on entry (POP order): offset, segment.
   POP AX (=offset); POP CX (=segment, discard). Result: AX = offset. SP balanced. *)
PROCEDURE OFS*(VAR v): INTEGER;
  INLINE(058H, 059H);

(* PTR(s, o: INTEGER): ADDRESS
   Constructs a far pointer from segment s and offset o.
   Caller pushes s first (deepest), then o.
   Stack on entry (POP order): o, s.
   POP AX=o (offset); POP DX=s (segment). Result: DX:AX = far pointer. SP balanced. *)
PROCEDURE PTR*(s, o: INTEGER): ADDRESS;
  INLINE(058H, 05AH);

(* MOVE(VAR src, dst; n: INTEGER)
   Copies n bytes from src to dst using REP MOVSB.
   Caller pushes address of src (seg+ofs, 4 bytes), address of dst (4 bytes), n (2 bytes).
   Stack on entry (POP order): n, dst_ofs, dst_seg, src_ofs, src_seg.
   POP CX=n; POP DI=dst_ofs; POP ES=dst_seg; POP SI=src_ofs; POP AX=src_seg;
   PUSH DS; MOV DS,AX; CLD; REP MOVSB; POP DS — SP balanced. *)
PROCEDURE MOVE*(VAR src, dst; n: INTEGER);
  INLINE(059H,05FH,007H,05EH,058H,01EH,08EH,0D8H,0FCH,0F3H,0A4H,01FH);

(* FILL(VAR dst; n: INTEGER; b: BYTE)
   Fills n bytes at dst with byte b using REP STOSB.
   Caller pushes address of dst (seg+ofs, 4 bytes), n (2 bytes), b (2 bytes).
   Stack on entry (POP order): b, n, dst_ofs, dst_seg.
   POP AX=b; POP CX=n; POP DI=dst_ofs; POP ES=dst_seg; CLD; REP STOSB — SP balanced. *)
PROCEDURE FILL*(VAR dst; n: INTEGER; b: BYTE);
  INLINE(058H,059H,05FH,007H,0FCH,0F3H,0AAH);

PROCEDURE COMPARE*(VAR Mem1, Mem2; len: INTEGER): BOOLEAN;
  (* POP CX; POP DI; POP ES; POP SI; POP AX; PUSH DS; MOV DS, AX; CLD;
     MOV AX, 1; JCXZ done; REPE CMPSB; JZ done; DEC AX; done: POP DS *)
  INLINE(059H, 05FH, 007H, 05EH, 058H, 01EH, 08EH, 0D8H, 0FCH, 0B8H, 001H, 000H, 0E3H, 003H, 0F3H, 0A6H, 074H, 001H, 048H, 01FH);

PROCEDURE MAX*(a, b: INTEGER): INTEGER;
  (* POP BX; POP AX; CMP AX, BX; JG done; MOV AX, BX; done: *)
  INLINE(05BH, 058H, 03BH, 0C3H, 07FH, 002H, 089H, 0D8H);

PROCEDURE MIN*(a, b: INTEGER): INTEGER;
  (* POP BX; POP AX; CMP AX, BX; JL done; MOV AX, BX; done: *)
  INLINE(05BH, 058H, 03BH, 0C3H, 07CH, 002H, 089H, 0D8H);

PROCEDURE LENGTH*(s: ARRAY OF CHAR): INTEGER;
  (* POP DI; POP ES; POP CX; MOV DX, CX; JCXZ done; XOR AL, AL; CLD; REPNZ SCASB;
     JNZ skip; DEC DX; skip: SUB DX, CX; done: MOV AX, DX *)
  INLINE(05FH, 007H, 059H, 089H, 0CAH, 0E3H, 00AH, 032H, 0C0H, 0FCH, 0F2H, 0AEH, 075H, 001H, 04AH, 029H, 0CAH, 089H, 0D0H);

PROCEDURE UCASE*(c: CHAR): CHAR;
  (* POP AX; CMP AL, 61H; JB done; CMP AL, 7AH; JA done; SUB AL, 20H; done: *)
  INLINE(058H, 03CH, 061H, 072H, 006H, 03CH, 07AH, 077H, 002H, 02CH, 020H);

PROCEDURE LCASE*(c: CHAR): CHAR;
  (* POP AX; CMP AL, 41H; JB done; CMP AL, 5AH; JA done; ADD AL, 20H; done: *)
  INLINE(058H, 03CH, 041H, 072H, 006H, 03CH, 05AH, 077H, 002H, 004H, 020H);

PROCEDURE ISDIGIT*(c: CHAR): BOOLEAN;
  (* POP AX; SUB AL, 30H; CMP AL, 0AH; SBB AX, AX; NEG AX *)
  INLINE(058H, 02CH, 030H, 03CH, 00AH, 019H, 0C0H, 0F7H, 0D8H);

PROCEDURE ISALPHA*(c: CHAR): BOOLEAN;
  (* POP AX; OR AL, 20H; SUB AL, 61H; CMP AL, 1AH; SBB AX, AX; NEG AX *)
  INLINE(058H, 00CH, 020H, 02CH, 061H, 03CH, 01AH, 019H, 0C0H, 0F7H, 0D8H);

PROCEDURE ISSPACE*(c: CHAR): BOOLEAN;
  (* POP AX; DEC AL; CMP AL, 20H; SBB AX, AX; NEG AX *)
  INLINE(058H, 0FEH, 0C8H, 03CH, 020H, 019H, 0C0H, 0F7H, 0D8H);

PROCEDURE ISHEX*(c: CHAR): BOOLEAN;
  (* POP AX; OR AL, 20H; SUB AL, 30H; CMP AL, 0AH; JB ok; SUB AL, 31H; CMP AL, 06H; ok: SBB AX, AX; NEG AX *)
  INLINE(058H, 00CH, 020H, 02CH, 030H, 03CH, 00AH, 072H, 004H, 02CH, 031H, 03CH, 006H, 019H, 0C0H, 0F7H, 0D8H);

PROCEDURE UPSTR*(VAR s: ARRAY OF CHAR);
  (* POP BX; POP ES; POP CX; JCXZ done; loop: MOV AL, ES:[BX]; OR AL, AL; JZ done;
     CMP AL, 61H; JB skip; CMP AL, 7AH; JA skip; SUB AL, 20H; MOV ES:[BX], AL;
     skip: INC BX; LOOP loop; done: *)
  INLINE(05BH, 007H, 059H, 0E3H, 019H, 026H, 08AH, 007H, 008H, 0C0H, 074H, 012H, 03CH, 061H, 072H, 009H, 03CH, 07AH, 077H, 005H, 02CH, 020H, 026H, 088H, 007H, 043H, 0E2H, 0E9H);

PROCEDURE LOSTR*(VAR s: ARRAY OF CHAR);
  (* POP BX; POP ES; POP CX; JCXZ done; loop: MOV AL, ES:[BX]; OR AL, AL; JZ done;
     CMP AL, 41H; JB skip; CMP AL, 5AH; JA skip; ADD AL, 20H; MOV ES:[BX], AL;
     skip: INC BX; LOOP loop; done: *)
  INLINE(05BH, 007H, 059H, 0E3H, 019H, 026H, 08AH, 007H, 008H, 0C0H, 074H, 12H, 3CH, 41H, 72H, 09H, 3CH, 5AH, 77H, 05H, 04H, 20H, 26H, 88H, 07H, 43H, 0E2H, 0E9H);

PROCEDURE BLOCKWRITE*(h: INTEGER; VAR buf; len: INTEGER): INTEGER;
  (* POP CX; POP DX; POP AX; POP BX; PUSH DS; MOV DS, AX; MOV AH, 40H;
     INT 21H; JNC done; XOR AX, AX; done: POP DS *)
  INLINE(059H, 05AH, 058H, 05BH, 01EH, 08EH, 0D8H, 0B4H, 040H, 0CDH, 021H, 073H, 002H, 031H, 0C0H, 01FH);

PROCEDURE BLOCKREAD*(h: INTEGER; VAR buf; len: INTEGER): INTEGER;
  (* POP CX; POP DX; POP AX; POP BX; PUSH DS; MOV DS, AX; MOV AH, 3FH;
     INT 21H; JNC done; XOR AX, AX; done: POP DS *)
  INLINE(059H, 05AH, 058H, 05BH, 01EH, 08EH, 0D8H, 0B4H, 03FH, 0CDH, 021H, 073H, 002H, 031H, 0C0H, 01FH);

PROCEDURE FCLOSE*(h: INTEGER);
  (* POP BX; MOV AH, 3EH; INT 21H *)
  INLINE(05BH, 0B4H, 03EH, 0CDH, 021H);

PROCEDURE FPOS*(h: INTEGER): LONGINT;
  (* POP BX; XOR CX, CX; XOR DX, DX; MOV AX, 4201H; INT 21H; JNC ok; MOV AX, 0FFFFH; MOV DX, AX; ok: *)
  INLINE(05BH, 031H, 0C9H, 031H, 0D2H, 0B8H, 001H, 042H, 0CDH, 021H, 073H, 005H, 0B8H, 0FFH, 0FFH, 089H, 0C2H);

PROCEDURE FSIZE*(h: INTEGER): LONGINT;
  (* POP BX; PUSH SI; PUSH DI; XOR CX, CX; XOR DX, DX; MOV AX, 4201H; INT 21H;
     PUSH DX; PUSH AX; XOR CX, CX; XOR DX, DX; MOV AX, 4202H; INT 21H;
     MOV SI, AX; MOV DI, DX; POP DX; POP CX; MOV AX, 4200H; INT 21H;
     MOV AX, SI; MOV DX, DI; POP DI; POP SI *)
  INLINE(05BH, 056H, 057H, 031H, 0C9H, 031H, 0D2H, 0B8H, 001H, 042H, 0CDH, 021H, 052H, 050H, 031H, 0C9H, 031H, 0D2H, 0B8H, 002H, 042H, 0CDH, 021H, 089H, 0C6H, 089H, 0D7H, 05AH, 059H, 0B8H, 000H, 042H, 0CDH, 021H, 089H, 0F0H, 089H, 0FAH, 05FH, 05EH);

PROCEDURE FSEEK*(h: INTEGER; ofs: LONGINT);
  (* POP AX; POP CX; POP BX; MOV DX, AX; MOV AX, 4200H; INT 21H *)
  INLINE(058H, 059H, 05BH, 089H, 0C2H, 0B8H, 000H, 042H, 0CDH, 021H);

PROCEDURE PORTOUT*(port : INTEGER; b: BYTE);
  INLINE(058H,05AH,0EEH);

PROCEDURE PORTIN*(port : INTEGER) : BYTE;
  INLINE(05AH,0ECH);

PROCEDURE DOSCHAR*(c: CHAR);
  (* POP DX; MOV AH, 02H; INT 21H *)
  INLINE(05AH, 0B4H, 002H, 0CDH, 021H);

PROCEDURE DOSMESSAGE*(msg: ARRAY OF CHAR);
  (* POP DX; POP AX; POP CX; PUSH DS; MOV DS, AX; MOV AH, 09H; INT 21H; POP DS *)
  INLINE(05AH, 058H, 059H, 01EH, 08EH, 0D8H, 0B4H, 009H, 0CDH, 021H, 01FH);

PROCEDURE VIDEOMODE*(mode: BYTE);
  (* POP AX; MOV AH, 00H; INT 10H *)
  INLINE(058H, 0B4H, 000H, 0CDH, 010H);

PROCEDURE Error(errCode : INTEGER; msg : ARRAY OF CHAR);
BEGIN
    DOSMESSAGE("Run-time error $");
    DOSMESSAGE(msg);
    Halt(errCode)
END Error;

PROCEDURE ErrDivisionByZero*;
BEGIN
    Error(200, "200: Division by zero$");
END ErrDivisionByZero;

PROCEDURE ErrIndexOutOfBounds*;
BEGIN
    Error(201, "201: Array index out of bounds$");
END ErrIndexOutOfBounds;

PROCEDURE ErrStackOverflow*;
BEGIN
    Error(202, "202: Stack overflow$");
END ErrStackOverflow;

PROCEDURE ErrInvalidPointerOperation*;
BEGIN
    Error(204, "204: Invalid pointer operation$");
END ErrInvalidPointerOperation;

(*$L SYS.RDF*)

END SYSTEM.