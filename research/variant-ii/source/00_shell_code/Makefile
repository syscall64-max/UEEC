.DEFAULT_GOAL := all
include ../mk/architectures.mk
CFLAGS ?= -std=gnu11 -Wall -Wextra -O2 -static
PYTHON ?= python3
.PHONY: all build test clean help sanitize $(ARCHS) $(addprefix build-,$(ARCHS))
all: test
test:
	@pass=0; fail=0; for a in $(ARCHS); do \
	  if $(MAKE) --no-print-directory $$a; then pass=$$((pass+1)); \
	  else echo "[FAIL] $$a"; fail=$$((fail+1)); fi; \
	done; printf 'Passed: %s\nFailed: %s\nSkipped: 0\n' $$pass $$fail; test $$fail -eq 0
build: $(addprefix build-,$(ARCHS))
define ARCH_RULE
build-$(1):
	@mkdir -p build/$(1)
	$$(PYTHON) ../tools/emit_db.py ../test_resources/asm/$(1).asm build/$(1)/code.bin
	cmp build/$(1)/code.bin ../test_resources/bin/$(1).bin
	$$(CC_$(1)) $$(CFLAGS) $$(FLAGS_$(1)) loader.c -o build/$(1)/loader
	$$(CC_$(1)) $$(CFLAGS) $$(FLAGS_$(1)) -I../hdrs ../tools/check_header.c -o build/$(1)/check_header
$(1): build-$(1)
	$$(RUN_$(1)) ./build/$(1)/check_header
	$$(RUN_$(1)) ./build/$(1)/loader build/$(1)/code.bin
	@echo '[PASS] $(1)'
endef
$(foreach a,$(ARCHS),$(eval $(call ARCH_RULE,$(a))))
sanitize:
	$(MAKE) x86_64 CFLAGS='-std=gnu11 -Wall -Wextra -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-pie -no-pie'
clean:
	rm -rf build
help:
	@echo 'make [all|build|test|sanitize|clean|help|$(ARCHS)]'
