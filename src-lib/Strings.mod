(*$R-*)
MODULE Strings;

PROCEDURE Length*(s: ARRAY OF CHAR): INTEGER;
BEGIN
  RETURN SYSTEM.LENGTH(s)
END Length;

PROCEDURE NearLength(s: ARRAY OF CHAR): INTEGER; NEAR;
BEGIN
  RETURN SYSTEM.LENGTH(s)
END NearLength;

PROCEDURE Pos*(pat, s: ARRAY OF CHAR; pos: INTEGER): INTEGER;
VAR i, j, lp, ls: INTEGER; found: BOOLEAN;
BEGIN
  lp := NearLength(pat);
  ls := NearLength(s);
  IF lp = 0 THEN RETURN pos END;
  i := pos; found := FALSE;
  WHILE (i <= ls - lp) & ~found DO
    j := 0;
    WHILE (j < lp) & (s[i + j] = pat[j]) DO
      INC(j)
    END;
    IF j = lp THEN found := TRUE ELSE INC(i) END
  END;
  IF found THEN RETURN i ELSE RETURN -1 END
END Pos;

PROCEDURE Append*(s: ARRAY OF CHAR; VAR d: ARRAY OF CHAR);
VAR i, j, ld, ls: INTEGER;
BEGIN
  i := NearLength(d);
  ld := LEN(d);
  ls := LEN(s);
  j := 0;
  WHILE (i < ld - 1) & (j < ls) & (s[j] # 0X) DO
    d[i] := s[j];
    INC(i); INC(j)
  END;
  d[i] := 0X
END Append;

PROCEDURE Insert*(src: ARRAY OF CHAR; pos: INTEGER; VAR dst: ARRAY OF CHAR);
VAR sl, dl, i, max: INTEGER;
BEGIN
  sl := NearLength(src);
  dl := NearLength(dst);
  max := LEN(dst) - 1;
  IF pos < 0 THEN pos := 0 END;
  IF pos > dl THEN pos := dl END;
  IF sl > 0 THEN
    i := dl;
    WHILE i >= pos DO
      IF i + sl <= max THEN
        dst[i + sl] := dst[i]
      END;
      DEC(i)
    END;
    i := 0;
    WHILE (i < sl) & (pos + i < max) DO
      dst[pos + i] := src[i];
      INC(i)
    END
  END
END Insert;

PROCEDURE Delete*(VAR s: ARRAY OF CHAR; pos, n: INTEGER);
VAR i, l: INTEGER;
BEGIN
  l := NearLength(s);
  IF pos < 0 THEN pos := 0 END;
  IF pos >= l THEN n := 0 END;
  IF pos + n > l THEN n := l - pos END;
  IF n > 0 THEN
    i := pos;
    WHILE i + n <= l DO
      s[i] := s[i + n];
      INC(i)
    END
  END
END Delete;

PROCEDURE Replace*(src: ARRAY OF CHAR; pos: INTEGER; VAR dst: ARRAY OF CHAR);
VAR i, sl, dl: INTEGER;
BEGIN
  sl := NearLength(src);
  dl := NearLength(dst);
  IF pos < 0 THEN pos := 0 END;
  IF pos > dl THEN pos := dl END;
  i := 0;
  WHILE (i < sl) & (pos + i < LEN(dst) - 1) DO
    dst[pos + i] := src[i];
    INC(i)
  END;
  IF pos + i > dl THEN dst[pos + i] := 0X END
END Replace;

PROCEDURE Extract*(src: ARRAY OF CHAR; pos, n: INTEGER; VAR dst: ARRAY OF CHAR);
VAR i, sl: INTEGER;
BEGIN
  sl := NearLength(src);
  IF pos < 0 THEN pos := 0 END;
  IF pos >= sl THEN n := 0 END;
  IF pos + n > sl THEN n := sl - pos END;
  i := 0;
  WHILE (i < n) & (i < LEN(dst) - 1) DO
    dst[i] := src[pos + i];
    INC(i)
  END;
  dst[i] := 0X
END Extract;

PROCEDURE Equal*(a, b: ARRAY OF CHAR): BOOLEAN;
VAR i, len: INTEGER;
BEGIN
  i := 0;
  len := SYSTEM.MIN(LEN(a), LEN(b));
  WHILE (i < len) & (a[i] = b[i]) & (a[i] # 0X) DO
    INC(i)
  END;
  RETURN a[i] = b[i]
END Equal;

PROCEDURE Copy*(src: ARRAY OF CHAR; VAR dst: ARRAY OF CHAR);
VAR len : INTEGER;
BEGIN
  len := SYSTEM.MIN(NearLength(src), LEN(dst) - 1);
  SYSTEM.MOVE(src, dst, len);
  dst[len] := 0X
END Copy;

END Strings.
