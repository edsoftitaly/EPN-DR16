// EPN DR-16 :: program table
// Source: RMX16 user manual Appendix B (AMS / UA Expanded, verbatim reprint of original)
// Values marked [est] are not given in the manual and are estimates.
#pragma once

namespace epn
{
enum class Algo { Fdn, Chorus, Echo, Delay, Taps, Freeze };
enum class TapMode { None, Nonlin2, Nonlin1, Reverse1, Reverse2, Image };

struct ProgramSpec
{
    const char* name;      // 8-char starburst field
    Algo algo;
    int voicing;           // index into FDN voicing table (Algo::Fdn / Freeze)
    TapMode tap;
    int preMaxMs;          // maximum pre-delay
    int preStepMs;         // nudge step
    float decMin, decMax;  // decay control range (display units)
    bool lo, hi;           // decay filter availability
    // defaults loaded on program change ("custom settings are lost")
    int preDef; float decDef; int loDef; int hiDef;
};

constexpr int kNumPrograms = 18;

inline const ProgramSpec& program (int index1to18)
{
    static const ProgramSpec t[kNumPrograms] = {
        //  name        algo          v  tap               preMax step dMin dMax  lo     hi     preDef dDef lo  hi
        { "AMBIENCE", Algo::Fdn,     0, TapMode::None,     200, 10, 0.0f, 9.9f, true,  true,   0, 2.0f,  0,  0 },
        { "ROOM A1",  Algo::Fdn,     1, TapMode::None,     300, 10, 0.0f, 9.9f, false, true,   0, 1.2f,  0, -7 },
        { "HALL C1",  Algo::Fdn,     2, TapMode::None,     200, 10, 0.0f, 9.9f, true,  true,  20, 2.4f,  0, -2 }, // preMax [est]
        { "PLATE A1", Algo::Fdn,     3, TapMode::None,     300, 10, 0.0f, 9.9f, true,  true,   0, 2.0f,  0, -1 },
        { "HALL B3",  Algo::Fdn,     4, TapMode::None,     200, 10, 0.0f, 9.9f, true,  true,  20, 2.6f,  0, -2 },
        { "CHORUS",   Algo::Chorus,  0, TapMode::None,      50,  1, 0.0f, 9.9f, false, true,  12, 3.0f,  0, -3 },
        { "ECHO",     Algo::Echo,    0, TapMode::None,    1200, 10, 0.0f, 9.9f, true,  true, 300, 3.0f,  0, -3 },
        { "NONLIN 2", Algo::Taps,    0, TapMode::Nonlin2,  200, 10, 0.1f, 9.9f, false, true,   0, 3.0f,  0,  0 }, // preMax [est]
        { "REVERSE 1",Algo::Taps,    0, TapMode::Reverse1, 200, 10, 0.1f, 9.9f, false, false,  0, 4.0f,  0,  0 }, // preMax [est]
        { "REVERSE 2",Algo::Taps,    0, TapMode::Reverse2, 200, 10, 0.1f, 9.9f, false, true,   0, 4.0f,  0,  0 }, // preMax [est]
        { "FREEZE",   Algo::Freeze,  0, TapMode::None,       0, 10, 0.0f, 9.9f, false, false,  0, 0.0f,  0,  0 },
        { "ROOM A0",  Algo::Fdn,     5, TapMode::None,     300, 10, 0.0f, 9.9f, false, true,   0, 1.2f,  0, -7 },
        { "ROOM B1",  Algo::Fdn,     6, TapMode::None,     200, 10, 0.1f, 0.7f, false, true,   0, 0.5f,  0, -3 }, // preMax [est]
        { "HALL A1",  Algo::Fdn,     7, TapMode::None,     300, 10, 0.0f, 9.9f, false, true,  20, 2.4f,  0, -2 },
        { "PLATE B1", Algo::Fdn,     8, TapMode::None,     300, 10, 0.0f, 9.9f, true,  true,   0, 2.0f,  0, -1 },
        { "DELAY",    Algo::Delay,   0, TapMode::None,     810, 10, 0.0f, 9.9f, false, false, 250, 0.0f, 0,  0 },
        { "IMAGE P1", Algo::Taps,    0, TapMode::Image,    200, 10, 0.1f, 9.9f, false, true,   0, 5.0f,  0,  0 }, // preMax [est]
        { "NONLIN 1", Algo::Taps,    0, TapMode::Nonlin1,  200, 10, 0.1f, 9.9f, false, false, 30, 4.0f,  0,  0 }, // preMax [est]
    };
    int i = index1to18 < 1 ? 1 : (index1to18 > kNumPrograms ? kNumPrograms : index1to18);
    return t[i - 1];
}

constexpr int kEchoAMaxMs = 1200;
constexpr int kEchoBMaxMs = 400;
} // namespace epn
