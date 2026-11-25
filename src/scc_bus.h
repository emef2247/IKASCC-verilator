#ifndef SCC_BUS_H
#define SCC_BUS_H
#include "ikascc_wrapper.h"
typedef struct {
    uint64_t origin_cycle,last_write_cycle,accesses,late_events,max_late_cycles,scheduled_sample;
    int failed;
} scc_bus_t;
void scc_bus_init(scc_bus_t* bus);
void scc_bus_wait_until_sample(scc_bus_t* bus,uint64_t sample);
/* One physical bus write: address A15..A11 in abhi, A7..A0 in ablo. */
void scc_bus_write(scc_bus_t* bus,uint8_t abhi,uint8_t ablo,uint8_t data);
/* Konami mapper BR2 (0x9000 page) := 0x3F, switching that page to SCC mode. */
void scc_bus_enable_scc(scc_bus_t* bus);
int scc_bus_debug_open(const char* path);
int scc_bus_debug_close(void);
void scc_bus_report(const scc_bus_t* bus);
#endif
