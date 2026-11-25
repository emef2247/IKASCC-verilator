#include "wav_writer.h"
#include <stdio.h>
static void le(FILE* fp,uint32_t value,int bytes) {
    while(bytes--) { fputc(value&255,fp);value>>=8; }
}
int wav_write_mono16(const char* path,const int16_t* samples,size_t frames,int rate) {
    if(frames>(UINT32_MAX-36u)/2u || rate<=0 || rate>1000000 || (frames && !samples)) return -1;
    FILE* fp=fopen(path,"wb");if(!fp)return -1;
    fwrite("RIFF",1,4,fp);le(fp,36u+(uint32_t)frames*2u,4);fwrite("WAVEfmt ",1,8,fp);
    le(fp,16,4);le(fp,1,2);le(fp,1,2);le(fp,(uint32_t)rate,4);le(fp,(uint32_t)rate*2u,4);
    le(fp,2,2);le(fp,16,2);fwrite("data",1,4,fp);le(fp,(uint32_t)frames*2u,4);
    const uint16_t endian=1;
    if(*(const unsigned char*)&endian) fwrite(samples,2,frames,fp);
    else for(size_t i=0;i<frames;i++) le(fp,(uint16_t)samples[i],2);
    int result=ferror(fp)?-1:0;if(fclose(fp))result=-1;return result;
}
