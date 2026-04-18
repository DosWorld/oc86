MODULE TestProcVar;

IMPORT Out;

VAR
    failed: INTEGER;
    total: INTEGER;

PROCEDURE Check(name: ARRAY OF CHAR; cond: BOOLEAN);
BEGIN
    INC(total);
    IF ~cond THEN
        Out.String("FAIL: "); Out.String(name); Out.Ln;
        INC(failed)
    END
END Check;

PROCEDURE Add(a, b: INTEGER): INTEGER; FAR;
BEGIN RETURN a + b END Add;

PROCEDURE Sub(a, b: INTEGER): INTEGER; FAR;
BEGIN RETURN a - b END Sub;

PROCEDURE Double(x: INTEGER): INTEGER; FAR;
BEGIN RETURN x + x END Double;

PROCEDURE PrintNum(n: INTEGER); FAR;
BEGIN Out.Int(n, 0) END PrintNum;

PROCEDURE SumThree(a, b, c: INTEGER): INTEGER; FAR;
BEGIN RETURN a + b + c END SumThree;

PROCEDURE ApplyTwice(f: PROCEDURE(x: INTEGER): INTEGER; val: INTEGER): INTEGER; FAR;
BEGIN RETURN f(f(val)) END ApplyTwice;

PROCEDURE ChooseOperation(op: INTEGER): PROCEDURE(a,b: INTEGER): INTEGER; FAR;
    VAR res: PROCEDURE(a,b: INTEGER): INTEGER;
BEGIN
    IF op = 1 THEN res := Add
    ELSE res := Sub
    END;
    RETURN res
END ChooseOperation;

PROCEDURE CallWithTwo(f: PROCEDURE(a,b: INTEGER): INTEGER; a,b: INTEGER): INTEGER; FAR;
BEGIN RETURN f(a,b) END CallWithTwo;

TYPE
    ProcIntInt = PROCEDURE(a,b: INTEGER): INTEGER;
    ProcInt    = PROCEDURE(x: INTEGER): INTEGER;
    PProcInt   = POINTER TO ProcInt;

    RecWithProc = RECORD
        name: ARRAY 16 OF CHAR;
        func: PROCEDURE(a,b: INTEGER): INTEGER
    END;

VAR
    fn:     PROCEDURE(a,b: INTEGER): INTEGER;
    fn1:    PROCEDURE(x: INTEGER): INTEGER;
    act:    PROCEDURE(n: INTEGER);
    procArr: ARRAY 4 OF PROCEDURE(x: INTEGER): INTEGER;
    rec:    RecWithProc;
    pproc:  PProcInt;
    nilProc: PROCEDURE;

PROCEDURE FactorialRec(n: INTEGER): INTEGER; FAR;
BEGIN
    IF n <= 1 THEN RETURN 1
    ELSE RETURN n * fn1(n-1)
    END
END FactorialRec;

PROCEDURE RunAllTests*;
    VAR r, i: INTEGER; p: ProcIntInt;
BEGIN
    failed := 0; total := 0;

    fn := Add;
    r := fn(3,4);
    Check("FAR Add(3,4)=7", r = 7);

    fn := Sub;
    r := fn(10,3);
    Check("FAR Sub(10,3)=7", r = 7);

    fn := Add;
    r := fn(fn(1,2), fn(3,4));
    Check("FAR nested Add(Add(1,2),Add(3,4))=10", r = 10);

    fn1 := Double;
    r := fn1(5);
    Check("FAR Double(5)=10", r = 10);
    r := fn1(fn1(3));
    Check("FAR Double(Double(3))=12", r = 12);

    r := CallWithTwo(Add, 5, 6);
    Check("CallWithTwo(Add,5,6)=11", r = 11);
    r := CallWithTwo(Sub, 20, 7);
    Check("CallWithTwo(Sub,20,7)=13", r = 13);
    r := ApplyTwice(Double, 3);
    Check("ApplyTwice(Double,3)=12", r = 12);

    p := ChooseOperation(1);
    r := p(100, 200);
    Check("ChooseOperation(1) returns Add -> 300", r = 300);
    p := ChooseOperation(2);
    r := p(100, 200);
    Check("ChooseOperation(2) returns Sub -> -100", r = -100);

    procArr[0] := Double;
    procArr[1] := Double;
    procArr[2] := Double;
    procArr[3] := Double;
    r := procArr[0](5) + procArr[1](5) + procArr[2](5) + procArr[3](5);
    Check("Array of proc vars: 4*Double(5)=40", r = 40);

    rec.func := Add;
    r := rec.func(7,8);
    Check("Record field proc var Add(7,8)=15", r = 15);
    rec.func := Sub;
    r := rec.func(15,3);
    Check("Record field proc var Sub(15,3)=12", r = 12);

    NEW(pproc);
    pproc^ := Double;
    r := pproc^(9);
    Check("Pointer to proc var -> Double(9)=18", r = 18);

    nilProc := NIL;
    Check("Assign NIL works", nilProc = NIL);
    fn := NIL;
    Check("FAR proc var assigned NIL", fn = NIL);

    fn := Add; r := fn(1,2); Check("After Add: 3", r = 3);
    fn := Sub; r := fn(5,2); Check("After Sub: 3", r = 3);
    fn := Add; r := fn(2,3); Check("After Add again: 5", r = 5);
    fn := Sub; r := fn(10,4); Check("After Sub again: 6", r = 6);

    fn := Add;
    r := fn(32767, 1);
    Check("Overflow: 32767+1 = -32768", r = -32768);
    r := fn(-32768, -1);
    Check("Underflow: -32768-1 = 32767", r = 32767);
    fn1 := Double;
    r := fn1(16384);
    Check("Double overflow: 2*16384 = -32768", r = -32768);

    fn1 := FactorialRec;
    r := fn1(5);
    Check("Recursion through proc var: factorial(5)=120", r = 120);

    Out.String("Total tests: "); Out.Int(total, 0); Out.Ln;
    Out.String("Failed: "); Out.Int(failed, 0); Out.Ln
END RunAllTests;

END TestProcVar.
