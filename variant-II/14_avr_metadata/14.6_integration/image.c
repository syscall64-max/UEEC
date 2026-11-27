#define UEEC_LAYOUT_NO_MAIN
#include "layout.c"
/* Intel HEX: byte addresses, extended linear address records, no entry record. */
static int record(FILE *f,unsigned type,unsigned addr,const uint8_t *p,unsigned n){
 unsigned sum=n+(addr>>8)+(addr&255)+type;
 if(fprintf(f,":%02X%04X%02X",n,addr,type)<0)return 1;
 for(unsigned i=0;i<n;i++){sum+=p[i];if(fprintf(f,"%02X",(unsigned)p[i])<0)return 1;}
 return fprintf(f,"%02X\n",(0u-sum)&255)<0;
}
static int hex_image(FILE *f,const avr_images *m){
 for(size_t at=0;at<m->used;){
  if(!(at&65535)){uint8_t hi[2]={(uint8_t)(at>>24),(uint8_t)(at>>16)};if(record(f,4,0,hi,2))return 1;}
  unsigned n=(unsigned)(m->used-at>16?16:m->used-at);
  if(record(f,0,(unsigned)(at&65535),m->flash+at,n))return 1;
  at+=n;
 }
 return record(f,1,0,NULL,0);
}
int main(int argc,char **argv){
 if(argc!=4)return 2;
 avr_images m={0};int rc=run_layout(3,argv,&m);
 if(rc)goto done;
 const char *names[]={"flash.bin","sram.bin","flash.hex"};
 char *paths[3]={0};FILE *out[3]={0};unsigned created=0;
 rc=2;
 for(unsigned i=0;i<3;i++){
  size_t n=strlen(argv[3]);if(n>SIZE_MAX-32)goto cleanup;
  paths[i]=malloc(n+32);if(!paths[i])goto cleanup;
  snprintf(paths[i],n+32,"%s/%s",argv[3],names[i]);
  out[i]=fopen(paths[i],"wbx");if(!out[i])goto cleanup;created++;
 }
 if(fwrite(m.flash,1,m.flash_size,out[0])!=m.flash_size||fwrite(m.ram,1,m.ram_size,out[1])!=m.ram_size||hex_image(out[2],&m))goto cleanup;
 rc=0;
cleanup:
 for(unsigned i=0;i<3;i++)if(out[i]&&fclose(out[i]))rc=2;
 if(rc)for(unsigned i=0;i<created;i++)unlink(paths[i]);
 for(unsigned i=0;i<3;i++)free(paths[i]);
 if(!rc)printf("IMAGE flash=%zu sram=%zu base=%" PRIu64 " sp=%" PRIu64 " entry=%" PRIu64 " used=%" PRIu64 "\n",m.flash_size,m.ram_size,m.base,m.sp,m.entry,m.used);
done:
 free(m.flash);free(m.ram);return rc;
}
