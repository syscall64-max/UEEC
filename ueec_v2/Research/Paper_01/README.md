# UEEC — Paper 01 research materials

Research snapshot: `d41a9eab979403184e88499745b9e9acaa89b5fb` (second-generation organization branch). These files document an experimental study of UEEC for Linux CPU targets. They are research drafts, not a peer-reviewed publication. Existing implementation files and specifications are outside this directory and are unchanged.

## Contents

- [Experiments](Experiments/): 26 unpacked experiment packages, retaining their original internal layout, source, fixtures, test runners and package results. Each package has its own instructions.
- [Archives](Archives/): the same 26 original ZIP packages, byte-for-byte preserved. Use these if an exact original distribution is required.
- [Documentation](Documentation/): 20 Georgian DOCX reports (D00–D13, DV01–DV05 including DV03.2).
- [Results](Results/): experiment registry, original V05 audit CSV, archive inventory, benchmark run registry, research-environment extract and selected terminal logs (local account/path redacted).
- [Final benchmarks](Results/Final_Benchmarks/): the user-supplied `UEEC_Final_Results.tar.gz` (primary samples and summaries from the latest E12/V04 runs); readable copies of summary/memory CSV files. The large raw samples remain compressed in the original tarball.
- `SHA256SUMS`: integrity manifest for this entire directory except the manifest itself. From this directory, run `sha256sum -c SHA256SUMS` before running experiments. Individual experiment manifests may describe the pre-run package and generated outputs can change during reruns.

## Evidence and limits

The run logs report 356,748 mixed checks, expected rejections, regressions, stress runs and configurations across the study, not unique test cases or independent programs. E01 structural measurements are separate. The five reported user benchmark runs total 3,119,040 timing observations; only the last E12 and V04 runs (1,486,800 observations) are preserved in the final benchmark archive. Earlier package-local CSVs are not the latest user benchmark data. The original V05 audit reports 2,122 manifest entries and 155 source comparisons across the 26 archives; the publication packaging step rechecked archive CRCs and their whole-file SHA-256 values against that audit, not every experiment execution.

The study distinguishes experimental first-generation loaders and Python reference models from the original second-generation C implementation. V01 is regression validation of 12 CPU targets, with direct x86-64 execution and 11 QEMU targets, not testing on 12 physical processors. ASan/UBSan cover x86-64 loader code. Structural estimates for PE/Mach-O are not Windows/macOS runtime validation. ELF comparison fixtures and protection/alignment policies differ across experiments; no universal size or speed advantage is established. Read D00 and D13 and the individual reports for limitations.

## Reproduction

Use the `README.md` and `run.sh` (or documented Makefile commands) inside the chosen experiment. Some packages require GCC, Python, cross-compilers and QEMU. Run in a disposable working copy to preserve the archived evidence. The final benchmark archive can be unpacked separately with `tar -xzf UEEC_Final_Results.tar.gz`. The public research materials contain the completed scope of Paper 01 only.
