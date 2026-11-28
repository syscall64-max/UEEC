# Source inventory — 2026-11-28

Baseline: research/variant-ii-stages-11-14 at 64d352d35d774113e69ee61b9803f6ffac172601. Paths below refer to that immutable commit. Preserve all original source and Makefiles.

| Component | Original source | Status |
|---|---|---|
| Core header | variant-II/14_avr_metadata/14.6_integration/ueec.h | copied verbatim |
| Export header | variant-II/13_import_export/13.1_export_processing/ueec_export.h | copied verbatim; endian portability review pending |
| AVR metadata | variant-II/14_avr_metadata/14.6_integration/avr_metadata.h | copied verbatim; experimental |
| Linux/CPU/ISA_Dispatch | variant-II/11_isa_dispatch | historical snapshot |
| Linux/CPU/Relocation | variant-II/12_relocation | historical snapshot |
| Linux/CPU/Import_Export | variant-II/13_import_export | historical snapshot |
| Linux/MPU/AVR | variant-II/14_avr_metadata | host tools; target bare metal |
| Linux/MPU/STM32 | Variant II stage 15 | not started |

The final 11.5, 12.6 and 13.6 ueec.h blobs are identical (48b238786dc54cad16539ae3e9a14330505b8025). The selected 14.6 header differs but retains base sizes and version 1. Relocation and import declarations are draft views derived from codecs, not source copies. No existing build includes were changed. Review before adopting centralized headers.
