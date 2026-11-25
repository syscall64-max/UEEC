PYTHON ?= python3
ARCH ?= $(shell $(PYTHON) -c 'from architectures import native_arch; print(native_arch())')

.PHONY: all loader loaders network cert serve payloads payloads-native generate test test-all test-network test-network-all test-transfer-all test-network-sanitize test-sanitize inspect clean
all: loader generate network

PORT ?= 9443
CERT ?= build/cert.pem
KEY ?= build/key.pem
SOURCE ?= build/sum.ueec

network: build/ueec-net

build/ueec-net: net/ueec_net.c net/selection.h net/ueec_wire.c net/ueec_wire.h dispatch_common.h cpu.h cpu_x86.h ueec_metadata.h ueec_capabilities.h reference/ueec.h reference/ueec_load.c
	mkdir -p build
	$(CC) -std=c11 -O2 -D_DEFAULT_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror net/ueec_net.c net/ueec_wire.c -o $@ -lssl -lcrypto

cert:
	mkdir -p build
	@if [ ! -f "$(KEY)" ] || [ ! -f "$(CERT)" ]; then \
	  openssl req -x509 -newkey rsa:2048 -nodes -days 30 -subj '/CN=localhost' \
	    -addext 'subjectAltName=DNS:localhost,IP:127.0.0.1,IP:::1' -keyout "$(KEY)" -out "$(CERT)" && chmod 600 "$(KEY)"; \
	fi

serve: network generate
	./build/ueec-net server --source "$(SOURCE)" --cert "$(CERT)" --key "$(KEY)" --port "$(PORT)"

loader:
	$(PYTHON) tools/build.py loader --arch $(ARCH)

loaders:
	$(PYTHON) tools/build.py loader --arch all

# Prebuilt leaf CODE is included with SHA-256 manifests. Rebuild explicitly.
payloads:
	$(PYTHON) tools/build.py payloads --arch all

payloads-native:
	$(PYTHON) tools/build.py payloads --arch $(ARCH)

generate:
	$(PYTHON) generate.py

inspect: all
	./build/ueec-loader --groups
	./build/ueec-loader --list-technologies build/sum.ueec

test: all
	$(PYTHON) test_dispatch.py --arch $(ARCH)
	$(PYTHON) test_network.py --arch $(ARCH)

test-all: loaders generate network
	$(PYTHON) tools/test_all.py
	$(PYTHON) test_network.py --all --qemu
	$(PYTHON) test_network.py
	@echo 'UEEC: all 12 architecture dispatch/TLS suites + native suites PASS (no skipped targets)'

test-network: all
	$(PYTHON) test_network.py --arch $(ARCH)

test-network-all: loaders generate network
	$(PYTHON) test_network.py --all --qemu

# Native TLS transport only; synthetic target masks, no target execution claim.
test-transfer-all: network generate
	$(PYTHON) test_network.py --synthetic

test-network-sanitize: all
	$(CC) -std=c11 -O1 -g -D_DEFAULT_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer net/ueec_net.c net/ueec_wire.c -o build/ueec-net-sanitize -lssl -lcrypto
	$(PYTHON) test_network.py --arch $(ARCH) --net build/ueec-net-sanitize

# Host-only parser/dispatch regression with memory/undefined-behavior checks.
test-sanitize: generate
	mkdir -p build
	$(CC) -std=c11 -O1 -g -D_DEFAULT_SOURCE -D_FILE_OFFSET_BITS=64 -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer loader.c -o build/ueec-loader-sanitize
	$(PYTHON) test_dispatch.py --arch $(ARCH) --loader build/ueec-loader-sanitize

clean:
	rm -rf build
