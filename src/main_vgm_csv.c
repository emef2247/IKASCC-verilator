#include "ikascc_wrapper.h"
#include "scc_bus.h"
#include "vgm_player.h"
#include "vgm_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <strings.h>
#include <string>
#include <vector>
#include <limits.h>
#include <sys/stat.h>

static std::string resolved_path(const std::string& path) {
    char buffer[PATH_MAX];
    if (realpath(path.c_str(),buffer)) return buffer;
    const size_t slash = path.find_last_of('/');
    const std::string parent = slash == std::string::npos ? "." : (slash == 0 ? "/" : path.substr(0,slash));
    if (!realpath(parent.c_str(),buffer)) return path;
    return std::string(buffer)+"/"+path.substr(slash == std::string::npos ? 0 : slash+1);
}
static bool same_file(const std::string& a, const std::string& b) {
    struct stat sa, sb;
    if (stat(a.c_str(),&sa) == 0 && stat(b.c_str(),&sb) == 0 &&
        sa.st_dev == sb.st_dev && sa.st_ino == sb.st_ino) return true;
    return resolved_path(a) == resolved_path(b);
}
static void usage(const char* name) {
    printf("Usage: %s INPUT.vgm|INPUT.csv [--output FILE.wav] [--sample-rate 44100|48000]\n",name);
    printf("  --enable-csv     source bus-event CSV, output PCM CSV, and native-output CSV\n");
    printf("  --debug [FILE]   source timestamp / actual bus-clock log\n");
    printf("  --vcd [FILE]     full RTL trace (large; disabled by default)\n");
    printf("  --clock HZ       master clock when the input carries none (VGM header 0xD0 wins when nonzero)\n");
}
static bool is_vgm(const char* path) {
    const size_t n = strlen(path);
    return n >= 4 && (!strcasecmp(path+n-4,".vgm") || !strcasecmp(path+n-4,".vgz"));
}
int main(int argc, char** argv) {
    const char* input = NULL; const char* output = NULL;
    const char* debug = NULL; const char* vcd = NULL;
    bool csv = false; uint32_t clock = 3579545; int rate = 44100; bool clock_override = false;
    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];
        if (!strcmp(arg,"--help") || !strcmp(arg,"-h")) { usage(argv[0]); return 0; }
        if (!strcmp(arg,"--enable-csv")) csv = true;
        else if (!strcmp(arg,"--no-csv")) csv = false;
        else if (!strcmp(arg,"--vcd") || !strcmp(arg,"--debug")) {
            const char* name = !strcmp(arg,"--vcd") ? "ikascc_dump.vcd" : "scc_bus_calls.log";
            if (i+1 < argc && argv[i+1][0] != '-') name = argv[++i];
            if (!strcmp(arg,"--vcd")) vcd = name; else debug = name;
        } else if (!strcmp(arg,"--output") || !strcmp(arg,"--sample-rate") || !strcmp(arg,"--clock")) {
            if (i+1 >= argc) { fprintf(stderr,"Missing value for %s\n",arg); return 1; }
            const char* value = argv[++i];
            if (!strcmp(arg,"--output")) output = value;
            else {
                char* end = NULL; const unsigned long n = strtoul(value,&end,10);
                if (!*value || *end || n == 0 || n > 100000000) { fprintf(stderr,"Invalid %s\n",arg); return 1; }
                if (!strcmp(arg,"--sample-rate")) {
                    if (n != 44100 && n != 48000) { fprintf(stderr,"Invalid --sample-rate\n"); return 1; }
                    rate = (int)n;
                } else { clock = (uint32_t)n; clock_override = true; }
            }
        } else if (arg[0] == '-') { fprintf(stderr,"Unknown option %s\n",arg); usage(argv[0]); return 1; }
        else if (!input) input = arg;
        else { fprintf(stderr,"Multiple inputs given\n"); return 1; }
    }
    if (!input) { usage(argv[0]); return 1; }
    if (!output) output = "audio_samples.wav";
    if (same_file(input,output)) { fprintf(stderr,"Output must differ from input\n"); return 1; }

    const bool vgm_input = is_vgm(input);
    if (vgm_input) {
        uint32_t header_clock = 0;
        int rc = vgm_read_k051649_clock(input,&header_clock);
        if (rc < 0) { fprintf(stderr,"Invalid K051649 clock in VGM header (rc=%d)\n",rc); return 1; }
        if (rc == 0) clock = header_clock;
        else if (!clock_override) clock = 3579545;
    } else {
        uint32_t csv_clock = 0;
        int rc = vgm_player_read_csv_clock(input,&csv_clock);
        if (rc < 0) { fprintf(stderr,"Invalid CSV clock metadata (rc=%d)\n",rc); return 1; }
        if (rc == 0 && !clock_override) clock = csv_clock;
    }
    if (ikascc_set_clock_hz(clock)) { fprintf(stderr,"Invalid clock %" PRIu32 "\n",clock); return 1; }
    printf("K051649 clock=%" PRIu32 " Hz, WAV rate=%d Hz\n",clock,rate);

    ikascc_init();
    ikascc_reset();
    scc_bus_t bus; scc_bus_init(&bus);
    if (vcd) ikascc_trace_init(vcd);
    if (debug && scc_bus_debug_open(debug)) { fprintf(stderr,"Cannot open debug log %s\n",debug); ikascc_release(); return 1; }

    std::string event_csv, pcm_csv, native_csv;
    if (csv) {
        event_csv = std::string(output)+".events.csv";
        pcm_csv = std::string(output)+".pcm.csv";
        native_csv = std::string(output)+".native.csv";
        if (ikascc_audio_csv_open(pcm_csv.c_str(),native_csv.c_str())) {
            fprintf(stderr,"Cannot open audio CSV outputs\n"); scc_bus_debug_close(); ikascc_release(); return 1;
        }
    }
    ikascc_audio_wav_open(output,rate);
    int rc = vgm_input ? vgm_player_run_vgm(input,&bus,csv ? event_csv.c_str() : NULL)
                       : vgm_player_run_csv(input,&bus);
    int wav_rc = rc ? (ikascc_audio_wav_abort(),-1) : ikascc_audio_wav_close();
    scc_bus_report(&bus);
    if (debug) scc_bus_debug_close();
    ikascc_release();
    if (rc) { fprintf(stderr,"Playback failed (rc=%d)\n",rc); return 1; }
    return wav_rc ? 1 : 0;
}
