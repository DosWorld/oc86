MODULE TestRecords;

IMPORT Out;

VAR
  totalTests, passedTests: INTEGER;
  testResult: INTEGER;

PROCEDURE InitTest(name: ARRAY OF CHAR);
BEGIN
  Out.String("Testing: ");
  Out.String(name);
  Out.Ln;
  testResult := 0
END InitTest;

PROCEDURE Assert(condition: BOOLEAN; msg: ARRAY OF CHAR);
BEGIN
  IF ~condition THEN
    testResult := 1;
    Out.String("  FAILED: ");
    Out.String(msg);
    Out.Ln
  END
END Assert;

PROCEDURE AssertInt(actual, expected: INTEGER; msg: ARRAY OF CHAR);
BEGIN
  IF actual # expected THEN
    testResult := 1;
    Out.String("  FAILED: ");
    Out.String(msg);
    Out.String(" expected ");
    Out.Int(expected, 0);
    Out.String(" got ");
    Out.Int(actual, 0);
    Out.Ln
  END
END AssertInt;

PROCEDURE ReportTest;
BEGIN
  IF testResult = 0 THEN
    Out.String("  PASSED");
    Out.Ln;
    INC(passedTests)
  ELSE
    Out.String("  TEST FAILED");
    Out.Ln
  END;
  INC(totalTests);
  Out.Ln
END ReportTest;

(* ── Basic record field access ── *)

TYPE
  Point = RECORD
    x : INTEGER;
    y : INTEGER
  END;

VAR
  gPoint: Point;

PROCEDURE TestBasicFields*;
VAR
  p: Point;
BEGIN
  InitTest("Basic record fields");
  p.x := 10;
  p.y := 20;
  AssertInt(p.x, 10, "p.x");
  AssertInt(p.y, 20, "p.y");
  p.x := p.x + p.y;
  AssertInt(p.x, 30, "p.x+p.y");
  ReportTest
END TestBasicFields;

PROCEDURE TestGlobalRecord*;
BEGIN
  InitTest("Global record fields");
  gPoint.x := 100;
  gPoint.y := 200;
  AssertInt(gPoint.x, 100, "gPoint.x");
  AssertInt(gPoint.y, 200, "gPoint.y");
  gPoint.y := gPoint.x - gPoint.y;
  AssertInt(gPoint.y, -100, "gPoint.x-gPoint.y");
  ReportTest
END TestGlobalRecord;

(* ── Record passed by VAR param ── *)

PROCEDURE SetPoint(VAR p: Point; ax, ay: INTEGER);
BEGIN
  p.x := ax;
  p.y := ay
END SetPoint;

PROCEDURE SumPoint(VAR p: Point): INTEGER;
BEGIN
  RETURN p.x + p.y
END SumPoint;

PROCEDURE TestVarParam*;
VAR
  p: Point;
BEGIN
  InitTest("Record VAR param");
  SetPoint(p, 3, 4);
  AssertInt(p.x, 3, "x after SetPoint");
  AssertInt(p.y, 4, "y after SetPoint");
  AssertInt(SumPoint(p), 7, "SumPoint");
  SetPoint(p, -5, 5);
  AssertInt(SumPoint(p), 0, "SumPoint zero");
  ReportTest
END TestVarParam;

(* ── Nested record fields ── *)

TYPE
  Rect = RECORD
    topLeft     : Point;
    bottomRight : Point
  END;

PROCEDURE TestNestedRecord*;
VAR
  r: Rect;
BEGIN
  InitTest("Nested record fields");
  r.topLeft.x     := 0;
  r.topLeft.y     := 0;
  r.bottomRight.x := 80;
  r.bottomRight.y := 25;
  AssertInt(r.topLeft.x,     0,  "tl.x");
  AssertInt(r.topLeft.y,     0,  "tl.y");
  AssertInt(r.bottomRight.x, 80, "br.x");
  AssertInt(r.bottomRight.y, 25, "br.y");
  r.topLeft.x := r.bottomRight.x - 10;
  AssertInt(r.topLeft.x, 70, "tl.x updated");
  ReportTest
END TestNestedRecord;

(* ── Record field alignment (BYTE fields interleaved with INTEGER) ── *)

TYPE
  Mixed = RECORD
    a : BYTE;
    b : INTEGER;
    c : BYTE;
    d : INTEGER
  END;

PROCEDURE TestAlignment*;
VAR
  m: Mixed;
BEGIN
  InitTest("Record field alignment");
  m.a := 1;
  m.b := 1000;
  m.c := 2;
  m.d := 2000;
  Assert(ORD(m.a) = 1,    "m.a");
  AssertInt(m.b, 1000,    "m.b");
  Assert(ORD(m.c) = 2,    "m.c");
  AssertInt(m.d, 2000,    "m.d");
  (* modify one field and verify others unchanged *)
  m.b := m.b + m.d;
  AssertInt(m.b, 3000,    "m.b after add");
  Assert(ORD(m.a) = 1,    "m.a unchanged");
  Assert(ORD(m.c) = 2,    "m.c unchanged");
  ReportTest
END TestAlignment;

(* ── Multiple records independent ── *)

PROCEDURE TestMultipleRecords*;
VAR
  p1, p2, p3: Point;
BEGIN
  InitTest("Multiple independent records");
  p1.x := 1; p1.y := 2;
  p2.x := 3; p2.y := 4;
  p3.x := p1.x + p2.x;
  p3.y := p1.y + p2.y;
  AssertInt(p3.x, 4, "p3.x");
  AssertInt(p3.y, 6, "p3.y");
  (* modify p1 — p3 must be unaffected *)
  p1.x := 99;
  AssertInt(p3.x, 4, "p3.x unchanged");
  ReportTest
END TestMultipleRecords;

(* ── Record with CHAR and BYTE fields ── *)

TYPE
  Tagged = RECORD
    id  : INTEGER;
    tag : CHAR;
    val : INTEGER
  END;

  (* CHAR field adjacent to an ARRAY field — exercises byte-load correctness
     after REP MOVSB array assignment (the bug fixed: array write must not
     corrupt adjacent CHAR when reading it back as a byte not a word). *)
  Named = RECORD
    id   : INTEGER;
    tag  : CHAR;
    name : ARRAY 8 OF CHAR
  END;

VAR
  gNamed: Named;

PROCEDURE TestCharField*;
VAR
  t: Tagged;
BEGIN
  InitTest("Record with CHAR field");
  t.id  := 42;
  t.tag := 'Z';
  t.val := 100;
  AssertInt(t.id,       42,  "id");
  Assert(ORD(t.tag) = 90,   "tag 'Z'");
  AssertInt(t.val,      100, "val");
  t.tag := CHR(65);
  Assert(ORD(t.tag) = 65,   "tag 'A'");
  AssertInt(t.id,       42,  "id unchanged");
  AssertInt(t.val,      100, "val unchanged");
  ReportTest
END TestCharField;

PROCEDURE TestCharAdjacentArray*;
(* Regression test: CHAR field followed by ARRAY field.
   Before the fix, ORD(n.tag) after n.name := "hello" returned a 16-bit
   word (tag byte | name[0] byte) instead of just the tag byte. *)
VAR
  n: Named;
BEGIN
  InitTest("CHAR field adjacent to ARRAY field");
  n.id   := 99;
  n.tag  := 'A';
  n.name := "hello";
  AssertInt(n.id,         99, "id after name assign");
  Assert(ORD(n.tag) = 65,    "tag after name assign");
  Assert(n.name[0] = 'h',    "name[0]");
  Assert(n.name[4] = 'o',    "name[4]");
  (* global version — exercises M_GLOBAL byte load path *)
  gNamed.id   := 7;
  gNamed.tag  := 'X';
  gNamed.name := "world";
  AssertInt(gNamed.id,      7,  "global id");
  Assert(ORD(gNamed.tag) = 88, "global tag after name assign");
  Assert(gNamed.name[0] = 'w', "global name[0]");
  Assert(gNamed.name[4] = 'd', "global name[4]");
  ReportTest
END TestCharAdjacentArray;

(* ── Record extension (base type) ── *)

TYPE
  Animal = RECORD
    legs : INTEGER;
    sound: INTEGER   (* 0=bark 1=meow 2=moo *)
  END;

  Dog = RECORD (Animal)
    tricks: INTEGER
  END;

  Cat = RECORD (Animal)
    indoor: INTEGER  (* 0=outdoor 1=indoor *)
  END;

PROCEDURE InitAnimal(VAR a: Animal; legs, sound: INTEGER);
BEGIN
  a.legs  := legs;
  a.sound := sound
END InitAnimal;

PROCEDURE TestRecordExtension*;
VAR
  d: Dog;
  c: Cat;
BEGIN
  InitTest("Record extension (base type)");
  (* Dog inherits Animal fields *)
  d.legs   := 4;
  d.sound  := 0;   (* bark *)
  d.tricks := 3;
  AssertInt(d.legs,   4, "dog.legs");
  AssertInt(d.sound,  0, "dog.sound");
  AssertInt(d.tricks, 3, "dog.tricks");
  (* Cat inherits Animal fields *)
  c.legs   := 4;
  c.sound  := 1;   (* meow *)
  c.indoor := 1;
  AssertInt(c.legs,   4, "cat.legs");
  AssertInt(c.sound,  1, "cat.sound");
  AssertInt(c.indoor, 1, "cat.indoor");
  (* Modify base field via extended record *)
  d.legs := d.legs * 2;
  AssertInt(d.legs,   8, "dog.legs*2");
  AssertInt(d.tricks, 3, "dog.tricks unchanged");
  ReportTest
END TestRecordExtension;

(* ── Base fields via VAR param procedure ── *)

PROCEDURE AddLegs(VAR a: Animal; extra: INTEGER);
BEGIN
  a.legs := a.legs + extra
END AddLegs;

PROCEDURE TestExtensionVarParam*;
VAR
  d: Dog;
BEGIN
  InitTest("Extension: base fields via VAR param");
  d.legs   := 4;
  d.sound  := 0;
  d.tricks := 5;
  AddLegs(d, 2);
  AssertInt(d.legs,   6, "legs after AddLegs");
  AssertInt(d.tricks, 5, "tricks unchanged");
  ReportTest
END TestExtensionVarParam;

(* ── Two-level extension ── *)

TYPE
  Vehicle = RECORD
    wheels : INTEGER;
    speed  : INTEGER
  END;

  MotorVehicle = RECORD (Vehicle)
    hp : INTEGER
  END;

  Car = RECORD (MotorVehicle)
    doors : INTEGER
  END;

PROCEDURE TestTwoLevelExtension*;
VAR
  car: Car;
BEGIN
  InitTest("Two-level record extension");
  car.wheels := 4;
  car.speed  := 120;
  car.hp     := 150;
  car.doors  := 4;
  AssertInt(car.wheels, 4,   "car.wheels");
  AssertInt(car.speed,  120, "car.speed");
  AssertInt(car.hp,     150, "car.hp");
  AssertInt(car.doors,  4,   "car.doors");
  car.speed  := car.speed + car.hp;
  AssertInt(car.speed, 270, "car.speed+hp");
  AssertInt(car.wheels, 4,  "car.wheels unchanged");
  ReportTest
END TestTwoLevelExtension;

(* ── Heap-allocated records ── *)

TYPE
  PNode = POINTER TO RECORD
    val  : INTEGER;
    next : PNode
  END;

PROCEDURE TestHeapRecord*;
VAR
  p, q: PNode;
BEGIN
  InitTest("Heap-allocated records (NEW/DISPOSE)");
  NEW(p);
  Assert(p # NIL, "p not nil");
  p^.val  := 42;
  p^.next := NIL;
  AssertInt(p^.val, 42, "p.val");
  Assert(p^.next = NIL, "p.next nil");

  NEW(q);
  Assert(q # NIL, "q not nil");
  q^.val  := 99;
  q^.next := p;
  AssertInt(q^.val,    99, "q.val");
  AssertInt(q^.next^.val, 42, "q.next.val");

  DISPOSE(q);
  DISPOSE(p);
  ReportTest
END TestHeapRecord;

(* ── Heap-allocated extended record ── *)

TYPE
  PDog = POINTER TO Dog;

PROCEDURE TestHeapExtension*;
VAR
  pd: PDog;
BEGIN
  InitTest("Heap-allocated extended record");
  NEW(pd);
  Assert(pd # NIL, "pd not nil");
  pd^.legs   := 4;
  pd^.sound  := 0;
  pd^.tricks := 7;
  AssertInt(pd^.legs,   4, "pd.legs");
  AssertInt(pd^.sound,  0, "pd.sound");
  AssertInt(pd^.tricks, 7, "pd.tricks");
  pd^.legs := pd^.legs - 1;
  AssertInt(pd^.legs, 3, "pd.legs dec");
  DISPOSE(pd);
  ReportTest
END TestHeapExtension;

(* ── Linked list with self-referential pointer ── *)

PROCEDURE TestLinkedList*;
VAR
  a, b, c, cur: PNode;
  sum, count: INTEGER;
BEGIN
  InitTest("Linked list (self-referential)");
  NEW(a); NEW(b); NEW(c);
  a^.val := 10; a^.next := b;
  b^.val := 20; b^.next := c;
  c^.val := 30; c^.next := NIL;

  sum := 0; count := 0;
  cur := a;
  WHILE cur # NIL DO
    sum   := sum + cur^.val;
    count := count + 1;
    cur   := cur^.next
  END;
  AssertInt(count, 3,  "list length");
  AssertInt(sum,   60, "list sum");

  DISPOSE(a); DISPOSE(b); DISPOSE(c);
  ReportTest
END TestLinkedList;

PROCEDURE PrintSummary;
BEGIN
  Out.Ln;
  Out.String("========================================");
  Out.Ln;
  Out.String("Total tests: ");
  Out.Int(totalTests, 0);
  Out.Ln;
  Out.String("Passed: ");
  Out.Int(passedTests, 0);
  Out.Ln;
  Out.String("Failed: ");
  Out.Int(totalTests - passedTests, 0);
  Out.Ln;
  Out.String("========================================");
  Out.Ln
END PrintSummary;

PROCEDURE RunAllTests*;
BEGIN
  Out.String("========================================");
  Out.Ln;
  Out.String("Starting Record Tests");
  Out.Ln;
  Out.String("========================================");
  Out.Ln;
  totalTests  := 0;
  passedTests := 0;

  TestBasicFields;
  TestGlobalRecord;
  TestVarParam;
  TestNestedRecord;
  TestAlignment;
  TestMultipleRecords;
  TestCharField;
  TestCharAdjacentArray;
  TestRecordExtension;
  TestExtensionVarParam;
  TestTwoLevelExtension;
  TestHeapRecord;
  TestHeapExtension;
  TestLinkedList;

  PrintSummary
END RunAllTests;

END TestRecords.
