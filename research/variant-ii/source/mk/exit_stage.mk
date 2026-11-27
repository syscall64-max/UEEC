.DEFAULT_GOAL := all
include ../mk/architectures.mk
CFLAGS ?= -std=c11 -O2 -static -Wall -Wextra -Wpedantic -Wconversion -Wshadow
PYTHON ?= python3
.PHONY: all build test sanitize clean help fixtures $(ARCHS) $(addprefix build-,$(ARCHS))
all: test
fixtures:
	$(PYTHON) ../tools/gen_exit.py build/fixtures
build: $(addprefix build-,$(ARCHS))
test:
	@pass=0; fail=0; for a in $(ARCHS); do \
	  if $(MAKE) --no-print-directory $$a; then pass=$$((pass+1)); else echo "[FAIL] $$a"; fail=$$((fail+1)); fi; \
	done; printf 'Passed: %s\nFailed: %s\nSkipped: 0\n' $$pass $$fail; test $$fail -eq 0
# Every architecture rebuilds its tool; all fixtures are shared.
define ARCH_RULE
build-$(1):
	@mkdir -p build/$(1)
	$$(CC_$(1)) $$(CFLAGS) $$(FLAGS_$(1)) $$(SOURCE) -o build/$(1)/tool
$(1): build-$(1) fixtures
	$$(PYTHON) ../tools/test_exit.py $$(STAGE) ./build/$(1)/tool build/fixtures --runner='$$(RUN_$(1))'
endef
$(foreach a,$(ARCHS),$(eval $(call ARCH_RULE,$(a))))
sanitize:
	$(MAKE) x86_64 CFLAGS='-std=c11 -O1 -g -Wall -Wextra -fsanitize=address,undefined -fno-sanitize-recover=all -fno-pie -no-pie'
clean:
	rm -rf build
help:
	@echo 'make [all|build|test|sanitize|clean|help|$(ARCHS)]'
