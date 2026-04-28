MODULE TestTypelessVar;
IMPORT Out;

VAR failed: INTEGER;
    gInt: INTEGER;
    gInt2: INTEGER;
    gInt3: INTEGER;

PROCEDURE Fail(n: INTEGER);
BEGIN Out.String("FAIL "); Out.Int(n, 0); Out.Ln; INC(failed) END Fail;

(* Callee receives the ADDRESS (far pointer) of the actual argument *)
PROCEDURE GetAddr(VAR p): SYSTEM.ADDRESS;
BEGIN RETURN p END GetAddr;

(* Write an INTEGER through a typeless VAR address *)
PROCEDURE WriteInt(VAR p; val: INTEGER);
VAR ip: POINTER TO INTEGER;
BEGIN ip := p; ip^ := val END WriteInt;

(* Read an INTEGER through a typeless VAR address *)
PROCEDURE ReadInt(VAR p): INTEGER;
VAR ip: POINTER TO INTEGER;
BEGIN ip := p; RETURN ip^ END ReadInt;

(* Swap two integers via their typeless VAR addresses *)
PROCEDURE SwapInt(VAR a; VAR b);
VAR tmp: INTEGER;
BEGIN
    tmp := ReadInt(a);
    WriteInt(a, ReadInt(b));
    WriteInt(b, tmp)
END SwapInt;

(* Count calls — callee accumulates into a typeless VAR counter *)
PROCEDURE Bump(VAR counter);
VAR ip: POINTER TO INTEGER;
BEGIN ip := counter; INC(ip^) END Bump;

PROCEDURE RunAllTests*;
VAR addr1, addr2: SYSTEM.ADDRESS;
    lInt: INTEGER;
BEGIN
    failed := 0;

    (* Test 1: GetAddr on global returns non-zero address *)
    gInt := 42;
    addr1 := GetAddr(gInt);
    IF addr1 = 0 THEN Fail(1) END;

    (* Test 2: two distinct globals have distinct addresses *)
    gInt2 := 0;
    addr1 := GetAddr(gInt);
    addr2 := GetAddr(gInt2);
    IF addr1 = addr2 THEN Fail(2) END;

    (* Test 3: ReadInt via typeless VAR reads correct value from global *)
    gInt := 99;
    IF ReadInt(gInt) # 99 THEN Fail(3) END;

    (* Test 4: WriteInt via typeless VAR modifies the global *)
    WriteInt(gInt, 77);
    IF gInt # 77 THEN Fail(4) END;

    (* Test 5: GetAddr on local returns non-zero address *)
    lInt := 55;
    addr1 := GetAddr(lInt);
    IF addr1 = 0 THEN Fail(5) END;

    (* Test 6: GetAddr stable — same global gives same address on repeated calls *)
    gInt := 5;
    addr1 := GetAddr(gInt);
    addr2 := GetAddr(gInt);
    IF addr1 # addr2 THEN Fail(6) END;

    (* Test 7: SwapInt swaps two globals via typeless VAR *)
    gInt := 10; gInt2 := 20;
    SwapInt(gInt, gInt2);
    IF gInt # 20 THEN Fail(7) END;
    IF gInt2 # 10 THEN Fail(71) END;

    (* Test 8: Bump increments global via typeless VAR address *)
    gInt3 := 0;
    Bump(gInt3); Bump(gInt3); Bump(gInt3);
    IF gInt3 # 3 THEN Fail(8) END;

    (* Test 9: multiple typeless VAR params, mixed with typed params *)
    gInt := 5; gInt2 := 7;
    WriteInt(gInt, ReadInt(gInt2));
    IF gInt # 7 THEN Fail(9) END;

    Out.String("Failed: "); Out.Int(failed, 0); Out.Ln
END RunAllTests;

END TestTypelessVar.
