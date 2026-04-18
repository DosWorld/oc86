.PHONY: all src src-lib clean test oc-dos

all: src src-lib

src:
	$(MAKE) -C src

src-lib: src
	$(MAKE) -C src-lib clean
	$(MAKE) -C src-lib install

# Full clean rebuild then run all tests — mandatory before/between test runs
test:
	$(MAKE) clean
	$(MAKE) src
	$(MAKE) src-lib
	$(MAKE) -C src tests
	bash tests/test_src.sh
	bash tests/test_system_intrinsics.sh

# Build DOS executable with Open Watcom (compact model)
oc-dos:
	wmake -f src/Makefile.wat

clean:
	$(MAKE) -C src clean
	$(MAKE) -C src-lib clean
