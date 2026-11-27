SOURCE := ueec_info.c
STAGE := info
include ../mk/exit_stage.mk

# Retain the exact compiler-generated assembly for investigating GNU assembler warnings.
.PHONY: diagnose-loongarch64
diagnose-loongarch64:
	@mkdir -p build/loongarch64-diagnostic
	$(CC_loongarch64) $(CFLAGS) $(FLAGS_loongarch64) -save-temps=obj $(SOURCE) -o build/loongarch64-diagnostic/tool
