// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — ColorDef implementation
//
// Ported from: itwinjs-core core/common/src/ColorDef.ts
#include "dqCommon/ColorDef.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <regex>
#include <sstream>

BEGIN_DQ_COMMON_NAMESPACE

// Well-known constants — ported from: itwinjs-core ColorDef static fields
const ColorDef ColorDef::black = ColorDef(ColorByName::black);
const ColorDef ColorDef::white = ColorDef(ColorByName::white);
const ColorDef ColorDef::red = ColorDef(ColorByName::red);
const ColorDef ColorDef::green = ColorDef(ColorByName::green);
const ColorDef ColorDef::blue = ColorDef(ColorByName::blue);

// Helper: pack R,G,B,T into 0xTTBBGGRR
// Ported from: itwinjs-core ColorDef.computeTbgrFromComponents()
#pragma warning(push)
#pragma warning(disable : 4458) // 参数名与 itwinjs 参考 1:1，遮蔽同名静态成员仅 MSVC /W4 提示
ColorDefProps ColorDef::computeTbgrFromComponents(int red, int green, int blue, int transparency)
{
    const auto r = static_cast<uint8_t>((std::max)(0, (std::min)(255, red)));
    const auto g = static_cast<uint8_t>((std::max)(0, (std::min)(255, green)));
    const auto b = static_cast<uint8_t>((std::max)(0, (std::min)(255, blue)));
    const auto t = static_cast<uint8_t>((std::max)(0, (std::min)(255, transparency)));
    return static_cast<uint32_t>(r) | (static_cast<uint32_t>(g) << 8) | (static_cast<uint32_t>(b) << 16) |
           (static_cast<uint32_t>(t) << 24);
}
#pragma warning(pop)

// Ported from: itwinjs-core ColorDef.getColors()
ColorComponents ColorDef::getColors(ColorDefProps tbgr) noexcept
{
    ColorComponents c;
    c.r = static_cast<uint8_t>(tbgr & 0xFF);
    c.g = static_cast<uint8_t>((tbgr >> 8) & 0xFF);
    c.b = static_cast<uint8_t>((tbgr >> 16) & 0xFF);
    c.t = static_cast<uint8_t>((tbgr >> 24) & 0xFF);
    return c;
}

ColorComponents ColorDef::getColors() const noexcept
{
    return getColors(m_tbgr);
}

// Ported from: itwinjs-core ColorDef.getAbgr()
uint32_t ColorDef::getAbgr(ColorDefProps tbgr) noexcept
{
    // Convert transparency to alpha: alpha = 255 - transparency
    const uint8_t t = static_cast<uint8_t>((tbgr >> 24) & 0xFF);
    const uint8_t a = 255 - t;
    return (tbgr & 0x00FFFFFF) | (static_cast<uint32_t>(a) << 24);
}

// Ported from: itwinjs-core ColorDef.getRgb()
// Converts TBGR (R in low byte) to RRGGBB (B in low byte).
uint32_t ColorDef::getRgb(ColorDefProps tbgr) noexcept
{
    const uint8_t r = static_cast<uint8_t>(tbgr & 0xFF);
    const uint8_t g = static_cast<uint8_t>((tbgr >> 8) & 0xFF);
    const uint8_t b = static_cast<uint8_t>((tbgr >> 16) & 0xFF);
    return (static_cast<uint32_t>(r) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
}

// Ported from: itwinjs-core ColorDef.getAlpha()
int ColorDef::getAlpha(ColorDefProps tbgr) noexcept
{
    return 255 - static_cast<int>((tbgr >> 24) & 0xFF);
}

// Ported from: itwinjs-core ColorDef.isOpaque()
bool ColorDef::isOpaque(ColorDefProps tbgr) noexcept
{
    return getAlpha(tbgr) == 255;
}

// Ported from: itwinjs-core ColorDef.getTransparency()
int ColorDef::getTransparency(ColorDefProps tbgr) noexcept
{
    return static_cast<int>((tbgr >> 24) & 0xFF);
}

// Ported from: itwinjs-core ColorDef.withAlpha()
ColorDefProps ColorDef::withAlpha(ColorDefProps tbgr, int alpha) noexcept
{
    const uint8_t a = static_cast<uint8_t>(255 - (alpha | 0));
    return (tbgr & 0x00FFFFFF) | (static_cast<uint32_t>(a) << 24);
}

// Ported from: itwinjs-core ColorDef.withTransparency()
ColorDefProps ColorDef::withTransparency(ColorDefProps tbgr, int transparency) noexcept
{
    return withAlpha(tbgr, 255 - transparency);
}

// --- Factory methods ---

// Create() methods are inline in ColorDef.h.
// Ported from: itwinjs-core ColorDef.create()

// Ported from: itwinjs-core ColorDef.from()
#pragma warning(push)
#pragma warning(disable : 4458) // 参数名与 itwinjs 参考 1:1，遮蔽同名静态成员仅 MSVC /W4 提示
ColorDef ColorDef::from(int red, int green, int blue, int transparency)
{
    return fromTbgr(computeTbgrFromComponents(red, green, blue, transparency));
}
#pragma warning(pop)

// Ported from: itwinjs-core ColorDef.fromTbgr()
ColorDef ColorDef::fromTbgr(ColorDefProps tbgr)
{
    // Cache well-known colors (matches itwinjs pattern)
    switch (tbgr) {
    case ColorByName::black:
        return black;
    case ColorByName::white:
        return white;
    case ColorByName::red:
        return red;
    case ColorByName::green:
        return green;
    case ColorByName::blue:
        return blue;
    default:
        return ColorDef(tbgr);
    }
}

// Ported from: itwinjs-core ColorDef.fromAbgr()
ColorDef ColorDef::fromAbgr(uint32_t abgr)
{
    return fromTbgr(getAbgr(abgr));
}

// Ported from: itwinjs-core ColorDef.computeTbgr()
ColorDefProps ColorDef::computeTbgr(const std::string& val)
{
    auto result = tryComputeTbgrFromString(val);
    return result.value_or(0);
}

ColorDefProps ColorDef::computeTbgr(ColorDefProps val)
{
    return val;
}

// Ported from: itwinjs-core ColorDef.computeTbgrFromString()
ColorDefProps ColorDef::computeTbgrFromString(const std::string& val)
{
    return computeTbgr(val);
}

// Helper: to lowercase
static std::string ToLower(const std::string& s)
{
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

// Helper: trim whitespace
static std::string Trim(const std::string& s)
{
    const auto start = s.find_first_not_of(" \t\n\r");
    if (start == std::string::npos)
        return {};
    const auto end = s.find_last_not_of(" \t\n\r");
    return s.substr(start, end - start + 1);
}

// Helper: check if string ends with '%'
static bool HasPercent(const std::string& str)
{
    return !str.empty() && str.back() == '%';
}

// Helper: parse float or percent → 0..255
static double FloatOrPercent(const std::string& str)
{
    const double v = std::stod(str);
    const double clamped = HasPercent(str) ? v / 100.0 : v;
    return 255.0 * std::max(0.0, std::min(1.0, clamped));
}

// Helper: parse int or percent → 0..255
static int IntOrPercent(const std::string& str)
{
    if (HasPercent(str)) {
        const double v = std::stod(str.substr(0, str.size() - 1)) / 100.0 * 255.0;
        return static_cast<int>(std::max(0.0, std::min(255.0, v)));
    }
    return static_cast<int>(std::max(0.0, std::min(255.0, static_cast<double>(std::stoi(str)))));
}

// Ported from: itwinjs-core ColorDef.tryComputeTbgrFromString()
std::optional<ColorDefProps> ColorDef::tryComputeTbgrFromString(const std::string& input)
{
    if (input.empty())
        return std::nullopt;

    const std::string val = ToLower(Trim(input));

    // Try rgb/rgba/hsl/hsla patterns
    // Ported from: itwinjs-core regex /^((?:rgb|hsl)a?)\(\s*([^\)]*)\)/
    {
        static const std::regex rgbHslRegex(R"((rgb|hsl)(a?)\(\s*([^)]*)\))");
        std::smatch m;
        if (std::regex_search(val, m, rgbHslRegex)) {
            const std::string name = m[1].str();
            const std::string components = m[3].str();

            if (name == "rgb") {
                // rgb(r,g,b) or rgba(r,g,b,a)
                // Ported from: itwinjs-core regex for rgb components
                static const std::regex rgbRegex(
                    R"((\d+%*)\s*[, ]\s*(\d+%*)\s*[, ]\s*(\d+%*)\s*([,/]\s*([0-9]*\.?[0-9]+%*)\s*)?)");
                std::smatch cm;
                if (std::regex_match(components, cm, rgbRegex)) {
                    const int r = IntOrPercent(cm[1].str());
                    const int g = IntOrPercent(cm[2].str());
                    const int b = IntOrPercent(cm[3].str());
                    // TS: transparency = 255 - floatOrPercent(alpha), then Uint8Array truncates
                    const int t = cm[5].matched
                        ? static_cast<int>(255.0 - FloatOrPercent(cm[5].str()))
                        : 0;
                    return computeTbgrFromComponents(r, g, b, t);
                }
            } else if (name == "hsl") {
                // hsl(h,s%,l%) or hsla(h,s%,l%,a)
                // Ported from: itwinjs-core regex for hsl components
                static const std::regex hslRegex(
                    R"(([0-9]*\.?[0-9]+)\s*,\s*(\d+)%\s*,\s*(\d+)%\s*(,\s*([0-9]*\.?[0-9]+)\s*)?)");
                std::smatch cm;
                if (std::regex_match(components, cm, hslRegex)) {
                    const double h = std::stod(cm[1].str()) / 360.0;
                    const double s = std::stoi(cm[2].str()) / 100.0;
                    const double l = std::stoi(cm[3].str()) / 100.0;
                    const int t = cm[5].matched
                        ? static_cast<int>(255.0 - FloatOrPercent(cm[5].str()))
                        : 0;
                    return computeTbgrFromHSL(h, s, l, t);
                }
            }
        }
    }

    // Try hex color: #rgb or #rrggbb
    // Ported from: itwinjs-core regex /^\#([a-f0-9]+)$/
    {
        static const std::regex hexRegex(R"(#([a-f0-9]+))");
        std::smatch m;
        if (std::regex_match(val, m, hexRegex)) {
            const std::string hex = m[1].str();
            const size_t size = hex.size();

            if (size == 3) {
                // #ff0 → #ffff00
                const int r = std::stoi(std::string(2, hex[0]), nullptr, 16);
                const int g = std::stoi(std::string(2, hex[1]), nullptr, 16);
                const int b = std::stoi(std::string(2, hex[2]), nullptr, 16);
                return computeTbgrFromComponents(r, g, b, 0);
            }
            if (size == 6) {
                // #ff0000
                const int r = std::stoi(hex.substr(0, 2), nullptr, 16);
                const int g = std::stoi(hex.substr(2, 2), nullptr, 16);
                const int b = std::stoi(hex.substr(4, 2), nullptr, 16);
                return computeTbgrFromComponents(r, g, b, 0);
            }
        }
    }

    // Try named color — ported from: itwinjs-core ColorByName lookup
    // We need to iterate all ColorByName constants and compare lowercase names.
    // Using a static lookup table for efficiency.
    // clang-format off
    static const struct { const char* name; uint32_t value; } s_namedColors[] = {
        {"aliceblue", ColorByName::aliceBlue}, {"amber", ColorByName::amber},
        {"antiquewhite", ColorByName::antiqueWhite}, {"aqua", ColorByName::aqua},
        {"aquamarine", ColorByName::aquamarine}, {"azure", ColorByName::azure},
        {"beige", ColorByName::beige}, {"bisque", ColorByName::bisque},
        {"black", ColorByName::black}, {"blanchedalmond", ColorByName::blanchedAlmond},
        {"blue", ColorByName::blue}, {"blueviolet", ColorByName::blueViolet},
        {"brown", ColorByName::brown}, {"burlywood", ColorByName::burlyWood},
        {"cadetblue", ColorByName::cadetBlue}, {"chartreuse", ColorByName::chartreuse},
        {"chocolate", ColorByName::chocolate}, {"coral", ColorByName::coral},
        {"cornflowerblue", ColorByName::cornflowerBlue}, {"cornsilk", ColorByName::cornSilk},
        {"crimson", ColorByName::crimson}, {"cyan", ColorByName::cyan},
        {"darkblue", ColorByName::darkBlue}, {"darkbrown", ColorByName::darkBrown},
        {"darkcyan", ColorByName::darkCyan}, {"darkgoldenrod", ColorByName::darkGoldenrod},
        {"darkgray", ColorByName::darkGray}, {"darkgreen", ColorByName::darkGreen},
        {"darkgrey", ColorByName::darkGrey}, {"darkkhaki", ColorByName::darkKhaki},
        {"darkmagenta", ColorByName::darkMagenta}, {"darkolivegreen", ColorByName::darkOliveGreen},
        {"darkorange", ColorByName::darkOrange}, {"darkorchid", ColorByName::darkOrchid},
        {"darkred", ColorByName::darkRed}, {"darksalmon", ColorByName::darkSalmon},
        {"darkseagreen", ColorByName::darkSeagreen}, {"darkslateblue", ColorByName::darkSlateBlue},
        {"darkslategray", ColorByName::darkSlateGray}, {"darkslategrey", ColorByName::darkSlateGrey},
        {"darkturquoise", ColorByName::darkTurquoise}, {"darkviolet", ColorByName::darkViolet},
        {"deeppink", ColorByName::deepPink}, {"deepskyblue", ColorByName::deepSkyBlue},
        {"dimgray", ColorByName::dimGray}, {"dimgrey", ColorByName::dimGrey},
        {"dodgerblue", ColorByName::dodgerBlue}, {"firebrick", ColorByName::fireBrick},
        {"floralwhite", ColorByName::floralWhite}, {"forestgreen", ColorByName::forestGreen},
        {"fuchsia", ColorByName::fuchsia}, {"gainsboro", ColorByName::gainsboro},
        {"ghostwhite", ColorByName::ghostWhite}, {"gold", ColorByName::gold},
        {"goldenrod", ColorByName::goldenrod}, {"gray", ColorByName::gray},
        {"green", ColorByName::green}, {"greenyellow", ColorByName::greenYellow},
        {"grey", ColorByName::grey}, {"honeydew", ColorByName::honeydew},
        {"hotpink", ColorByName::hotPink}, {"indianred", ColorByName::indianRed},
        {"indigo", ColorByName::indigo}, {"ivory", ColorByName::ivory},
        {"khaki", ColorByName::khaki}, {"lavender", ColorByName::lavender},
        {"lavenderblush", ColorByName::lavenderBlush}, {"lawngreen", ColorByName::lawnGreen},
        {"lemonchiffon", ColorByName::lemonChiffon}, {"lightblue", ColorByName::lightBlue},
        {"lightcoral", ColorByName::lightCoral}, {"lightcyan", ColorByName::lightCyan},
        {"lightgoldenrodyellow", ColorByName::lightGoldenrodYellow},
        {"lightgray", ColorByName::lightGray}, {"lightgreen", ColorByName::lightGreen},
        {"lightgrey", ColorByName::lightGrey}, {"lightpink", ColorByName::lightPink},
        {"lightsalmon", ColorByName::lightSalmon}, {"lightseagreen", ColorByName::lightSeagreen},
        {"lightskyblue", ColorByName::lightSkyBlue}, {"lightslategray", ColorByName::lightSlateGray},
        {"lightslategrey", ColorByName::lightSlateGrey},
        {"lightsteelblue", ColorByName::lightSteelBlue}, {"lightyellow", ColorByName::lightyellow},
        {"lime", ColorByName::lime}, {"limegreen", ColorByName::limeGreen},
        {"linen", ColorByName::linen}, {"magenta", ColorByName::magenta},
        {"maroon", ColorByName::maroon}, {"mediumaquamarine", ColorByName::mediumAquamarine},
        {"mediumblue", ColorByName::mediumBlue}, {"mediumorchid", ColorByName::mediumOrchid},
        {"mediumpurple", ColorByName::mediumPurple}, {"mediumseagreen", ColorByName::mediumSeaGreen},
        {"mediumslateblue", ColorByName::mediumSlateBlue},
        {"mediumspringgreen", ColorByName::mediumSpringGreen},
        {"mediumturquoise", ColorByName::mediumTurquoise},
        {"mediumvioletred", ColorByName::mediumVioletRed},
        {"midnightblue", ColorByName::midnightBlue}, {"mintcream", ColorByName::mintCream},
        {"mistyrose", ColorByName::mistyRose}, {"moccasin", ColorByName::moccasin},
        {"navajowhite", ColorByName::navajoWhite}, {"navy", ColorByName::navy},
        {"oldlace", ColorByName::oldLace}, {"olive", ColorByName::olive},
        {"olivedrab", ColorByName::oliveDrab}, {"orange", ColorByName::orange},
        {"orangered", ColorByName::orangeRed}, {"orchid", ColorByName::orchid},
        {"palegoldenrod", ColorByName::paleGoldenrod}, {"palegreen", ColorByName::paleGreen},
        {"paleturquoise", ColorByName::paleTurquoise}, {"palevioletred", ColorByName::paleVioletRed},
        {"papayawhip", ColorByName::papayaWhip}, {"peachpuff", ColorByName::peachPuff},
        {"peru", ColorByName::peru}, {"pink", ColorByName::pink},
        {"plum", ColorByName::plum}, {"powderblue", ColorByName::powderBlue},
        {"purple", ColorByName::purple}, {"rebeccapurple", ColorByName::rebeccaPurple},
        {"red", ColorByName::red}, {"rosybrown", ColorByName::rosyBrown},
        {"royalblue", ColorByName::royalBlue}, {"saddlebrown", ColorByName::saddleBrown},
        {"salmon", ColorByName::salmon}, {"sandybrown", ColorByName::sandyBrown},
        {"seagreen", ColorByName::seaGreen}, {"seashell", ColorByName::seaShell},
        {"sienna", ColorByName::sienna}, {"silver", ColorByName::silver},
        {"skyblue", ColorByName::skyBlue}, {"slateblue", ColorByName::slateBlue},
        {"slategray", ColorByName::slateGray}, {"slategrey", ColorByName::slateGrey},
        {"snow", ColorByName::snow}, {"springgreen", ColorByName::springGreen},
        {"steelblue", ColorByName::steelBlue}, {"tan", ColorByName::tan},
        {"teal", ColorByName::teal}, {"thistle", ColorByName::thistle},
        {"tomato", ColorByName::tomato}, {"turquoise", ColorByName::turquoise},
        {"violet", ColorByName::violet}, {"wheat", ColorByName::wheat},
        {"white", ColorByName::white}, {"whitesmoke", ColorByName::whiteSmoke},
        {"yellow", ColorByName::yellow}, {"yellowgreen", ColorByName::yellowGreen},
    };
    // clang-format on

    for (const auto& entry : s_namedColors) {
        if (val == entry.name)
            return entry.value;
    }

    return std::nullopt;
}

// Ported from: itwinjs-core ColorDef.fromString()
ColorDef ColorDef::fromString(const std::string& val)
{
    return fromTbgr(computeTbgrFromString(val));
}

// Ported from: itwinjs-core ColorDef.computeTbgrFromHSL()
ColorDefProps ColorDef::computeTbgrFromHSL(double h, double s, double l, int transparency)
{
    // Helper: hue to RGB component
    auto torgb = [](double p1, double q1, double t) -> double {
        if (t < 0.0)
            t += 1.0;
        if (t > 1.0)
            t -= 1.0;
        if (t < 1.0 / 6.0)
            return p1 + (q1 - p1) * 6.0 * t;
        if (t < 1.0 / 2.0)
            return q1;
        if (t < 2.0 / 3.0)
            return p1 + (q1 - p1) * 6.0 * (2.0 / 3.0 - t);
        return p1;
    };

    auto hue2rgb = [&torgb](double p1, double q1, double t) -> int {
        return static_cast<int>(std::round(torgb(p1, q1, t) * 255.0));
    };

    // Normalize h to [0,1]
    h = std::fmod(h, 1.0);
    if (h < 0.0)
        h += 1.0;
    s = std::max(0.0, std::min(1.0, s));
    l = std::max(0.0, std::min(1.0, l));

    if (s == 0.0) {
        const int gray = static_cast<int>(std::round(l * 255.0));
        return computeTbgrFromComponents(gray, gray, gray, transparency);
    }

    const double p = l <= 0.5 ? l * (1.0 + s) : l + s - (l * s);
    const double q = (2.0 * l) - p;
    return computeTbgrFromComponents(
        hue2rgb(q, p, h + 1.0 / 3.0),
        hue2rgb(q, p, h),
        hue2rgb(q, p, h - 1.0 / 3.0),
        transparency);
}

// Ported from: itwinjs-core ColorDef.fromHSL()
ColorDef ColorDef::fromHSL(double h, double s, double l, int transparency)
{
    return fromTbgr(computeTbgrFromHSL(h, s, l, transparency));
}

// Ported from: itwinjs-core ColorDef.fromHSV()
#pragma warning(push)
#pragma warning(disable : 4458) // 局部变量 white 与 itwinjs 参考 1:1，遮蔽同名静态成员仅 MSVC /W4 提示
ColorDef ColorDef::fromHSV(const HSVColor& hsv, int transparency)
{
    if (hsv.s == 0 || hsv.h == -1) {
        const int white = static_cast<int>(std::floor(
            (255.0 * hsv.v) / 100.0 + 0.5 + 3.0e-14));
        return from(white, white, white, 0);
    }

    double dhue = static_cast<double>(hsv.h);
    const double dsaturation = static_cast<double>(hsv.s);
    const double dvalue = static_cast<double>(hsv.v);

    if (dhue == 360.0)
        dhue = 0.0;
    dhue /= 60.0;
    const int hueIntpart = static_cast<int>(std::floor(dhue));
    const double hueFractpart = dhue - hueIntpart;
    const double valNorm = dvalue / 100.0;
    const double satNorm = dsaturation / 100.0;

    const int p = static_cast<int>(std::floor((valNorm * (1.0 - satNorm) * 255.0) + 0.5)) & 0xFF;
    const int q = static_cast<int>(std::floor((valNorm * (1.0 - (satNorm * hueFractpart)) * 255.0) + 0.5)) & 0xFF;
    const int t = static_cast<int>(std::floor((valNorm * (1.0 - (satNorm * (1.0 - hueFractpart))) * 255.0) + 0.5)) & 0xFF;
    const int v = static_cast<int>(std::floor(valNorm * 255.0 + 0.5)) & 0xFF;

    int r = 0, g = 0, b = 0;
    switch (hueIntpart) {
    case 0:
        r = v;
        g = t;
        b = p;
        break;
    case 1:
        r = q;
        g = v;
        b = p;
        break;
    case 2:
        r = p;
        g = v;
        b = t;
        break;
    case 3:
        r = p;
        g = q;
        b = v;
        break;
    case 4:
        r = t;
        g = p;
        b = v;
        break;
    case 5:
        r = v;
        g = p;
        b = q;
        break;
    }

    return from(r, g, b, transparency);
}
#pragma warning(pop)

// Ported from: itwinjs-core ColorDef.withAlpha() (instance method)
ColorDef ColorDef::withAlpha(int alpha) const
{
    const ColorDefProps newTbgr = withAlpha(m_tbgr, alpha);
    return newTbgr == m_tbgr ? *this : fromTbgr(newTbgr);
}

// Ported from: itwinjs-core ColorDef.withTransparency() (instance method)
ColorDef ColorDef::withTransparency(int transparency) const
{
    const ColorDefProps newTbgr = withTransparency(m_tbgr, transparency);
    return newTbgr == m_tbgr ? *this : fromTbgr(newTbgr);
}

// Ported from: itwinjs-core ColorDef.getName()
std::string ColorDef::getName(ColorDefProps tbgr)
{
    // Iterate named colors to find matching value.
    // Returns the FIRST match (matches itwinjs behavior for duplicates like cyan/aqua).
    // clang-format off
    static const struct { const char* name; uint32_t value; } s_namedColors[] = {
        {"aliceBlue", ColorByName::aliceBlue}, {"amber", ColorByName::amber},
        {"antiqueWhite", ColorByName::antiqueWhite}, {"aqua", ColorByName::aqua},
        {"aquamarine", ColorByName::aquamarine}, {"azure", ColorByName::azure},
        {"beige", ColorByName::beige}, {"bisque", ColorByName::bisque},
        {"black", ColorByName::black}, {"blanchedAlmond", ColorByName::blanchedAlmond},
        {"blue", ColorByName::blue}, {"blueViolet", ColorByName::blueViolet},
        {"brown", ColorByName::brown}, {"burlyWood", ColorByName::burlyWood},
        {"cadetBlue", ColorByName::cadetBlue}, {"chartreuse", ColorByName::chartreuse},
        {"chocolate", ColorByName::chocolate}, {"coral", ColorByName::coral},
        {"cornflowerBlue", ColorByName::cornflowerBlue}, {"cornSilk", ColorByName::cornSilk},
        {"crimson", ColorByName::crimson}, {"cyan", ColorByName::cyan},
        {"darkBlue", ColorByName::darkBlue}, {"darkBrown", ColorByName::darkBrown},
        {"darkCyan", ColorByName::darkCyan}, {"darkGoldenrod", ColorByName::darkGoldenrod},
        {"darkGray", ColorByName::darkGray}, {"darkGreen", ColorByName::darkGreen},
        {"darkGrey", ColorByName::darkGrey}, {"darkKhaki", ColorByName::darkKhaki},
        {"darkMagenta", ColorByName::darkMagenta}, {"darkOliveGreen", ColorByName::darkOliveGreen},
        {"darkOrange", ColorByName::darkOrange}, {"darkOrchid", ColorByName::darkOrchid},
        {"darkRed", ColorByName::darkRed}, {"darkSalmon", ColorByName::darkSalmon},
        {"darkSeagreen", ColorByName::darkSeagreen}, {"darkSlateBlue", ColorByName::darkSlateBlue},
        {"darkSlateGray", ColorByName::darkSlateGray}, {"darkSlateGrey", ColorByName::darkSlateGrey},
        {"darkTurquoise", ColorByName::darkTurquoise}, {"darkViolet", ColorByName::darkViolet},
        {"deepPink", ColorByName::deepPink}, {"deepSkyBlue", ColorByName::deepSkyBlue},
        {"dimGray", ColorByName::dimGray}, {"dimGrey", ColorByName::dimGrey},
        {"dodgerBlue", ColorByName::dodgerBlue}, {"fireBrick", ColorByName::fireBrick},
        {"floralWhite", ColorByName::floralWhite}, {"forestGreen", ColorByName::forestGreen},
        {"fuchsia", ColorByName::fuchsia}, {"gainsboro", ColorByName::gainsboro},
        {"ghostWhite", ColorByName::ghostWhite}, {"gold", ColorByName::gold},
        {"goldenrod", ColorByName::goldenrod}, {"gray", ColorByName::gray},
        {"green", ColorByName::green}, {"greenYellow", ColorByName::greenYellow},
        {"grey", ColorByName::grey}, {"honeydew", ColorByName::honeydew},
        {"hotPink", ColorByName::hotPink}, {"indianRed", ColorByName::indianRed},
        {"indigo", ColorByName::indigo}, {"ivory", ColorByName::ivory},
        {"khaki", ColorByName::khaki}, {"lavender", ColorByName::lavender},
        {"lavenderBlush", ColorByName::lavenderBlush}, {"lawnGreen", ColorByName::lawnGreen},
        {"lemonChiffon", ColorByName::lemonChiffon}, {"lightBlue", ColorByName::lightBlue},
        {"lightCoral", ColorByName::lightCoral}, {"lightCyan", ColorByName::lightCyan},
        {"lightGoldenrodYellow", ColorByName::lightGoldenrodYellow},
        {"lightGray", ColorByName::lightGray}, {"lightGreen", ColorByName::lightGreen},
        {"lightGrey", ColorByName::lightGrey}, {"lightPink", ColorByName::lightPink},
        {"lightSalmon", ColorByName::lightSalmon}, {"lightSeagreen", ColorByName::lightSeagreen},
        {"lightSkyBlue", ColorByName::lightSkyBlue}, {"lightSlateGray", ColorByName::lightSlateGray},
        {"lightSlateGrey", ColorByName::lightSlateGrey},
        {"lightSteelBlue", ColorByName::lightSteelBlue}, {"lightyellow", ColorByName::lightyellow},
        {"lime", ColorByName::lime}, {"limeGreen", ColorByName::limeGreen},
        {"linen", ColorByName::linen}, {"magenta", ColorByName::magenta},
        {"maroon", ColorByName::maroon}, {"mediumAquamarine", ColorByName::mediumAquamarine},
        {"mediumBlue", ColorByName::mediumBlue}, {"mediumOrchid", ColorByName::mediumOrchid},
        {"mediumPurple", ColorByName::mediumPurple}, {"mediumSeaGreen", ColorByName::mediumSeaGreen},
        {"mediumSlateBlue", ColorByName::mediumSlateBlue},
        {"mediumSpringGreen", ColorByName::mediumSpringGreen},
        {"mediumTurquoise", ColorByName::mediumTurquoise},
        {"mediumVioletRed", ColorByName::mediumVioletRed},
        {"midnightBlue", ColorByName::midnightBlue}, {"mintCream", ColorByName::mintCream},
        {"mistyRose", ColorByName::mistyRose}, {"moccasin", ColorByName::moccasin},
        {"navajoWhite", ColorByName::navajoWhite}, {"navy", ColorByName::navy},
        {"oldLace", ColorByName::oldLace}, {"olive", ColorByName::olive},
        {"oliveDrab", ColorByName::oliveDrab}, {"orange", ColorByName::orange},
        {"orangeRed", ColorByName::orangeRed}, {"orchid", ColorByName::orchid},
        {"paleGoldenrod", ColorByName::paleGoldenrod}, {"paleGreen", ColorByName::paleGreen},
        {"paleTurquoise", ColorByName::paleTurquoise}, {"paleVioletRed", ColorByName::paleVioletRed},
        {"papayaWhip", ColorByName::papayaWhip}, {"peachPuff", ColorByName::peachPuff},
        {"peru", ColorByName::peru}, {"pink", ColorByName::pink},
        {"plum", ColorByName::plum}, {"powderBlue", ColorByName::powderBlue},
        {"purple", ColorByName::purple}, {"rebeccaPurple", ColorByName::rebeccaPurple},
        {"red", ColorByName::red}, {"rosyBrown", ColorByName::rosyBrown},
        {"royalBlue", ColorByName::royalBlue}, {"saddleBrown", ColorByName::saddleBrown},
        {"salmon", ColorByName::salmon}, {"sandyBrown", ColorByName::sandyBrown},
        {"seaGreen", ColorByName::seaGreen}, {"seaShell", ColorByName::seaShell},
        {"sienna", ColorByName::sienna}, {"silver", ColorByName::silver},
        {"skyBlue", ColorByName::skyBlue}, {"slateBlue", ColorByName::slateBlue},
        {"slateGray", ColorByName::slateGray}, {"slateGrey", ColorByName::slateGrey},
        {"snow", ColorByName::snow}, {"springGreen", ColorByName::springGreen},
        {"steelBlue", ColorByName::steelBlue}, {"tan", ColorByName::tan},
        {"teal", ColorByName::teal}, {"thistle", ColorByName::thistle},
        {"tomato", ColorByName::tomato}, {"turquoise", ColorByName::turquoise},
        {"violet", ColorByName::violet}, {"wheat", ColorByName::wheat},
        {"white", ColorByName::white}, {"whiteSmoke", ColorByName::whiteSmoke},
        {"yellow", ColorByName::yellow}, {"yellowGreen", ColorByName::yellowGreen},
    };
    // clang-format on

    for (const auto& entry : s_namedColors) {
        if (entry.value == tbgr)
            return entry.name;
    }
    return {};
}

// Ported from: itwinjs-core ColorDef.toHexString()
std::string ColorDef::toHexString(ColorDefProps tbgr)
{
    const uint32_t rgb = getRgb(tbgr);
    char buf[8];
    std::snprintf(buf, sizeof(buf), "#%06x", rgb);
    return buf;
}

// Ported from: itwinjs-core ColorDef.toRgbString()
std::string ColorDef::toRgbString(ColorDefProps tbgr)
{
    const auto c = getColors(tbgr);
    char buf[32];
    std::snprintf(buf, sizeof(buf), "rgb(%d,%d,%d)", c.r, c.g, c.b);
    return buf;
}

// Ported from: itwinjs-core ColorDef.toRgbaString()
std::string ColorDef::toRgbaString(ColorDefProps tbgr)
{
    const auto c = getColors(tbgr);
    const double alpha = getAlpha(tbgr) / 255.0;
    // Use %g to match JS's default toString (removes trailing zeros).
    char buf[48];
    std::snprintf(buf, sizeof(buf), "rgba(%d,%d,%d,%g)", c.r, c.g, c.b, alpha);
    return buf;
}

// Ported from: itwinjs-core ColorDef.toHSL()
HSLColor ColorDef::toHSL() const
{
    auto c = getColors();
    const double r = c.r / 255.0;
    const double g = c.g / 255.0;
    const double b = c.b / 255.0;
    const double max = (std::max)({r, g, b});
    const double min = (std::min)({r, g, b});

    double hue = 0.0;
    double saturation;
    const double lightness = (min + max) / 2.0;

    if (min == max) {
        saturation = 0.0;
    } else {
        const double delta = max - min;
        saturation = lightness <= 0.5 ? delta / (max + min) : delta / (2.0 - max - min);
        if (max == r)
            hue = (g - b) / delta + (g < b ? 6.0 : 0.0);
        else if (max == g)
            hue = (b - r) / delta + 2.0;
        else
            hue = (r - g) / delta + 4.0;
        hue /= 6.0;
    }

    return HSLColor(hue, saturation, lightness);
}

// Ported from: itwinjs-core ColorDef.toHSV()
HSVColor ColorDef::toHSV() const
{
    const auto c = getColors();
    const int r = c.r;
    const int g = c.g;
    const int b = c.b;

    int min = (std::min)({r, g, b});
    int max = (std::max)({r, g, b});

    // amount of "blackness" present
    const int v = static_cast<int>(std::floor((max / 255.0 * 100.0) + 0.5));
    const int deltaRgb = max - min;
    const int s = (max != 0) ? static_cast<int>(std::floor((static_cast<double>(deltaRgb) / max * 100.0) + 0.5)) : 0;
    int h = 0;

    if (s != 0) {
        const double redDistance = static_cast<double>(max - r) / deltaRgb;
        const double greenDistance = static_cast<double>(max - g) / deltaRgb;
        const double blueDistance = static_cast<double>(max - b) / deltaRgb;

        double intermediateHue;
        if (r == max)
            intermediateHue = blueDistance - greenDistance;
        else if (g == max)
            intermediateHue = 2.0 + redDistance - blueDistance;
        else
            intermediateHue = 4.0 + greenDistance - redDistance;

        intermediateHue *= 60.0;
        if (intermediateHue < 0.0)
            intermediateHue += 360.0;

        h = static_cast<int>(std::floor(intermediateHue + 0.5));
        if (h >= 360)
            h = 0;
    }

    return HSVColor(h, s, v);
}

// Ported from: itwinjs-core ColorDef.lerp()
ColorDefProps ColorDef::lerp(ColorDefProps tbgr1, ColorDefProps tbgr2, double weight) noexcept
{
    auto c1 = getColors(tbgr1);
    const auto c2 = getColors(tbgr2);
    c1.r = static_cast<uint8_t>(c1.r + (c2.r - c1.r) * weight);
    c1.g = static_cast<uint8_t>(c1.g + (c2.g - c1.g) * weight);
    c1.b = static_cast<uint8_t>(c1.b + (c2.b - c1.b) * weight);
    return computeTbgrFromComponents(c1.r, c1.g, c1.b, c1.t);
}

ColorDef ColorDef::lerp(const ColorDef& color2, double weight) const
{
    return fromTbgr(lerp(m_tbgr, color2.m_tbgr, weight));
}

// Ported from: itwinjs-core ColorDef.inverse()
ColorDefProps ColorDef::inverse(ColorDefProps tbgr) noexcept
{
    const auto c = getColors(tbgr);
    return computeTbgrFromComponents(255 - c.r, 255 - c.g, 255 - c.b);
}

ColorDef ColorDef::inverse() const
{
    return fromTbgr(inverse(m_tbgr));
}

// Ported from: itwinjs-core ColorDef.isValidColor()
bool ColorDef::isValidColor(const std::string& val)
{
    return tryComputeTbgrFromString(val).has_value();
}

bool ColorDef::isValidColor(uint32_t val) noexcept
{
    return val <= 0xFFFFFFFF && val == static_cast<uint32_t>(val);
}

// Ported from: itwinjs-core ColorDef.visibilityCheck() (private)
static double VisibilityCheck(const ColorDef& fg, const ColorDef& bg)
{
    const auto fc = fg.getColors();
    const auto bc = bg.getColors();
    const double red = std::abs(fc.r - bc.r);
    const double green = std::abs(fc.g - bc.g);
    const double blue = std::abs(fc.b - bc.b);
    return (0.30 * red) + (0.59 * green) + (0.11 * blue);
}

// Ported from: itwinjs-core ColorDef.adjustedForContrast()
ColorDef ColorDef::adjustedForContrast(const ColorDef& other, std::optional<int> alpha) const
{
    const double visibility = VisibilityCheck(*this, other);
    if (static_cast<int>(HSVConstants::VISIBILITY_GOAL) <= visibility) {
        return alpha.has_value() ? withAlpha(alpha.value()) : *this;
    }

    const int adjPercent = static_cast<int>(
        std::floor(((static_cast<int>(HSVConstants::VISIBILITY_GOAL) - visibility) / 255.0) * 100.0));
    auto darkerHSV = toHSV();
    auto brightHSV = darkerHSV.clone();

    darkerHSV = darkerHSV.adjusted(true, adjPercent);
    brightHSV = brightHSV.adjusted(false, adjPercent);

    const int alphaVal = alpha.value_or(getAlpha());
    const auto darker = ColorDef::fromHSV(darkerHSV).withAlpha(alphaVal);
    const auto bright = ColorDef::fromHSV(brightHSV).withAlpha(alphaVal);

    if (bright.getRgb() == other.getRgb())
        return darker;
    if (darker.getRgb() == other.getRgb())
        return bright;

    return (VisibilityCheck(bright, other) >= VisibilityCheck(darker, other)) ? bright : darker;
}

// --- HSLColor/HSVColor implementations that depend on ColorDef ---

// Ported from: itwinjs-core HSLColor.toColorDef()
ColorDef HSLColor::toColorDef(int transparency) const
{
    return ColorDef::fromHSL(h, s, l, transparency);
}

// Ported from: itwinjs-core HSLColor.fromColorDef()
HSLColor HSLColor::fromColorDef(const ColorDef& colorDef)
{
    return colorDef.toHSL();
}

// Ported from: itwinjs-core HSVColor.toColorDef()
ColorDef HSVColor::toColorDef(int transparency) const
{
    return ColorDef::fromHSV(*this, transparency);
}

// Ported from: itwinjs-core HSVColor.fromColorDef()
HSVColor HSVColor::fromColorDef(const ColorDef& colorDef)
{
    return colorDef.toHSV();
}

END_DQ_COMMON_NAMESPACE
