#ifndef FIXED_H
#define FIXED_H

#include "common.h"

/* A 16.16 fixed-point number. */
class Fixed {
public:
    Fixed() : m_value(0) {}
    Fixed(float value) : m_value((int)(value * 65536.0f)) {}
    Fixed(double value) : m_value((int)(value * 65536.0)) {}

    static Fixed FromRaw(int raw)
    {
        Fixed value;
        value.m_value = raw;
        return value;
    }

    /* a whole number */
    static Fixed Int(int n)
    {
        Fixed value;
        value.m_value = n << 16;
        return value;
    }

    Fixed operator+(Fixed other) const
    {
        Fixed sum;
        sum.m_value = m_value + other.m_value;
        return sum;
    }

    Fixed operator-() const
    {
        Fixed negated;
        negated.m_value = -m_value;
        return negated;
    }

    Fixed operator-(Fixed other) const
    {
        Fixed difference;
        difference.m_value = m_value - other.m_value;
        return difference;
    }

    Fixed& operator+=(Fixed other)
    {
        m_value += other.m_value;
        return *this;
    }

    Fixed& operator-=(Fixed other)
    {
        m_value -= other.m_value;
        return *this;
    }

    int operator<(Fixed other) const { return m_value < other.m_value; }
    int operator>=(Fixed other) const { return m_value >= other.m_value; }
    int operator==(Fixed other) const { return m_value == other.m_value; }

    /* the largest value below this one */
    Fixed Previous() const
    {
        Fixed value;
        value.m_value = m_value - 1;
        return value;
    }

    Fixed operator*(int n) const
    {
        Fixed product;
        product.m_value = m_value * n;
        return product;
    }

    Fixed operator*(Fixed other) const
    {
        Fixed product;
        product.m_value = ((long long)m_value * other.m_value) >> 16;
        return product;
    }

    Fixed operator/(int n) const
    {
        Fixed quotient;
        quotient.m_value = m_value / n;
        return quotient;
    }

    /* 8 bits of precision kept on each side */
    Fixed operator/(Fixed other) const
    {
        short divisor = other.m_value >> 8;
        Fixed quotient;
        quotient.m_value = (m_value << 8) / divisor;
        return quotient;
    }

    int Raw() const { return m_value; }
    short Integer() const { return m_value >> 16; }

private:
    int m_value;
};

#endif
