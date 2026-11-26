# UEEC article 1 — experimental evidence index

Experiments 01–10 were performed on local research snapshots, not on this branch's final commit. Preserve raw data, sources and negative results. The loader changes in this branch are a V1 integration candidate, not yet validated by the full architecture matrix.

| Experiment | Scope |
|---|---|
| 01 | Export round trips and relocation encoding correctness; initial defects |
| 02 | Corrected block encoding, size and conversion timing |
| 03 | Synthetic export lookup |
| 04 | Real ELF library symbol datasets |
| 05 | GNU hash and PE lookup models versus UEEC |
| 06 | Relative relocation application models |
| 07 | Specialized block processing model |
| 08 | Checked loader comparison; specialization alone did not generally speed up |
| 09 | GCC O2/O3 and generated assembly |
| 10 | Width/endianness specialization and forced inline factorial comparison |

Experiment 10: 144 configurations, 101 repetitions (first excluded), four variants. Reported median speedup versus width-only V0: V1 endian specialization 1.4809x (-O2), 1.0761x (-O3); V2 forced inline 1.3333x, 1.0312x; V3 both 1.4626x, 1.087x. 58,176 checks per O2/O3/sanitizer run. These are preliminary x86-64 virtual-environment measurements, not cross-architecture or end-to-end loader results. No claim of universal superiority. PE/ELF comparisons use experimental models, not OS loaders. Only 16/32-bit records were timed in experiment 10.

## Original archive SHA-256 manifest

Raw archives are not included in this commit; upload the original archives and verify against these checksums.

```
6f259be772e714191af9d5637c5fc66b39c2b0450931bfe401ce6ae2422fbe34  UEEC_article_experiment_01.zip
4b7f24648196f62806651da6a18df1b56b57a69f35cb8bb47890b8026121ae39  UEEC_article_experiment_02.zip
1256da2abdf38562b870cefe6c9963ae7e7ea5a262a4122e0322232c60fe1f07  UEEC_article_experiment_03.zip
f05a426ec9944d9ac2fcaaae338851755e87c964b6420bcf8807b15a1993e0cb  UEEC_article_experiment_04.zip
38b38d0ff955a09afbca8f98cf6433d5a97723ec65e85bb24a39c46a4d9ac373  UEEC_article_experiment_05.zip
1001ee749a206ec42b035cbef2208b70f0d5e59b333093b4703aab4611e11c7d  UEEC_article_experiment_06.zip
2d907950f4ef585fa82ad941926e3cd494dedd92a2b029f3a006a8ea9e1f0de9  UEEC_article_experiment_07.zip
d12fe89809dcc85d784a01a685c4d4a299011ea4e4cf2768f35d1193312b8753  UEEC_article_experiment_08.zip
081123885e0f0fb8de33755ec2e9abdca90debf06147603ac82c3306a2b91c94  UEEC_article_experiment_09.zip
95404514f3fefe2c0fe3160409cc391abf852696faee9d2c28f27c090a2c3e24  UEEC_article_experiment_10.zip
```
