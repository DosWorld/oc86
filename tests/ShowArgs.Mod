MODULE ShowArgs;

(*
 * Example: print all command-line arguments using Args module.
 *
 * Build:
 *   OBERON_LIB=../lib ../src/oc -entry Run ShowArgs.Mod
 *   ../src/olink ShowArgs.om ../lib/SYSTEM.om ../lib/Out.om ../lib/Dos.om Args.om showargs.exe
 *
 * Run (in DOSBox-X):
 *   showargs.exe hello "world with spaces" foo
 *   => Arg 0: (empty - exe name slot)
 *   => Arg 1: hello
 *   => Arg 2: world with spaces
 *   => Arg 3: foo
 *)

IMPORT Args, Out;

VAR
    buf : ARRAY 128 OF CHAR;
    i   : INTEGER;

PROCEDURE Run*;
BEGIN
    Args.ArgInit;
    Out.String("Argument count: "); Out.Int(Args.ARGCOUNT(), 0); Out.Ln;
    i := 0;
    WHILE i < Args.ARGCOUNT() DO
        Args.ARG(i, buf);
        Out.String("Arg "); Out.Int(i, 0); Out.String(": ["); Out.String(buf); Out.String("]"); Out.Ln;
        INC(i)
    END
END Run;

END ShowArgs.
