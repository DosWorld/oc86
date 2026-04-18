MODULE TestDynamic;

IMPORT Out;

CONST
  Ok*         = 0;
  ErrList*    = 1;
  ErrTree*    = 2;
  ErrPtrDeref* = 3;

VAR
  totalTests, passedTests: INTEGER;
  testResult: INTEGER;

PROCEDURE InitTest(name: ARRAY OF CHAR);
BEGIN
  Out.String("Testing: "); Out.String(name); Out.Ln;
  testResult := Ok
END InitTest;

PROCEDURE Assert(cond: BOOLEAN; code: INTEGER);
BEGIN
  IF ~cond THEN
    testResult := code;
    Out.String("  FAILED (code "); Out.Int(code, 0); Out.String(")"); Out.Ln
  END
END Assert;

PROCEDURE AssertInt(actual, expected, code: INTEGER);
BEGIN
  IF actual # expected THEN
    testResult := code;
    Out.String("  FAILED: expected "); Out.Int(expected, 0);
    Out.String(", got "); Out.Int(actual, 0); Out.Ln
  END
END AssertInt;

PROCEDURE ReportTest;
BEGIN
  IF testResult = Ok THEN
    Out.String("  PASSED"); Out.Ln;
    INC(passedTests)
  ELSE
    Out.String("  TEST FAILED"); Out.Ln
  END;
  INC(totalTests);
  Out.Ln
END ReportTest;

PROCEDURE PrintSummary;
BEGIN
  Out.Ln;
  Out.String("========================================"); Out.Ln;
  Out.String("Total tests:  "); Out.Int(totalTests,  0); Out.Ln;
  Out.String("Passed:       "); Out.Int(passedTests,  0); Out.Ln;
  Out.String("Failed:       "); Out.Int(totalTests - passedTests, 0); Out.Ln;
  Out.String("========================================"); Out.Ln
END PrintSummary;

TYPE
  Node* = POINTER TO RECORD
    val:  INTEGER;
    next: Node
  END;

PROCEDURE ListPush(head: Node; v: INTEGER): Node;
VAR n: Node;
BEGIN
  NEW(n);
  n^.val  := v;
  n^.next := head;
  RETURN n
END ListPush;

PROCEDURE ListLen(head: Node): INTEGER;
VAR cnt: INTEGER;
BEGIN
  cnt := 0;
  WHILE head # NIL DO INC(cnt); head := head^.next END;
  RETURN cnt
END ListLen;

PROCEDURE ListSum(head: Node): INTEGER;
VAR s: INTEGER;
BEGIN
  s := 0;
  WHILE head # NIL DO s := s + head^.val; head := head^.next END;
  RETURN s
END ListSum;

PROCEDURE ListFind(head: Node; v: INTEGER): BOOLEAN;
VAR found: BOOLEAN;
BEGIN
  found := FALSE;
  WHILE (head # NIL) & ~found DO
    IF head^.val = v THEN found := TRUE END;
    head := head^.next
  END;
  RETURN found
END ListFind;

PROCEDURE ListRemove(head: Node; v: INTEGER): Node;
VAR prev, cur: Node;
BEGIN
  IF head = NIL THEN RETURN NIL END;
  IF head^.val = v THEN
    cur := head^.next;
    DISPOSE(head);
    RETURN cur
  END;
  prev := head; cur := head^.next;
  WHILE cur # NIL DO
    IF cur^.val = v THEN
      prev^.next := cur^.next;
      DISPOSE(cur);
      RETURN head
    END;
    prev := cur; cur := cur^.next
  END;
  RETURN head
END ListRemove;

PROCEDURE ListFree(head: Node);
VAR nxt: Node;
BEGIN
  WHILE head # NIL DO
    nxt := head^.next;
    DISPOSE(head);
    head := nxt
  END
END ListFree;

PROCEDURE ListReverse(head: Node): Node;
VAR prev, cur, nxt: Node;
BEGIN
  prev := NIL; cur := head;
  WHILE cur # NIL DO
    nxt      := cur^.next;
    cur^.next := prev;
    prev     := cur;
    cur      := nxt
  END;
  RETURN prev
END ListReverse;

PROCEDURE ListNth(head: Node; n: INTEGER): INTEGER;
VAR i: INTEGER;
BEGIN
  IF n < 0 THEN RETURN -1 END;
  i := 0;
  WHILE (head # NIL) & (i < n) DO head := head^.next; INC(i) END;
  IF head = NIL THEN RETURN -1 END;
  RETURN head^.val
END ListNth;

PROCEDURE TestListBuildAndTraverse*;
VAR head: Node; i: INTEGER;
BEGIN
  InitTest("List: build (push) and traverse (len/sum)");
  head := NIL;
  FOR i := 1 TO 5 DO head := ListPush(head, i) END;
  AssertInt(ListLen(head), 5, ErrList);
  AssertInt(ListSum(head), 15, ErrList);
  ListFree(head);
  ReportTest
END TestListBuildAndTraverse;

PROCEDURE TestListFind*;
VAR head: Node; i: INTEGER;
BEGIN
  InitTest("List: find (present and absent)");
  head := NIL;
  FOR i := 1 TO 5 DO head := ListPush(head, i * 10) END;
  Assert( ListFind(head, 10), ErrList);
  Assert( ListFind(head, 50), ErrList);
  Assert(~ListFind(head,  0), ErrList);
  Assert(~ListFind(head, 60), ErrList);
  ListFree(head);
  ReportTest
END TestListFind;

PROCEDURE TestListRemove*;
VAR head: Node;
BEGIN
  InitTest("List: remove head / middle / tail");
  head := NIL;
  head := ListPush(head, 3);
  head := ListPush(head, 2);
  head := ListPush(head, 1);
  head := ListRemove(head, 2);
  AssertInt(ListLen(head), 2, ErrList);
  Assert( ListFind(head, 1), ErrList);
  Assert(~ListFind(head, 2), ErrList);
  Assert( ListFind(head, 3), ErrList);

  head := ListRemove(head, 1);
  AssertInt(ListLen(head), 1, ErrList);
  Assert(~ListFind(head, 1), ErrList);

  head := ListRemove(head, 3);
  Assert(head = NIL, ErrList);
  ReportTest
END TestListRemove;

PROCEDURE TestListReverse*;
VAR head: Node;
BEGIN
  InitTest("List: reverse");
  head := NIL;
  head := ListPush(head, 3);
  head := ListPush(head, 2);
  head := ListPush(head, 1);
  head := ListReverse(head);
  AssertInt(ListNth(head, 0), 3, ErrList);
  AssertInt(ListNth(head, 1), 2, ErrList);
  AssertInt(ListNth(head, 2), 1, ErrList);
  AssertInt(ListLen(head), 3, ErrList);
  ListFree(head);
  ReportTest
END TestListReverse;

PROCEDURE TestListEmpty*;
VAR head: Node;
BEGIN
  InitTest("List: empty-list edge cases");
  head := NIL;
  AssertInt(ListLen(head),   0,  ErrList);
  AssertInt(ListSum(head),   0,  ErrList);
  Assert(~ListFind(head, 1),     ErrList);
  head := ListRemove(head, 1);
  Assert(head = NIL,             ErrList);
  head := ListReverse(head);
  Assert(head = NIL,             ErrList);
  AssertInt(ListNth(head, 0), -1, ErrList);
  ReportTest
END TestListEmpty;

PROCEDURE TestListNthEdge*;
VAR head: Node;
BEGIN
  InitTest("List: Nth edge cases (negative, beyond length)");
  head := NIL;
  head := ListPush(head, 3);
  head := ListPush(head, 2);
  head := ListPush(head, 1);
  AssertInt(ListNth(head, 0), 1, ErrList);
  AssertInt(ListNth(head, 2), 3, ErrList);
  AssertInt(ListNth(head, 3), -1, ErrList);
  AssertInt(ListNth(head, -1), -1, ErrList);
  ListFree(head);
  ReportTest
END TestListNthEdge;

PROCEDURE TestListRemoveNonExistent*;
VAR head, newHead: Node;
BEGIN
  InitTest("List: remove non-existent element");
  head := NIL;
  head := ListPush(head, 2);
  head := ListPush(head, 1);
  newHead := ListRemove(head, 99);
  Assert(newHead = head, ErrList);
  AssertInt(ListLen(head), 2, ErrList);
  Assert(ListFind(head, 1), ErrList);
  Assert(ListFind(head, 2), ErrList);
  ListFree(head);
  ReportTest
END TestListRemoveNonExistent;

PROCEDURE TestListRemoveTail*;
VAR head: Node;
BEGIN
  InitTest("List: remove tail element");
  head := NIL;
  head := ListPush(head, 3);
  head := ListPush(head, 2);
  head := ListPush(head, 1);
  head := ListRemove(head, 3);
  AssertInt(ListLen(head), 2, ErrList);
  AssertInt(ListNth(head, 0), 1, ErrList);
  AssertInt(ListNth(head, 1), 2, ErrList);
  Assert(~ListFind(head, 3), ErrList);
  ListFree(head);
  ReportTest
END TestListRemoveTail;

PROCEDURE TestListRemoveDuplicateFirst*;
VAR head: Node;
BEGIN
  InitTest("List: remove first occurrence of duplicate");
  head := NIL;
  head := ListPush(head, 2);
  head := ListPush(head, 1);
  head := ListPush(head, 2);
  head := ListPush(head, 1);
  head := ListRemove(head, 1);
  AssertInt(ListLen(head), 3, ErrList);
  AssertInt(ListNth(head, 0), 2, ErrList);
  AssertInt(ListNth(head, 1), 1, ErrList);
  AssertInt(ListNth(head, 2), 2, ErrList);
  ListFree(head);
  ReportTest
END TestListRemoveDuplicateFirst;

PROCEDURE TestListReverseSingle*;
VAR head, rev: Node;
BEGIN
  InitTest("List: reverse single element");
  head := ListPush(NIL, 42);
  rev := ListReverse(head);
  AssertInt(ListNth(rev, 0), 42, ErrList);
  Assert(rev^.next = NIL, ErrList);
  ListFree(rev);
  ReportTest
END TestListReverseSingle;

PROCEDURE TestListReverseTwo*;
VAR head, rev: Node;
BEGIN
  InitTest("List: reverse two elements");
  head := ListPush(NIL, 2);
  head := ListPush(head, 1);
  rev := ListReverse(head);
  AssertInt(ListLen(rev), 2, ErrList);
  AssertInt(ListNth(rev, 0), 2, ErrList);
  AssertInt(ListNth(rev, 1), 1, ErrList);
  ListFree(rev);
  ReportTest
END TestListReverseTwo;

PROCEDURE TestListSumOverflow*;
VAR head: Node;
BEGIN
  InitTest("List: sum overflow (16-bit)");
  head := NIL;
  head := ListPush(head, 20000);
  head := ListPush(head, 20000);
  AssertInt(ListSum(head), -25536, ErrList);
  ListFree(head);
  ReportTest
END TestListSumOverflow;

TYPE
  TreeNode* = POINTER TO RECORD
    key:   INTEGER;
    left:  TreeNode;
    right: TreeNode
  END;

PROCEDURE TreeInsert(root: TreeNode; k: INTEGER): TreeNode;
VAR n: TreeNode;
BEGIN
  IF root = NIL THEN
    NEW(n); n^.key := k; n^.left := NIL; n^.right := NIL;
    RETURN n
  ELSIF k < root^.key THEN
    root^.left  := TreeInsert(root^.left,  k)
  ELSIF k > root^.key THEN
    root^.right := TreeInsert(root^.right, k)
  END;
  RETURN root
END TreeInsert;

PROCEDURE TreeFind(root: TreeNode; k: INTEGER): BOOLEAN;
BEGIN
  IF root = NIL THEN RETURN FALSE
  ELSIF k < root^.key THEN RETURN TreeFind(root^.left,  k)
  ELSIF k > root^.key THEN RETURN TreeFind(root^.right, k)
  ELSE RETURN TRUE
  END
END TreeFind;

PROCEDURE TreeSize(root: TreeNode): INTEGER;
BEGIN
  IF root = NIL THEN RETURN 0 END;
  RETURN 1 + TreeSize(root^.left) + TreeSize(root^.right)
END TreeSize;

PROCEDURE TreeHeight(root: TreeNode): INTEGER;
VAR lh, rh: INTEGER;
BEGIN
  IF root = NIL THEN RETURN 0 END;
  lh := TreeHeight(root^.left);
  rh := TreeHeight(root^.right);
  IF lh > rh THEN RETURN lh + 1 ELSE RETURN rh + 1 END
END TreeHeight;

PROCEDURE TreeInorderSum(root: TreeNode): INTEGER;
BEGIN
  IF root = NIL THEN RETURN 0 END;
  RETURN TreeInorderSum(root^.left) + root^.key + TreeInorderSum(root^.right)
END TreeInorderSum;

PROCEDURE TreeMin(root: TreeNode): INTEGER;
BEGIN
  IF root = NIL THEN RETURN -1 END;
  IF root^.left = NIL THEN RETURN root^.key END;
  RETURN TreeMin(root^.left)
END TreeMin;

PROCEDURE TreeMax(root: TreeNode): INTEGER;
BEGIN
  IF root = NIL THEN RETURN -1 END;
  IF root^.right = NIL THEN RETURN root^.key END;
  RETURN TreeMax(root^.right)
END TreeMax;

PROCEDURE TreeFree(root: TreeNode);
BEGIN
  IF root = NIL THEN RETURN END;
  TreeFree(root^.left);
  TreeFree(root^.right);
  DISPOSE(root)
END TreeFree;

PROCEDURE TestTreeInsertFind*;
VAR root: TreeNode;
BEGIN
  InitTest("Tree: insert and find");
  root := NIL;
  root := TreeInsert(root, 5);
  root := TreeInsert(root, 3);
  root := TreeInsert(root, 7);
  root := TreeInsert(root, 1);
  root := TreeInsert(root, 4);
  root := TreeInsert(root, 6);
  root := TreeInsert(root, 9);
  AssertInt(TreeSize(root), 7, ErrTree);
  Assert( TreeFind(root, 5), ErrTree);
  Assert( TreeFind(root, 1), ErrTree);
  Assert( TreeFind(root, 9), ErrTree);
  Assert(~TreeFind(root, 0), ErrTree);
  Assert(~TreeFind(root, 8), ErrTree);
  TreeFree(root);
  ReportTest
END TestTreeInsertFind;

PROCEDURE TestTreeMinMax*;
VAR root: TreeNode;
BEGIN
  InitTest("Tree: min and max");
  root := NIL;
  root := TreeInsert(root, 5);
  root := TreeInsert(root, 3);
  root := TreeInsert(root, 7);
  root := TreeInsert(root, 1);
  root := TreeInsert(root, 9);
  AssertInt(TreeMin(root), 1, ErrTree);
  AssertInt(TreeMax(root), 9, ErrTree);
  TreeFree(root);
  ReportTest
END TestTreeMinMax;

PROCEDURE TestTreeHeight*;
VAR root: TreeNode;
BEGIN
  InitTest("Tree: height");
  root := NIL;
  AssertInt(TreeHeight(root), 0, ErrTree);

  root := TreeInsert(root, 5);
  AssertInt(TreeHeight(root), 1, ErrTree);

  root := TreeInsert(root, 3);
  root := TreeInsert(root, 7);
  AssertInt(TreeHeight(root), 2, ErrTree);

  root := TreeInsert(root, 1);
  AssertInt(TreeHeight(root), 3, ErrTree);

  TreeFree(root);
  ReportTest
END TestTreeHeight;

PROCEDURE TestTreeInorderSum*;
VAR root: TreeNode; i: INTEGER;
BEGIN
  InitTest("Tree: in-order sum (1..10)");
  root := NIL;
  root := TreeInsert(root,  5);
  root := TreeInsert(root,  3);
  root := TreeInsert(root,  8);
  root := TreeInsert(root,  1);
  root := TreeInsert(root,  4);
  root := TreeInsert(root,  7);
  root := TreeInsert(root, 10);
  root := TreeInsert(root,  2);
  root := TreeInsert(root,  6);
  root := TreeInsert(root,  9);
  AssertInt(TreeSize(root), 10, ErrTree);
  AssertInt(TreeInorderSum(root), 55, ErrTree);
  TreeFree(root);
  ReportTest
END TestTreeInorderSum;

PROCEDURE TestTreeDuplicate*;
VAR root: TreeNode;
BEGIN
  InitTest("Tree: duplicate insert ignored");
  root := NIL;
  root := TreeInsert(root, 5);
  root := TreeInsert(root, 5);
  root := TreeInsert(root, 5);
  AssertInt(TreeSize(root), 1, ErrTree);
  TreeFree(root);
  ReportTest
END TestTreeDuplicate;

PROCEDURE TestTreeEmpty*;
VAR root: TreeNode;
BEGIN
  InitTest("Tree: empty tree edge cases");
  root := NIL;
  AssertInt(TreeSize(root),      0,  ErrTree);
  AssertInt(TreeHeight(root),    0,  ErrTree);
  AssertInt(TreeInorderSum(root),0,  ErrTree);
  AssertInt(TreeMin(root),      -1,  ErrTree);
  AssertInt(TreeMax(root),      -1,  ErrTree);
  Assert(~TreeFind(root, 0),        ErrTree);
  ReportTest
END TestTreeEmpty;

PROCEDURE TestTreeNegativeKeys*;
VAR root: TreeNode;
BEGIN
  InitTest("Tree: negative keys");
  root := NIL;
  root := TreeInsert(root, -5);
  root := TreeInsert(root, -10);
  root := TreeInsert(root, -3);
  root := TreeInsert(root, -7);
  AssertInt(TreeSize(root), 4, ErrTree);
  AssertInt(TreeMin(root), -10, ErrTree);
  AssertInt(TreeMax(root), -3, ErrTree);
  Assert( TreeFind(root, -5), ErrTree);
  Assert( TreeFind(root, -10), ErrTree);
  Assert( TreeFind(root, -3), ErrTree);
  Assert(~TreeFind(root, 0), ErrTree);
  TreeFree(root);
  ReportTest
END TestTreeNegativeKeys;

PROCEDURE TestTreeDegenerateHeight*;
VAR root: TreeNode; i: INTEGER;
BEGIN
  InitTest("Tree: degenerate (ascending keys) height = N");
  root := NIL;
  FOR i := 1 TO 10 DO root := TreeInsert(root, i) END;
  AssertInt(TreeHeight(root), 10, ErrTree);
  AssertInt(TreeSize(root), 10, ErrTree);
  TreeFree(root);
  ReportTest
END TestTreeDegenerateHeight;

PROCEDURE TestTreeInorderSumNegative*;
VAR root: TreeNode;
BEGIN
  InitTest("Tree: in-order sum with negative keys");
  root := NIL;
  root := TreeInsert(root, -5);
  root := TreeInsert(root, -10);
  root := TreeInsert(root, 3);
  root := TreeInsert(root, 7);
  AssertInt(TreeInorderSum(root), -5, ErrTree);
  TreeFree(root);
  ReportTest
END TestTreeInorderSumNegative;

PROCEDURE TestOfsOnPtrDeref*;
VAR p: Node; o1, o2: INTEGER;
BEGIN
  InitTest("SYSTEM.OFS(n^) is stable");
  NEW(p);
  o1 := SYSTEM.OFS(p^);
  o2 := SYSTEM.OFS(p^);
  AssertInt(o1, o2, ErrPtrDeref);  (* repeated call gives same result *)
  DISPOSE(p);
  ReportTest
END TestOfsOnPtrDeref;

PROCEDURE TestSegOnPtrDeref*;
VAR p: Node; s1, s2: INTEGER;
BEGIN
  InitTest("SYSTEM.SEG(n^) is stable and non-zero");
  NEW(p);
  s1 := SYSTEM.SEG(p^);
  s2 := SYSTEM.SEG(p^);
  AssertInt(s1, s2, ErrPtrDeref);  (* repeated call gives same result *)
  Assert(s1 # 0, ErrPtrDeref);    (* heap segment is never 0 *)
  DISPOSE(p);
  ReportTest
END TestSegOnPtrDeref;

PROCEDURE TestAdrOnPtrDeref*;
VAR p, q: Node; adr: SYSTEM.ADDRESS;
BEGIN
  InitTest("SYSTEM.ADR(n^) = n: assigning result back gives same pointer");
  NEW(p);
  p^.val := 42;
  adr := SYSTEM.ADR(p^);  (* must equal the value of p *)
  q   := adr;             (* ADDRESS is assignment-compatible with POINTER *)
  Assert(q = p, ErrPtrDeref);      (* ADR(p^) round-trips back to p *)
  AssertInt(q^.val, 42, ErrPtrDeref);  (* same heap cell accessible via q *)
  DISPOSE(p);
  ReportTest
END TestAdrOnPtrDeref;

PROCEDURE TestSegOfsOnPtrDeref*;
VAR p: Node;
BEGIN
  InitTest("SYSTEM.SEG(n^) and SYSTEM.OFS(n^): check values");
  p := SYSTEM.PTR(123H, 456H);
  Assert(SYSTEM.SEG(p^) = 123H, ErrPtrDeref);
  Assert(SYSTEM.OFS(p^) = 456H, ErrPtrDeref);
  ReportTest
END TestSegOfsOnPtrDeref;

PROCEDURE RunAllTests*;
BEGIN
  Out.String("========================================"); Out.Ln;
  Out.String("Dynamic Data Structure Tests"); Out.Ln;
  Out.String("========================================"); Out.Ln;
  totalTests := 0; passedTests := 0;

  TestListBuildAndTraverse;
  TestListFind;
  TestListRemove;
  TestListReverse;
  TestListEmpty;
  TestListNthEdge;
  TestListRemoveNonExistent;
  TestListRemoveTail;
  TestListRemoveDuplicateFirst;
  TestListReverseSingle;
  TestListReverseTwo;
  TestListSumOverflow;

  TestTreeInsertFind;
  TestTreeMinMax;
  TestTreeHeight;
  TestTreeInorderSum;
  TestTreeDuplicate;
  TestTreeEmpty;
  TestTreeNegativeKeys;
  TestTreeDegenerateHeight;
  TestTreeInorderSumNegative;

  TestOfsOnPtrDeref;
  TestSegOnPtrDeref;
  TestAdrOnPtrDeref;
  TestSegOfsOnPtrDeref;
  PrintSummary
END RunAllTests;

END TestDynamic.
