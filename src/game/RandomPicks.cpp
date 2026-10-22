#include "common.h"

#include <stdio.h>
#include <stdlib.h>

#include "MemorySys.h"

#include <string>

#include "Format.h"
#include "RandomPicks.h"
#include "Random.h"

void RandomPicks(int lo, int hi, int count, u_long seed, int* picks)
{
    for (int i = 0; i < 7; i++) {
        seed = NextRandom(seed);
    }
    int range = hi - lo + 1;
    int used[min(count, range)]; /* the ones picked so far in this round, smallest first */
    for (int done = 0; done < count;) {
        int first = done;
        done += range;
        if (count < done) {
            done = count;
        }
        int top = hi;
        for (int i = first; i < done; i++) {
            seed = NextRandom(seed);
            /* one of the numbers left, at random: counted from `lo`, stepping over the ones used */
            int pick = lo + (int)((top - lo + 1) * (unsigned long long)seed >> 32);
            int numUsed = i - first;
            for (int j = 0; j < numUsed; j++) {
                if (pick < used[j]) {
                    break;
                }
                pick++;
            }
            picks[i] = pick;
            int at;
            for (at = 0; at < numUsed; at++) {
                if (used[at] >= pick) {
                    break;
                }
            }
            for (int k = numUsed; at < k; k--) {
                used[k] = used[k - 1];
            }
            used[at] = pick;
            top--;
        }
    }
}
