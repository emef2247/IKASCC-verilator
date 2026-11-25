#include "vgm_player.h"
#include "vgm_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <ctype.h>

/* A VGM K051649 write (0xD2) carries three operand bytes: port, reg,
 * data. The port selects the register group (0/4 waveform, 1 frequency,
 * 2 volume, 3 key on/off, 5 test; bit 7 is the dual-chip select, and
 * dual-chip headers are rejected by the parser's clock check). The
 * player translates each logical write into a physical bus write on
 * the SCC register page (A15..A11 = 0x13, the 0x9800 window):
 *   waveform  reg 0x00..0x5F -> page 0x00..0x5F
 *             reg 0x60..0xFF -> page 0x60..0x7F (ch4/ch5 shared RAM)
 *   frequency reg 0x00..0x09 -> page 0x80..0x89
 *   volume    reg 0x00..0x07 -> page 0x8A..0x91 (only 0..4 exist)
 *   key on/off               -> page 0x8F
 *   test      reg r          -> page 0xE0 | (r & 0x1F)
 * This matches the MAME/libvgm K051649 (non-plus) register model and
 * the group bases (0x9800/0x9880/0x988A/0x988F) used by msx_vgm2mml. */
typedef struct {
    scc_bus_t* bus;
    uint64_t sample, last_output_sample, events;
    FILE* csv;
    int failed;
} player_t;

static void emit_csv(player_t* p, uint8_t abhi, uint8_t ablo, uint8_t data) {
    if (p->csv && fprintf(p->csv,"%" PRIu64 ",%02X,%02X,%02X\n",
                          p->sample-p->last_output_sample,abhi,ablo,data) < 0) p->failed = 1;
    p->last_output_sample = p->sample;
}
static void do_write(player_t* p, uint8_t abhi, uint8_t ablo, uint8_t data) {
    scc_bus_wait_until_sample(p->bus,p->sample);
    scc_bus_write(p->bus,abhi,ablo,data);
    if (p->bus->failed) { p->failed = 1; return; }
    emit_csv(p,abhi,ablo,data);
    ++p->events;
}
static void on_wait(void* user, uint32_t samples) {
    player_t* p = (player_t*)user;
    if (p->failed || p->bus->failed) return;
    if (samples > UINT64_MAX-p->sample) {
        fprintf(stderr,"VGM sample timeline overflow\n"); p->failed = 1; return;
    }
    p->sample += samples;
}
static void on_write(void* user, VGMChipId chip, uint8_t port, uint8_t reg, uint8_t value, uint32_t post_wait) {
    player_t* p = (player_t*)user;
    if (p->failed || p->bus->failed) return;
    if (chip != VGM_CHIP_K051649) { p->failed = 1; return; }
    switch (port & 0x7F) {
        case 0: case 4:
            do_write(p,0x13,(uint8_t)(reg < 0x60 ? reg : 0x60 + (reg & 0x1F)),value);
            break;
        case 1: do_write(p,0x13,(uint8_t)(0x80 + (reg & 0x0F)),value); break;
        case 2: do_write(p,0x13,(uint8_t)(0x8A + (reg & 0x07)),value); break;
        case 3: do_write(p,0x13,0x8F,value); break;
        case 5: do_write(p,0x13,(uint8_t)(0xE0 | (reg & 0x1F)),value); break;
        default: break; /* groups 6/7 are unused */
    }
    if (!p->failed) on_wait(user,post_wait);
}
static void on_unknown(void* user, uint8_t opcode, uint32_t offset) {
    (void)user;
    fprintf(stderr,"Unsupported or malformed VGM command 0x%02X at 0x%X\n",opcode,offset);
}
int vgm_player_run_vgm(const char* path, scc_bus_t* bus, const char* event_csv_path) {
    if (!path || !bus) return -1;
    player_t p; memset(&p,0,sizeof(p)); p.bus=bus;
    if (event_csv_path) {
        p.csv=fopen(event_csv_path,"w"); if (!p.csv) return -1;
        if (fprintf(p.csv,"# clock_hz=%u\ndelay,abhi,ablo,data\n",ikascc_get_clock_hz()) < 0) p.failed=1;
    }
    /* The VGM stream assumes the SCC register page is already selected;
     * on the physical cartridge that is the BR2 := 0x3F mapper write. */
    if (!p.failed) do_write(&p,0x12,0x00,0x3F);
    VGMParserCallbacks cb; memset(&cb,0,sizeof(cb));
    cb.on_reg_write=on_write; cb.on_wait=on_wait; cb.on_unknown=on_unknown;
    int rc=vgm_parse_file(path,&cb,&p);
    if (!rc && !p.failed && !bus->failed) {
        scc_bus_wait_until_sample(bus,p.sample);
        if (p.csv && p.sample>p.last_output_sample &&
            fprintf(p.csv,"%" PRIu64 ",wait,00,00\n",p.sample-p.last_output_sample)<0) p.failed=1;
    }
    if (p.csv && fclose(p.csv)) p.failed=1;
    printf("VGM bus writes=%" PRIu64 ", timeline=%" PRIu64 " samples (%.6f s), first traversal only\n",p.events,p.sample,p.sample/44100.0);
    return rc ? rc : (p.failed || bus->failed ? -1 : 0);
}
static int parse_number(char* text,int base,uint64_t* result) {
    while (isspace((unsigned char)*text)) ++text;
    if (!*text || *text=='-') return -1;
    errno=0; char* end=NULL; unsigned long long n=strtoull(text,&end,base);
    if (errno || end==text) return -1;
    while (isspace((unsigned char)*end)) ++end;
    if (*end) return -1;
    *result=n; return 0;
}
int vgm_player_read_csv_clock(const char* path,uint32_t* clock_hz) {
    if (!path || !clock_hz) return -1;
    FILE* fp=fopen(path,"r"); if (!fp) return -1;
    char line[512]; uint32_t found=0; int rc=1;
    while (fgets(line,sizeof(line),fp)) {
        if (!strchr(line,'\n') && !feof(fp)) { rc=-2; break; }
        if (!strncmp(line,"# clock_hz=",11)) {
            uint64_t value=0;
            if (parse_number(line+11,10,&value) || value==0 || value>100000000 ||
                (found && found!=value)) { rc=-2; break; }
            found=(uint32_t)value; rc=0;
        }
    }
    if (ferror(fp)) rc=-1;
    if (fclose(fp)) rc=-1;
    if (rc==0) *clock_hz=found;
    return rc;
}
int vgm_player_run_csv(const char* path,scc_bus_t* bus) {
    if (!path || !bus) return -1;
    FILE* fp=fopen(path,"r"); if (!fp) return -1;
    char line[512]; uint64_t sample=0,events=0; unsigned row=0; int rc=0;
    while (fgets(line,sizeof(line),fp)) {
        ++row;
        if (!strchr(line,'\n') && !feof(fp)) { rc=-2; break; }
        if (!strcmp(line,"delay,abhi,ablo,data\n") || !strcmp(line,"delay,abhi,ablo,data\r\n")) continue;
        if (line[0]=='#' || line[0]=='\n' || line[0]=='\r') continue;
        char* c1=strchr(line,','); char* c2=c1?strchr(c1+1,','):NULL; char* c3=c2?strchr(c2+1,','):NULL;
        if (!c1 || !c2 || !c3 || strchr(c3+1,',')) { rc=-2; break; }
        *c1=0; *c2=0; *c3=0;
        uint64_t delay=0,abhi=0,ablo=0,data=0;
        if (parse_number(line,10,&delay) || delay>UINT64_MAX-sample) { rc=-2; break; }
        sample+=delay; scc_bus_wait_until_sample(bus,sample);
        if (!strcmp(c1+1,"wait")) {
            if (parse_number(c2+1,16,&ablo) || parse_number(c3+1,16,&data) || ablo>255 || data>255) { rc=-2; break; }
        } else {
            if (parse_number(c1+1,16,&abhi) || parse_number(c2+1,16,&ablo) || parse_number(c3+1,16,&data) ||
                abhi>31 || ablo>255 || data>255) { rc=-2; break; }
            scc_bus_write(bus,(uint8_t)abhi,(uint8_t)ablo,(uint8_t)data); ++events;
        }
        if (bus->failed) { rc=-3; break; }
    }
    if (rc==-2) fprintf(stderr,"Invalid event CSV row %u\n",row);
    if (ferror(fp)) rc=-3;
    if (fclose(fp)) rc=-3;
    printf("CSV bus writes=%" PRIu64 ", timeline=%" PRIu64 " samples (%.6f s)\n",events,sample,sample/44100.0);
    return rc;
}
