MODULE TestStrings;

IMPORT Out, Strings;

CONST
  Ok* = 0;
  ErrLength*   = 1;
  ErrPos*      = 2;
  ErrAppend*   = 3;
  ErrInsert*   = 4;
  ErrDelete*   = 5;
  ErrReplace*  = 6;
  ErrExtract*  = 7;

VAR
  totalTests, passedTests: INTEGER;
  testResult: INTEGER;
  (* Global (DS-segment) arrays for DS-path tests *)
  gbuf: ARRAY 32 OF CHAR;
  gdst: ARRAY 16 OF CHAR;

PROCEDURE InitTest(name: ARRAY OF CHAR);
BEGIN
  Out.String("Testing: ");
  Out.String(name);
  Out.Ln;
  testResult := Ok
END InitTest;

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

PROCEDURE AssertStr(actual, expected: ARRAY OF CHAR; errorCode: INTEGER);
VAR i: INTEGER; ok: BOOLEAN;
BEGIN
  i := 0; ok := TRUE;
  WHILE ok & (actual[i] # 0X) DO
    IF actual[i] # expected[i] THEN ok := FALSE END;
    INC(i)
  END;
  IF ok THEN ok := (expected[i] = 0X) END;
  IF ~ok THEN
    testResult := errorCode;
    Out.String("  FAILED: expected '");
    Out.String(expected);
    Out.String("', got '");
    Out.String(actual);
    Out.String("'");
    Out.Ln
  END
END AssertStr;

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

PROCEDURE TestLength*;
BEGIN
  InitTest("Strings.Length");
  AssertInt(Strings.Length(""),        0, ErrLength);
  AssertInt(Strings.Length("a"),       1, ErrLength);
  AssertInt(Strings.Length("hello"),   5, ErrLength);
  AssertInt(Strings.Length("abcdefg"), 7, ErrLength);
  ReportTest
END TestLength;

PROCEDURE TestPos*;
BEGIN
  InitTest("Strings.Pos");
  AssertInt(Strings.Pos("lo",  "hello",     0),  3, ErrPos);
  AssertInt(Strings.Pos("he",  "hello",     0),  0, ErrPos);
  AssertInt(Strings.Pos("xyz", "hello",     0), -1, ErrPos);
  AssertInt(Strings.Pos("l",   "hello",     0),  2, ErrPos);
  AssertInt(Strings.Pos("l",   "hello",     3),  3, ErrPos);
  AssertInt(Strings.Pos("",    "hello",     2),  2, ErrPos);
  AssertInt(Strings.Pos("llo", "hello",     0),  2, ErrPos);
  ReportTest
END TestPos;

PROCEDURE TestAppend*;
VAR
  buf: ARRAY 32 OF CHAR;
  dst: ARRAY 16 OF CHAR;
BEGIN
  InitTest("Strings.Append");

  buf := "hello";
  Strings.Append(" world", buf);
  AssertStr(buf, "hello world", ErrAppend);

  buf := "";
  Strings.Append("abc", buf);
  AssertStr(buf, "abc", ErrAppend);

  buf := "abc";
  Strings.Append("", buf);
  AssertStr(buf, "abc", ErrAppend);

  ReportTest
END TestAppend;

PROCEDURE TestInsert*;
VAR
  buf: ARRAY 32 OF CHAR;
  dst: ARRAY 16 OF CHAR;
BEGIN
  InitTest("Strings.Insert");

  buf := "helo";
  Strings.Insert("l", 2, buf);
  AssertStr(buf, "hello", ErrInsert);

  buf := "world";
  Strings.Insert("hello ", 0, buf);
  AssertStr(buf, "hello world", ErrInsert);

  buf := "hello";
  Strings.Insert("!", 5, buf);
  AssertStr(buf, "hello!", ErrInsert);

  ReportTest
END TestInsert;

PROCEDURE TestDelete*;
VAR
  buf: ARRAY 32 OF CHAR;
  dst: ARRAY 16 OF CHAR;
BEGIN
  InitTest("Strings.Delete");

  buf := "hello world";
  Strings.Delete(buf, 5, 6);
  AssertStr(buf, "hello", ErrDelete);

  buf := "abcdef";
  Strings.Delete(buf, 0, 3);
  AssertStr(buf, "def", ErrDelete);

  buf := "hello";
  Strings.Delete(buf, 2, 0);
  AssertStr(buf, "hello", ErrDelete);

  ReportTest
END TestDelete;

PROCEDURE TestReplace*;
VAR
  buf: ARRAY 32 OF CHAR;
  dst: ARRAY 16 OF CHAR;
BEGIN
  InitTest("Strings.Replace");

  buf := "hello world";
  Strings.Replace("there", 6, buf);
  AssertStr(buf, "hello there", ErrReplace);

  buf := "abcdef";
  Strings.Replace("XY", 0, buf);
  AssertStr(buf, "XYcdef", ErrReplace);

  ReportTest
END TestReplace;

PROCEDURE TestExtract*;
VAR
  buf: ARRAY 32 OF CHAR;
  dst: ARRAY 16 OF CHAR;
BEGIN
  InitTest("Strings.Extract");

  Strings.Extract("hello world", 6, 5, dst);
  AssertStr(dst, "world", ErrExtract);

  Strings.Extract("hello", 0, 3, dst);
  AssertStr(dst, "hel", ErrExtract);

  Strings.Extract("hello", 0, 0, dst);
  AssertStr(dst, "", ErrExtract);

  Strings.Extract("hello", 5, 3, dst);
  AssertStr(dst, "", ErrExtract);

  ReportTest
END TestExtract;

PROCEDURE TestAppendGlobal*;
BEGIN
  InitTest("Strings.Append (DS global)");

  gbuf := "hello";
  Strings.Append(" world", gbuf);
  AssertStr(gbuf, "hello world", ErrAppend);

  gbuf := "";
  Strings.Append("abc", gbuf);
  AssertStr(gbuf, "abc", ErrAppend);

  gbuf := "abc";
  Strings.Append("", gbuf);
  AssertStr(gbuf, "abc", ErrAppend);

  ReportTest
END TestAppendGlobal;

PROCEDURE TestInsertGlobal*;
BEGIN
  InitTest("Strings.Insert (DS global)");

  gbuf := "helo";
  Strings.Insert("l", 2, gbuf);
  AssertStr(gbuf, "hello", ErrInsert);

  gbuf := "world";
  Strings.Insert("hello ", 0, gbuf);
  AssertStr(gbuf, "hello world", ErrInsert);

  gbuf := "hello";
  Strings.Insert("!", 5, gbuf);
  AssertStr(gbuf, "hello!", ErrInsert);

  ReportTest
END TestInsertGlobal;

PROCEDURE TestDeleteGlobal*;
BEGIN
  InitTest("Strings.Delete (DS global)");

  gbuf := "hello world";
  Strings.Delete(gbuf, 5, 6);
  AssertStr(gbuf, "hello", ErrDelete);

  gbuf := "abcdef";
  Strings.Delete(gbuf, 0, 3);
  AssertStr(gbuf, "def", ErrDelete);

  gbuf := "hello";
  Strings.Delete(gbuf, 2, 0);
  AssertStr(gbuf, "hello", ErrDelete);

  ReportTest
END TestDeleteGlobal;

PROCEDURE TestReplaceGlobal*;
BEGIN
  InitTest("Strings.Replace (DS global)");

  gbuf := "hello world";
  Strings.Replace("there", 6, gbuf);
  AssertStr(gbuf, "hello there", ErrReplace);

  gbuf := "abcdef";
  Strings.Replace("XY", 0, gbuf);
  AssertStr(gbuf, "XYcdef", ErrReplace);

  ReportTest
END TestReplaceGlobal;

PROCEDURE TestExtractGlobal*;
BEGIN
  InitTest("Strings.Extract (DS global)");

  Strings.Extract("hello world", 6, 5, gdst);
  AssertStr(gdst, "world", ErrExtract);

  Strings.Extract("hello", 0, 3, gdst);
  AssertStr(gdst, "hel", ErrExtract);

  Strings.Extract("hello", 0, 0, gdst);
  AssertStr(gdst, "", ErrExtract);

  Strings.Extract("hello", 5, 3, gdst);
  AssertStr(gdst, "", ErrExtract);

  ReportTest
END TestExtractGlobal;

PROCEDURE RunAllTests*;
BEGIN
  Out.String("========================================");
  Out.Ln;
  Out.String("Starting Strings Library Tests");
  Out.Ln;
  Out.String("========================================");
  Out.Ln;

  totalTests := 0;
  passedTests := 0;

  TestLength;
  TestPos;
  TestAppend;
  TestInsert;
  TestDelete;
  TestReplace;
  TestExtract;
  TestAppendGlobal;
  TestInsertGlobal;
  TestDeleteGlobal;
  TestReplaceGlobal;
  TestExtractGlobal;

  PrintSummary
END RunAllTests;

END TestStrings.
