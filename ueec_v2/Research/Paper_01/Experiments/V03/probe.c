#define main original_main
#include "load.c"
#undef main
int main(int argc,char **argv){if(argc!=3)return 2;load_mode mode;if(parse_mode(argv[1],&mode))return 2;int fd=open(argv[2],O_RDONLY);struct stat st;if(fd<0||fstat(fd,&st))return 2;file_view f={0};f.size=st.st_size;f.data=mmap(NULL,f.size,PROT_READ,MAP_PRIVATE,fd,0);ueec_view v;loaded_image im;size_t node;if(f.data==MAP_FAILED||parse_ueec(&f,&v)||select_node(&f,&v,NULL,&node)||build_runtime_image(fd,&f,&v,node,mode,&im))return 1;
 if(im.local_count!=5)return 1;
 size_t total=0;for(size_t i=0;i<5;i++)total+=im.allocation_size[i];
 uintptr_t ptr;memcpy(&ptr,(void*)(im.section_address[2]+8),8);if(ptr!=im.section_address[3])return 1;
 int r=execute_entry(im.entry,(void*)im.section_address[4],(void*)im.section_address[2],(void*)im.section_address[3]);
 printf("result=%d allocation=%zu regions=%zu\n",r,total,im.region_count);release_image(&im);munmap((void*)f.data,f.size);close(fd);return 0;}
