MODULE TestPointers;
(* Comprehensive pointer tests: NIL checks, pointer-to-record,
   pointer-to-array, pointer equality/comparison, pointer chains,
   pointer arithmetic via SYSTEM.ADR, VAR params of pointer type. *)

IMPORT Out;

VAR
  totalTests, passedTests: INTEGER;
  testResult: INTEGER;

PROCEDURE BeginTest(name: ARRAY OF CHAR);
BEGIN
  Out.String("Testing: "); Out.String(name); Out.Ln;
  testResult := 0
END BeginTest;

PROCEDURE Fail(msg: ARRAY OF CHAR; got, expected: INTEGER);
BEGIN
  testResult := 1;
  Out.String("  FAIL: "); Out.String(msg);
  Out.String(" got="); Out.Int(got, 0);
  Out.String(" expected="); Out.Int(expected, 0); Out.Ln
END Fail;

PROCEDURE Check(label: ARRAY OF CHAR; ok: BOOLEAN);
BEGIN
  IF ~ok THEN
    testResult := 1;
    Out.String("  FAIL: "); Out.String(label); Out.Ln
  END
END Check;

PROCEDURE EndTest;
BEGIN
  INC(totalTests);
  IF testResult = 0 THEN
    INC(passedTests); Out.String("  PASSED"); Out.Ln
  ELSE
    Out.String("  FAILED"); Out.Ln
  END
END EndTest;

(* ── Types used throughout ── *)

TYPE
  PRec  = POINTER TO RECORD a, b: INTEGER END;
  PArr  = POINTER TO ARRAY 10 OF INTEGER;
  PChar = POINTER TO ARRAY 16 OF CHAR;

  Node  = POINTER TO RECORD
    val:  INTEGER;
    next: Node
  END;

  BaseRec = RECORD tag: INTEGER END;
  ExtRec  = RECORD (BaseRec) extra: INTEGER END;

(* Global pointer vars to test global pointer load/store *)
VAR
  gp: PRec;
  gn: Node;

(* ── Test 1: NIL check on fresh pointer ── *)
PROCEDURE TestNilCheck*;
VAR p: PRec;
BEGIN
  BeginTest("Pointer NIL check");
  p := NIL;
  Check("p = NIL before NEW", p = NIL);
  NEW(p);
  Check("p # NIL after NEW", p # NIL);
  DISPOSE(p);
  p := NIL;
  Check("p = NIL after dispose+assign", p = NIL);
  EndTest
END TestNilCheck;

(* ── Test 2: Pointer-to-record field read/write ── *)
PROCEDURE TestPtrRecord*;
VAR p: PRec;
BEGIN
  BeginTest("Pointer-to-record field access");
  NEW(p);
  p^.a := 11; p^.b := 22;
  IF p^.a # 11 THEN Fail("a", p^.a, 11) END;
  IF p^.b # 22 THEN Fail("b", p^.b, 22) END;
  p^.a := p^.a + p^.b;
  IF p^.a # 33 THEN Fail("a+b", p^.a, 33) END;
  IF p^.b # 22 THEN Fail("b unchanged", p^.b, 22) END;
  DISPOSE(p);
  EndTest
END TestPtrRecord;

(* ── Test 3: Pointer-to-array element access ── *)
PROCEDURE TestPtrArray*;
VAR pa: PArr; i, sum: INTEGER;
BEGIN
  BeginTest("Pointer-to-array element access");
  NEW(pa);
  FOR i := 0 TO 9 DO pa^[i] := i * 3 END;
  IF pa^[0] # 0  THEN Fail("pa[0]", pa^[0], 0)  END;
  IF pa^[5] # 15 THEN Fail("pa[5]", pa^[5], 15) END;
  IF pa^[9] # 27 THEN Fail("pa[9]", pa^[9], 27) END;
  sum := 0;
  FOR i := 0 TO 9 DO sum := sum + pa^[i] END;
  IF sum # 135 THEN Fail("sum 0..9 *3", sum, 135) END;
  DISPOSE(pa);
  EndTest
END TestPtrArray;

(* ── Test 4: Pointer-to-CHAR array ── *)
PROCEDURE TestPtrCharArray*;
VAR pc: PChar; i: INTEGER;
BEGIN
  BeginTest("Pointer-to-CHAR array");
  NEW(pc);
  pc^[0] := 'H'; pc^[1] := 'i'; pc^[2] := CHR(0);
  Check("pc[0]='H'", pc^[0] = 'H');
  Check("pc[1]='i'", pc^[1] = 'i');
  Check("pc[2]=NUL", ORD(pc^[2]) = 0);
  pc^[0] := CHR(ORD(pc^[0]) + 1);  (* 'H' -> 'I' *)
  Check("pc[0]='I'", pc^[0] = 'I');
  DISPOSE(pc);
  EndTest
END TestPtrCharArray;

(* ── Test 5: Pointer equality and inequality ── *)
PROCEDURE TestPtrEquality*;
VAR p, q, r: PRec;
BEGIN
  BeginTest("Pointer equality / inequality");
  NEW(p); NEW(q);
  Check("p # q (different objects)", p # q);
  Check("p = p (self)", p = p);
  r := p;
  Check("r = p (alias)", r = p);
  Check("r # q", r # q);
  DISPOSE(p); DISPOSE(q);
  (* After dispose the pointer values are unspecified, but NIL tests still work *)
  p := NIL; q := NIL;
  Check("nil = nil", p = q);
  EndTest
END TestPtrEquality;

(* ── Test 6: Multiple allocation and independence ── *)
PROCEDURE TestMultiAlloc*;
VAR p1, p2, p3: PRec;
BEGIN
  BeginTest("Multiple allocations are independent");
  NEW(p1); NEW(p2); NEW(p3);
  p1^.a := 100; p2^.a := 200; p3^.a := 300;
  (* Verify no aliasing *)
  IF p1^.a # 100 THEN Fail("p1.a", p1^.a, 100) END;
  IF p2^.a # 200 THEN Fail("p2.a", p2^.a, 200) END;
  IF p3^.a # 300 THEN Fail("p3.a", p3^.a, 300) END;
  (* Modify p2 - p1 and p3 must be unchanged *)
  p2^.a := 999;
  IF p1^.a # 100 THEN Fail("p1.a still 100", p1^.a, 100) END;
  IF p3^.a # 300 THEN Fail("p3.a still 300", p3^.a, 300) END;
  DISPOSE(p1); DISPOSE(p2); DISPOSE(p3);
  EndTest
END TestMultiAlloc;

(* ── Test 7: Global pointer ── *)
PROCEDURE TestGlobalPointer*;
BEGIN
  BeginTest("Global pointer load/store");
  gp := NIL;
  Check("gp = NIL initially", gp = NIL);
  NEW(gp);
  Check("gp # NIL after NEW", gp # NIL);
  gp^.a := 42; gp^.b := -7;
  IF gp^.a # 42  THEN Fail("gp.a", gp^.a, 42)  END;
  IF gp^.b # -7  THEN Fail("gp.b", gp^.b, -7)  END;
  DISPOSE(gp);
  gp := NIL;
  EndTest
END TestGlobalPointer;

(* ── Test 8: Pointer passed by value (callee gets copy) ── *)
PROCEDURE ReadThroughPtr(p: PRec): INTEGER;
BEGIN RETURN p^.a + p^.b END ReadThroughPtr;

PROCEDURE TestPtrValueParam*;
VAR p: PRec; result: INTEGER;
BEGIN
  BeginTest("Pointer passed by value");
  NEW(p); p^.a := 5; p^.b := 7;
  result := ReadThroughPtr(p);
  IF result # 12 THEN Fail("a+b via ptr param", result, 12) END;
  DISPOSE(p);
  EndTest
END TestPtrValueParam;

(* ── Test 9: Pointer passed by VAR (callee can redirect) ── *)
PROCEDURE NewAndInit(VAR p: PRec; av, bv: INTEGER);
BEGIN
  NEW(p); p^.a := av; p^.b := bv
END NewAndInit;

PROCEDURE TestPtrVarParam*;
VAR p: PRec;
BEGIN
  BeginTest("Pointer passed as VAR param");
  p := NIL;
  NewAndInit(p, 8, 9);
  Check("p # NIL after NewAndInit", p # NIL);
  IF p^.a # 8 THEN Fail("a", p^.a, 8) END;
  IF p^.b # 9 THEN Fail("b", p^.b, 9) END;
  DISPOSE(p);
  EndTest
END TestPtrVarParam;

(* ── Test 10: Pointer chain (singly-linked list) ── *)
PROCEDURE TestPtrChain*;
VAR a, b, c, cur: Node; sum, count: INTEGER;
BEGIN
  BeginTest("Pointer chain traversal");
  NEW(a); NEW(b); NEW(c);
  a^.val := 1; a^.next := b;
  b^.val := 2; b^.next := c;
  c^.val := 3; c^.next := NIL;
  (* traverse *)
  sum := 0; count := 0;
  cur := a;
  WHILE cur # NIL DO
    sum := sum + cur^.val;
    INC(count);
    cur := cur^.next
  END;
  IF count # 3 THEN Fail("count", count, 3) END;
  IF sum   # 6 THEN Fail("sum",   sum,   6) END;
  (* follow chain manually *)
  IF a^.next^.val # 2 THEN Fail("a->b.val", a^.next^.val, 2) END;
  IF a^.next^.next^.val # 3 THEN Fail("a->b->c.val", a^.next^.next^.val, 3) END;
  Check("c.next = NIL", c^.next = NIL);
  DISPOSE(a); DISPOSE(b); DISPOSE(c);
  EndTest
END TestPtrChain;

(* ── Test 11: NIL pointer field in record ── *)
PROCEDURE TestNilField*;
VAR p, q: Node;
BEGIN
  BeginTest("NIL pointer field in heap record");
  NEW(p);
  p^.val := 7;
  p^.next := NIL;
  Check("p.next = NIL", p^.next = NIL);
  NEW(q);
  q^.val := 8; q^.next := p;
  Check("q.next # NIL", q^.next # NIL);
  IF q^.next^.val # 7 THEN Fail("q.next.val", q^.next^.val, 7) END;
  DISPOSE(p); DISPOSE(q);
  EndTest
END TestNilField;

(* ── Test 12: Pointer re-use (dispose old, allocate new) ── *)
PROCEDURE TestPtrReuse*;
VAR p: PRec; firstA: INTEGER;
BEGIN
  BeginTest("Pointer reuse after DISPOSE");
  NEW(p); p^.a := 111; p^.b := 222;
  firstA := p^.a;
  DISPOSE(p);
  (* allocate new object at potentially same location *)
  NEW(p); p^.a := 333; p^.b := 444;
  IF p^.a # 333 THEN Fail("new a", p^.a, 333) END;
  IF p^.b # 444 THEN Fail("new b", p^.b, 444) END;
  DISPOSE(p);
  EndTest
END TestPtrReuse;

(* ── Test 13: Self-referential pointer (recursive record via pointer) ── *)
PROCEDURE TestSelfRef*;
VAR root, n2, n3: Node; cur: Node; depth: INTEGER;
BEGIN
  BeginTest("Self-referential record pointer");
  NEW(root); NEW(n2); NEW(n3);
  root^.val := 10; root^.next := n2;
  n2^.val   := 20; n2^.next   := n3;
  n3^.val   := 30; n3^.next   := NIL;
  depth := 0; cur := root;
  WHILE cur # NIL DO INC(depth); cur := cur^.next END;
  IF depth # 3 THEN Fail("depth", depth, 3) END;
  IF root^.next^.next^.val # 30 THEN
    Fail("root->n2->n3.val", root^.next^.next^.val, 30)
  END;
  DISPOSE(root); DISPOSE(n2); DISPOSE(n3);
  EndTest
END TestSelfRef;

(* ── Test 14: Pointer to large array -- all elements accessible ── *)
PROCEDURE TestLargeArrayPtr*;
TYPE BigArr = POINTER TO ARRAY 50 OF INTEGER;
VAR pa: BigArr; i, sum: INTEGER;
BEGIN
  BeginTest("Pointer to large array (50 elements)");
  NEW(pa);
  FOR i := 0 TO 49 DO pa^[i] := i + 1 END;
  (* sum 1..50 = 1275 *)
  sum := 0;
  FOR i := 0 TO 49 DO sum := sum + pa^[i] END;
  IF sum # 1275 THEN Fail("sum 1..50", sum, 1275) END;
  IF pa^[0]  # 1  THEN Fail("pa[0]",  pa^[0],  1)  END;
  IF pa^[49] # 50 THEN Fail("pa[49]", pa^[49], 50) END;
  DISPOSE(pa);
  EndTest
END TestLargeArrayPtr;

(* ── Test 15: Global Node pointer used across calls ── *)
PROCEDURE BuildGlobalList(n: INTEGER);
VAR i: INTEGER; p: Node;
BEGIN
  gn := NIL;
  FOR i := 1 TO n DO
    NEW(p); p^.val := i; p^.next := gn; gn := p
  END
END BuildGlobalList;

PROCEDURE FreeGlobalList;
VAR p: Node;
BEGIN
  WHILE gn # NIL DO p := gn^.next; DISPOSE(gn); gn := p END
END FreeGlobalList;

PROCEDURE SumGlobalList(): INTEGER;
VAR s: INTEGER; p: Node;
BEGIN
  s := 0; p := gn;
  WHILE p # NIL DO s := s + p^.val; p := p^.next END;
  RETURN s
END SumGlobalList;

PROCEDURE TestGlobalList*;
BEGIN
  BeginTest("Global pointer list build/sum/free");
  BuildGlobalList(5);     (* list: 5->4->3->2->1 *)
  IF SumGlobalList() # 15 THEN Fail("sum 1..5", SumGlobalList(), 15) END;
  FreeGlobalList;
  Check("gn = NIL after free", gn = NIL);
  EndTest
END TestGlobalList;

PROCEDURE PrintSummary;
BEGIN
  Out.Ln;
  Out.String("========================================"); Out.Ln;
  Out.String("Total tests: "); Out.Int(totalTests, 0); Out.Ln;
  Out.String("Passed: ");      Out.Int(passedTests, 0); Out.Ln;
  Out.String("Failed: ");      Out.Int(totalTests - passedTests, 0); Out.Ln;
  Out.String("========================================"); Out.Ln
END PrintSummary;

PROCEDURE RunAllTests*;
BEGIN
  Out.String("========================================"); Out.Ln;
  Out.String("Starting Pointer Tests"); Out.Ln;
  Out.String("========================================"); Out.Ln;
  totalTests := 0; passedTests := 0;

  TestNilCheck;
  TestPtrRecord;
  TestPtrArray;
  TestPtrCharArray;
  TestPtrEquality;
  TestMultiAlloc;
  TestGlobalPointer;
  TestPtrValueParam;
  TestPtrVarParam;
  TestPtrChain;
  TestNilField;
  TestPtrReuse;
  TestSelfRef;
  TestLargeArrayPtr;
  TestGlobalList;

  PrintSummary
END RunAllTests;

END TestPointers.
