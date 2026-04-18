MODULE Crt;

IMPORT Out;

CONST

(* CRT modes *)

BW40*    = 0;            (* 40x25 B/W on Color Adapter *)
CO40*    = 1;            (* 40x25 Color on Color Adapter *)
BW80*    = 2;            (* 80x25 B/W on Color Adapter *)
CO80*    = 3;            (* 80x25 Color on Color Adapter *)
Mono*    = 7;            (* 80x25 on Monochrome Adapter *)
Font8x8* = 256;          (* Add-in for ROM font *)

(* Mode constants for TP 3.0 compatibility *)

C40*     = CO40;
C80*     = CO80;

(* Foreground and background color constants *)

Black*     = 0;
Blue*      = 1;
Green*     = 2;
Cyan*      = 3;
Red*       = 4;
Magenta*   = 5;
Brown*     = 6;
LightGray* = 7;

(* Foreground color constants *)

DarkGray*      = 8;
LightBlue*     = 9;
LightGreen*    = 10;
LightCyan*     = 11;
LightRed*      = 12;
LightMagenta*  = 13;
Yellow*        = 14;
White*         = 15;

(* Add-in for blinking *)

Blink* = 128;

VAR

(* Interface variables *)

CheckBreak*: BOOLEAN;    (* Enable Ctrl-Break *)
CheckEOF*: BOOLEAN;      (* Enable Ctrl-Z *)
DirectVideo*: BOOLEAN;   (* Enable direct video addressing *)
CheckSnow*: BOOLEAN;     (* Enable snow filtering *)
LastMode*: BYTE;         (* Current text mode *)
TextAttr*: BYTE;         (* Current text attribute *)
WindMin*: INTEGER;       (* Window upper left coordinates *)
WindMax*: INTEGER;       (* Window lower right coordinates *)

(* Interface PROCEDUREs *)

(*
PROCEDURE KeyPressed*: BOOLEAN;
PROCEDURE ReadKey*: Char;
PROCEDURE TextMode*(Mode: Integer);
PROCEDURE Window*(X1,Y1,X2,Y2: BYTE);
PROCEDURE GotoXY*(X,Y: BYTE);
PROCEDURE WhereX*: BYTE;
PROCEDURE WhereY*: BYTE;
PROCEDURE ClrScr*;
PROCEDURE ClrEol*;
PROCEDURE InsLine*;
PROCEDURE DelLine*;
PROCEDURE TextColor*(Color: BYTE);
PROCEDURE TextBackground*(Color: BYTE);
PROCEDURE LowVideo*;
PROCEDURE HighVideo*;
PROCEDURE NormVideo*;
PROCEDURE Delay*(MS: INTEGER);
PROCEDURE Sound*(Hz: INTEGER);
PROCEDURE NoSound*;
*)

PROCEDURE KeyPressed*: BOOLEAN;
VAR regs : SYSTEM.Registers;
BEGIN
    regs.AX := 0B00H;
    SYSTEM.Intr(21H, regs);
    RETURN (regs.AX DIV 256) = 0FFH;
END KeyPressed;

PROCEDURE ReadKey*: CHAR;
VAR regs : SYSTEM.Registers;
BEGIN
    regs.AX := 0800H;
    SYSTEM.Intr(21H, regs);
    RETURN CHR(regs.AX MOD 256)
END ReadKey;

PROCEDURE TextMode*(Mode: BYTE);
VAR regs : SYSTEM.Registers;
BEGIN
    regs.AX := Mode;
    SYSTEM.Intr(10H, regs);
END TextMode;

PROCEDURE GotoXY*(X, Y: INTEGER);
BEGIN
    Out.Char(CHR(27));
    Out.Char('[');
    Out.Int(Y, 0);
    Out.Char(';');
    Out.Int(X, 0);
    Out.Char('H')
END GotoXY;

PROCEDURE ClrScr*;
BEGIN
    Out.Char(CHR(27));
    Out.String("[2J");
    Out.Char(CHR(27));
    Out.String("[1;1H");
END ClrScr;

PROCEDURE ClrEol*;
BEGIN
    Out.Char(CHR(27));
    Out.String("[K");
END ClrEol;

PROCEDURE TextColor*(fg : INTEGER);
BEGIN
    Out.Char(CHR(27));
    Out.String("[3");
    Out.Char(CHR(30H + (fg MOD 8)));
    Out.Char('m');
END TextColor;

PROCEDURE TextBackground*(bg : INTEGER);
BEGIN
    Out.Char(CHR(27));
    Out.String("[4");
    Out.Char(CHR(30H + (bg MOD 8)));
    Out.Char('m');
END TextBackground;

END Crt.
