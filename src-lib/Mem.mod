MODULE Mem;
(* Heap wrapper enforcing the DOS 65000-byte-per-call limit.
   Provides typed Alloc/Free through an open-array byte pointer.
   Callers use SYSTEM.ADR on the result to get a raw far address. *)

(* Allocate 'size' bytes.  Returns NIL if size <= 0 or > 65000. *)

PROCEDURE Alloc*(size: INTEGER): SYSTEM.ADDRESS;
BEGIN
  IF (size <= 0) OR (size > 65000) THEN
    RETURN NIL
  END;
  RETURN SYSTEM.Alloc(size)
END Alloc;

(* Free a block previously returned by Alloc. *)
PROCEDURE Free*(p: SYSTEM.ADDRESS);
BEGIN
  IF p # NIL THEN
    SYSTEM.Free(p)
  END
END Free;

END Mem.
