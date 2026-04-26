OC      = ../src/oc
LIBDIR  = ../lib

all: SYSTEM.om Out.om Out87.om In.om Strings.om Files.om Crt.om Dos.om IO.om Mem.om

install: all
	cp SYSTEM.om Out.om Out87.om In.om Strings.om Files.om Dos.om Crt.om IO.om Mem.om $(LIBDIR)/

SYSTEM.om: SYS.RDF SYSTEM.mod $(OC)
	$(OC) -SYSTEM SYSTEM.mod

Out.om: Out.mod SYSTEM.om $(OC)
	OBERON_LIB=. $(OC) Out.mod

Out87.om: Out87.mod Out.om SYSTEM.om $(OC)
	OBERON_LIB=. $(OC) Out87.mod

In.om: In.mod SYSTEM.om $(OC)
	OBERON_LIB=. $(OC) In.mod

Files.om: Files.mod SYSTEM.om $(OC)
	OBERON_LIB=. $(OC) Files.mod

Strings.om: Strings.mod SYSTEM.om $(OC)
	OBERON_LIB=. $(OC) Strings.mod

Dos.om: Dos.mod SYSTEM.om $(OC)
	OBERON_LIB=. $(OC) Dos.mod

Crt.om: Crt.mod SYSTEM.om $(OC)
	OBERON_LIB=. $(OC) Crt.mod

IO.om: IO.mod Files.om Out.om SYSTEM.om $(OC)
	OBERON_LIB=. $(OC) IO.mod

Mem.om: Mem.mod SYSTEM.om $(OC)
	OBERON_LIB=. $(OC) Mem.mod

clean:
	rm -f SYSTEM.om Out.om Out87.om Strings.om Files.om Dos.om Crt.om In.om IO.om Mem.om

.PHONY: all install clean
