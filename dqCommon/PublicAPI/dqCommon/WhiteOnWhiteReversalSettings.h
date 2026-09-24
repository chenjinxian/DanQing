// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — White-on-white reversal settings
// Ported from: itwinjs-core core/common/src/WhiteOnWhiteReversalSettings.ts
#pragma once

#include "Export.h"
#include "DqCommon.h"

#include <cstdint>

BEGIN_DQ_COMMON_NAMESPACE

// JSON persistence.
struct WhiteOnWhiteReversalProps {
    bool ignoreBackgroundColor = false;
};

// Controls white-on-white reversal behavior.
// Only two singleton instances exist (ignore / no-ignore).
// Ported from: itwinjs-core WhiteOnWhiteReversalSettings
class DQ_COMMON_EXPORT WhiteOnWhiteReversalSettings {
public:
    bool ignoreBackgroundColor;

    static const WhiteOnWhiteReversalSettings& defaults() noexcept;
    static const WhiteOnWhiteReversalSettings& fromJSON(const WhiteOnWhiteReversalProps* props = nullptr);

    WhiteOnWhiteReversalProps toJSON() const;
    bool equals(const WhiteOnWhiteReversalSettings& rhs) const noexcept { return this == &rhs; }

private:
    explicit WhiteOnWhiteReversalSettings(bool ignore) : ignoreBackgroundColor(ignore) {}
};

inline const WhiteOnWhiteReversalSettings& WhiteOnWhiteReversalSettings::defaults() noexcept
{
    static const WhiteOnWhiteReversalSettings s_default(false);
    return s_default;
}

inline const WhiteOnWhiteReversalSettings& WhiteOnWhiteReversalSettings::fromJSON(const WhiteOnWhiteReversalProps* props)
{
    static const WhiteOnWhiteReversalSettings s_ignore(true);
    static const WhiteOnWhiteReversalSettings s_noIgnore(false);
    return (props && props->ignoreBackgroundColor) ? s_ignore : s_noIgnore;
}

inline WhiteOnWhiteReversalProps WhiteOnWhiteReversalSettings::toJSON() const
{
    WhiteOnWhiteReversalProps props;
    props.ignoreBackgroundColor = ignoreBackgroundColor;
    return props;
}

END_DQ_COMMON_NAMESPACE
