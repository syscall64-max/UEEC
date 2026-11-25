#ifndef UEEC_VM_RUNTIME_H
#define UEEC_VM_RUNTIME_H
#include <stddef.h>
#include <stdint.h>
/* CODE/IDATA storage remains owned by the UEEC loader. Each call gets a
 * fresh register file, stack, and call-frame chain, freed before returning. */
int ueec_vm_execute(const uint8_t *code, size_t code_size,
                    uint8_t *idata, size_t idata_size,
                    uint64_t max_steps, int trace);
#endif
