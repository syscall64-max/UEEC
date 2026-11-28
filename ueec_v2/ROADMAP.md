# UEEC v2 roadmap — proposed
1. Finish wire-format audit (CPU metadata, exact relocation type namespaces, section index rules, ABI and compatibility decisions).
2. Compile and verify centralized headers; resolve endian/packing portability; test independently before changing stage builds.
3. Integrate Core incrementally into new working implementations; keep historical snapshots untouched. Re-run native, cross and sanitizer matrices; record evidence.
4. Review organization branch and merge only after acceptance and regression gates.
5. Resume stage 15 STM32 research; validate device profiles, metadata, memory and target-specific behavior. Scope to be agreed.
6. Audit v1 capabilities (00–10, 15–21) for reuse or migration: tooling, network delivery, VM, benchmarks, native compilation and ELF conversion.
7. Future research: loader hardening, broader ABI, AVR relocation/startup and physical tests, kernel integration, VM/JIT, other operating systems and accelerator targets.

Ordering after organization is provisional; no dates or support promises.
