#ifndef CDSYS_H
#define CDSYS_H

#include "common.h"

#include <libcd.h>

/*
 * CD drive handling.
 *
 * Vib-Ribbon builds its courses from music on any audio CD the player inserts,
 * so the game is constantly watching the drive: it notices the lid being
 * opened and closed, reads the table of contents (TOC) of whatever disc is in
 * the drive, tells the game disc (the "home" disc) apart from music CDs, and
 * plays individual CD-DA tracks.
 *
 * Results are delivered by writing into memory the game hands over in
 * advance ("receivers"): a flag set to 1, or a record filled in and then
 * marked ready. Only one receiver of each kind can be connected at a time.
 */

/* Identifies a disc by its TOC: number of the last track and the start of the lead-out. */
struct DiscId {
    int lastTrack;
    int leadOut;

    DiscId() : lastTrack(0) {}
    DiscId(int lastTrack, int leadOut) : lastTrack(lastTrack), leadOut(leadOut) {}

    int operator==(const DiscId& other) const {
        return lastTrack == other.lastTrack && leadOut == other.leadOut;
    }
};

/* the CD plays this many sectors a second */
#define SECTORS_PER_SECOND 75

/* One TOC entry: start sector of a track, and the entry that follows it (NULL after the lead-out). */
struct TrackInfo {
    int start;
    TrackInfo* next;

    TrackInfo() : start(0), next(0) {}
    TrackInfo(int start, TrackInfo* next) : start(start), next(next) {}

    /* in sectors; 0 for the lead-out */
    int Length() const { return next != NULL ? next->start - start : 0; }
};

/* Something CdSys fills in for whoever asked; `ready` is set once it has. */
struct CdReport {
    int ready;

    CdReport() : ready(0) {}
};

/* Filled in when a requested track actually starts playing. */
struct TrackStartInfo : CdReport {
    int track;
    int lengthSeconds;
};

/* Filled in once a newly inserted disc has been identified. */
struct DiscInfo : CdReport {
    int numTracks;     /* audio tracks */
    int lengthSeconds; /* total length of the audio tracks */
    int isHomeDisc;
    int id;            /* (lead-out << 7) | last track */
};

/* Set when the drive's lid opens (SetShellOpenReceiver()). */
struct ShellOpen : CdReport {};

class CdSys {
public:
    static void Init(DiscId* homeDisc);
    static void Quit();
    static void Update();

    static void PlayTrack(int track);
    static int  GetNumTracks();
    static int  GetCurrentTrack();
    static TrackInfo* GetFirstAudioTrack();

    /*
     * Receivers of what happens. The title only sets the disc info and
     * lid-open ones, the kiosk the track start one too; only the game clears
     * them.
     */
    static void SetTrackStartReceiver(TrackStartInfo* info);
    static void ClearTrackStartReceiver(TrackStartInfo* info);
    static void SetTrackEndReceiver(volatile int* flag);
    static void ClearTrackEndReceiver(volatile int* flag);
    static void SetDiscInfoReceiver(DiscInfo* info);
    static void ClearDiscInfoReceiver(DiscInfo* info);
    static void SetShellOpenReceiver(volatile int* flag);
    static void ClearShellOpenReceiver(volatile int* flag);
    static void SetShellCloseReceiver(volatile int* flag);
    static void ClearShellCloseReceiver(volatile int* flag);
    static void SetNoDiscReceiver(volatile int* flag);
    static void ClearNoDiscReceiver(volatile int* flag);

private:
    enum DiscState {
        DISC_UNKNOWN,
        DISC_NONE,
        DISC_READING_TOC,
        DISC_IDENTIFIED
    };
    enum PlayState {
        PLAY_IDLE,
        PLAY_STARTING, /* the seek done, playing muted until the track's first sector is reached */
        PLAY_PLAYING
    };

    static void GetlocCallback(u_char status, u_char* result);
    static void GetTNCallback(u_char status, u_char* result);
    static void ReadSCallback(u_char status, u_char* result);
    static void SeekCallback(u_char status, u_char* result);
    static void GetTDCallback(u_char status, u_char* result);
    static void ResetToc();
    static int  IsTocValid();
    static void StartReadToc();

    /* TOC entry of a track; tracks are numbered from 1 */
    static TrackInfo* Track(int number) { return s_tracks + number - 1; }
    /* a PlayStation disc starts with a data track; audio tracks are numbered after it */
    static int NumDataTracks() { return s_firstTrackIsData != 0; }
    static int NumAudioTracks() { return s_lastTrack - (s_firstTrackIsData != 0); }
    static TrackInfo* GetLeadOut() { return &s_tracks[s_lastTrack]; }

    static TrackInfo s_tracks[100 + 1]; /* tracks 1-99 and the lead-out, indexed by track - 1 */
    static DiscId s_disc;               /* the disc in the drive */

    /* The compiler lays these out in reverse order of declaration (.sdata). */

    static int s_discState;
    static int s_tocTrack;                  /* track whose TOC entry is being read */
    static int s_playTrack;
    static int s_playState;
    static DiscId* s_homeDisc;
    static volatile int s_currentTrack;     /* track under the head, from DslGetlocP */
    static int s_shellWasOpen;
    static int s_requestedTrack;
    static volatile int* s_noDiscReceiver;
    static volatile int* s_shellCloseReceiver;
    static volatile int* s_shellOpenReceiver;
    static DiscInfo* s_discInfoReceiver;
    static volatile int* s_trackEndReceiver;
    static TrackStartInfo* s_trackStartReceiver;
    static int s_shellCount;                /* DsShellOpen() at the last disc change */
    static int s_isHomeDisc;
    static volatile int s_tocValid;
    static volatile int s_firstTrackIsData; /* -1 until known; set when track 1 is readable as data */
    static volatile int s_lastTrack;        /* -1 until the TOC header has been read */
    static u_char s_seekMode;               /* DslSetmode before seeking to a track */
};

#endif
