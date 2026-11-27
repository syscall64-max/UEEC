.DEFAULT_GOAL := all
include ../mk/architectures.mk
CFLAGS ?= -Wall -Wextra -O2 -static
HOSTCC ?= gcc
HOSTCFLAGS ?= -Wall -Wextra -O2
PYTHON ?= python3
.PHONY: all build test sanitize clean help $(ARCHS) $(addprefix build-,$(ARCHS))
all: test
build: $(addprefix build-,$(ARCHS))
test:
	@pass=0; fail=0; for a in $(ARCHS); do \
	  if $(MAKE) --no-print-directory $$a; then pass=$$((pass+1)); else echo "[FAIL] $$a"; fail=$$((fail+1)); fi; \
	done; printf 'Passed: %s\nFailed: %s\nSkipped: 0\n' $$pass $$fail; test $$fail -eq 0
define ARCH_RULE
build-$(1):
	@mkdir -p build/$(1)
	@set -e; for tool in $$(HOSTTOOLS); do $$(HOSTCC) $$(HOSTCFLAGS) $$$$tool.c -o build/$(1)/$$$$tool; done
	@set -e; for tool in $$(LOADERS); do $$(CC_$(1)) $$(CFLAGS) $$(FLAGS_$(1)) $$$$tool.c -o build/$(1)/$$$$tool; done
	$$(if $$(filter $(STAGE),3 4 5 6),$$(HOSTCC) $$(HOSTCFLAGS) ../03_reloc/gen.c -o build/$(1)/gen_reloc,true)
	$$(if $$(filter 6,$$(STAGE)),$$(HOSTCC) $$(HOSTCFLAGS) ../05_align/align.c -o build/$(1)/align,true)
$(1): build-$(1)
	$$(PYTHON) ../tools/test_legacy_stages.py $$(STAGE) $(1) build/$(1) --runner='$$(RUN_$(1))'
endef
$(foreach a,$(ARCHS),$(eval $(call ARCH_RULE,$(a))))
# Legacy demos intentionally do not free all allocations at exit; ASan/UBSan
# checks memory accesses and undefined behavior here, not leak completeness.
sanitize:
	ASAN_OPTIONS=detect_leaks=0 $(MAKE) x86_64 CFLAGS='-Wall -Wextra -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-pie -no-pie' HOSTCFLAGS='-Wall -Wextra -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-pie -no-pie'
clean:
	rm -rf build
help:
	@echo 'make [all|build|test|sanitize|clean|help|$(ARCHS)]'
