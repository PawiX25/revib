/*
 * The SPU (libspu, and libsnd's pitch helper): 512 KB of sound memory, 24
 * ADPCM voices with their envelopes, the CD input, and the capture buffers
 * at the start of sound memory (CD left 0x000, CD right 0x400, voice 1 0x800,
 * voice 3 0xC00, 512 samples each), which the game reads back with its
 * interrupt to follow and analyse the music.
 *
 * The game also plays its delayed copy of the music through the reverb's
 * work area (FbStream.cpp): with the reverb type "off" moved to the last 512
 * bytes of sound memory, what is written there is heard, 22050 samples a
 * second, at the reverb's depth. The SPU here does just that for the reverb.
 */
#include <string.h>
#include <math.h>
#include <sys/types.h>

extern "C" {
#include <libspu.h>
}

#include "psx.h"

#define SPU_RAM 0x80000
#define NUM_VOICES 24

static uint8_t s_ram[SPU_RAM];

enum { ENV_OFF, ENV_ATTACK, ENV_DECAY, ENV_SUSTAIN, ENV_RELEASE };

struct Voice {
    uint32_t start;     /* the sample's first block */
    uint32_t loop;
    uint32_t addr;      /* the block being played */
    uint16_t pitch;
    int16_t volL, volR;
    uint16_t adsr1, adsr2;
    int keyOn;          /* keyed on (until keyed off) */
    int env;            /* 0..0x7FFF */
    int state;
    int envCounter;
    uint32_t counter;   /* 4.12: within the decoded samples */
    int16_t decoded[28];
    int index;          /* the sample of `decoded` */
    int prev1, prev2;   /* the ADPCM filter's last two samples */
    int16_t history[4]; /* the last four decoded samples, the newest last: the interpolation's */
    int out;            /* the last output, enveloped */
};

#include "gauss.h"

/* the voices interpolated as the hardware does (the page's choice; linear otherwise) */
static int s_gaussian = 0;

extern "C" void port_set_gaussian(int on)
{
    s_gaussian = on;
}

static Voice s_voices[NUM_VOICES];

static struct {
    int16_t mvolL, mvolR;
    int16_t cdVolL, cdVolR;
    int cdMix, cdReverb;
} s_common;

static uint32_t s_transferAddr;
static SpuTransferCallbackProc s_transferCallback;
static int s_transferPending;

static uint32_t s_irqAddr;
static int s_irqOn;
static SpuIRQCallbackProc s_irqCallback;

static int s_capture; /* the position in the capture buffers, 0..511 */

static uint32_t s_reverbStart = 0x7FFF0;
static int16_t s_depthL, s_depthR;
static int s_reverbOn;

extern "C" long _spu_rev_startaddr[SPU_REV_MODE_MAX] = {
    0xFFFE, 0xFB28, 0xFC18, 0xF6F8, 0xF204, 0xEA44, 0xE128, 0xCFF8, 0xCFF8, 0xF880,
};

/* --- voices --- */

static const int s_filterPos[5] = { 0, 60, 115, 98, 122 };
static const int s_filterNeg[5] = { 0, 0, -52, -55, -60 };

static void DecodeBlock(Voice* v)
{
    const uint8_t* block = &s_ram[v->addr & (SPU_RAM - 16)];
    int shift = block[0] & 15;
    int filter = (block[0] >> 4) & 7;
    if (filter > 4) filter = 4;
    if (shift > 12) shift = 9;
    int flags = block[1];
    if (flags & 4) {
        v->loop = v->addr;
    }
    for (int i = 0; i < 28; i++) {
        int nibble = (block[2 + i / 2] >> ((i & 1) * 4)) & 15;
        int sample = (int16_t)(nibble << 12) >> shift;
        sample += (v->prev1 * s_filterPos[filter] + v->prev2 * s_filterNeg[filter] + 32) >> 6;
        if (sample > 32767) sample = 32767;
        if (sample < -32768) sample = -32768;
        v->decoded[i] = sample;
        v->prev2 = v->prev1;
        v->prev1 = sample;
    }
}

/* after the block: on to the next one, the loop, or the end */
static void NextBlock(Voice* v)
{
    int flags = s_ram[(v->addr & (SPU_RAM - 16)) + 1];
    if (flags & 1) {
        v->addr = v->loop;
        if (!(flags & 2)) {
            /* the end, without a repeat: silence */
            v->state = ENV_OFF;
            v->env = 0;
        }
    } else {
        v->addr = (v->addr + 16) & (SPU_RAM - 1);
    }
    DecodeBlock(v);
}

static void KeyOn(Voice* v)
{
    v->addr = v->start;
    v->loop = v->start;
    v->prev1 = v->prev2 = 0;
    v->counter = 0;
    v->index = 0;
    memset(v->history, 0, sizeof(v->history));
    v->env = 0;
    v->envCounter = 0;
    v->state = ENV_ATTACK;
    v->keyOn = 1;
    DecodeBlock(v);
}

static void KeyOff(Voice* v)
{
    v->keyOn = 0;
    if (v->state != ENV_OFF) {
        v->state = ENV_RELEASE;
        v->envCounter = 0;
    }
}

/* one step of the envelope (psx-spx's "ADSR"), every `1 << (shift - 11)` samples */
static void EnvelopeStep(Voice* v, int exponential, int decrease, int shift, int step)
{
    int cycles = 1 << (shift > 11 ? shift - 11 : 0);
    int delta = step << (shift < 11 ? 11 - shift : 0);
    if (exponential && !decrease && v->env > 0x6000) {
        cycles *= 4;
    }
    if (exponential && decrease) {
        delta = (delta * v->env) >> 15;
    }
    if (++v->envCounter < cycles) {
        return;
    }
    v->envCounter = 0;
    int env = v->env + delta;
    if (env < 0) env = 0;
    if (env > 0x7FFF) env = 0x7FFF;
    v->env = env;
}

static void Envelope(Voice* v)
{
    uint16_t a1 = v->adsr1, a2 = v->adsr2;
    switch (v->state) {
    case ENV_ATTACK:
        EnvelopeStep(v, a1 >> 15, 0, (a1 >> 10) & 31, 7 - ((a1 >> 8) & 3));
        if (v->env >= 0x7FFF) {
            v->state = ENV_DECAY;
            v->envCounter = 0;
        }
        break;
    case ENV_DECAY: {
        EnvelopeStep(v, 1, 1, ((a1 >> 4) & 15) * 4, -8);
        int level = ((a1 & 15) + 1) * 0x800;
        if (v->env <= level) {
            v->state = ENV_SUSTAIN;
            v->envCounter = 0;
        }
        break;
    }
    case ENV_SUSTAIN: {
        int decrease = (a2 >> 14) & 1;
        int stepBits = (a2 >> 6) & 3;
        EnvelopeStep(v, a2 >> 15, decrease, (a2 >> 8) & 31, decrease ? -8 + stepBits : 7 - stepBits);
        break;
    }
    case ENV_RELEASE:
        EnvelopeStep(v, (a2 >> 5) & 1, 1, (a2 & 31) * 4, -8);
        if (v->env == 0) {
            v->state = ENV_OFF;
        }
        break;
    }
}

static int VoiceSample(Voice* v)
{
    if (v->state == ENV_OFF) {
        v->out = 0;
        return 0;
    }
    const int16_t* h = v->history;
    int sample;
    if (s_gaussian) {
        /* the hardware's Gaussian interpolation over the last four samples: softer */
        int f = (v->counter >> 4) & 0xFF;
        sample = ((s_gauss[0x0FF - f] * h[0]) >> 15) + ((s_gauss[0x1FF - f] * h[1]) >> 15) +
                 ((s_gauss[0x100 + f] * h[2]) >> 15) + ((s_gauss[f] * h[3]) >> 15);
    } else {
        /* linear, between the last two: brighter (with the high hiss of the samples' imaging) */
        sample = h[2] + (((h[3] - h[2]) * (int)(v->counter & 0xFFF)) >> 12);
    }
    uint32_t step = v->pitch > 0x3FFF ? 0x3FFF : v->pitch;
    v->counter += step;
    while (v->counter >= 0x1000) {
        v->counter -= 0x1000;
        v->history[0] = v->history[1];
        v->history[1] = v->history[2];
        v->history[2] = v->history[3];
        v->history[3] = v->decoded[v->index];
        if (++v->index == 28) {
            v->index = 0;
            NextBlock(v);
            if (v->state == ENV_OFF) {
                break;
            }
        }
    }
    Envelope(v);
    int out = (sample * v->env) >> 15;
    v->out = out;
    return out;
}

static inline int Clamp16(int v)
{
    return v < -32768 ? -32768 : v > 32767 ? 32767 : v;
}

/* --- the SPU's time --- */

static inline void CheckIrq(uint32_t addr)
{
    if (s_irqOn && (addr & ~7u) == (s_irqAddr & ~7u)) {
        s_irqOn = 0;
        sys_interrupt(s_irqCallback);
    }
}

static int16_t s_out[4096 * 2];
static int16_t s_cd[4096 * 2];
static int s_reverbPhase;
static int16_t s_reverbSample;

/*
 * The music of high quality (the page's option): the CD's own stereo
 * samples, kept for the game's delay, played instead of its copy in the
 * reverb's work area (mono, 22050 Hz), at the same time and level.
 */
#define HISTORY (1 << 19) /* stereo samples: 11.9 s */
static int16_t s_history[HISTORY * 2];
static uint32_t s_historyAt;
static int s_hqMusic = 1;
static int s_zeroRun = 64; /* the work area's samples in a row that were 0 */
/*
 * From a sample of the CD to its copy in the work area being heard: the
 * game's delay (AudioStream::Init(8000): 1379 blocks of 256 samples) and
 * the capture's buffer the copy goes through (measured).
 */
static const int s_hqDelay = 1379 * 256 + 768;

extern "C" void port_set_hq_music(int on)
{
    s_hqMusic = on;
}

extern "C" void spu_run(int frames)
{
    while (frames > 0) {
        int n = frames > 4096 ? 4096 : frames;
        cd_audio(s_cd, n);
        for (int i = 0; i < n; i++) {
            if (s_transferPending) {
                s_transferPending = 0;
                sys_interrupt(s_transferCallback);
            }

            int left = 0, right = 0;
            for (int k = 0; k < NUM_VOICES; k++) {
                Voice* v = &s_voices[k];
                int s = VoiceSample(v);
                if (s != 0) {
                    left += (s * v->volL * 2) >> 15;
                    right += (s * v->volR * 2) >> 15;
                }
            }

            int cdL = s_cd[i * 2], cdR = s_cd[i * 2 + 1];
            /* the capture buffers: the CD's sound as it comes in, voices 1 and 3 */
            uint32_t pos = s_capture * 2;
            *(int16_t*)&s_ram[0x000 + pos] = cdL;
            *(int16_t*)&s_ram[0x400 + pos] = cdR;
            *(int16_t*)&s_ram[0x800 + pos] = Clamp16(s_voices[1].out);
            *(int16_t*)&s_ram[0xC00 + pos] = Clamp16(s_voices[3].out);
            CheckIrq(0x000 + pos);
            CheckIrq(0x400 + pos);
            CheckIrq(0x800 + pos);
            CheckIrq(0xC00 + pos);
            s_capture = (s_capture + 1) & 511;

            if (s_common.cdMix) {
                left += (cdL * s_common.cdVolL) >> 15;
                right += (cdR * s_common.cdVolR) >> 15;
            }

            s_history[s_historyAt * 2] = cdL;
            s_history[s_historyAt * 2 + 1] = cdR;
            s_historyAt = (s_historyAt + 1) & (HISTORY - 1);

            /* the reverb, as the game uses it: its work area played back at 22050 Hz, in step with the capture */
            if (s_reverbOn && s_reverbStart >= 0x7FE00) {
                if ((s_reverbPhase & 1) == 0) {
                    uint32_t addr = s_reverbStart + ((s_reverbPhase >> 1) & 255) * 2;
                    s_reverbSample = *(int16_t*)&s_ram[addr & (SPU_RAM - 2)];
                    s_zeroRun = s_reverbSample == 0 ? s_zeroRun + 1 : 0;
                }
                if (s_hqMusic) {
                    /* the same moment of the music, from the CD's own samples; silent where the game's copy is (its silent blocks) */
                    uint32_t at = (s_historyAt - 1 - s_hqDelay) & (HISTORY - 1);
                    int hqL = s_history[at * 2], hqR = s_history[at * 2 + 1];
                    if (s_zeroRun >= 64) {
                        hqL = hqR = 0;
                    }
                    left += (hqL * s_depthL) >> 15;
                    right += (hqR * s_depthR) >> 15;
                } else {
                    left += (s_reverbSample * s_depthL) >> 15;
                    right += (s_reverbSample * s_depthR) >> 15;
                }
            }
            s_reverbPhase = (s_reverbPhase + 1) & 511;

            left = (Clamp16(left) * s_common.mvolL * 2) >> 15;
            right = (Clamp16(right) * s_common.mvolR * 2) >> 15;
            s_out[i * 2] = Clamp16(left);
            s_out[i * 2 + 1] = Clamp16(right);
        }
        js_audio_push(s_out, n);
        frames -= n;
    }
}

extern "C" void spu_init(void)
{
    memset(s_ram, 0, sizeof(s_ram));
    memset(s_voices, 0, sizeof(s_voices));
    memset(&s_common, 0, sizeof(s_common));
}

/* --- sound memory's allocator (SpuMalloc) --- */

#define MAX_BLOCKS 32
static struct {
    uint32_t addr, size;
    int used;
} s_blocks[MAX_BLOCKS];
static int s_numBlocks;

extern "C" {

void SpuInit(void)
{
    spu_init();
    s_irqOn = 0;
    s_irqCallback = NULL;
    s_transferCallback = NULL;
    s_reverbOn = 0;
    s_capture = 0;
    s_reverbPhase = 0;
}

void SpuQuit(void)
{
    for (int i = 0; i < NUM_VOICES; i++) {
        s_voices[i].state = ENV_OFF;
        s_voices[i].env = 0;
    }
}

long SpuInitMalloc(long num, char* top)
{
    s_numBlocks = 1;
    s_blocks[0].addr = 0x1010;
    s_blocks[0].size = 0x7FE00 - 0x1010;
    s_blocks[0].used = 0;
    return num;
}

long SpuMalloc(long size)
{
    size = (size + 15) & ~15;
    for (int i = 0; i < s_numBlocks; i++) {
        if (!s_blocks[i].used && s_blocks[i].size >= (uint32_t)size) {
            if (s_blocks[i].size > (uint32_t)size && s_numBlocks < MAX_BLOCKS) {
                memmove(&s_blocks[i + 1], &s_blocks[i], (s_numBlocks - i) * sizeof(s_blocks[0]));
                s_numBlocks++;
                s_blocks[i + 1].addr = s_blocks[i].addr + size;
                s_blocks[i + 1].size = s_blocks[i].size - size;
                s_blocks[i + 1].used = 0;
                s_blocks[i].size = size;
            }
            s_blocks[i].used = 1;
            return s_blocks[i].addr;
        }
    }
    return -1;
}

void SpuFree(unsigned long addr)
{
    for (int i = 0; i < s_numBlocks; i++) {
        if (s_blocks[i].addr == addr) {
            s_blocks[i].used = 0;
            /* joined to free neighbours */
            if (i + 1 < s_numBlocks && !s_blocks[i + 1].used) {
                s_blocks[i].size += s_blocks[i + 1].size;
                memmove(&s_blocks[i + 1], &s_blocks[i + 2], (s_numBlocks - i - 2) * sizeof(s_blocks[0]));
                s_numBlocks--;
            }
            if (i > 0 && !s_blocks[i - 1].used) {
                s_blocks[i - 1].size += s_blocks[i].size;
                memmove(&s_blocks[i], &s_blocks[i + 1], (s_numBlocks - i - 1) * sizeof(s_blocks[0]));
                s_numBlocks--;
            }
            return;
        }
    }
}

/* --- transfers --- */

long SpuSetTransferMode(long mode)
{
    return mode;
}

unsigned long SpuSetTransferStartAddr(unsigned long addr)
{
    s_transferAddr = addr & (SPU_RAM - 1) & ~7u;
    return s_transferAddr;
}

unsigned long SpuWrite(unsigned char* addr, unsigned long size)
{
    for (unsigned long i = 0; i < size; i++) {
        s_ram[(s_transferAddr + i) & (SPU_RAM - 1)] = addr[i];
    }
    s_transferPending = s_transferCallback != NULL;
    return size;
}

unsigned long SpuWrite0(unsigned long size)
{
    for (unsigned long i = 0; i < size; i++) {
        s_ram[(s_transferAddr + i) & (SPU_RAM - 1)] = 0;
    }
    s_transferPending = s_transferCallback != NULL;
    return size;
}

unsigned long SpuRead(unsigned char* addr, unsigned long size)
{
    for (unsigned long i = 0; i < size; i++) {
        addr[i] = s_ram[(s_transferAddr + i) & (SPU_RAM - 1)];
    }
    s_transferPending = s_transferCallback != NULL;
    return size;
}

long SpuIsTransferCompleted(long flag)
{
    return 1;
}

SpuTransferCallbackProc SpuSetTransferCallback(SpuTransferCallbackProc func)
{
    SpuTransferCallbackProc previous = s_transferCallback;
    s_transferCallback = func;
    return previous;
}

/* --- interrupts --- */

long SpuSetIRQ(long on_off)
{
    s_irqOn = on_off == SPU_ON;
    return on_off;
}

unsigned long SpuSetIRQAddr(unsigned long addr)
{
    s_irqAddr = addr & (SPU_RAM - 1);
    return s_irqAddr;
}

unsigned long SpuGetIRQAddr(void)
{
    return s_irqAddr;
}

SpuIRQCallbackProc SpuSetIRQCallback(SpuIRQCallbackProc func)
{
    SpuIRQCallbackProc previous = s_irqCallback;
    s_irqCallback = func;
    return previous;
}

/* --- voices --- */

void SpuSetVoiceAttr(SpuVoiceAttr* arg)
{
    unsigned long mask = arg->mask;
    for (int i = 0; i < NUM_VOICES; i++) {
        if (!(arg->voice & (1u << i))) {
            continue;
        }
        Voice* v = &s_voices[i];
        if (mask == 0 || (mask & SPU_VOICE_WDSA)) v->start = arg->addr & (SPU_RAM - 1) & ~7u;
        if (mask == 0 || (mask & SPU_VOICE_LSAX)) v->loop = arg->loop_addr & (SPU_RAM - 1) & ~7u;
        if (mask == 0 || (mask & SPU_VOICE_PITCH)) v->pitch = arg->pitch;
        if (mask == 0 || (mask & SPU_VOICE_VOLL)) v->volL = arg->volume.left;
        if (mask == 0 || (mask & SPU_VOICE_VOLR)) v->volR = arg->volume.right;
        if (mask == 0 || (mask & SPU_VOICE_ADSR_ADSR1)) v->adsr1 = arg->adsr1;
        if (mask == 0 || (mask & SPU_VOICE_ADSR_ADSR2)) v->adsr2 = arg->adsr2;
    }
}

void SpuSetKey(long on_off, unsigned long voice_bit)
{
    for (int i = 0; i < NUM_VOICES; i++) {
        if (voice_bit & (1u << i)) {
            if (on_off == SPU_ON) {
                KeyOn(&s_voices[i]);
            } else {
                KeyOff(&s_voices[i]);
            }
        }
    }
}

long SpuGetKeyStatus(unsigned long voice_bit)
{
    for (int i = 0; i < NUM_VOICES; i++) {
        if (voice_bit & (1u << i)) {
            Voice* v = &s_voices[i];
            int sounding = v->state != ENV_OFF && v->env > 0;
            if (v->keyOn) {
                /* (just keyed on, its attack not yet begun: on) */
                return sounding || v->state == ENV_ATTACK ? SPU_ON : SPU_ON_ENV_OFF;
            }
            return sounding ? SPU_OFF_ENV_ON : SPU_OFF;
        }
    }
    return SPU_OFF;
}

/* --- common attributes --- */

void SpuSetCommonAttr(SpuCommonAttr* attr)
{
    unsigned long mask = attr->mask;
    if (mask == 0 || (mask & SPU_COMMON_MVOLL)) s_common.mvolL = attr->mvol.left;
    if (mask == 0 || (mask & SPU_COMMON_MVOLR)) s_common.mvolR = attr->mvol.right;
    if (mask == 0 || (mask & SPU_COMMON_CDVOLL)) s_common.cdVolL = attr->cd.volume.left;
    if (mask == 0 || (mask & SPU_COMMON_CDVOLR)) s_common.cdVolR = attr->cd.volume.right;
    if (mask == 0 || (mask & SPU_COMMON_CDREV)) s_common.cdReverb = attr->cd.reverb;
    if (mask == 0 || (mask & SPU_COMMON_CDMIX)) s_common.cdMix = attr->cd.mix;
}

void SpuGetCommonAttr(SpuCommonAttr* attr)
{
    memset(attr, 0, sizeof(*attr));
    attr->mvol.left = s_common.mvolL;
    attr->mvol.right = s_common.mvolR;
    attr->mvolx = attr->mvol;
    attr->cd.volume.left = s_common.cdVolL;
    attr->cd.volume.right = s_common.cdVolR;
    attr->cd.reverb = s_common.cdReverb;
    attr->cd.mix = s_common.cdMix;
}

void SpuSetCommonMasterVolume(short mvol_left, short mvol_right)
{
    s_common.mvolL = mvol_left;
    s_common.mvolR = mvol_right;
}

void SpuSetCommonCDMix(long cd_mix)
{
    s_common.cdMix = cd_mix == SPU_ON;
}

void SpuSetCommonCDVolume(short cd_left, short cd_right)
{
    s_common.cdVolL = cd_left;
    s_common.cdVolR = cd_right;
}

/* --- reverb --- */

long SpuReserveReverbWorkArea(long on_off)
{
    s_reverbOn = on_off == SPU_ON;
    return on_off;
}

long SpuSetReverbModeType(long mode)
{
    if (mode >= 0 && mode < SPU_REV_MODE_MAX) {
        s_reverbStart = (uint32_t)_spu_rev_startaddr[mode] * 8;
    }
    return 0;
}

void SpuSetReverbModeDepth(short depth_left, short depth_right)
{
    s_depthL = depth_left;
    s_depthR = depth_right;
}

void SpuSetReverbModeDelayTime(long delay) {}
void SpuSetReverbModeFeedback(long feedback) {}

/* --- libsnd --- */

/*
 * The pitch of `note` (and `fine`, in 128ths of a semitone) for a sample
 * whose own note is `center` (and `shift`).
 */
unsigned short SsPitchFromNote(short note, short fine, unsigned char center, unsigned char shift)
{
    double semitones = (note - center) + (fine - shift) / 128.0;
    double pitch = 4096.0 * pow(2.0, semitones / 12.0);
    if (pitch > 0x3FFF) pitch = 0x3FFF;
    return (unsigned short)pitch;
}

} /* extern "C" */
