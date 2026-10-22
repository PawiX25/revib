#include "common.h"

#include <libapi.h>
#include <libetc.h>
#include <libspu.h>
#include <libds.h>
#include <libcd.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "CdSys.h"

static inline int BcdToInt(int bcd) { return bcd / 16 * 10 + bcd % 16; }
static inline int IntToBcd(int i) { return i / 10 * 16 + i % 10; }
/* (a function of its own: `!open` written in place compiles differently) */
static inline bool IsClosed(bool open) { return !open; }

/* Receivers can only be connected one at a time. */
static inline void CheckNotConnected(volatile const void* receiver)
{
#line 511 "C:/psj/dev/locale/game/CdSys.cpp"
    assert(receiver == NULL);
#line 25 "src/game/CdSys.cpp"
}

/* `receiver` is the one connected. */
static inline void CheckConnected(volatile const void* receiver, volatile const void* connected)
{
#line 520 "C:/psj/dev/locale/game/CdSys.cpp"
    assert(receiver == connected);
#line 33 "src/game/CdSys.cpp"
}

TrackInfo CdSys::s_tracks[100 + 1];
DiscId CdSys::s_disc;

int CdSys::s_discState;
int CdSys::s_tocTrack;
int CdSys::s_playTrack;
int CdSys::s_playState;
DiscId* CdSys::s_homeDisc;
volatile int CdSys::s_currentTrack;
int CdSys::s_shellWasOpen;
int CdSys::s_requestedTrack;
volatile int* CdSys::s_noDiscReceiver;
volatile int* CdSys::s_shellCloseReceiver;
volatile int* CdSys::s_shellOpenReceiver;
DiscInfo* CdSys::s_discInfoReceiver;
volatile int* CdSys::s_trackEndReceiver;
TrackStartInfo* CdSys::s_trackStartReceiver;
int CdSys::s_shellCount;
int CdSys::s_isHomeDisc;
volatile int CdSys::s_tocValid;
volatile int CdSys::s_firstTrackIsData;
volatile int CdSys::s_lastTrack;
u_char CdSys::s_seekMode;

/* DslGetlocP: follows the head while a track plays. */
void CdSys::GetlocCallback(u_char status, u_char* result)
{
    if (status != DslComplete) {
        return;
    }

    s_currentTrack = BcdToInt(result[0]);

    CdlLOC pos;
    pos.minute = result[5];
    pos.second = result[6];
    pos.sector = result[7];
    int sector = CdPosToInt(&pos);

    switch (s_playState) {
    case PLAY_IDLE:
        break;

    case PLAY_STARTING:
        if (sector >= Track(s_playTrack)->start) {
            s_playState = PLAY_PLAYING;
            DsCommand(DslDemute, NULL, NULL, 0);
            if (s_trackStartReceiver != NULL) {
                s_trackStartReceiver->track = s_playTrack - (s_firstTrackIsData != 0);
                s_trackStartReceiver->lengthSeconds = Track(s_playTrack)->Length() / SECTORS_PER_SECOND;
                s_trackStartReceiver->ready = 1;
            }
        }
        break;

    case PLAY_PLAYING:
        /* within 5 sectors of the next track (or the lead-out): the track has ended */
        if (sector >= s_tracks[s_playTrack].start - 5) {
            s_playState = PLAY_IDLE;
            DsCommand(DslMute, NULL, NULL, 0);
            if (s_trackEndReceiver != NULL) {
                *s_trackEndReceiver = 1;
            }
        }
        break;
    }
}

/* DslGetTN: number of the last track. */
void CdSys::GetTNCallback(u_char status, u_char* result)
{
    if (status == DslComplete) {
        s_lastTrack = BcdToInt(result[2]);
    }
}

/* DslReadS on track 1: succeeds only if the first track is a data track. */
void CdSys::ReadSCallback(u_char status, u_char* result)
{
    static CdlLOC pos;

    s_firstTrackIsData = status == DslComplete;
    if (status != DslComplete) {
        /* an audio CD: start playing it */
        CdIntToPos(s_tracks[0].start, &pos);
        DsCommand(DslPlay, (u_char*)&pos, NULL, 0);
    }
}

/*
 * CD audio is played by the kiosk and the game: in the title these are
 * inline functions nothing calls (only PlayTrack()'s message and static
 * position remain).
 */
#ifndef TITLE
#define CD_AUDIO
#else
#define CD_AUDIO inline
#endif
/* and only the game asks for the track under the head and the first audio track */
#ifdef GAME
#define CD_INFO
#else
#define CD_INFO inline
#endif

/* DslSeekP issued by PlayTrack(): play muted until the track's first sector is reached. */
CD_AUDIO void CdSys::SeekCallback(u_char status, u_char* result)
{
    if (status == DslComplete) {
        s_playState = PLAY_STARTING;
        s_playTrack = s_requestedTrack;
        DsCommand(DslMute, NULL, NULL, 0);
        DsCommand(DslPlay, NULL, NULL, 0);
    }
}

/* DslGetTD: start of one track; reads the following ones, then the lead-out. */
void CdSys::GetTDCallback(u_char status, u_char* result)
{
    static u_char param;
    static CdlLOC pos;

    if (status != DslComplete) {
        return;
    }

    CdlLOC loc;
    loc.minute = result[1];
    loc.second = result[2];
    loc.sector = 0;
    int start = CdPosToInt(&loc);

    int track = s_tocTrack;
    if (track <= s_lastTrack) {
        s_tracks[track - 1] = TrackInfo(start, &s_tracks[track]);
        s_tocTrack = track + 1;
        /* track 0 asks for the lead-out */
        param = IntToBcd(s_tocTrack <= s_lastTrack ? track + 1 : 0);
        DsCommand(DslGetTD, &param, GetTDCallback, 0);
    } else {
        s_tracks[track - 1] = TrackInfo(start, NULL);
        s_tocValid = 1;
        CdIntToPos(s_tracks[0].start, &pos);
        DsCommand(DslReadS, (u_char*)&pos, ReadSCallback, 0);
    }
}

void CdSys::ResetToc()
{
    s_lastTrack = s_firstTrackIsData = -1;
    s_tocValid = 0;
}

int CdSys::IsTocValid()
{
    return s_lastTrack >= 0 && s_firstTrackIsData >= 0 && s_tocValid != 0;
}

void CdSys::StartReadToc()
{
    ResetToc();
    DsCommand(DslGetTN, NULL, GetTNCallback, 0);
    s_tocTrack = 1;
    static u_char param = IntToBcd(1);
    DsCommand(DslGetTD, &param, GetTDCallback, 0);
}

void CdSys::Init(DiscId* homeDisc)
{
    s_homeDisc = homeDisc;
    ResetToc();
    s_currentTrack = 0;
    s_trackStartReceiver = NULL;
    s_trackEndReceiver = NULL;
    s_discInfoReceiver = NULL;
    s_shellOpenReceiver = NULL;
    s_shellCloseReceiver = NULL;
    s_noDiscReceiver = NULL;
    s_discState = DISC_UNKNOWN;
    s_shellCount = -1;
    s_isHomeDisc = 0;
    s_playState = PLAY_IDLE;

    /* DsInit() resets the SPU's CD input settings; put them back. */
    SpuCommonAttr attr;
    SpuGetCommonAttr(&attr);
    DsInit();
    attr.mask = 0;
    SpuSetCommonAttr(&attr);

    VSync(4);
    int shellOpen = (DsStatus() >> 4) & 1; /* DslStatShellOpen: the lid is (or was) open */
    if (s_homeDisc->lastTrack <= 0 && shellOpen) {
        /* started with the lid open: there is no game disc to come back to */
        *s_homeDisc = DiscId(1, 0);
        printf("setting null home disk\n");
    }
    s_shellWasOpen = shellOpen;
}

void CdSys::Quit()
{
    DsFlush();
    DsClose();
}

/* Plays an audio track (numbered without the data track). */
CD_AUDIO void CdSys::PlayTrack(int track)
{
    if (s_discState != DISC_IDENTIFIED) {
        return;
    }
    if (track <= 0 || GetNumTracks() < track) {
        printf("CdSys::PlayTrack(): requested track out of range\n");
        return;
    }
    static CdlLOC pos;
    s_requestedTrack = track + (s_firstTrackIsData != 0);
    /* seek to 5 sectors before the track: from there it is played muted until its first sector */
    CdIntToPos(Track(s_requestedTrack)->start - 5, &pos);
    DsCommand(DslSetmode, &s_seekMode, NULL, 0);
    DsCommand(DslSeekP, (u_char*)&pos, SeekCallback, 0);
}

CD_AUDIO int CdSys::GetNumTracks()
{
    return NumAudioTracks();
}

CD_INFO int CdSys::GetCurrentTrack()
{
    return s_currentTrack - (s_firstTrackIsData != 0);
}

CD_INFO TrackInfo* CdSys::GetFirstAudioTrack()
{
    return Track(NumDataTracks() + 1);
}

void CdSys::Update()
{
    int shellOpen = (DsStatus() >> 4) & 1; /* DslStatShellOpen */
    bool isOpen = shellOpen != 0;
    /* the lid has been opened or closed since the last update */
    if (s_shellWasOpen ? IsClosed(isOpen) : isOpen) {
        volatile int* receiver = shellOpen ? s_shellOpenReceiver : s_shellCloseReceiver;
        if (receiver != NULL) {
            *receiver = 1;
        }
    }
    s_shellWasOpen = shellOpen;

    switch (DsSystemStatus()) {
    case DslReady: {
        int count = DsShellOpen();
        if (count != s_shellCount) {
            /* a disc has been inserted */
            s_shellCount = count;
            s_discState = DISC_READING_TOC;
            s_playState = PLAY_IDLE;
            StartReadToc();
        } else {
            DsCommand(DslGetlocP, NULL, GetlocCallback, 0);
        }
        break;
    }
    case DslNoCD: {
        int count = DsShellOpen();
        if (count != s_shellCount) {
            s_shellCount = count;
            s_discState = DISC_NONE;
            if (s_noDiscReceiver != NULL) {
                *s_noDiscReceiver = 1;
            }
        }
        break;
    }
    }

    if (s_discState != DISC_READING_TOC || !IsTocValid()) {
        return;
    }

    s_disc = DiscId(s_lastTrack, s_tracks[s_lastTrack].start);
    if (s_homeDisc->lastTrack <= 0) {
        /* the first disc seen is the game disc */
        *s_homeDisc = s_disc;
    }
    s_isHomeDisc = *s_homeDisc == s_disc;
    s_discState = DISC_IDENTIFIED;

    if (s_discInfoReceiver != NULL) {
        s_discInfoReceiver->numTracks = NumAudioTracks();
        s_discInfoReceiver->lengthSeconds =
            (GetLeadOut()->start - Track(NumDataTracks() + 1)->start) / SECTORS_PER_SECOND;
        s_discInfoReceiver->isHomeDisc = s_isHomeDisc;
        s_discInfoReceiver->id = (s_disc.leadOut << 7) | s_disc.lastTrack;
        s_discInfoReceiver->ready = 1;
    }
}

#ifndef TITLE
void CdSys::SetTrackStartReceiver(TrackStartInfo* info)
{
    SwEnterCriticalSection();
    CheckNotConnected(s_trackStartReceiver);
    s_trackStartReceiver = info;
    SwExitCriticalSection();
}
#endif

#ifdef GAME
void CdSys::ClearTrackStartReceiver(TrackStartInfo* info)
{
    SwEnterCriticalSection();
    CheckConnected(info, s_trackStartReceiver);
    s_trackStartReceiver = NULL;
    SwExitCriticalSection();
}

void CdSys::SetTrackEndReceiver(volatile int* flag)
{
    SwEnterCriticalSection();
    CheckNotConnected(s_trackEndReceiver);
    s_trackEndReceiver = flag;
    SwExitCriticalSection();
}

void CdSys::ClearTrackEndReceiver(volatile int* flag)
{
    SwEnterCriticalSection();
    CheckConnected(flag, s_trackEndReceiver);
    s_trackEndReceiver = NULL;
    SwExitCriticalSection();
}
#endif

void CdSys::SetDiscInfoReceiver(DiscInfo* info)
{
    SwEnterCriticalSection();
    CheckNotConnected(s_discInfoReceiver);
    s_discInfoReceiver = info;
    SwExitCriticalSection();
}

#ifdef GAME
void CdSys::ClearDiscInfoReceiver(DiscInfo* info)
{
    SwEnterCriticalSection();
    CheckConnected(info, s_discInfoReceiver);
    s_discInfoReceiver = NULL;
    SwExitCriticalSection();
}
#endif

void CdSys::SetShellOpenReceiver(volatile int* flag)
{
    SwEnterCriticalSection();
    CheckNotConnected(s_shellOpenReceiver);
    s_shellOpenReceiver = flag;
    SwExitCriticalSection();
}

#ifdef GAME
void CdSys::ClearShellOpenReceiver(volatile int* flag)
{
    SwEnterCriticalSection();
    CheckConnected(flag, s_shellOpenReceiver);
    s_shellOpenReceiver = NULL;
    SwExitCriticalSection();
}

void CdSys::SetShellCloseReceiver(volatile int* flag)
{
    SwEnterCriticalSection();
    CheckNotConnected(s_shellCloseReceiver);
    s_shellCloseReceiver = flag;
    SwExitCriticalSection();
}

void CdSys::ClearShellCloseReceiver(volatile int* flag)
{
    SwEnterCriticalSection();
    CheckConnected(flag, s_shellCloseReceiver);
    s_shellCloseReceiver = NULL;
    SwExitCriticalSection();
}

void CdSys::SetNoDiscReceiver(volatile int* flag)
{
    SwEnterCriticalSection();
    CheckNotConnected(s_noDiscReceiver);
    s_noDiscReceiver = flag;
    SwExitCriticalSection();
}

void CdSys::ClearNoDiscReceiver(volatile int* flag)
{
    SwEnterCriticalSection();
    CheckConnected(flag, s_noDiscReceiver);
    s_noDiscReceiver = NULL;
    SwExitCriticalSection();
}
#endif
