MODULE TestProcedures;

IMPORT Out;

CONST
  Ok* = 0;
  ErrCall* = 1;
  ErrStack* = 2;
  ErrRecursion* = 3;
  ErrVarParam* = 4;

VAR
  totalTests, passedTests: INTEGER;
  testResult: INTEGER;
  globalCounter: INTEGER;

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

PROCEDURE TestProcedureNoParams*;
VAR
  a, b: INTEGER;
  
  PROCEDURE AddGlobal;
  BEGIN
    globalCounter := globalCounter + 10
  END AddGlobal;
  
  PROCEDURE SetValues;
  BEGIN
    a := 100;
    b := 200
  END SetValues;
  
BEGIN
  InitTest("Procedure call without parameters");
  
  globalCounter := 0;
  AddGlobal;
  AssertInt(globalCounter, 10, ErrCall);
  
  a := 0; b := 0;
  SetValues;
  AssertInt(a, 100, ErrCall);
  AssertInt(b, 200, ErrCall);
  
  ReportTest
END TestProcedureNoParams;

PROCEDURE TestProcedureValueParams*;
VAR
  result: INTEGER;
  
  PROCEDURE Multiply(a, b: INTEGER; VAR res: INTEGER);
  BEGIN
    res := a * b
  END Multiply;
  
  PROCEDURE SumAndSquare(x, y: INTEGER): INTEGER;
  VAR
    s: INTEGER;
  BEGIN
    s := x + y;
    RETURN s * s
  END SumAndSquare;
  
  PROCEDURE NestedCalls(a, b, c: INTEGER): INTEGER;
  VAR
    temp: INTEGER;
  BEGIN
    temp := SumAndSquare(a, b);
    RETURN temp + c
  END NestedCalls;
  
BEGIN
  InitTest("Procedure with value parameters");
  
  Multiply(15, 20, result);
  AssertInt(result, 300, ErrCall);
  
  result := SumAndSquare(5, 3);
  AssertInt(result, 64, ErrCall);
  
  result := NestedCalls(2, 3, 10);
  AssertInt(result, 35, ErrCall);
  
  ReportTest
END TestProcedureValueParams;

PROCEDURE TestVarParameters*;
VAR
  a, b, c: INTEGER;
  
  PROCEDURE Increment(VAR x: INTEGER);
  BEGIN
    x := x + 1
  END Increment;
  
  PROCEDURE Swap(VAR x, y: INTEGER);
  VAR
    temp: INTEGER;
  BEGIN
    temp := x;
    x := y;
    y := temp
  END Swap;
  
  PROCEDURE MultiplyByFactor(VAR value: INTEGER; factor: INTEGER);
  BEGIN
    value := value * factor
  END MultiplyByFactor;
  
  PROCEDURE SetToZero(VAR a, b, c: INTEGER);
  BEGIN
    a := 0;
    b := 0;
    c := 0
  END SetToZero;
  
BEGIN
  InitTest("VAR parameters (by reference)");
  
  a := 10;
  Increment(a);
  AssertInt(a, 11, ErrVarParam);
  
  a := 5; b := 15;
  Swap(a, b);
  AssertInt(a, 15, ErrVarParam);
  AssertInt(b, 5, ErrVarParam);
  
  a := 7;
  MultiplyByFactor(a, 6);
  AssertInt(a, 42, ErrVarParam);
  
  a := 100; b := 200; c := 300;
  SetToZero(a, b, c);
  AssertInt(a, 0, ErrVarParam);
  AssertInt(b, 0, ErrVarParam);
  AssertInt(c, 0, ErrVarParam);
  
  ReportTest
END TestVarParameters;

PROCEDURE TestMixedParameters*;
VAR
  a, b, result: INTEGER;
  
  PROCEDURE ComplexOp(VAR x: INTEGER; y, z: INTEGER; VAR w: INTEGER);
  BEGIN
    x := x + y;
    w := x * z
  END ComplexOp;
  
  PROCEDURE Calculate(VAR out1: INTEGER; in1, in2: INTEGER; VAR out2: INTEGER): INTEGER;
  VAR
    temp: INTEGER;
  BEGIN
    temp := in1 + in2;
    out1 := temp * 2;
    out2 := temp * 3;
    RETURN temp
  END Calculate;
  
BEGIN
  InitTest("Mixed value and VAR parameters");
  
  a := 10; b := 5; result := 0;
  ComplexOp(a, 3, 4, result);
  AssertInt(a, 13, ErrVarParam);
  AssertInt(result, 52, ErrVarParam);
  
  a := 0; b := 0; result := 0;
  a := Calculate(b, 4, 6, result);
  AssertInt(a, 10, ErrVarParam);
  AssertInt(b, 20, ErrVarParam);
  AssertInt(result, 30, ErrVarParam);
  
  ReportTest
END TestMixedParameters;

PROCEDURE TestLocalVariables*;
VAR
  global: INTEGER;
  
  PROCEDURE NestedWithLocals(x: INTEGER): INTEGER;
  VAR
    local1, local2, local3: INTEGER;
  BEGIN
    local1 := x * 2;
    local2 := local1 + 5;
    local3 := local2 * 3;
    global := global + local3;
    RETURN local3
  END NestedWithLocals;
  
  PROCEDURE DeepNested(a: INTEGER): INTEGER;
  VAR
    b: INTEGER;
    
    PROCEDURE Inner(c: INTEGER): INTEGER;
    VAR
      d: INTEGER;
    BEGIN
      d := c * 4;
      b := b + d;
      RETURN d
    END Inner;
    
  BEGIN
    b := a * 2;
    RETURN Inner(b)
  END DeepNested;
  
  PROCEDURE ManyLocals(x, y, z: INTEGER): INTEGER;
  VAR
    l1, l2, l3, l4, l5: INTEGER;
  BEGIN
    l1 := x + y;
    l2 := y + z;
    l3 := l1 * l2;
    l4 := l3 DIV 10;
    l5 := l4 - x;
    RETURN l5
  END ManyLocals;
  
VAR
  res: INTEGER;
BEGIN
  InitTest("Local variables and stack frames");
  
  global := 0;
  res := NestedWithLocals(10);
  AssertInt(res, 75, ErrStack);
  AssertInt(global, 75, ErrStack);
  
  res := DeepNested(3);
  AssertInt(res, 24, ErrStack);
  
  res := ManyLocals(5, 10, 15);
  AssertInt(res, 32, ErrStack);
  
  ReportTest
END TestLocalVariables;

PROCEDURE TestRecursionFactorial*;
VAR
  callCount: INTEGER;
  
  PROCEDURE Factorial(n: INTEGER): INTEGER;
  BEGIN
    INC(callCount);
    IF n <= 1 THEN
      RETURN 1
    ELSE
      RETURN n * Factorial(n - 1)
    END
  END Factorial;
  
  PROCEDURE TailFactorial(n, acc: INTEGER): INTEGER;
  BEGIN
    INC(callCount);
    IF n <= 1 THEN
      RETURN acc
    ELSE
      RETURN TailFactorial(n - 1, n * acc)
    END
  END TailFactorial;
  
VAR
  result: INTEGER;
BEGIN
  InitTest("Recursion - factorial");
  
  callCount := 0;
  result := Factorial(5);
  AssertInt(result, 120, ErrRecursion);
  AssertInt(callCount, 5, ErrRecursion);
  
  callCount := 0;
  result := Factorial(1);
  AssertInt(result, 1, ErrRecursion);
  AssertInt(callCount, 1, ErrRecursion);
  
  callCount := 0;
  result := TailFactorial(5, 1);
  AssertInt(result, 120, ErrRecursion);
  AssertInt(callCount, 5, ErrRecursion);
  
  ReportTest
END TestRecursionFactorial;

PROCEDURE TestRecursionFibonacci*;
VAR
  callCount: INTEGER;
  
  PROCEDURE Fibonacci(n: INTEGER): INTEGER;
  BEGIN
    INC(callCount);
    IF n <= 1 THEN
      RETURN n
    ELSE
      RETURN Fibonacci(n - 1) + Fibonacci(n - 2)
    END
  END Fibonacci;
  
  PROCEDURE FibIterative(n: INTEGER): INTEGER;
  VAR
    a, b, c, i: INTEGER;
  BEGIN
    IF n <= 1 THEN
      RETURN n
    ELSE
      a := 0; b := 1;
      FOR i := 2 TO n DO
        c := a + b;
        a := b;
        b := c
      END;
      RETURN b
    END
  END FibIterative;
  
VAR
  result: INTEGER;
BEGIN
  InitTest("Recursion - fibonacci");
  
  callCount := 0;
  result := Fibonacci(0);
  AssertInt(result, 0, ErrRecursion);
  
  callCount := 0;
  result := Fibonacci(1);
  AssertInt(result, 1, ErrRecursion);
  
  callCount := 0;
  result := Fibonacci(6);
  AssertInt(result, 8, ErrRecursion);
  
  result := FibIterative(10);
  AssertInt(result, 55, ErrRecursion);
  
  ReportTest
END TestRecursionFibonacci;

PROCEDURE TestDeepRecursion*;
VAR
  maxDepth: INTEGER;
  depthReached: INTEGER;
  
  PROCEDURE RecursiveDepth(depth, limit: INTEGER);
  VAR
    local: INTEGER;
  BEGIN
    local := depth;
    IF depth >= limit THEN
      depthReached := depth
    ELSE
      RecursiveDepth(depth + 1, limit)
    END
  END RecursiveDepth;
  
  PROCEDURE BinaryTreeDepth(n: INTEGER): INTEGER;
  VAR
    left, right: INTEGER;
  BEGIN
    IF n <= 0 THEN
      RETURN 0
    ELSE
      left := BinaryTreeDepth(n - 1);
      right := BinaryTreeDepth(n - 1);
      IF left > right THEN
        RETURN left + 1
      ELSE
        RETURN right + 1
      END
    END
  END BinaryTreeDepth;
  
VAR
  result: INTEGER;
BEGIN
  InitTest("Deep recursion and stack usage");
  
  depthReached := 0;
  RecursiveDepth(1, 100);
  AssertInt(depthReached, 100, ErrRecursion);
  
  result := BinaryTreeDepth(4);
  AssertInt(result, 4, ErrRecursion);
  
  ReportTest
END TestDeepRecursion;

PROCEDURE TestProcedureFunctionReturn*;
VAR
  result: INTEGER;
  
  PROCEDURE ReturnEarly(flag: BOOLEAN; value: INTEGER): INTEGER;
  BEGIN
    IF flag THEN
      RETURN value * 2
    END;
    RETURN value * 3
  END ReturnEarly;
  
  PROCEDURE MultipleReturns(x: INTEGER): INTEGER;
  BEGIN
    IF x < 0 THEN
      RETURN -1
    ELSIF x = 0 THEN
      RETURN 0
    ELSIF x < 10 THEN
      RETURN 1
    ELSE
      RETURN 2
    END
  END MultipleReturns;
  
  PROCEDURE NestedReturns(a, b: INTEGER): INTEGER;
    
    PROCEDURE Inner(c: INTEGER): INTEGER;
    BEGIN
      IF c > 10 THEN
        RETURN c - 10
      ELSE
        RETURN c * 2
      END
    END Inner;
    
  BEGIN
    RETURN Inner(a) + Inner(b)
  END NestedReturns;
  
BEGIN
  InitTest("Function returns and multiple exit points");
  
  result := ReturnEarly(TRUE, 5);
  AssertInt(result, 10, ErrCall);
  
  result := ReturnEarly(FALSE, 5);
  AssertInt(result, 15, ErrCall);
  
  result := MultipleReturns(-5);
  AssertInt(result, -1, ErrCall);
  
  result := MultipleReturns(0);
  AssertInt(result, 0, ErrCall);
  
  result := MultipleReturns(5);
  AssertInt(result, 1, ErrCall);
  
  result := MultipleReturns(20);
  AssertInt(result, 2, ErrCall);
  
  result := NestedReturns(5, 15);
  AssertInt(result, 15, ErrCall);
  
  ReportTest
END TestProcedureFunctionReturn;

PROCEDURE TestStackFrameIsolation*;
VAR
  global1, global2: INTEGER;
  
  PROCEDURE ModifyLocals(a, b: INTEGER): INTEGER;
  VAR
    x, y: INTEGER;
  BEGIN
    x := a * 2;
    y := b * 3;
    global1 := x;
    global2 := y;
    RETURN x + y
  END ModifyLocals;
  
  PROCEDURE NestedModify(a: INTEGER): INTEGER;
  VAR
    x: INTEGER;
    
    PROCEDURE InnerModify(b: INTEGER): INTEGER;
    VAR
      y: INTEGER;
    BEGIN
      y := b * 4;
      x := x + y;
      RETURN y
    END InnerModify;
    
  BEGIN
    x := a;
    RETURN InnerModify(x + 1)
  END NestedModify;
  
  PROCEDURE DeepNestedLevels(level: INTEGER): INTEGER;
  VAR
    value: INTEGER;
    
    PROCEDURE Level1(v1: INTEGER): INTEGER;
    VAR
      l1: INTEGER;
      
      PROCEDURE Level2(v2: INTEGER): INTEGER;
      VAR
        l2: INTEGER;
        
        PROCEDURE Level3(v3: INTEGER): INTEGER;
        BEGIN
          l2 := v3 * 2;
          l1 := l1 + l2;
          RETURN l2
        END Level3;
        
      BEGIN
        l2 := v2;
        RETURN Level3(v2 + 1)
      END Level2;
      
    BEGIN
      l1 := v1;
      RETURN Level2(v1 + 2)
    END Level1;
    
  BEGIN
    value := level;
    RETURN Level1(value + 3)
  END DeepNestedLevels;
  
VAR
  res: INTEGER;
BEGIN
  InitTest("Stack frame isolation");
  
  global1 := 0; global2 := 0;
  res := ModifyLocals(10, 20);
  AssertInt(res, 80, ErrStack);
  AssertInt(global1, 20, ErrStack);
  AssertInt(global2, 60, ErrStack);
  
  res := NestedModify(5);
  AssertInt(res, 24, ErrStack);
  
  res := DeepNestedLevels(1);
  AssertInt(res, 14, ErrStack);
  
  ReportTest
END TestStackFrameIsolation;

PROCEDURE TestFunctionComposition*;
VAR
  result: INTEGER;
  
  PROCEDURE Square(x: INTEGER): INTEGER;
  BEGIN
    RETURN x * x
  END Square;
  
  PROCEDURE Double(x: INTEGER): INTEGER;
  BEGIN
    RETURN x * 2
  END Double;
  
  PROCEDURE Add(x, y: INTEGER): INTEGER;
  BEGIN
    RETURN x + y
  END Add;
  
  PROCEDURE Compose(x, y, z: INTEGER): INTEGER;
  BEGIN
    RETURN Add(Square(Double(x)), Add(Square(y), Double(z)))
  END Compose;
  
  PROCEDURE ChainCall(a, b, c: INTEGER): INTEGER;
  BEGIN
    RETURN Double(Add(Square(a), Square(b))) + c
  END ChainCall;
  
BEGIN
  InitTest("Function composition and chaining");
  
  result := Compose(2, 3, 4);
  AssertInt(result, 33, ErrCall);
  
  result := ChainCall(2, 3, 5);
  AssertInt(result, 31, ErrCall);
  
  ReportTest
END TestFunctionComposition;

PROCEDURE RunAllTests*;
BEGIN
  Out.String("========================================");
  Out.Ln;
  Out.String("Starting Procedures and Stack Tests");
  Out.Ln;
  Out.String("========================================");
  Out.Ln;
  
  totalTests := 0;
  passedTests := 0;
  
  TestProcedureNoParams;
  TestProcedureValueParams;
  TestVarParameters;
  TestMixedParameters;
  TestLocalVariables;
  TestRecursionFactorial;
  TestRecursionFibonacci;
  TestDeepRecursion;
  TestProcedureFunctionReturn;
  TestStackFrameIsolation;
  TestFunctionComposition;
  
  PrintSummary
END RunAllTests;

END TestProcedures.