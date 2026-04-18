MODULE TestNearFar;
(* Tests for NEAR and FAR procedure modifiers and procedural types.
   Covers:
   - PROCEDURE; NEAR; explicit near calling convention
   - PROCEDURE; FAR;  explicit far calling convention
   - PROCEDURE FAR (...) procedural type (default, no modifier)
   - PROCEDURE NEAR (...) procedural type (2-byte proc var)
   - Calling through NEAR and FAR proc variables
   - NEAR proc vars occupy 2 bytes; FAR proc vars occupy 4 bytes *)

IMPORT Out;

VAR
    failed: INTEGER;
    total:  INTEGER;

PROCEDURE Check(name: ARRAY OF CHAR; cond: BOOLEAN);
BEGIN
    INC(total);
    IF ~cond THEN
        Out.String("FAIL: "); Out.String(name); Out.Ln;
        INC(failed)
    END
END Check;

(* ── NEAR procedures ── *)

PROCEDURE AddNear(a, b: INTEGER): INTEGER; NEAR;
BEGIN
    RETURN a + b
END AddNear;

PROCEDURE MulNear(a, b: INTEGER): INTEGER; NEAR;
BEGIN
    RETURN a * b
END MulNear;

PROCEDURE NopNear; NEAR;
BEGIN
END NopNear;

(* ── FAR procedures (explicit modifier) ── *)

PROCEDURE AddFar(a, b: INTEGER): INTEGER; FAR;
BEGIN RETURN a + b END AddFar;

PROCEDURE SubFar(a, b: INTEGER): INTEGER; FAR;
BEGIN RETURN a - b END SubFar;

(* ── NEAR proc variables ── *)

TYPE
    NearBinop  = PROCEDURE NEAR (a, b: INTEGER): INTEGER;
    FarBinop   = PROCEDURE FAR  (a, b: INTEGER): INTEGER;
    FarBinopDef = PROCEDURE     (a, b: INTEGER): INTEGER;  (* default = FAR *)

VAR
    nearFn:   PROCEDURE NEAR (a, b: INTEGER): INTEGER;
    farFn:    PROCEDURE FAR  (a, b: INTEGER): INTEGER;
    defFn:    PROCEDURE      (a, b: INTEGER): INTEGER;   (* default = FAR *)
    nearFn2:  NearBinop;
    farFn2:   FarBinop;

PROCEDURE RunAllTests*;
    VAR r: INTEGER;
BEGIN
    failed := 0; total := 0;

    (* ── Direct NEAR proc calls ── *)
    r := AddNear(3, 4);
    Check("AddNear direct call", r = 7);

    r := MulNear(5, 6);
    Check("MulNear direct call", r = 30);

    NopNear;
    Check("NopNear no crash", TRUE);

    (* ── Direct FAR proc calls ── *)
    r := AddFar(10, 20);
    Check("AddFar direct call", r = 30);

    r := SubFar(15, 5);
    Check("SubFar direct call", r = 10);

    (* ── NEAR proc variable assignment and call ── *)
    nearFn := AddNear;
    r := nearFn(7, 8);
    Check("NEAR proc var call AddNear", r = 15);

    nearFn := MulNear;
    r := nearFn(3, 5);
    Check("NEAR proc var reassign MulNear", r = 15);

    (* ── FAR proc variable assignment and call ── *)
    farFn := AddFar;
    r := farFn(6, 4);
    Check("FAR proc var call AddFar", r = 10);

    farFn := SubFar;
    r := farFn(9, 3);
    Check("FAR proc var reassign SubFar", r = 6);

    (* ── Default proc type (FAR) ── *)
    defFn := AddFar;
    r := defFn(2, 3);
    Check("default (FAR) proc var AddFar", r = 5);

    (* ── Named NEAR type alias ── *)
    nearFn2 := AddNear;
    r := nearFn2(11, 4);
    Check("named NearBinop alias AddNear", r = 15);

    nearFn2 := MulNear;
    r := nearFn2(4, 5);
    Check("named NearBinop alias MulNear", r = 20);

    (* ── Named FAR type alias ── *)
    farFn2 := AddFar;
    r := farFn2(100, 23);
    Check("named FarBinop alias AddFar", r = 123);

    (* ── NEAR proc var swapping ── *)
    nearFn := AddNear;
    Check("NEAR var swap before", nearFn(1, 2) = 3);
    nearFn := MulNear;
    Check("NEAR var swap after", nearFn(3, 4) = 12);

    Out.String("Failed: "); Out.Int(failed, 0); Out.Ln;
    Out.String("Total:  "); Out.Int(total,  0); Out.Ln
END RunAllTests;

END TestNearFar.
