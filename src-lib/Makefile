OC      = ../src/oc
LIBDIR  = ../lib

all: SYSTEM.lib Out.lib In.lib Strings.lib Files.lib Crt.lib Dos.lib

install: all
	cp SYSTEM.lib Out.lib In.lib Strings.lib Files.lib Dos.lib Crt.lib $(LIBDIR)/

SYSTEM.lib: SYSTEM.RDF SYSTEM.DEF $(OC)
	$(OC) --make-lib SYSTEM.RDF SYSTEM.DEF

Out.lib: Out.mod SYSTEM.lib $(OC)
	OBERON_LIB=. $(OC) Out.mod

In.lib: In.mod SYSTEM.lib $(OC)
	OBERON_LIB=. $(OC) In.mod

Files.lib: Files.mod SYSTEM.lib $(OC)
	OBERON_LIB=. $(OC) Files.mod

Strings.lib: Strings.mod SYSTEM.lib $(OC)
	OBERON_LIB=. $(OC) Strings.mod

Dos.lib: Dos.mod SYSTEM.lib $(OC)
	OBERON_LIB=. $(OC) Dos.mod

Crt.lib: Crt.mod SYSTEM.lib $(OC)
	OBERON_LIB=. $(OC) Crt.mod

clean:
	rm -f SYSTEM.lib Out.lib Strings.lib Files.lib Dos.lib Crt.lib In.lib

.PHONY: all install clean
