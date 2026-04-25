# Open Watcom wmake file -- builds oc.exe for DOS (real mode)
# Usage: wmake -f Makefile.wat
#
# DOS command line limit is 127 chars; wlink response files bypass it.
# @oc.lnk and @olink.lnk are generated here before linking.

CC     = wcc
#CFLAGS = -bt=dos -ml -w4 -e25 -zq -I.
CFLAGS = -bt=dos -ml -ox -w4 -e25 -zq -I.
LD     = wlink
TARGET = oc.exe

OBJS = main.obj scanner.obj symbols.obj codegen.obj rdoff.obj parser.obj pexpr.obj &
       import.obj tar.obj def.obj compat.obj

OLINK_OBJS = olink.obj compat.obj tar.obj

all: $(TARGET) olink.exe .SYMBOLIC

$(TARGET): $(OBJS)
    @%create oc.lnk
    @%append oc.lnk system dos
    @%append oc.lnk name $(TARGET)
    @%append oc.lnk file main.obj
    @%append oc.lnk file scanner.obj
    @%append oc.lnk file symbols.obj
    @%append oc.lnk file codegen.obj
    @%append oc.lnk file rdoff.obj
    @%append oc.lnk file parser.obj
    @%append oc.lnk file pexpr.obj
    @%append oc.lnk file import.obj
    @%append oc.lnk file tar.obj
    @%append oc.lnk file def.obj
    @%append oc.lnk file compat.obj
    @%append oc.lnk option stack=50000
    $(LD) @oc.lnk

olink.exe: $(OLINK_OBJS)
    @%create olink.lnk
    @%append olink.lnk system dos
    @%append olink.lnk name olink.exe
    @%append olink.lnk file olink.obj
    @%append olink.lnk file compat.obj
    @%append olink.lnk file tar.obj
    @%append olink.lnk option stack=50000
    $(LD) @olink.lnk

.c.obj:
    $(CC) $(CFLAGS) $[@

clean: .SYMBOLIC
    @if exist *.obj del *.obj
    @if exist $(TARGET) del $(TARGET)
    @if exist *.map del *.map
    @if exist *.err del *.err
    @if exist oc.lnk del oc.lnk
    @if exist olink.lnk del olink.lnk
