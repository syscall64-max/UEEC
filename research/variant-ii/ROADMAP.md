# UEEC Variant II — Development Roadmap

Agreed research organization, 2026-11-27. Major numbers identify research topics; subnumbers identify implementation or experimental milestones. Existing historical stage identifiers and paths remain unchanged. Planned items are not claims of completed implementation. Variant I prototypes are migration inputs, not evidence of Variant II completion.

## 10 — Import, Linking & Loading

| Stage | Scope | Status |
|---|---|---|
| 10.1 | External Library Import | Implemented / tested (research prototype) |
| 10.2 | Internal Library Import | Implemented / tested (research prototype) |
| 10.3 | Mixed Library Import | Implemented / tested (research prototype) |
| 10.4 | Container Merging | Implemented / tested (research prototype) |
| 10.5 | Granulation / Degranulation | Implemented / tested (research prototype) |
| 10.6 | Unified Loading | Implemented / tested (research prototype) |
| 10.7.1 | Fixed Address Loading | Implemented / tested (research prototype) |
| 10.7.2 | Offline Prelinking | Implemented / tested (research prototype) |
| 10.8 | Direct Mapping & Execution | Implemented / tested (research prototype) |
| 10.9 | Performance Evaluation | Core experiments completed; follow-ups pending |
| 10.10 | Multi-Program Containers | Planned |
| 10.11 | Shared Memory Pages | Planned |
| 10.12 | Memory Protection Optimization | Planned |

## 11 — Multiarchitecture & ISA Dispatch

| Stage | Scope | Status |
|---|---|---|
| 11.1 | CPU Architecture Detection | Planned |
| 11.2 | CPU Feature Detection | Planned |
| 11.3 | Operating System Detection | Planned |
| 11.4 | Node Selection | Planned |
| 11.5 | Automatic ISA Dispatch | Planned |
| 11.6 | Multiarchitecture Testing | Planned |
| 11.7 | Performance Evaluation | Planned |

## 12 — Metadata

| Stage | Scope | Status |
|---|---|---|
| 12.1 | Metadata Structure | Planned |
| 12.2 | Architecture-Specific Metadata | Planned |
| 12.3 | Shared Metadata | Planned |
| 12.4 | Metadata Extraction & Conversion | Planned |
| 12.5 | Metadata Validation | Planned |

## 13 — Network Transfer

| Stage | Scope | Status |
|---|---|---|
| 13.1 | UEEC Transfer Protocol | Planned |
| 13.2 | Plain TCP Transfer | Planned |
| 13.3 | TLS Transfer | Planned |
| 13.4 | Network Architecture & ISA Dispatch | Planned |
| 13.5 | Selective Node Transfer | Planned |
| 13.6 | Network Performance Evaluation | Planned |

## 14 — Embedded Systems: AVR

| Stage | Scope | Status |
|---|---|---|
| 14.1 | AVR Architecture Metadata | Planned |
| 14.2 | AVR Device Selection | Planned |
| 14.3 | AVR Node Extraction | Planned |
| 14.4 | AVR Firmware Generation | Planned |
| 14.5 | AVR Validation | Planned |

## 15 — Embedded Systems: STM32

| Stage | Scope | Status |
|---|---|---|
| 15.1 | STM32 Architecture Metadata | Planned |
| 15.2 | STM32 Device Selection | Planned |
| 15.3 | STM32 Node Extraction | Planned |
| 15.4 | STM32 Firmware Generation | Planned |
| 15.5 | STM32 Validation | Planned |

## 16 — UEEC Virtual Machine

| Stage | Scope | Status |
|---|---|---|
| 16.1 | VM Architecture | Planned |
| 16.2 | Instruction Set | Planned |
| 16.3 | Registers & Memory | Planned |
| 16.4 | Stack & Function Calls | Planned |
| 16.5 | VM Node Loading | Planned |
| 16.6 | Bytecode Execution | Planned |
| 16.7 | Native & VM Node Interaction | Planned |
| 16.8 | VM Validation | Planned |

## 17 — Virtual Machine Performance

| Stage | Scope | Status |
|---|---|---|
| 17.1 | Baseline VM Benchmark | Planned |
| 17.2 | Arithmetic Operations | Planned |
| 17.3 | Memory Operations | Planned |
| 17.4 | Control Flow | Planned |
| 17.5 | Function Calls | Planned |
| 17.6 | VM vs Native Performance | Planned |

## 18 — Load-Time Compilation & JIT

| Stage | Scope | Status |
|---|---|---|
| 18.1 | VM-to-Native Compilation | Planned |
| 18.2 | Architecture-Specific Code Generation | Planned |
| 18.3 | CPU Feature-Based Optimization | Planned |
| 18.4 | Block-Based JIT Compilation | Planned |
| 18.5 | Virtual Sections | Planned |
| 18.6 | Compiled Code Caching | Planned |
| 18.7 | Persistent Native Code Storage | Planned |
| 18.8 | Performance Evaluation | Planned |

## 19 — Client-Side Compilation

| Stage | Scope | Status |
|---|---|---|
| 19.1 | VM Node Request | Planned |
| 19.2 | VM Node Transfer | Planned |
| 19.3 | Client-Side Native Compilation | Planned |
| 19.4 | Native UEEC Generation | Planned |
| 19.5 | Local Granulation & Prelinking | Planned |
| 19.6 | Direct Mapping & Execution | Planned |

## 20 — Server-Side Compilation

| Stage | Scope | Status |
|---|---|---|
| 20.1 | Client Architecture & OS Description | Planned |
| 20.2 | Client CPU Feature Description | Planned |
| 20.3 | Server-Side Native Compilation | Planned |
| 20.4 | Optimized UEEC Generation | Planned |
| 20.5 | Compiled Node Transfer | Planned |
| 20.6 | Client-Side Granulation | Planned |
| 20.7 | Compilation & Transfer Performance | Planned |

## 21 — Native Executable Format Conversion

| Stage | Scope | Status |
|---|---|---|
| 21.1 | UEEC-to-ELF Conversion | Planned |
| 21.2 | ELF Section & Segment Generation | Planned |
| 21.3 | Static Executable Generation | Planned |
| 21.4 | Shared Library Generation | Planned |
| 21.5 | Multiarchitecture ELF Generation | Planned |
| 21.6 | ELF Execution Validation | Planned |
| 21.7 | UEEC vs Generated ELF Performance | Planned |

### 10.9 — Performance evaluation detail

Existing experiment IDs: 10.9.1 baseline; 10.9.2 section size; 10.9.3 relocations; 10.9.4 imports; 10.9.5 libraries; 10.9.6 combined workload; 10.9.7 cold/warm page cache; 10.9.8 hardware and OS metrics; 10.9.9 ELF comparison. Follow-ups: 10.9.2b full-page-access size sweep; 10.9.10 prelinking cost; 10.9.11 protection optimization measurement (implementation 10.12); 10.9.12 native ELF conversion evaluation (implementation 21); 10.9.13 optimized ELF comparison; 10.9.14 multi-program shared-library scalability (implementation 10.10–10.11). These follow-ups remain planned.

## Organization rules

- Preserve historical source archives, test paths and checksums; do not rename old stages in place.
- Group new work by major topic, with numbered subdirectories (e.g. `11_isa_dispatch/11.1_architecture_detection/`). Each implementation milestone should include README, build instructions or Makefile, source, tests and results as applicable.
- TCP and TLS share application protocol and node-selection logic. TLS means Transport Layer Security; plain TCP is for controlled testing. Verify peer identity for secure transport.
- Metadata additions must coexist with existing fixed-loading and prelinking metadata; avoid identifier collisions.
- Distinguish native tests from emulated tests, functional validation from performance measurements, and proposed features from implemented features.
- Next implementation milestone: 11.1 CPU Architecture Detection.
