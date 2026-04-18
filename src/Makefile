CC      = cc
CFLAGS  = -Wall -Wpedantic -std=c89 -O2 -g
# Open Watcom target: wmake -f Makefile.wat (large model -ml, DOS)
SRCS    = main.c scanner.c symbols.c codegen.c rdoff.c parser.c pexpr.c import.c tar.c def.c compat.c
OBJS    = $(SRCS:.c=.o)
TARGET  = oc
OLINK   = olink

TEST_BINS = \
    ../tests/test_scanner \
    ../tests/test_symbols \
    ../tests/test_codegen \
    ../tests/test_rdoff \
    ../tests/test_tar \
    ../tests/test_def

all: $(TARGET) $(OLINK)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

$(OLINK): olink.c compat.c
	$(CC) $(CFLAGS) -o $@ olink.c compat.c

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# ── unit test binaries ──────────────────────────────────────
tests: $(TEST_BINS)

../tests/test_scanner: ../tests/test_scanner.c scanner.o
	$(CC) $(CFLAGS) -o $@ $^

../tests/test_symbols: ../tests/test_symbols.c symbols.o
	$(CC) $(CFLAGS) -o $@ $^

../tests/test_codegen: ../tests/test_codegen.c codegen.o symbols.o rdoff.o
	$(CC) $(CFLAGS) -o $@ $^

../tests/test_rdoff: ../tests/test_rdoff.c rdoff.o
	$(CC) $(CFLAGS) -o $@ $^

../tests/test_tar: ../tests/test_tar.c tar.o
	$(CC) $(CFLAGS) -o $@ $^

../tests/test_def: ../tests/test_def.c def.o symbols.o
	$(CC) $(CFLAGS) -o $@ $^

test: $(TARGET)
	./$(TARGET) ../tests/Hello.Mod
	./$(TARGET) ../tests/Arith.Mod
	OBERON_LIB=../lib ./$(TARGET) ../src-lib/Out.mod
	OBERON_LIB=.:../lib ./$(TARGET) ../tests/OutHello.Mod

clean:
	rm -f $(OBJS) $(TARGET) $(OLINK) *.rdf *.def *.om $(TEST_BINS)

.PHONY: all test tests clean
