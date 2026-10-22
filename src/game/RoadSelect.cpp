#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "Format.h"
#include "RoadSelect.h"
#include "Random.h"

/* from 0 to n - 1, from a step of the generator */
static inline int RandomBelow(u_long seed, int n)
{
    unsigned long long x = seed;
    return x * n >> 32;
}

RoadSequence::RoadSequence(const RoadList* list)
{
    m_list = list;
    m_index = 0;
}

int RoadSequence::Next()
{
    int road = m_list->roads[m_index++];
    if (m_index >= m_list->count) {
        m_index = 0;
    }
    return road;
}

RoadDraw::RoadDraw(const RoadOdds* odds, int round, int seed)
{
    m_odds = odds;
    m_seed = odds->seed * (100 * seed + round);
}

int RoadDraw::Next()
{
    const RoadOdds* odds = m_odds;
    m_seed = NextRandom(m_seed);
    int pick = (int)(odds->total * (unsigned long long)m_seed >> 32) + 1;
    int road;
    for (road = 0; road < 10; road++) {
        if (odds->below[road] >= pick) {
            return road;
        }
    }
#line 40 "C:/psj/dev/locale/game/RoadSelect.cpp"
    assert(road < 10);
#line 53 "src/game/RoadSelect.cpp"
}

RoadTempo::RoadTempo(int start, u_long length, int pattern, u_long seed)
{
    m_turn = 0;
    m_start = start;
    m_length = length;
    m_pattern = pattern;
    m_seed = seed;
}

int RoadTempo::IsOver(int time)
{
    return (u_long)(time - m_start) > m_length;
}

Fixed RoadTempo::Next(int time)
{
    if (IsOver(time)) {
        return 1.0;
    }
    switch (m_pattern) {
    case PATTERN_TWO_AT_RANDOM: {
        m_seed = NextRandom(m_seed);
        int r = RandomBelow(m_seed, 10);
        if (r < 5) {
            return 1.25;
        }
        return 0.5;
    }
    case PATTERN_THREE_AT_RANDOM: {
        m_seed = NextRandom(m_seed);
        int r = RandomBelow(m_seed, 10);
        if (r < 3) {
            return 1.25;
        }
        if (r >= 7) {
            return 0.5;
        }
        return 1 / 1.5;
    }
    case PATTERN_TWO_IN_TURN:
        if (++m_turn >= 2) {
            m_turn = 0;
        }
        if (m_turn == 0) {
            return 1.25;
        }
        return 0.5;
    default:
        if (++m_turn >= 3) {
            m_turn = 0;
        }
        if (m_turn == 0) {
            return 1.25;
        }
        if (m_turn == 1) {
            return 1 / 1.5;
        }
        return 0.5;
    }
}
