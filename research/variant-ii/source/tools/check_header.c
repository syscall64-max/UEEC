#include "ueec.h"
_Static_assert(sizeof(ueec_hdr)==10, "header size");
_Static_assert(offsetof(ueec_hdr,format)==4, "format offset");
_Static_assert(offsetof(ueec_hdr,nodes)==6, "nodes offset");
_Static_assert(offsetof(ueec_hdr,names)==8, "names offset");
_Static_assert(sizeof(ueec_node_hdr)==4, "node size");
_Static_assert(sizeof(ueec_node_info)==6, "node info size");
_Static_assert(sizeof(ueec_sh16)==6, "sh16 size");
_Static_assert(sizeof(ueec_sh32)==10, "sh32 size");
_Static_assert(sizeof(ueec_sh64)==18, "sh64 size");
int main(void) { return 0; }
