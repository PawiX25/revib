#ifndef TRACKDISPLAY_H
#define TRACKDISPLAY_H

#include "common.h"

#include <stdio.h>

#include "MemorySys.h"
#include <string>

#include "CdSys.h"

/*
 * The CD's track and the number of tracks, as text. A track asked for would
 * be shown in brackets until it plays, but nothing asks for one: m_asked
 * stays 0, and m_askedTrack is never set.
 */
class TrackDisplay {
public:
    TrackDisplay() : m_asked(0) {}

    string Text()
    {
        Update();
        char text[40];
        if (!m_asked) {
            sprintf(text, "track =  %2d /%2d", m_track, m_numTracks);
        } else {
            sprintf(text, "track = (%2d)/%2d", m_askedTrack, m_numTracks);
        }
        return text;
    }

private:
    void Update()
    {
        m_numTracks = CdSys::GetNumTracks();
        m_track = CdSys::GetCurrentTrack();
        if (m_asked && m_askedTrack == m_track) {
            m_asked = 0;
        }
    }

    int m_asked;
    int m_askedTrack;
    int m_numTracks;
    int m_track;
};

#endif
