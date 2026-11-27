.DEFAULT_GOAL := all
include ../mk/architectures.mk
CFLAGS ?= -Wall -Wextra -O2 -static
HOSTCC ?= gcc
HOSTCFLAGS ?= -Wall -Wextra -O2
PAYLOAD_FLAGS := -Wall -Wextra -Werror -O2 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-instrument-functions -fno-lto
PAYLOAD_FLAGS_arm32 := -marm
PAYLOAD_FLAGS_riscv32 := -mno-relax
PAYLOAD_FLAGS_riscv64 := -mno-relax
PAYLOAD_FLAGS_loongarch64 := -mno-relax
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
	mkdir -p build/$(1)
	$$(CC_$(1)) $$(FLAGS_$(1)) $$(CFLAGS) -Werror loader.c -o build/$(1)/loader
	$$(CC_$(1)) $$(FLAGS_$(1)) $$(PAYLOAD_FLAGS) $$(PAYLOAD_FLAGS_$(1)) -c payload.c -o build/$(1)/payload.o
	$$(PYTHON) extract_code.py build/$(1) '$$(CC_$(1))'
	$$(PYTHON) fixtures.py build/$(1) $(1)
	$$(HOSTCC) $$(HOSTCFLAGS) ../09_convert_export/convert_export.c -o build/$(1)/convert_export
$(1): build-$(1)
	$$(PYTHON) test_import.py build/$(1) --runner='$$(RUN_$(1))'
	$$(PYTHON) test_merge.py build/$(1) --runner='$$(RUN_$(1))'
endef
$(foreach a,$(ARCHS),$(eval $(call ARCH_RULE,$(a))))
sanitize:
	$(MAKE) x86_64 CFLAGS='-Wall -Wextra -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -fno-pie -no-pie'
clean:
	rm -rf build
help:
	@echo 'make [all|build|test|sanitize|clean|help|$(ARCHS)]'
