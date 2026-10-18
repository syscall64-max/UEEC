SHELL := /bin/bash
CC ?= gcc
CFLAGS ?= -O2 -g -std=c11 -Wall -Wextra -Wpedantic -Wconversion -Wshadow
LDLIBS := -lssl -lcrypto
PORT ?= 9443
GRANULE ?= 4096
CERT ?= cert.pem
KEY ?= key.pem
SOURCE ?= build/multi.ueec

ARCHES := arm32 arm64 loongarch64 powerpc32 powerpc64 riscv32 riscv64 sparc32 sparc64 x86 x86_64

.PHONY: all build fixture cert serve check-env test-all test-available test-native test-unit test-transfer-all clean help $(addprefix test-,$(ARCHES))
all: build fixture
build: build/ueec-net
fixture: build/multi.ueec

build/.dir:
	mkdir -p build
	touch $@

build/ueec-net: src/ueec_net.c src/ueec_wire.c src/ueec_wire.h | build/.dir
	$(CC) $(CFLAGS) -Isrc src/ueec_net.c src/ueec_wire.c -o $@ $(LDLIBS)

build/multi.ueec: reference/02_ueec_load/ueec.asm reference/02_ueec_load/ueec_fixture.S | build/.dir
	@if command -v nasm >/dev/null; then \
	  nasm -f bin reference/02_ueec_load/ueec.asm -o "$@"; \
	else \
	  echo "NASM absent: using equivalent GNU assembler fixture"; \
	  as --64 reference/02_ueec_load/ueec_fixture.S -o build/ueec-fixture.o && \
	  objcopy -O binary -j .data build/ueec-fixture.o "$@"; \
	fi

build/test-wire: src/test_wire.c src/ueec_wire.c src/ueec_wire.h | build/.dir
	$(CC) $(CFLAGS) -Isrc src/test_wire.c src/ueec_wire.c -o $@

test-unit: build/test-wire fixture
	./build/test-wire build/multi.ueec

# Requires a running TLS server in terminal 1. Wire-only across 11 archs; no QEMU.
test-transfer-all: build/ueec-net fixture
	@set -e; mkdir -p build/transfers; \
	  for arch in $$(seq 0 10); do \
	    for mode in compact granulated; do \
	      output="build/transfers/$${arch}_$${mode}.ueec"; rm -f -- "$$output"; \
	      ./build/ueec-net client --port "$(PORT)" --ca "$(CERT)" --machine "$$arch" --system 2 \
	        --mode "$$mode" --granule "$(GRANULE)" --output "$$output"; \
	    done; \
	  done; echo "PASS: 11 architectures x 2 modes TLS transfer and reconstruction"

cert:
	@if [[ ! -f "$(KEY)" || ! -f "$(CERT)" ]]; then \
	  openssl req -x509 -newkey rsa:2048 -nodes -keyout "$(KEY)" -out "$(CERT)" \
	  -days 7 -subj '/CN=localhost' -addext 'subjectAltName=DNS:localhost' && \
	  chmod 600 "$(KEY)" && chmod 644 "$(CERT)"; \
	  else echo 'Certificate files already exist (no overwrite)'; fi

serve: build/ueec-net fixture
	./build/ueec-net server --source "$(SOURCE)" --cert "$(CERT)" --key "$(KEY)" --port "$(PORT)"

check-env:
	@bash scripts/check-env.sh

test-native: test-x86_64

# Run in terminal 2; terminal 1 must already run `make serve`.
$(addprefix test-,$(ARCHES)): test-%: build/ueec-net fixture
	@bash scripts/test-arch.sh "$*" "$(PORT)" "$(GRANULE)" "$(CERT)" strict

test-all: build/ueec-net fixture
	@set -e; for arch in $(ARCHES); do \
	  $(MAKE) --no-print-directory test-$$arch PORT="$(PORT)" GRANULE="$(GRANULE)" CERT="$(CERT)"; \
	done; echo 'PASS: all 11 architectures, compact + granulated';

test-available: build/ueec-net fixture
	@set -e; for arch in $(ARCHES); do \
	  bash scripts/test-arch.sh "$$arch" "$(PORT)" "$(GRANULE)" "$(CERT)" skip-missing; \
	done; echo 'Finished available architectures; SKIP is not PASS';

clean:
	rm -rf -- build reference/02_ueec_load/build
help:
	@echo 'Terminal 1: make cert && make serve'
	@echo 'Terminal 2: make test-unit; make test-transfer-all; make test-arm64; make test-all (strict 11 architectures)'
	@echo '            make test-available (missing toolchains are SKIP)'
	@echo 'Optional: make test-all PORT=9443 GRANULE=4096'
