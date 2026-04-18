MODULE TestHello;

IMPORT Dos, Out;

PROCEDURE RunAllTests*;
VAR i : INTEGER;
    p : ARRAY 127 OF CHAR;
BEGIN
    i := 1;
    WHILE i <= Dos.ARGCOUNT DO
	Dos.ARG(i, p);
	IF i # 1 THEN
	    Out.Char(' ')
	END;
	Out.String(p);
	INC(i)
    END;
    Out.Ln
END Run;

END TestHello.