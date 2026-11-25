#include "scc_bus.h"
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
static FILE* debug=NULL;
static void advance(unsigned clocks) {while(clocks--)ikascc_step_emuclk_1cycle();}
static int deadline(scc_bus_t* bus,uint64_t sample,uint64_t* result) {
    const uint64_t hz=ikascc_get_clock_hz();
    const uint64_t extra=((sample%44100)*hz+44099)/44100;
    if(bus->origin_cycle>UINT64_MAX-extra || sample/44100>(UINT64_MAX-bus->origin_cycle-extra)/hz){bus->failed=1;return -1;}
    *result=bus->origin_cycle+(sample/44100)*hz+extra;return 0;
}
void scc_bus_init(scc_bus_t* bus) {memset(bus,0,sizeof(*bus));bus->origin_cycle=ikascc_get_clock_count();}
void scc_bus_wait_until_sample(scc_bus_t* bus,uint64_t sample) {
    uint64_t target;if(bus->failed||deadline(bus,sample,&target))return;
    while(ikascc_get_clock_count()<target)ikascc_step_emuclk_1cycle();
    bus->scheduled_sample=sample;
}
void scc_bus_write(scc_bus_t* bus,uint8_t abhi,uint8_t ablo,uint8_t data) {
    if(bus->failed)return;
    ikascc_set_ABHI(abhi);ikascc_set_ABLO(ablo);ikascc_set_DB(data);
    advance(1);
    ikascc_set_CS_n(0);ikascc_set_WR_n(0);advance(6);
    ikascc_set_WR_n(1);ikascc_set_CS_n(1);
    bus->last_write_cycle=ikascc_get_clock_count();
    uint64_t target;if(deadline(bus,bus->scheduled_sample,&target))return;
    const uint64_t actual=bus->last_write_cycle;
    const uint64_t late=actual>target?actual-target:0;
    ++bus->accesses;if(late)++bus->late_events;if(late>bus->max_late_cycles)bus->max_late_cycles=late;
    if(debug)fprintf(debug,"%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%02X,%02X,%02X\n",
        bus->accesses,bus->scheduled_sample,actual-bus->origin_cycle,late,abhi,ablo,data);
    /* Let the two-stage bus synchronizers and the wavetable RAM write
     * complete before the next transaction changes the bus. */
    advance(6);
}
void scc_bus_enable_scc(scc_bus_t* bus) {
    /* BR2 window is A15..A11 = 0x12 (0x9000-0x97FF); ABLO is not decoded. */
    scc_bus_write(bus,0x12,0x00,0x3F);
}
int scc_bus_debug_open(const char* path) {
    debug=fopen(path,"w");if(!debug)return -1;
    fprintf(debug,"access_idx,vgm_sample,actual_clock,late_clocks,abhi,ablo,data\n");return 0;
}
int scc_bus_debug_close(void){
    int rc=0;if(debug){if(ferror(debug))rc=-1;if(fclose(debug))rc=-1;}
    debug=NULL;if(rc)fprintf(stderr,"Failed writing bus diagnostic CSV\n");return rc;
}
void scc_bus_report(const scc_bus_t* bus) {
    printf("Bus writes=%" PRIu64 ", delayed writes=%" PRIu64 ", max lateness=%" PRIu64 " clocks (%.3f ms)\n",
        bus->accesses,bus->late_events,bus->max_late_cycles,bus->max_late_cycles*1000.0/ikascc_get_clock_hz());
}
