#include "common.h"

#include <libgte.h>

#include <deque>
#include <algorithm>

#include "Gameplay.h"

SpeedRange::SpeedRange() : m_min(), m_max(), m_points(0), m_speed()
{
}

SpeedRange::SpeedRange(Fixed min, Fixed max, int points, int scored)
    : m_min(min), m_max(max), m_points(points), m_speed(min), m_scored(scored)
{
}

void SpeedRange::Add(int points)
{
    if (m_points != 0) {
        m_scored += points;
        /* the square root of the share of the points scored (4.12) */
        int share = csqrt((m_scored << 12) / m_points);
        m_speed = m_min + Fixed::FromRaw((long long)(m_max - m_min).Raw() * share >> 12);
    }
}

void Score::Hit(int road, int times)
{
    int base = IsDouble(road) ? 3 : 1;
    int bonus = m_numPresses != 0 ? m_pressBonus : 0;
    int chain = m_chain;
    m_points += (base + bonus + chain) * times;
    m_lastBonus = bonus;
    m_lastBase = base;
    m_lastChain = chain;
    m_chain = min(chain + 1, 10);
    m_numPresses = 0;
}

int Score::Press(int road, int partRoad)
{
    int result = PRESS_NONE;
    if (m_numPresses < 2) {
        if (m_numPresses == 0) {
            m_pressBonus = 0;
        }
        int again = 0;
        int two = IsDouble(partRoad);
        for (int i = 0; i < m_numPresses; i++) {
            if (road == m_presses[i]) {
                again = 1;
            }
        }
        int match = 0;
        if (two) {
            if (road == FirstOfTwo(partRoad) || road == SecondOfTwo(partRoad)) {
                match = 1;
            }
        } else if (road == partRoad) {
            match = 1;
        }
        if (again || match) {
            result = PRESS_SAME;
        } else if (two) {
            result = PRESS_OTHER_DOUBLE;
        } else {
            result = PRESS_OTHER;
        }
        m_pressBonus += result;
        m_presses[m_numPresses++] = road;
    }
    return result;
}
