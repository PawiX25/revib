/*
 * The CD drive (libds, and libcd's position helpers). Commands complete at
 * once: their callbacks are called before DsCommand() returns, and a read
 * hands over all its sectors in turn to the ready callback.
 *
 * Two discs are at hand: the one in the drive (the page can swap it, as the
 * lid is opened and closed: the game's disc, or an audio CD made of music
 * files), which gives the table of contents and the CD audio, and the
 * game's own disc, from which the files are always read.
 */
#include <string.h>
#include <ctype.h>
#include <sys/types.h>

extern "C" {
#include <libds.h>
}

#include "psx.h"

#define SECTOR_SAMPLES 588 /* stereo samples of CD audio in a sector */

static int s_seek;           /* the sector of the last DslSetloc/DslSeekP */
static int s_playing;
static int s_muted;
static int s_position;       /* the head: a sector */
static int s_sample;         /* within the sector */
static int16_t s_audio[SECTOR_SAMPLES * 2];
static int s_audioSector = -1;

static int s_starts[101];    /* of the tracks, then the lead-out */
static int s_numTracks;
static int s_tocChanges = -1;

static DslRCB s_ready;
static uint8_t s_sector[2048];

static inline int Bcd(int i) { return i / 10 * 16 + i % 10; }
static inline int FromBcd(int b) { return b / 16 * 10 + b % 16; }

static void ToMsf(int lba, u_char* msf)
{
    int a = lba + 150;
    msf[0] = Bcd(a / (60 * 75));
    msf[1] = Bcd(a / 75 % 60);
    msf[2] = Bcd(a % 75);
}

static int FromMsf(const u_char* msf)
{
    return (FromBcd(msf[0]) * 60 + FromBcd(msf[1])) * 75 + FromBcd(msf[2]) - 150;
}

static void ReadToc(void)
{
    int changes = js_cd_changes();
    if (changes == s_tocChanges) {
        return;
    }
    s_tocChanges = changes;
    s_numTracks = js_cd_toc(s_starts, 100);
}

static int TrackOf(int lba)
{
    ReadToc();
    for (int t = s_numTracks; t >= 1; t--) {
        if (lba >= s_starts[t - 1]) {
            return t;
        }
    }
    return 1;
}

static int HasDisc(void)
{
    return js_cd_state() == 0;
}

static int ReadRaw(int lba, uint8_t* dest)
{
    int r = js_cd_read(lba, dest);
    if (r < 0) {
        js_cd_wait(lba);
        r = js_cd_read(lba, dest);
    }
    return r > 0;
}

static u_char Status(void)
{
    u_char status = 0x02; /* the motor turns */
    if (js_cd_state() == 1) {
        status = 0x10;
    }
    if (s_playing) {
        status |= DslStatPlay;
    }
    return status;
}

/* the sound of the CD playing, for the SPU */
extern "C" void cd_audio(int16_t* dest, int frames)
{
    if (!s_playing || !HasDisc()) {
        memset(dest, 0, frames * 4);
        return;
    }
    ReadToc();
    int end = s_numTracks > 0 ? s_starts[s_numTracks] : 0;
    for (int i = 0; i < frames; i++) {
        if (s_position >= end) {
            s_playing = 0;
            memset(dest + i * 2, 0, (frames - i) * 4);
            return;
        }
        if (s_audioSector != s_position) {
            if (!ReadRaw(s_position, (uint8_t*)s_audio)) {
                memset(s_audio, 0, sizeof(s_audio));
            }
            s_audioSector = s_position;
        }
        if (s_muted) {
            dest[i * 2] = dest[i * 2 + 1] = 0;
        } else {
            dest[i * 2] = s_audio[s_sample * 2];
            dest[i * 2 + 1] = s_audio[s_sample * 2 + 1];
        }
        if (++s_sample == SECTOR_SAMPLES) {
            s_sample = 0;
            s_position++;
        }
    }
}

/* the drive's status polled since the last vertical blank */
static int s_polls = 0;

extern "C" void cd_vsync(void)
{
    s_polls = 0;
    if (!HasDisc()) {
        s_playing = 0;
    }
}

/* --- ISO 9660, on the game's disc --- */

static uint32_t Le32(const uint8_t* p)
{
    return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* the entry `name` (without its ";1") of the directory at `lba` */
static int FindEntry(int lba, uint32_t size, const char* name, int* outLba, uint32_t* outSize)
{
    uint8_t sector[2048];
    for (uint32_t done = 0; done < size; done += 2048, lba++) {
        if (!js_data_read(lba, sector)) {
            return 0;
        }
        for (int off = 0; off < 2048;) {
            int len = sector[off];
            if (len == 0) {
                break;
            }
            int nameLen = sector[off + 32];
            char entry[64];
            int n = 0;
            for (int i = 0; i < nameLen && n < 63; i++) {
                char c = sector[off + 33 + i];
                if (c == ';') {
                    break;
                }
                entry[n++] = toupper(c);
            }
            entry[n] = 0;
            if (strcmp(entry, name) == 0) {
                *outLba = Le32(&sector[off + 2]);
                *outSize = Le32(&sector[off + 10]);
                return 1;
            }
            off += len;
        }
    }
    return 0;
}

/* "\DIR\NAME;1": its first sector and size */
static int FindFile(const char* path, int* lba, uint32_t* size)
{
    uint8_t pvd[2048];
    if (!js_data_read(16, pvd)) {
        return 0;
    }
    int dir = Le32(&pvd[156 + 2]);
    uint32_t dirSize = Le32(&pvd[156 + 10]);
    char part[64];
    const char* p = path;
    while (*p == '\\') {
        p++;
    }
    for (;;) {
        int n = 0;
        while (*p && *p != '\\' && *p != ';' && n < 63) {
            part[n++] = toupper(*p++);
        }
        part[n] = 0;
        if (*p == ';') {
            while (*p && *p != '\\') {
                p++;
            }
        }
        int entryLba;
        uint32_t entrySize;
        if (!FindEntry(dir, dirSize, part, &entryLba, &entrySize)) {
            return 0;
        }
        if (*p == '\\') {
            p++;
            dir = entryLba;
            dirSize = entrySize;
            continue;
        }
        *lba = entryLba;
        *size = entrySize;
        return 1;
    }
}

extern "C" {

int DsInit(void)
{
    s_playing = 0;
    s_muted = 0;
    s_ready = NULL;
    return 1;
}

void DsClose(void)
{
    s_playing = 0;
}

void DsFlush(void) {}

int DsSystemStatus(void)
{
    /*
     * Polled again and again within a frame: the game is waiting for the
     * drive in a loop of its own (the kiosk, for its track to start), and
     * time must pass, as it does on the console.
     */
    if (++s_polls > 2) {
        sys_idle();
    }
    return HasDisc() ? DslReady : DslNoCD;
}

u_char DsStatus(void)
{
    return Status();
}

int DsShellOpen(void)
{
    return js_cd_changes();
}

DslFILE* DsSearchFile(DslFILE* fp, char* name)
{
    int lba;
    uint32_t size;
    if (!FindFile(name, &lba, &size)) {
        return NULL;
    }
    ToMsf(lba, &fp->pos.minute);
    fp->pos.track = 0;
    fp->size = size;
    const char* base = strrchr(name, '\\');
    strncpy(fp->name, base ? base + 1 : name, sizeof(fp->name) - 1);
    fp->name[sizeof(fp->name) - 1] = 0;
    return fp;
}

int DsGetSector(void* madr, int size)
{
    if (size > 512) {
        size = 512;
    }
    memcpy(madr, s_sector, size * 4);
    return 1;
}

static int s_readSector;
static int s_readActive;

int DsStartReadySystem(DslRCB func, int count)
{
    s_ready = func;
    s_readActive = 1;
    u_char result[8] = { Status(), 0, 0, 0, 0, 0, 0, 0 };
    /* the sectors one after the other, until the callback has had enough */
    int guard = 0;
    while (s_readActive && s_ready != NULL && guard++ < 100000) {
        if (!js_data_read(s_readSector, s_sector)) {
            DslRCB f = s_ready;
            s_readActive = 0;
            f(DslDiskError, result, NULL);
            break;
        }
        s_readSector++;
        s_ready(DslDataReady, result, (u_long*)s_sector);
    }
    return 1;
}

void DsEndReadySystem(void)
{
    s_readActive = 0;
    s_ready = NULL;
}

int DsCommand(u_char com, u_char* param, DslCB cbsync, int count)
{
    u_char result[8];
    memset(result, 0, sizeof(result));
    u_char intr = DslComplete;
    ReadToc();
    switch (com) {
    case DslSetloc:
        if (param) {
            s_seek = FromMsf(param);
        }
        break;
    case DslSeekP:
    case DslSeekL:
        if (param) {
            s_seek = FromMsf(param);
        }
        s_playing = 0;
        s_position = s_seek;
        s_sample = 0;
        break;
    case DslPlay:
        if (param) {
            s_seek = FromMsf(param);
            s_position = s_seek;
            s_sample = 0;
        }
        s_playing = HasDisc();
        break;
    case DslPause:
    case DslStop:
        s_playing = 0;
        break;
    case DslMute:
        s_muted = 1;
        break;
    case DslDemute:
        s_muted = 0;
        break;
    case DslReadN:
    case DslReadS: {
        if (param) {
            s_seek = FromMsf(param);
        }
        s_playing = 0;
        s_readSector = s_seek;
        if (com == DslReadN) {
            /* the game's files: always from its own disc */
            break;
        }
        /* DslReadS, CdSys's test of the disc in the drive: a data track first */
        int data = HasDisc() && s_numTracks > 0 && (s_numTracks == 1 || s_seek < s_starts[1]);
        uint8_t raw[2352];
        if (data && ReadRaw(s_seek, raw)) {
            /* an audio sector has no sync pattern */
            static const uint8_t sync[12] = { 0, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0 };
            data = memcmp(raw, sync, 12) == 0;
        }
        if (!data) {
            intr = DslDiskError;
        }
        break;
    }
    case DslGetlocP: {
        int track = TrackOf(s_position);
        result[0] = Bcd(track);
        result[1] = 1;
        ToMsf(s_position - s_starts[track - 1] - 150, &result[2]);
        ToMsf(s_position, &result[5]);
        break;
    }
    case DslGetTN:
        result[0] = Status();
        result[1] = Bcd(1);
        result[2] = Bcd(s_numTracks);
        break;
    case DslGetTD: {
        int track = param ? FromBcd(param[0]) : 0;
        int lba = track == 0 || track > s_numTracks ? s_starts[s_numTracks] : s_starts[track - 1];
        u_char msf[3];
        ToMsf(lba, msf);
        result[0] = Status();
        result[1] = msf[0];
        result[2] = msf[1];
        break;
    }
    default:
        break;
    }
    if (!HasDisc() && com != DslNop && com != DslSetmode && com != DslMute && com != DslDemute && com != DslReadN) {
        intr = DslDiskError;
    }
    if (result[0] == 0) {
        result[0] = Status();
    }
    if (cbsync != NULL) {
        cbsync(intr, result);
    }
    return 1;
}

/* --- libcd's positions --- */

typedef struct {
    u_char minute, second, sector, track;
} CdlLOC;

CdlLOC* CdIntToPos(int i, CdlLOC* p)
{
    ToMsf(i, &p->minute);
    return p;
}

int CdPosToInt(CdlLOC* p)
{
    return FromMsf(&p->minute);
}

} /* extern "C" */
