#ifndef STATICQUEUE_H
#define STATICQUEUE_H

#include "common.h"

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

/*
 * A queue of a power of two items, in a ring: one side puts them in, the
 * other takes them out (the two sides keeping out of each other's way, as
 * an interrupt and the game do, with critical sections).
 */
template <class T>
class StaticQueue {
public:
    StaticQueue(int size) : m_mask(size - 1), m_items(new T[size]), m_write(0), m_read(0)
    {
        int powerOfTwo = 0;
        for (int i = 0; i < 32 && !powerOfTwo; i++) {
            if (1 << i == Size()) {
                powerOfTwo = 1;
            }
        }
#line 32 "C:/psj/dev/locale/game/StaticQueue.h"
        assert(powerOfTwo);
#line 29 "src/game/StaticQueue.h"
    }

    ~StaticQueue() { delete[] m_items; }

    u_int Size() const { return m_mask + 1; }
    u_int Count() const { return (m_write - m_read) & m_mask; }
    /* one item is always free, so the read and write ends meet only when it is empty */
    u_int Capacity() const { return m_mask; }

    void Put(const T& item)
    {
#line 42 "C:/psj/dev/locale/game/StaticQueue.h"
        assert(Count() < Capacity());
#line 43 "src/game/StaticQueue.h"
        m_items[m_write] = item;
        m_write = (m_write + 1) & m_mask;
    }

    /* the oldest item */
    T& Front() { return m_items[m_read]; }

    void Pop()
    {
#line 54 "C:/psj/dev/locale/game/StaticQueue.h"
        assert(m_write != m_read);
#line 55 "src/game/StaticQueue.h"
        m_read = (m_read + 1) & m_mask;
    }

private:
    u_int m_mask;
    T* m_items;
    u_int m_write;
    u_int m_read;
};

#endif
