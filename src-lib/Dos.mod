MODULE Dos;

CONST

ReadOnly*  = 01H;
Hidden*    = 02H;
SysFile*   = 04H;
VolumeID*  = 08H;
Directory* = 10H;
Archive*   = 20H;
AnyFile*   = 3FH;

TYPE

DateTime* = RECORD
    Year, Month, Day, Hour, Min, Sec: INTEGER;
END;


END Dos.
