.DEFAULT_GOAL := all
include mk/architectures.mk
STAGES := 00_shell_code 01_ueec_info 02_ueec_load 03_reloc 04_convert 05_align 06_extract 07_pe 08_export 09_convert_export 10_import 10_import_internal 10_import_mixed 10_merge 10_granulation 10_unified_loading 10_fixed_loading 10_prelink 10_direct_mapping
.PHONY: all build test sanitize clean help $(ARCHS)
all build test sanitize clean $(ARCHS):
	@fail=0; for d in $(STAGES); do \
	  echo "=== $$d: $@ ==="; \
	  if $(MAKE) -C $$d $@; then echo "[PASS] $$d"; else echo "[FAIL] $$d"; fail=$$((fail+1)); fi; \
	done; test $$fail -eq 0
help:
	@echo 'make [all|build|test|sanitize|clean|help|$(ARCHS)]'
	@echo 'Active stages: $(STAGES)'
