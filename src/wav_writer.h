#ifndef WAV_WRITER_H
#define WAV_WRITER_H
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
int wav_write_mono16(const char* path,const int16_t* samples,size_t frames,int rate);
#ifdef __cplusplus
}
#endif
#endif
