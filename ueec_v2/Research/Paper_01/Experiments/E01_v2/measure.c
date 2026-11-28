#include <stdio.h>
#include <elf.h>
#include "ueec.h"
#define SHOW(T) printf("%s,%zu\n", #T, sizeof(T))
int main(void) {
 puts("structure,bytes");
 SHOW(ueec_hdr); SHOW(ueec_node_hdr); SHOW(ueec_node_info);
 SHOW(ueec_sh16); SHOW(ueec_sh32); SHOW(ueec_sh64);
 SHOW(Elf32_Ehdr); SHOW(Elf64_Ehdr); SHOW(Elf32_Shdr); SHOW(Elf64_Shdr); SHOW(Elf32_Phdr); SHOW(Elf64_Phdr);
 return 0;
}
