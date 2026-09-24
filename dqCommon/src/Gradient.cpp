// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — Gradient implementation
//
// Ported from: itwinjs-core core/common/src/Gradient.ts
#include "dqCommon/Gradient.h"
#include "dqCommon/ThematicDisplay.h"

#include <algorithm>
#include <cmath>

BEGIN_DQ_COMMON_NAMESPACE

// ---------------------------------------------------------------------------
// Fixed color-scheme tables (ref lines 129-137)
// ---------------------------------------------------------------------------
// NB: these color values are ordered as [value, R, B, G] in the ref (comment line 130).
// We store them as [value, R, G, B] (standard RGB) and pass (R, G, B) to computeTbgrFromComponents
// exactly as the ref does at line 147: computeTbgrFromComponents(keyValue[1], keyValue[3], keyValue[2]).
namespace {
struct FixedKey { double value; int r; int g; int b; };

// Ported from: itwinjs-core Gradient.Symb._fixedSchemeKeys (lines 131-135)
// Order of entries matches ThematicGradientColorScheme enum order (BlueRed, RedBlue, Monochrome, Topographic, SeaMountain).
const std::vector<std::vector<FixedKey>> kFixedSchemeKeys = {
    { // BlueRed (index 0). Ref: [[0,0,255,0],[0.25,0,255,255],[0.5,0,0,255],[0.75,255,0,255],[1,255,0,0]]
        {0.0,    0,   0,   255},
        {0.25,   0,   255, 255},
        {0.5,    0,   0,   255},
        {0.75,   255, 0,   255},
        {1.0,    255, 0,   0},
    },
    { // RedBlue (index 1). Ref: [[0,255,0,0],[0.25,255,0,255],[0.5,0,0,255],[0.75,0,255,255],[1,0,255,0]]
        {0.0,    255, 0,   0},
        {0.25,   255, 0,   255},
        {0.5,    0,   0,   255},
        {0.75,   0,   255, 255},
        {1.0,    0,   255, 0},
    },
    { // Monochrome (index 2). Ref: [[0,0,0,0],[1,255,255,255]]
        {0.0, 0,   0,   0},
        {1.0, 255, 255, 255},
    },
    { // Topographic (index 3). Ref: [[0,152,148,188],[0.5,204,160,204],[1,152,72,128]]
        {0.0, 152, 148, 188},
        {0.5, 204, 160, 204},
        {1.0, 152, 72,  128},
    },
    { // SeaMountain (index 4). Ref: [[0,0,255,0],[0.2,72,96,160],...,[1,240,240,240]]
        {0.0, 0,   255, 0},
        {0.2, 72,  96,  160},
        {0.4, 152, 96,  160},
        {0.6, 128, 32,  104},
        {0.7, 148, 180, 128},
        {1.0, 240, 240, 240},
    },
};

// Ported from: itwinjs-core Gradient.Symb._fixedCustomKeys (line 137)
const std::vector<FixedKey> kFixedCustomKeys = {
    {0.0, 255, 0, 0},
    {1.0, 0,   255, 0},
};

// Ported from: itwinjs-core Gradient.Symb.roundToByte() (lines 249-251)
uint8_t RoundToByte(double num) noexcept
{
    const int val = static_cast<int>(std::min(num + 0.5, 255.0));
    return static_cast<uint8_t>(val & 0xFF);
}
}  // namespace

// ---------------------------------------------------------------------------
// fromJSON / toJSON
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core Gradient.Symb.fromJSON() (lines 114-127)
GradientSymb GradientSymb::fromJSON(const GradientSymbProps& props)
{
    GradientSymb result;
    result.mode = props.mode;
    result.flags = props.flags;
    result.angleDegrees = props.angleDegrees;
    result.tint = props.tint;
    result.shift = props.shift;
    for (const auto& key : props.keys)
        result.keys.emplace_back(key);
    if (props.thematicSettings) {
        result.thematicSettings = ThematicGradientSettings::fromJSON(&*props.thematicSettings);
    }
    return result;
}

// Ported from: itwinjs-core Gradient.Symb.toJSON() (lines 160-166)
GradientSymbProps GradientSymb::toJSON() const
{
    GradientSymbProps props;
    props.mode = mode;
    props.flags = flags;
    props.angleDegrees = angleDegrees;
    props.tint = tint;
    props.shift = shift;
    for (const auto& key : keys)
        props.keys.push_back({key.value, key.color.getTbgr()});
    if (thematicSettings) {
        props.thematicSettings = thematicSettings->toJSON();
    }
    return props;
}

// ---------------------------------------------------------------------------
// compareSymb
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core Gradient.Symb.compareSymb() (lines 182-236)
int GradientSymb::compare(const GradientSymb& lhs, const GradientSymb& rhs) noexcept
{
    if (&lhs == &rhs)
        return 0;
    if (lhs.mode != rhs.mode)
        return static_cast<int>(lhs.mode) - static_cast<int>(rhs.mode);
    if (lhs.flags != rhs.flags)
        return static_cast<int>(lhs.flags) - static_cast<int>(rhs.flags);
    if (lhs.tint.has_value() != rhs.tint.has_value())
        return lhs.tint.has_value() ? 1 : -1;
    if (lhs.tint.has_value() && lhs.tint.value() != rhs.tint.value())
        return lhs.tint.value() < rhs.tint.value() ? -1 : 1;
    if (lhs.shift != rhs.shift)
        return lhs.shift < rhs.shift ? -1 : 1;
    // angle: ref uses Angle.isAlmostEqualNoPeriodShift; DanQing uses flat degrees. Period-tolerance
    // for compareSymb is approximated by comparing degrees values directly (documented adaptation).
    if (lhs.angleDegrees != rhs.angleDegrees)
        return lhs.angleDegrees < rhs.angleDegrees ? -1 : 1;
    if (lhs.keys.size() != rhs.keys.size())
        return static_cast<int>(lhs.keys.size()) - static_cast<int>(rhs.keys.size());
    for (size_t i = 0; i < lhs.keys.size(); i++) {
        if (lhs.keys[i].value != rhs.keys[i].value)
            return lhs.keys[i].value < rhs.keys[i].value ? -1 : 1;
        if (!lhs.keys[i].color.equals(rhs.keys[i].color))
            return static_cast<int>(lhs.keys[i].color.getTbgr()) - static_cast<int>(rhs.keys[i].color.getTbgr());
    }
    // thematicSettings (ref lines 225-234)
    const bool lhsHas = lhs.thematicSettings.has_value();
    const bool rhsHas = rhs.thematicSettings.has_value();
    if (lhsHas != rhsHas)
        return lhsHas ? 1 : -1;
    if (lhsHas) {
        const int d = ThematicGradientSettings::compare(*lhs.thematicSettings, *rhs.thematicSettings);
        if (d != 0)
            return d;
    }
    return 0;
}

// ---------------------------------------------------------------------------
// mapColor
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core Gradient.Symb.mapColor() (lines 254-289)
ColorDef GradientSymb::mapColor(double value) const noexcept
{
    if (value < 0.0)
        value = 0.0;
    else if (value > 1.0)
        value = 1.0;

    if ((static_cast<uint8_t>(flags) & static_cast<uint8_t>(GradientFlags::Invert)) != 0)
        value = 1.0 - value;

    if (keys.empty())
        return ColorDef::black;
    if (keys.size() == 1)
        return keys[0].color;

    size_t idx = 0;
    double w0, w1;
    if (keys.size() <= 2) {
        w0 = 1.0 - value;
        w1 = value;
    } else {
        while (idx < keys.size() - 2 && value > keys[idx + 1].value)
            idx++;
        const double d = keys[idx + 1].value - keys[idx].value;
        w1 = d < 0.0001 ? 0.0 : (value - keys[idx].value) / d;
        w0 = 1.0 - w1;
    }

    const auto c0 = keys[idx].color.getColors();
    const auto c1 = keys[idx + 1].color.getColors();
    const auto red = RoundToByte(w0 * c0.r + w1 * c1.r);
    const auto green = RoundToByte(w0 * c0.g + w1 * c1.g);
    const auto blue = RoundToByte(w0 * c0.b + w1 * c1.b);
    const auto transparency = RoundToByte(w0 * c0.t + w1 * c1.t);
    return ColorDef::from(red, green, blue, transparency);
}

// Ported from: itwinjs-core Gradient.Symb.hasTranslucency (lines 291-298)
bool GradientSymb::hasTranslucency() const noexcept
{
    for (const auto& key : keys) {
        if (!key.color.isOpaque())
            return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
// createThematic
// ---------------------------------------------------------------------------

// Ported from: itwinjs-core Gradient.Symb.createThematic() (lines 140-158)
GradientSymb GradientSymb::createThematic(const ThematicGradientSettings& settings)
{
    GradientSymb result;
    result.mode = GradientMode::Thematic;
    result.thematicSettings = settings;

    if (static_cast<int>(settings.colorScheme) < static_cast<int>(ThematicGradientColorScheme::Custom)) {
        const auto& scheme = kFixedSchemeKeys[static_cast<size_t>(settings.colorScheme)];
        for (const auto& k : scheme) {
            // ref line 147: computeTbgrFromComponents(keyValue[1]=R, keyValue[3]=G, keyValue[2]=B)
            const ColorDefProps tbgr = ColorDef::computeTbgrFromComponents(k.r, k.b, k.g);
            result.keys.emplace_back(k.value, ColorDef::fromTbgr(tbgr));
        }
    } else {
        // Custom color scheme; must use customKeys (ref requires at least two).
        if (settings.customKeys.size() > 1) {
            for (const auto& keyColor : settings.customKeys)
                result.keys.push_back(keyColor);
        } else {
            // Revert to the basic fixed-custom key scheme.
            for (const auto& k : kFixedCustomKeys) {
                const ColorDefProps tbgr = ColorDef::computeTbgrFromComponents(k.r, k.b, k.g);
                result.keys.emplace_back(k.value, ColorDef::fromTbgr(tbgr));
            }
        }
    }
    return result;
}

// ---------------------------------------------------------------------------
// Image production
// ---------------------------------------------------------------------------

namespace {
// Write one RGBA pixel into the image buffer at *currentIdx (ref addColor closure, lines 320-325).
// The ref walks the buffer from the end (currentIdx starts at length-1, decrements per channel).
inline void AddColor(std::vector<uint8_t>& image, int& currentIdx, const ColorDef& color)
{
    const auto c = color.getColors();
    image[static_cast<size_t>(currentIdx)] = static_cast<uint8_t>(color.getAlpha()); --currentIdx;
    image[static_cast<size_t>(currentIdx)] = c.b; --currentIdx;
    image[static_cast<size_t>(currentIdx)] = c.g; --currentIdx;
    image[static_cast<size_t>(currentIdx)] = c.r; --currentIdx;
}
}  // namespace

// Ported from: itwinjs-core Gradient.Symb.getThematicImageForRenderer() (lines 307-359)
std::optional<ImageBuffer> GradientSymb::getThematicImageForRenderer(int maxDimension) const
{
    ThematicGradientSettings settings = thematicSettings ? *thematicSettings : ThematicGradientSettings::defaults();

    const int stepCount = std::min(settings.stepCount, maxDimension);
    const int dimension = (settings.mode == ThematicGradientMode::Smooth) ? maxDimension : stepCount;
    if (dimension <= 0)
        return std::nullopt;

    std::vector<uint8_t> image(static_cast<size_t>(1) * static_cast<size_t>(dimension) * 4, 0);
    int currentIdx = static_cast<int>(image.size()) - 1;

    switch (settings.mode) {
        case ThematicGradientMode::Smooth: {
            for (int j = 0; j < dimension; ++j) {
                const double f = 1.0 - static_cast<double>(j) / dimension;
                AddColor(image, currentIdx, mapColor(f));
            }
            break;
        }
        case ThematicGradientMode::SteppedWithDelimiter:
        case ThematicGradientMode::IsoLines:
        case ThematicGradientMode::Stepped: {
            // stepCount must be >= 2 for the renderer gradient; we tolerate gracefully otherwise.
            for (int j = 0; j < dimension; ++j) {
                const double f = (dimension > 1)
                    ? (1.0 - static_cast<double>(j) / (dimension - 1))
                    : 1.0;
                AddColor(image, currentIdx, mapColor(f));
            }
            break;
        }
    }

    return ImageBuffer::create(std::move(image), ImageBufferFormat::Rgba, 1);
}

// Ported from: itwinjs-core Gradient.Symb.getImage() (lines 367-372)
std::optional<ImageBuffer> GradientSymb::getImage(int width, int height) const
{
    if (mode == GradientMode::Thematic)
        width = 1;
    ProduceImageArgs args;
    args.width = width;
    args.height = height;
    args.includeThematicMargin = true;
    return produceImage(args);
}

// Ported from: itwinjs-core Gradient.Symb.produceImage() (lines 375-525)
#pragma warning(push)
#pragma warning(disable : 4458) // 局部变量 shift 与参考 1:1，遮蔽同名成员仅 MSVC /W4 提示
std::optional<ImageBuffer> GradientSymb::produceImage(const ProduceImageArgs& args) const
{
    const int width = args.width;
    const int height = args.height;
    const bool includeThematicMargin = args.includeThematicMargin;

    if (width <= 0 || height <= 0)
        return std::nullopt;

    const double thisAngle = angleDegrees * (std::acos(-1.0) / 180.0);  // degrees → radians
    const double cosA = std::cos(thisAngle);
    const double sinA = std::sin(thisAngle);
    std::vector<uint8_t> image(static_cast<size_t>(width) * static_cast<size_t>(height) * 4, 0);
    int currentIdx = static_cast<int>(image.size()) - 1;
    const double shift = std::min(1.0, std::abs(this->shift));

    auto putColor = [&image, &currentIdx](const ColorDef& color) {
        AddColor(image, currentIdx, color);
    };

    switch (mode) {
        case GradientMode::Linear:
        case GradientMode::Cylindrical: {
            const double xs = 0.5 - 0.25 * shift * cosA;
            const double ys = 0.5 - 0.25 * shift * sinA;
            double dMax = 0.0, dMin = 0.0;
            for (int j = 0; j < 2; ++j) {
                for (int i = 0; i < 2; ++i) {
                    const double d = (i - xs) * cosA + (j - ys) * sinA;
                    if (d < dMin) dMin = d;
                    if (d > dMax) dMax = d;
                }
            }
            for (int j = 0; j < height; ++j) {
                const double y = static_cast<double>(j) / height - ys;
                for (int i = 0; i < width; ++i) {
                    const double x = static_cast<double>(i) / width - xs;
                    const double d = x * cosA + y * sinA;
                    double f;
                    if (mode == GradientMode::Linear) {
                        if (d > 0) f = 0.5 + 0.5 * d / dMax;
                        else       f = 0.5 - 0.5 * d / dMin;
                    } else {
                        if (d > 0) f = std::sin(std::acos(-1.0) / 2 * (1.0 - d / dMax));
                        else       f = std::sin(std::acos(-1.0) / 2 * (1.0 - d / dMin));
                    }
                    putColor(mapColor(f));
                }
            }
            break;
        }
        case GradientMode::Curved: {
            const double xs = 0.5 + 0.5 * sinA - 0.25 * shift * cosA;
            const double ys = 0.5 - 0.5 * cosA - 0.25 * shift * sinA;
            for (int j = 0; j < height; ++j) {
                const double y = static_cast<double>(j) / height - ys;
                for (int i = 0; i < width; ++i) {
                    const double x = static_cast<double>(i) / width - xs;
                    const double xr = 0.8 * (x * cosA + y * sinA);
                    const double yr = y * cosA - x * sinA;
                    const double f = std::sin(std::acos(-1.0) / 2 * (1.0 - std::sqrt(xr * xr + yr * yr)));
                    putColor(mapColor(f));
                }
            }
            break;
        }
        case GradientMode::Spherical: {
            const double r = 0.5 + 0.125 * std::sin(2.0 * thisAngle);
            const double xs = 0.5 * shift * (cosA + sinA) * r;
            const double ys = 0.5 * shift * (sinA - cosA) * r;
            for (int j = 0; j < height; ++j) {
                const double y = ys + static_cast<double>(j) / height - 0.5;
                for (int i = 0; i < width; ++i) {
                    const double x = xs + static_cast<double>(i) / width - 0.5;
                    const double f = std::sin(std::acos(-1.0) / 2 * (1.0 - std::sqrt(x * x + y * y) / r));
                    putColor(mapColor(f));
                }
            }
            break;
        }
        case GradientMode::Hemispherical: {
            const double xs = 0.5 + 0.5 * sinA - 0.5 * shift * cosA;
            const double ys = 0.5 - 0.5 * cosA - 0.5 * shift * sinA;
            for (int j = 0; j < height; ++j) {
                const double y = static_cast<double>(j) / height - ys;
                for (int i = 0; i < width; ++i) {
                    const double x = static_cast<double>(i) / width - xs;
                    const double f = std::sin(std::acos(-1.0) / 2 * (1.0 - std::sqrt(x * x + y * y)));
                    putColor(mapColor(f));
                }
            }
            break;
        }
        case GradientMode::Thematic: {
            const ThematicGradientSettings settings = thematicSettings ? *thematicSettings : ThematicGradientSettings::defaults();
            for (int j = 0; j < height; ++j) {
                double f = 1.0 - static_cast<double>(j) / height;
                ColorDef color = ColorDef::black;
                if (includeThematicMargin && (f < ThematicGradientSettings::margin() || f > ThematicGradientSettings::contentMax())) {
                    color = settings.marginColor;
                } else {
                    f = (f - ThematicGradientSettings::margin()) / ThematicGradientSettings::contentRange();
                    switch (settings.mode) {
                        case ThematicGradientMode::SteppedWithDelimiter:
                        case ThematicGradientMode::IsoLines:
                        case ThematicGradientMode::Stepped: {
                            if (settings.stepCount > 1) {
                                const double fStep = std::floor(f * settings.stepCount - 0.00001) / (settings.stepCount - 1);
                                color = mapColor(fStep);
                            } else {
                                // ref throws; DanQing is -fno-exceptions, so fall back to a smooth sample.
                                color = mapColor(f);
                            }
                            break;
                        }
                        case ThematicGradientMode::Smooth:
                        default:
                            color = mapColor(f);
                            break;
                    }
                }
                for (int i = 0; i < width; ++i) {
                    putColor(color);
                }
            }
            break;
        }
        default:
            break;
    }

    return ImageBuffer::create(std::move(image), ImageBufferFormat::Rgba, width);
}
#pragma warning(pop)

END_DQ_COMMON_NAMESPACE
