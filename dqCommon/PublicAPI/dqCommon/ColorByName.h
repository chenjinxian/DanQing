// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Named color constants
//
// Ported from: itwinjs-core core/common/src/ColorByName.ts
// Each value is a 32-bit integer in 0x00BBGGRR format (red in the low byte,
// transparency 0 = fully opaque). This matches ColorDef's internal TBGR format
// with zero transparency.
#pragma once

#include "DqCommon.h"

#include <cstdint>

BEGIN_DQ_COMMON_NAMESPACE

// A set of known colors by HTML color name, as a 32-bit integer in 0xBBGGRR form.
// Ported from: itwinjs-core core/common/src/ColorByName.ts
struct DQ_COMMON_EXPORT ColorByName {
    static constexpr uint32_t aliceBlue = 0xFFF8F0;
    static constexpr uint32_t amber = 0x00BFFF;
    static constexpr uint32_t antiqueWhite = 0xD7EBFA;
    static constexpr uint32_t aqua = 0xFFFF00;
    static constexpr uint32_t aquamarine = 0xD4FF7F;
    static constexpr uint32_t azure = 0xFFFFF0;
    static constexpr uint32_t beige = 0xDCF5F5;
    static constexpr uint32_t bisque = 0xC4E4FF;
    static constexpr uint32_t black = 0x000000;
    static constexpr uint32_t blanchedAlmond = 0xCDEBFF;
    static constexpr uint32_t blue = 0xFF0000;
    static constexpr uint32_t blueViolet = 0xE22B8A;
    static constexpr uint32_t brown = 0x2A2AA5;
    static constexpr uint32_t burlyWood = 0x87B8DE;
    static constexpr uint32_t cadetBlue = 0xA09E5F;
    static constexpr uint32_t chartreuse = 0x00FF7F;
    static constexpr uint32_t chocolate = 0x1E69D2;
    static constexpr uint32_t coral = 0x507FFF;
    static constexpr uint32_t cornflowerBlue = 0xED9564;
    static constexpr uint32_t cornSilk = 0xDCF8FF;
    static constexpr uint32_t crimson = 0x3C14DC;
    static constexpr uint32_t cyan = 0xFFFF00;
    static constexpr uint32_t darkBlue = 0x8B0000;
    static constexpr uint32_t darkBrown = 0x214365;
    static constexpr uint32_t darkCyan = 0x8B8B00;
    static constexpr uint32_t darkGoldenrod = 0x0B86B8;
    static constexpr uint32_t darkGray = 0xA9A9A9;
    static constexpr uint32_t darkGreen = 0x006400;
    static constexpr uint32_t darkGrey = 0xA9A9A9;
    static constexpr uint32_t darkKhaki = 0x6BB7BD;
    static constexpr uint32_t darkMagenta = 0x8B008B;
    static constexpr uint32_t darkOliveGreen = 0x2F6B55;
    static constexpr uint32_t darkOrange = 0x008CFF;
    static constexpr uint32_t darkOrchid = 0xCC3299;
    static constexpr uint32_t darkRed = 0x00008B;
    static constexpr uint32_t darkSalmon = 0x7A96E9;
    static constexpr uint32_t darkSeagreen = 0x8FBC8F;
    static constexpr uint32_t darkSlateBlue = 0x8B3D48;
    static constexpr uint32_t darkSlateGray = 0x4F4F2F;
    static constexpr uint32_t darkSlateGrey = 0x4F4F2F;
    static constexpr uint32_t darkTurquoise = 0xD1CE00;
    static constexpr uint32_t darkViolet = 0xD30094;
    static constexpr uint32_t deepPink = 0x9314FF;
    static constexpr uint32_t deepSkyBlue = 0xFFBF00;
    static constexpr uint32_t dimGray = 0x696969;
    static constexpr uint32_t dimGrey = 0x696969;
    static constexpr uint32_t dodgerBlue = 0xFF901E;
    static constexpr uint32_t fireBrick = 0x2222B2;
    static constexpr uint32_t floralWhite = 0xF0FAFF;
    static constexpr uint32_t forestGreen = 0x228B22;
    static constexpr uint32_t fuchsia = 0xFF00FF;
    static constexpr uint32_t gainsboro = 0xDCDCDC;
    static constexpr uint32_t ghostWhite = 0xFFF8F8;
    static constexpr uint32_t gold = 0x00D7FF;
    static constexpr uint32_t goldenrod = 0x20A5DA;
    static constexpr uint32_t gray = 0x808080;
    static constexpr uint32_t green = 0x008000;
    static constexpr uint32_t greenYellow = 0x2FFFAD;
    static constexpr uint32_t grey = 0x808080;
    static constexpr uint32_t honeydew = 0xF0FFF0;
    static constexpr uint32_t hotPink = 0xB469FF;
    static constexpr uint32_t indianRed = 0x5C5CCD;
    static constexpr uint32_t indigo = 0x82004B;
    static constexpr uint32_t ivory = 0xF0FFFF;
    static constexpr uint32_t khaki = 0x8CE6F0;
    static constexpr uint32_t lavender = 0xFAE6E6;
    static constexpr uint32_t lavenderBlush = 0xF5F0FF;
    static constexpr uint32_t lawnGreen = 0x00FC7C;
    static constexpr uint32_t lemonChiffon = 0xCDFAFF;
    static constexpr uint32_t lightBlue = 0xE6D8AD;
    static constexpr uint32_t lightCoral = 0x8080F0;
    static constexpr uint32_t lightCyan = 0xFFFFE0;
    static constexpr uint32_t lightGoldenrodYellow = 0xD2FAFA;
    static constexpr uint32_t lightGray = 0xD3D3D3;
    static constexpr uint32_t lightGreen = 0x90EE90;
    static constexpr uint32_t lightGrey = 0xD3D3D3;
    static constexpr uint32_t lightPink = 0xC1B6FF;
    static constexpr uint32_t lightSalmon = 0x7AA0FF;
    static constexpr uint32_t lightSeagreen = 0xAAB220;
    static constexpr uint32_t lightSkyBlue = 0xFACE87;
    static constexpr uint32_t lightSlateGray = 0x998877;
    static constexpr uint32_t lightSlateGrey = 0x998877;
    static constexpr uint32_t lightSteelBlue = 0xDEC4B0;
    static constexpr uint32_t lightyellow = 0xE0FFFF;
    static constexpr uint32_t lime = 0x00FF00;
    static constexpr uint32_t limeGreen = 0x32CD32;
    static constexpr uint32_t linen = 0xE6F0FA;
    static constexpr uint32_t magenta = 0xFF00FF;
    static constexpr uint32_t maroon = 0x000080;
    static constexpr uint32_t mediumAquamarine = 0xAACD66;
    static constexpr uint32_t mediumBlue = 0xCD0000;
    static constexpr uint32_t mediumOrchid = 0xD355BA;
    static constexpr uint32_t mediumPurple = 0xDB7093;
    static constexpr uint32_t mediumSeaGreen = 0x71B33C;
    static constexpr uint32_t mediumSlateBlue = 0xEE687B;
    static constexpr uint32_t mediumSpringGreen = 0x9AFA00;
    static constexpr uint32_t mediumTurquoise = 0xCCD148;
    static constexpr uint32_t mediumVioletRed = 0x8515C7;
    static constexpr uint32_t midnightBlue = 0x701919;
    static constexpr uint32_t mintCream = 0xFAFFF5;
    static constexpr uint32_t mistyRose = 0xE1E4FF;
    static constexpr uint32_t moccasin = 0xB5E4FF;
    static constexpr uint32_t navajoWhite = 0xADDEFF;
    static constexpr uint32_t navy = 0x800000;
    static constexpr uint32_t oldLace = 0xE6F5FD;
    static constexpr uint32_t olive = 0x008080;
    static constexpr uint32_t oliveDrab = 0x238E6B;
    static constexpr uint32_t orange = 0x00A5FF;
    static constexpr uint32_t orangeRed = 0x0045FF;
    static constexpr uint32_t orchid = 0xD670DA;
    static constexpr uint32_t paleGoldenrod = 0xAAE8EE;
    static constexpr uint32_t paleGreen = 0x98FB98;
    static constexpr uint32_t paleTurquoise = 0xEEEEAF;
    static constexpr uint32_t paleVioletRed = 0x9370DB;
    static constexpr uint32_t papayaWhip = 0xD5EFFF;
    static constexpr uint32_t peachPuff = 0xB9DAFF;
    static constexpr uint32_t peru = 0x3F85CD;
    static constexpr uint32_t pink = 0xCBC0FF;
    static constexpr uint32_t plum = 0xDDA0DD;
    static constexpr uint32_t powderBlue = 0xE6E0B0;
    static constexpr uint32_t purple = 0x800080;
    static constexpr uint32_t rebeccaPurple = 0x993366;
    static constexpr uint32_t red = 0x0000FF;
    static constexpr uint32_t rosyBrown = 0x8F8FBC;
    static constexpr uint32_t royalBlue = 0xE16941;
    static constexpr uint32_t saddleBrown = 0x13458B;
    static constexpr uint32_t salmon = 0x7280FA;
    static constexpr uint32_t sandyBrown = 0x60A4F4;
    static constexpr uint32_t seaGreen = 0x578B2E;
    static constexpr uint32_t seaShell = 0xEEF5FF;
    static constexpr uint32_t sienna = 0x2D52A0;
    static constexpr uint32_t silver = 0xC0C0C0;
    static constexpr uint32_t skyBlue = 0xEBCE87;
    static constexpr uint32_t slateBlue = 0xCD5A6A;
    static constexpr uint32_t slateGray = 0x908070;
    static constexpr uint32_t slateGrey = 0x908070;
    static constexpr uint32_t snow = 0xFAFAFF;
    static constexpr uint32_t springGreen = 0x7FFF00;
    static constexpr uint32_t steelBlue = 0xB48246;
    static constexpr uint32_t tan = 0x8CB4D2;
    static constexpr uint32_t teal = 0x808000;
    static constexpr uint32_t thistle = 0xD8BFD8;
    static constexpr uint32_t tomato = 0x4763FF;
    static constexpr uint32_t turquoise = 0xD0E040;
    static constexpr uint32_t violet = 0xEE82EE;
    static constexpr uint32_t wheat = 0xB3DEF5;
    static constexpr uint32_t white = 0xFFFFFF;
    static constexpr uint32_t whiteSmoke = 0xF5F5F5;
    static constexpr uint32_t yellow = 0x00FFFF;
    static constexpr uint32_t yellowGreen = 0x32CD9A;
};

END_DQ_COMMON_NAMESPACE
