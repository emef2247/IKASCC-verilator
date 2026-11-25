#include "ikascc_wrapper.h"
#include "VIKASCC.h"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include "wav_writer.h"
#include <cstdio>
#include <cinttypes>
#include <string>
#include <vector>
static VIKASCC* top=nullptr;
static VerilatedVcdC* trace=nullptr;
static uint32_t clock_hz=3579545;
static uint64_t cycles=0,time_ps=0,time_remainder=0;
static bool recording=false;
static uint64_t origin=0,origin_ps=0,previous_tick=0,next_tick=0;
static uint64_t changes=0;
static int rate=44100;
static int16_t previous=0,last_native=0;
static bool have_native=false;
static std::vector<int16_t> pcm;
static std::string wav_path;
static FILE* audio_csv=nullptr;
static FILE* native_csv=nullptr;
int ikascc_set_clock_hz(uint32_t hz) { if(top || !hz || hz>100000000)return -1;clock_hz=hz;return 0; }
uint32_t ikascc_get_clock_hz(void) {return clock_hz;}
uint64_t ikascc_get_clock_count(void) {return cycles;}
uint64_t ikascc_get_sim_time(void) {return time_ps;}
/* o_SOUND is a signed 11-bit value; Verilator stores it unsigned, so the
 * sign extension is explicit here. The WAV conversion scales it by 32,
 * mapping the chip's own summer range onto the full int16 range. */
static int16_t native_sample(void) {
    int32_t raw=(int32_t)(top->o_SOUND & 0x7FF);
    if(raw & 0x400) raw-=0x800;
    return (int16_t)(raw*32);
}
static void emit(int16_t v) {
    pcm.push_back(v);
    if(audio_csv)std::fprintf(audio_csv,"%zu,%d\n",pcm.size()-1,int(v));
    next_tick+=clock_hz;
}
static int16_t interpolate(int16_t from,int16_t to,uint64_t offset,uint64_t width) {
    int64_t n=(int64_t(to)-from)*int64_t(offset);
    n+=n>=0 ? int64_t(width/2) : -int64_t(width/2);
    return int16_t(from+n/int64_t(width));
}
static void capture(void) {
    const int16_t cur=native_sample();
    if(recording) {
        if(!have_native || cur!=last_native) {
            if(native_csv)std::fprintf(native_csv,"%" PRIu64 ",%" PRIu64 ",%d\n",
                cycles-origin,time_ps-origin_ps,int(cur/32));
            ++changes;last_native=cur;have_native=true;
        }
        const uint64_t tick=(cycles-origin)* (uint64_t)rate;
        while(next_tick<=tick && tick>previous_tick) {
            emit(interpolate(previous,cur,next_tick-previous_tick,tick-previous_tick));
        }
        previous_tick=tick;previous=cur;
    }
}
static void half_tick(uint8_t value) {
    if(!top)return;
    time_remainder+=1000000000000ULL;time_ps+=time_remainder/(2ULL*clock_hz);time_remainder%=2ULL*clock_hz;
    if(value)++cycles;
    top->i_EMUCLK=value;top->eval();
#if VM_TRACE
    if(trace)trace->dump(time_ps);
#endif
    if(value)capture();
}
void ikascc_step_emuclk_posedge(void) {half_tick(1);}
void ikascc_step_emuclk_negedge(void) {half_tick(0);}
void ikascc_init(void) {
    if(top)return;Verilated::traceEverOn(true);top=new VIKASCC;
    cycles=time_ps=time_remainder=0;top->i_EMUCLK=0;top->i_MCLK_PCEN_n=0;top->i_RST_n=1;
    top->i_CS_n=top->i_RD_n=top->i_WR_n=1;top->i_ABLO=top->i_ABHI=top->i_DB=0;top->eval();
}
void ikascc_reset(void) {
    if(!top)return;top->i_RST_n=0;
    for(int i=0;i<1024;i++)ikascc_step_emuclk_1cycle();
    top->i_RST_n=1;for(int i=0;i<256;i++)ikascc_step_emuclk_1cycle();
}
void ikascc_set_CS_n(uint8_t v) {if(top)top->i_CS_n=!!v;}
void ikascc_set_RD_n(uint8_t v) {if(top)top->i_RD_n=!!v;}
void ikascc_set_WR_n(uint8_t v) {if(top)top->i_WR_n=!!v;}
void ikascc_set_ABLO(uint8_t v) {if(top)top->i_ABLO=v;}
void ikascc_set_ABHI(uint8_t v) {if(top)top->i_ABHI=v&31;}
void ikascc_set_DB(uint8_t v) {if(top)top->i_DB=v;}
uint8_t ikascc_get_DB(void) {return top?top->o_DB:0;}
uint8_t ikascc_get_DB_OE(void) {return top?top->o_DB_OE:0;}
void ikascc_audio_wav_open(const char* path,int sample_rate) {
    if(recording)return;pcm.clear();wav_path=path?path:"audio_samples.wav";rate=sample_rate;
    origin=cycles;origin_ps=time_ps;previous_tick=0;next_tick=clock_hz;
    previous=native_sample();last_native=previous;have_native=true;changes=0;recording=true;
}
int ikascc_audio_csv_open(const char* path,const char* native_path) {
    audio_csv=std::fopen(path,"w");if(!audio_csv)return -1;
    native_csv=std::fopen(native_path,"w");if(!native_csv){std::fclose(audio_csv);audio_csv=nullptr;return -1;}
    std::fprintf(audio_csv,"frame,sample\n");std::fprintf(native_csv,"clock,t_ps,sound11\n");return 0;
}
static int close_csv(void) {
    int rc=0;
    if(audio_csv){if(std::ferror(audio_csv))rc=-1;if(std::fclose(audio_csv))rc=-1;}
    if(native_csv){if(std::ferror(native_csv))rc=-1;if(std::fclose(native_csv))rc=-1;}
    audio_csv=native_csv=nullptr;return rc;
}
int ikascc_audio_wav_close(void) {
    if(!recording)return 0;
    const uint64_t end=(cycles-origin)*(uint64_t)rate;
    while(next_tick<=end)emit(previous);
    int rc=close_csv();if(wav_write_mono16(wav_path.c_str(),pcm.data(),pcm.size(),rate))rc=-1;
    if(rc)std::fprintf(stderr,"Failed writing audio outputs: %s\n",wav_path.c_str());
    else std::printf("WAV written: %s (%zu mono frames @ %d Hz)\n",wav_path.c_str(),pcm.size(),rate);
    std::printf("Native output changes=%" PRIu64 "\n",changes);
    recording=false;return rc;
}
void ikascc_audio_wav_abort(void) {close_csv();recording=false;pcm.clear();}
void ikascc_trace_init(const char* path) {
#if VM_TRACE
    if(!top||trace)return;trace=new VerilatedVcdC;top->trace(trace,99);trace->open(path);
#else
    (void)path;
#endif
}
void ikascc_trace_close(void) {
#if VM_TRACE
    if(trace){trace->close();delete trace;trace=nullptr;}
#endif
}
void ikascc_release(void) {ikascc_audio_wav_close();ikascc_trace_close();if(top){top->final();delete top;top=nullptr;}}
