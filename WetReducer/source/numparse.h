//------------------------------------------------------------------------
// Copyright(c) 2026 Yonie.
//------------------------------------------------------------------------
// A number read from a panel geometry string, locale-free.

#pragma once

namespace Yonie {

// strtod and atof follow LC_NUMERIC, and a host that runs in the user's locale (Ardour)
// with a decimal comma read "25.5" as 25 - every click missed. The geometry strings are
// always written with a dot, so parse them without the C library: sign, digits, an
// optional fraction and an optional exponent.
inline const char* parseNumber(const char* p, double& v)
{
    const char* s = p;
    double sign = 1.0;
    if (*s == '-' || *s == '+')
        sign = (*s++ == '-') ? -1.0 : 1.0;
    if (!((*s >= '0' && *s <= '9') || (*s == '.' && s[1] >= '0' && s[1] <= '9')))
        return p;
    double x = 0.0;
    while (*s >= '0' && *s <= '9')
        x = x * 10.0 + (*s++ - '0');
    if (*s == '.')
    {
        double scale = 0.1;
        for (++s; *s >= '0' && *s <= '9'; ++s, scale *= 0.1)
            x += (*s - '0') * scale;
    }
    if ((*s == 'e' || *s == 'E') && ((s[1] >= '0' && s[1] <= '9') ||
        ((s[1] == '-' || s[1] == '+') && s[2] >= '0' && s[2] <= '9')))
    {
        ++s;
        const int esign = (*s == '-') ? -1 : 1;
        if (*s == '-' || *s == '+')
            ++s;
        int e = 0;
        while (*s >= '0' && *s <= '9')
            e = e * 10 + (*s++ - '0');
        for (int k = 0; k < e; ++k)
            x = esign > 0 ? x * 10.0 : x / 10.0;
    }
    v = sign * x;
    return s;
}

} // namespace Yonie
