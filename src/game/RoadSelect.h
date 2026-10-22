#ifndef ROADSELECT_H
#define ROADSELECT_H

#include "common.h"

#include "Fixed.h"
#include "FastAllocator.h"

/* Held by reference count. */
class Shared {
public:
    Shared() : m_refs(1) {}
    virtual ~Shared() {}

    void AddRef() { m_refs++; }
    /* gives up a reference, deleting it with the last one */
    void Release()
    {
        if (m_refs == 1) {
            delete this;
        } else {
            m_refs--;
        }
    }

    int m_refs;
};

/* Chooses the kind of road that comes next. */
class RoadSelector : public Shared {
public:
    void* operator new(size_t size) { return FastAllocator::Allocate(size); }
    void operator delete(void* p, size_t size) { FastAllocator::Free(p, size); }

    virtual ~RoadSelector() {}
    virtual int Next() = 0;

    int m_road;
};

/* A list of roads: its count, then the roads. */
struct RoadList {
    int count;
    int roads[1];
};

/* The roads of a list, one after the other, round and round. */
class RoadSequence : public RoadSelector {
public:
    RoadSequence(const RoadList* list);

    int Next();

private:
    int m_index;
    const RoadList* m_list;
};

/* How often each road comes, when they come at random. */
struct RoadOdds {
    int unused;
    int seed;
    int total;
    int below[10]; /* total of the odds of the roads up to this one */
};

/* Roads at random, each as often as its odds say. */
class RoadDraw : public RoadSelector {
public:
    /* the random numbers start from the odds' seed, the round and the script's seed */
    RoadDraw(const RoadOdds* odds, int round, int seed);

    int Next();

private:
    const RoadOdds* m_odds;
    u_long m_seed;
};

/*
 * How long the parts are, for a while: longer and shorter ones (their
 * lengths scaled by 1.25, 1 / 1.5 or 0.5) in turns, or at random.
 */
class RoadTempo : public Shared {
public:
    /* the patterns; any other one is 1 / 1.5, 0.5 and 1.25 in turn */
    enum Pattern {
        PATTERN_TWO_AT_RANDOM = 2,   /* 1.25 or 0.5, as likely */
        PATTERN_THREE_AT_RANDOM = 3, /* 1.25, 1 / 1.5 or 0.5: 3, 4 and 3 times in 10 */
        PATTERN_TWO_IN_TURN = 22     /* 0.5 and 1.25 in turn */
    };

    RoadTempo(int start, u_long length, int pattern, u_long seed);

    void* operator new(size_t size) { return FastAllocator::Allocate(size); }
    void operator delete(void* p, size_t size) { FastAllocator::Free(p, size); }

    /* the time `time` is past the end */
    int IsOver(int time);
    /* what the next part's length is scaled by (1 once over) */
    Fixed Next(int time);

private:
    int m_unused;
    u_long m_seed;
    int m_turn;
    int m_start;
    u_long m_length;
    int m_pattern;
};

#endif
