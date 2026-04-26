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

(* PUT(a: ADDRESS; x: INTEGER)
   Stores the word x to the far pointer address a.
   Caller pushes a as {segment:2, offset:2} (deepest first), then x (2 bytes).
   Stack on entry (POP order): x, offset, segment.
   POP AX=x; POP BX=offset; POP ES=segment; MOV ES:[BX],AX. SP balanced. *)
PROCEDURE PUT*(a: ADDRESS; x: INTEGER);
  INLINE(058H, 05BH, 007H, 026H, 089H, 007H);

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

PROCEDURE PORTOUT*(port : INTEGER; b: BYTE);
  INLINE(058H,05AH,0EEH);

PROCEDURE PORTIN*(port : INTEGER) : BYTE;
  INLINE(05AH,0ECH);


PROCEDURE Message(msg : ARRAY OF CHAR);
VAR
    r : Registers;
BEGIN
    r.DS := SEG(msg);
    r.DX := OFS(msg);
    r.AX := 0900H;
    Intr(21H, r);
END Message;

PROCEDURE Error(msg : ARRAY OF CHAR);
BEGIN
    Message("Run-time error: $");
    Message(msg);
    Halt(2)
END Error;

PROCEDURE ErrIndexOutOfBounds*;
BEGIN
    Error("Array index out of bounds$");
END ErrIndexOutOfBounds;

(*$L SYS.RDF*)

END SYSTEM.