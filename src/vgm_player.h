#ifndef VGM_PLAYER_H
#define VGM_PLAYER_H
#include "scc_bus.h"
/* 0: clock found; 1: metadata absent; negative: invalid/read failure. */
int vgm_player_read_csv_clock(const char* path,uint32_t* clock_hz);
int vgm_player_run_csv(const char* path,scc_bus_t* bus);
int vgm_player_run_vgm(const char* path,scc_bus_t* bus,const char* event_csv_path);
#endif
