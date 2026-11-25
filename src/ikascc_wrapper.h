#ifndef IKASCC_WRAPPER_H
#define IKASCC_WRAPPER_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
int ikascc_set_clock_hz(uint32_t hz);
uint32_t ikascc_get_clock_hz(void);
uint64_t ikascc_get_clock_count(void);
uint64_t ikascc_get_sim_time(void);
void ikascc_init(void);
void ikascc_release(void);
void ikascc_reset(void);
void ikascc_step_emuclk_posedge(void);
void ikascc_step_emuclk_negedge(void);
static inline void ikascc_step_emuclk_1cycle(void) {
    ikascc_step_emuclk_posedge(); ikascc_step_emuclk_negedge();
}
void ikascc_set_CS_n(uint8_t v);
void ikascc_set_RD_n(uint8_t v);
void ikascc_set_WR_n(uint8_t v);
void ikascc_set_ABLO(uint8_t v);
void ikascc_set_ABHI(uint8_t v);
void ikascc_set_DB(uint8_t v);
uint8_t ikascc_get_DB(void);
uint8_t ikascc_get_DB_OE(void);
void ikascc_trace_init(const char* path);
void ikascc_trace_close(void);
void ikascc_audio_wav_open(const char* path, int sample_rate);
int ikascc_audio_csv_open(const char* path, const char* native_path);
int ikascc_audio_wav_close(void);
void ikascc_audio_wav_abort(void);
#ifdef __cplusplus
}
#endif
#endif
