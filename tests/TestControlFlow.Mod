MODULE TestControlFlow;

IMPORT Out;

CONST
  Ok* = 0;
  ErrIf* = 1;
  ErrLoop* = 2;
  ErrCase* = 3;
  ErrNested* = 4;

VAR
  totalTests, passedTests: INTEGER;
  testResult: INTEGER;

PROCEDURE InitTest(name: ARRAY OF CHAR);
BEGIN
  Out.String("Testing: ");
  Out.String(name);
  Out.Ln;
  testResult := Ok
END InitTest;

PROCEDURE Assert(condition: BOOLEAN; errorCode: INTEGER);
BEGIN
  IF ~condition THEN
    testResult := errorCode;
    Out.String("  FAILED (error code: ");
    Out.Int(errorCode, 0);
    Out.String(")");
    Out.Ln
  END
END Assert;

PROCEDURE AssertInt(actual, expected: INTEGER; errorCode: INTEGER);
BEGIN
  IF actual # expected THEN
    testResult := errorCode;
    Out.String("  FAILED: expected ");
    Out.Int(expected, 0);
    Out.String(", got ");
    Out.Int(actual, 0);
    Out.Ln
  END
END AssertInt;

PROCEDURE AssertBool(actual, expected: BOOLEAN; errorCode: INTEGER);
BEGIN
  IF actual # expected THEN
    testResult := errorCode;
    Out.String("  FAILED: expected ");
    IF expected THEN Out.String("TRUE") ELSE Out.String("FALSE") END;
    Out.String(", got ");
    IF actual THEN Out.String("TRUE") ELSE Out.String("FALSE") END;
    Out.Ln
  END
END AssertBool;

PROCEDURE ReportTest;
BEGIN
  IF testResult = Ok THEN
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

PROCEDURE TestIfSimple*;
VAR
  a, b, c: INTEGER;
BEGIN
  InitTest("IF-THEN-ELSE simple conditions");
  
  a := 10; b := 20; c := 0;
  IF a < b THEN
    c := 100
  END;
  AssertInt(c, 100, ErrIf);
  
  c := 0;
  IF a > b THEN
    c := 100
  ELSE
    c := 200
  END;
  AssertInt(c, 200, ErrIf);
  
  c := 0;
  IF a = b THEN
    c := 100
  ELSIF a < b THEN
    c := 200
  ELSE
    c := 300
  END;
  AssertInt(c, 200, ErrIf);
  
  c := 0;
  IF a > b THEN
    c := 100
  ELSIF a = b THEN
    c := 200
  ELSIF a < b THEN
    c := 300
  END;
  AssertInt(c, 300, ErrIf);
  
  ReportTest
END TestIfSimple;

PROCEDURE TestIfComplexConditions*;
VAR
  a, b, c, d, result: INTEGER;
BEGIN
  InitTest("IF with complex boolean expressions");
  
  a := 5; b := 10; c := 15; d := 20; result := 0;
  IF (a < b) & (c < d) THEN
    result := 1
  END;
  AssertInt(result, 1, ErrIf);
  
  result := 0;
  IF (a > b) OR (c < d) THEN
    result := 2
  END;
  AssertInt(result, 2, ErrIf);
  
  result := 0;
  IF (a > b) OR (c > d) THEN
    result := 3
  ELSE
    result := 4
  END;
  AssertInt(result, 4, ErrIf);
  
  result := 0;
  IF ~(a < b) THEN
    result := 5
  ELSE
    result := 6
  END;
  AssertInt(result, 6, ErrIf);
  
  result := 0;
  IF (a < b) & ~(c > d) THEN
    result := 7
  END;
  AssertInt(result, 7, ErrIf);
  
  ReportTest
END TestIfComplexConditions;

PROCEDURE TestIfNested*;
VAR
  a, b, c, result: INTEGER;
BEGIN
  InitTest("Nested IF statements");
  
  a := 10; b := 20; c := 30; result := 0;
  
  IF a < b THEN
    IF b < c THEN
      result := 100
    ELSE
      result := 200
    END
  ELSE
    result := 300
  END;
  AssertInt(result, 100, ErrNested);
  
  result := 0;
  IF a > b THEN
    result := 400
  ELSE
    IF b > c THEN
      result := 500
    ELSE
      IF a < c THEN
        result := 600
      END
    END
  END;
  AssertInt(result, 600, ErrNested);
  
  result := 0;
  IF a < b THEN
    IF a < c THEN
      IF b < c THEN
        result := 1
      ELSE
        result := 2
      END
    ELSE
      result := 3
    END
  ELSE
    result := 4
  END;
  AssertInt(result, 1, ErrNested);
  
  ReportTest
END TestIfNested;

PROCEDURE TestIfElseIfChain*;
VAR
  x, result: INTEGER;
BEGIN
  InitTest("IF-ELSIF chains");
  
  result := 0;
  x := 5;
  IF x < 0 THEN
    result := -1
  ELSIF x = 0 THEN
    result := 0
  ELSIF x < 10 THEN
    result := 1
  ELSIF x < 20 THEN
    result := 2
  ELSE
    result := 3
  END;
  AssertInt(result, 1, ErrIf);
  
  x := 15;
  IF x < 0 THEN
    result := -1
  ELSIF x = 0 THEN
    result := 0
  ELSIF x < 10 THEN
    result := 1
  ELSIF x < 20 THEN
    result := 2
  ELSE
    result := 3
  END;
  AssertInt(result, 2, ErrIf);
  
  x := 25;
  IF x < 0 THEN
    result := -1
  ELSIF x = 0 THEN
    result := 0
  ELSIF x < 10 THEN
    result := 1
  ELSIF x < 20 THEN
    result := 2
  ELSE
    result := 3
  END;
  AssertInt(result, 3, ErrIf);
  
  ReportTest
END TestIfElseIfChain;

PROCEDURE TestWhileLoop*;
VAR
  i, sum, product: INTEGER;
BEGIN
  InitTest("WHILE loops");
  
  i := 1; sum := 0;
  WHILE i <= 10 DO
    sum := sum + i;
    i := i + 1
  END;
  AssertInt(sum, 55, ErrLoop);
  
  i := 10; sum := 0;
  WHILE i > 0 DO
    sum := sum + i;
    i := i - 1
  END;
  AssertInt(sum, 55, ErrLoop);
  
  i := 1; product := 1;
  WHILE i < 5 DO
    product := product * i;
    i := i + 1
  END;
  AssertInt(product, 24, ErrLoop);
  
  ReportTest
END TestWhileLoop;

PROCEDURE TestRepeatLoop*;
VAR
  i, sum, count: INTEGER;
BEGIN
  InitTest("REPEAT loops");
  
  i := 1; sum := 0;
  REPEAT
    sum := sum + i;
    i := i + 1
  UNTIL i > 10;
  AssertInt(sum, 55, ErrLoop);
  
  i := 10; sum := 0;
  REPEAT
    sum := sum + i;
    i := i - 1
  UNTIL i = 0;
  AssertInt(sum, 55, ErrLoop);
  
  count := 0;
  REPEAT
    INC(count)
  UNTIL count >= 5;
  AssertInt(count, 5, ErrLoop);
  
  ReportTest
END TestRepeatLoop;

PROCEDURE TestForLoopAscending*;
VAR
  i, sum, product: INTEGER;
BEGIN
  InitTest("FOR loops ascending");
  
  sum := 0;
  FOR i := 1 TO 10 DO
    sum := sum + i
  END;
  AssertInt(sum, 55, ErrLoop);
  
  product := 1;
  FOR i := 1 TO 5 DO
    product := product * i
  END;
  AssertInt(product, 120, ErrLoop);
  
  sum := 0;
  FOR i := 0 TO 100 BY 10 DO
    sum := sum + i
  END;
  AssertInt(sum, 550, ErrLoop);
  
  ReportTest
END TestForLoopAscending;

PROCEDURE TestForLoopDescending*;
VAR
  i, sum, product: INTEGER;
BEGIN
  InitTest("FOR loops descending");
  
  sum := 0;
  FOR i := 10 TO 1 BY -1 DO
    sum := sum + i
  END;
  AssertInt(sum, 55, ErrLoop);
  
  product := 1;
  FOR i := 5 TO 1 BY -1 DO
    product := product * i
  END;
  AssertInt(product, 120, ErrLoop);
  
  sum := 0;
  FOR i := 100 TO 0 BY -10 DO
    sum := sum + i
  END;
  AssertInt(sum, 550, ErrLoop);
  
  ReportTest
END TestForLoopDescending;

PROCEDURE TestForLoopNested*;
VAR
  i, j, sum: INTEGER;
BEGIN
  InitTest("Nested FOR loops");
  
  sum := 0;
  FOR i := 1 TO 5 DO
    FOR j := 1 TO 5 DO
      sum := sum + i * j
    END
  END;
  AssertInt(sum, 225, ErrLoop);
  
  sum := 0;
  FOR i := 1 TO 3 DO
    FOR j := 1 TO i DO
      sum := sum + j
    END
  END;
  AssertInt(sum, 10, ErrLoop);
  
  ReportTest
END TestForLoopNested;

PROCEDURE TestForLoopVariableScope*;
VAR
  i, j, counter: INTEGER;
  loopVar: INTEGER;
BEGIN
  InitTest("FOR loop variable scope and register usage");
  
  counter := 0;
  FOR i := 1 TO 10 DO
    counter := counter + i;
    loopVar := i * 2
  END;
  AssertInt(counter, 55, ErrLoop);
  
  i := 100;
  AssertInt(i, 100, ErrLoop);
  
  counter := 0;
  FOR i := 1 TO 5 DO
    FOR j := 1 TO 5 DO
      counter := counter + 1
    END
  END;
  AssertInt(counter, 25, ErrLoop);
  
  ReportTest
END TestForLoopVariableScope;

PROCEDURE TestLoopComplexConditions*;
VAR
  i, sum, count: INTEGER;
  found: BOOLEAN;
BEGIN
  InitTest("Loops with complex conditions");
  
  i := 1; sum := 0;
  WHILE (i <= 20) & (sum < 50) DO
    sum := sum + i;
    i := i + 1
  END;
  AssertInt(sum, 55, ErrLoop);
  AssertInt(i, 11, ErrLoop);
  
  i := 1; found := FALSE;
  REPEAT
    IF i * i = 36 THEN
      found := TRUE
    END;
    i := i + 1
  UNTIL found OR (i > 10);
  AssertBool(found, TRUE, ErrLoop);
  AssertInt(i, 7, ErrLoop);
  
  count := 0;
  FOR i := 1 TO 20 DO
    IF (i MOD 2 = 0) & (i MOD 3 = 0) THEN
      count := count + 1
    END
  END;
  AssertInt(count, 3, ErrLoop);
  
  ReportTest
END TestLoopComplexConditions;

PROCEDURE TestCaseSimple*;
VAR
  value, result: INTEGER;
BEGIN
  InitTest("CASE statement simple");
  
  result := 0;
  value := 2;
  CASE value OF
    1: result := 10
  | 2: result := 20
  | 3: result := 30
  ELSE
    result := 99
  END;
  AssertInt(result, 20, ErrCase);
  
  result := 0;
  value := 5;
  CASE value OF
    1: result := 10
  | 2: result := 20
  | 3: result := 30
  ELSE
    result := 99
  END;
  AssertInt(result, 99, ErrCase);
  
  result := 0;
  value := 1;
  CASE value OF
    1: result := 100
  | 2, 3, 4: result := 200
  | 5 .. 9: result := 300
  ELSE
    result := 400
  END;
  AssertInt(result, 100, ErrCase);
  
  ReportTest
END TestCaseSimple;

PROCEDURE TestCaseRanges*;
VAR
  value, result: INTEGER;
BEGIN
  InitTest("CASE with ranges");
  
  result := 0;
  value := 3;
  CASE value OF
    1 .. 5: result := 10
  | 6 .. 10: result := 20
  | 11 .. 15: result := 30
  ELSE
    result := 99
  END;
  AssertInt(result, 10, ErrCase);
  
  result := 0;
  value := 8;
  CASE value OF
    1 .. 5: result := 10
  | 6 .. 10: result := 20
  | 11 .. 15: result := 30
  ELSE
    result := 99
  END;
  AssertInt(result, 20, ErrCase);
  
  result := 0;
  value := 12;
  CASE value OF
    1 .. 5: result := 10
  | 6 .. 10: result := 20
  | 11 .. 15: result := 30
  ELSE
    result := 99
  END;
  AssertInt(result, 30, ErrCase);
  
  result := 0;
  value := 20;
  CASE value OF
    1 .. 5: result := 10
  | 6 .. 10: result := 20
  | 11 .. 15: result := 30
  ELSE
    result := 99
  END;
  AssertInt(result, 99, ErrCase);
  
  ReportTest
END TestCaseRanges;

PROCEDURE TestCaseMixed*;
VAR
  value, result: INTEGER;
BEGIN
  InitTest("CASE with mixed constants and ranges");
  
  result := 0;
  value := 2;
  CASE value OF
    1, 3, 5, 7, 9: result := 10
  | 2, 4, 6, 8, 10: result := 20
  | 11 .. 20: result := 30
  ELSE
    result := 99
  END;
  AssertInt(result, 20, ErrCase);
  
  result := 0;
  value := 5;
  CASE value OF
    1, 3, 5, 7, 9: result := 10
  | 2, 4, 6, 8, 10: result := 20
  | 11 .. 20: result := 30
  ELSE
    result := 99
  END;
  AssertInt(result, 10, ErrCase);
  
  result := 0;
  value := 15;
  CASE value OF
    1, 3, 5, 7, 9: result := 10
  | 2, 4, 6, 8, 10: result := 20
  | 11 .. 20: result := 30
  ELSE
    result := 99
  END;
  AssertInt(result, 30, ErrCase);
  
  ReportTest
END TestCaseMixed;

PROCEDURE TestCaseNested*;
VAR
  x, y, result: INTEGER;
BEGIN
  InitTest("Nested CASE statements");
  
  result := 0;
  x := 2; y := 3;
  CASE x OF
    1: 
      CASE y OF
        1: result := 11
      | 2: result := 12
      ELSE
        result := 19
      END
  | 2:
      CASE y OF
        1: result := 21
      | 2: result := 22
      | 3: result := 23
      ELSE
        result := 29
      END
  | 3:
      result := 30
  ELSE
    result := 99
  END;
  AssertInt(result, 23, ErrCase);
  
  result := 0;
  x := 1; y := 2;
  CASE x OF
    1: 
      CASE y OF
        1: result := 11
      | 2: result := 12
      ELSE
        result := 19
      END
  | 2:
      CASE y OF
        1: result := 21
      | 2: result := 22
      | 3: result := 23
      ELSE
        result := 29
      END
  | 3:
      result := 30
  ELSE
    result := 99
  END;
  AssertInt(result, 12, ErrCase);
  
  ReportTest
END TestCaseNested;

PROCEDURE TestBreakAndReturn*;
VAR
  i, sum, result: INTEGER;
  
  PROCEDURE FindFirst(start, limit: INTEGER): INTEGER;
  VAR
    i: INTEGER;
  BEGIN
    FOR i := start TO limit DO
      IF i * i > 100 THEN
        RETURN i
      END
    END;
    RETURN -1
  END FindFirst;
  
BEGIN
  InitTest("Early exit from loops");
  
  i := 1; sum := 0;
  WHILE i <= 20 DO
    sum := sum + i;
    i := i + 1;
    IF sum >= 55 THEN
      i := 21
    END
  END;
  AssertInt(sum, 55, ErrLoop);
  
  result := FindFirst(1, 20);
  AssertInt(result, 11, ErrLoop);
  
  result := FindFirst(15, 20);
  AssertInt(result, 15, ErrLoop);
  
  ReportTest
END TestBreakAndReturn;

PROCEDURE RunAllTests*;
BEGIN
  Out.String("========================================");
  Out.Ln;
  Out.String("Starting Control Flow Tests");
  Out.Ln;
  Out.String("========================================");
  Out.Ln;
  
  totalTests := 0;
  passedTests := 0;
  
  TestIfSimple;
  TestIfComplexConditions;
  TestIfNested;
  TestIfElseIfChain;
  
  TestWhileLoop;
  TestRepeatLoop;
  
  TestForLoopAscending;
  TestForLoopDescending;
  TestForLoopNested;
  TestForLoopVariableScope;
  TestLoopComplexConditions;
  
  TestCaseSimple;
  TestCaseRanges;
  TestCaseMixed;
  TestCaseNested;
  
  TestBreakAndReturn;
  
  PrintSummary
END RunAllTests;

END TestControlFlow.