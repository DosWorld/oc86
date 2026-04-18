# Open Watcom wmake file -- builds oc.exe for DOS (real mode)
# Usage: wmake -f Makefile.wat

CC     = wcc
CFLAGS = -bt=dos -ml -ox -w4 -e25 -zq -I.
LD     = wlink
TARGET = oc.exe

OBJS = main.obj scanner.obj symbols.obj codegen.obj rdoff.obj parser.obj pexpr.obj &
       import.obj tar.obj def.obj compat.obj

OLINK_OBJS = olink.obj compat.obj

all: $(TARGET) olink.exe .SYMBOLIC

$(TARGET): $(OBJS)
    $(LD) system dos name $(TARGET) file { $(OBJS) }

olink.exe: $(OLINK_OBJS)
    $(LD) system dos name olink.exe file { $(OLINK_OBJS) }

.c.obj:
    $(CC) $(CFLAGS) $[@

clean: .SYMBOLIC
    @if exist *.obj del *.obj
    @if exist $(TARGET) del $(TARGET)
    @if exist *.map del *.map
    @if exist *.err del *.err
