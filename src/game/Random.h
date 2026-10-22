#ifndef RANDOM_H
#define RANDOM_H

#include "common.h"

/* one step of Random's generator (below), from `seed` */
static inline u_long NextRandom(u_long seed)
{
    return seed * 1664525 + 1013904223;
}

/* A linear congruential generator (the constants of Numerical Recipes). */
class Random {
public:
    Random() : m_seed(0) {}

    u_long Next()
    {
        m_seed = m_seed * 1664525 + 1013904223;
        return m_seed;
    }

    /* a number from 0 to `n` - 1 */
    int Below(int n) { return ((unsigned long long)Next() * n) >> 32; }

    /* the seed itself, the next number being the one after it */
    void Set(u_long seed) { m_seed = seed; }

    /* starts again from `seed` */
    void Seed(u_long seed) { m_seed = seed * 1664525 + 1013904223; }

private:
    u_long m_seed;
};

#endif
