MODULE Strings;

PROCEDURE Length*(s: ARRAY OF CHAR): INTEGER;
VAR i: INTEGER;
BEGIN
  i := 0;
  WHILE (i < LEN(s)) & (s[i] # 0X) DO
    INC(i)
  END;
  RETURN i
END Length;

PROCEDURE Pos*(pat, s: ARRAY OF CHAR; pos: INTEGER): INTEGER;
VAR i, j, lp, ls: INTEGER; found: BOOLEAN;
BEGIN
  lp := Length(pat);
  ls := Length(s);
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
  i := Length(d);
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
  sl := Length(src);
  dl := Length(dst);
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
  l := Length(s);
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
  sl := Length(src);
  dl := Length(dst);
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
  sl := Length(src);
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

END Strings.
