// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — Tool Assistance instruction model
//
// Ported from: itwinjs-core core/frontend/src/tools/ToolAssistance.ts
//
// 移植面 = ViewTool provideToolAssistance 族（ViewTool.ts:630-657/:3211-3234/
// :3563-3582）的消费面：ToolAssistanceImage / ToolAssistanceInputMethod /
// ToolAssistanceInstruction / ToolAssistanceSection / ToolAssistanceInstructions
// + createInstruction/createSection/createInstructions 工厂 + inputsLabel。
// 未移植（无消费者——随对应工具面落地时补）：keyboard-info 辅助族
//（altKey/ctrlKey/shiftKey/方向符号等 :141-231）、createTouchCursorInstructions
//（:296-307——accuSnap touchCursor 面）、translatePrompt/translateInput
//（:160-167——DanQing 无 localization 系统，提示文本经 ViewTool::translate
// 的 en 内嵌表解析，见 ViewTool.cpp EQUIVALENCE 登记）。
//
// 消费链：工具 → NotificationManager::setToolAssistance（事件扇出）→ 宿主
//（DisplayTestApp DtaTools → 状态栏 InputHints——M-O(2) 3i 接线）。
#pragma once

#include "Export.h"

#include <optional>
#include <string>
#include <vector>

namespace dqApp {

// ---------------------------------------------------------------------------
// ToolAssistanceImage — Ported from: itwinjs-core ToolAssistanceImage
//（ToolAssistance.ts:17-52——枚举序 1:1）。
// ---------------------------------------------------------------------------
enum class ToolAssistanceImage : uint8_t {
    Keyboard,          // :18（keyboardInfo 应携带）
    AcceptPoint,       // :20
    CursorClick,       // :22
    LeftClick,         // :24
    RightClick,        // :26
    MouseWheel,        // :28
    LeftClickDrag,     // :30
    RightClickDrag,    // :32
    MouseWheelClickDrag,  // :34
    OneTouchTap,       // :36
    OneTouchDoubleTap,  // :38
    OneTouchDrag,      // :40
    TwoTouchTap,       // :42
    TwoTouchDrag,      // :44
    TwoTouchPinch,     // :46
    TouchCursorTap,    // :48
    TouchCursorDrag,   // :50
};

// ---------------------------------------------------------------------------
// ToolAssistanceInputMethod — Ported from: itwinjs-core
// ToolAssistanceInputMethod（ToolAssistance.ts:58-65）。
// ---------------------------------------------------------------------------
enum class ToolAssistanceInputMethod : uint8_t {
    Both,   // 两种输入法皆适用
    Mouse,  // 仅鼠标
    Touch,  // 仅触摸
};

// ---------------------------------------------------------------------------
// ToolAssistanceKeyboardInfo — Ported from: itwinjs-core
// ToolAssistanceKeyboardInfo（ToolAssistance.ts:87-92）。
// ---------------------------------------------------------------------------
struct ToolAssistanceKeyboardInfo {
    std::vector<std::string> keys;                    // ← keys
    std::optional<std::vector<std::string>> bottomKeys;  // ← bottomKeys?
};

// ---------------------------------------------------------------------------
// ToolAssistanceInstruction — Ported from: itwinjs-core
// ToolAssistanceInstruction（ToolAssistance.ts:98-111）。
// 参考 image: string | ToolAssistanceImage 联合类型（§3.4）——C++ 双字段承载：
// imageSpec 非空 = 字符串形（WebFont/SVG 图标名，如 ViewTool iconSpec）；
// 否则用枚举形（键帽图——宿主映射为输入序列）。
// ---------------------------------------------------------------------------
struct ToolAssistanceInstruction {
    std::string imageSpec;                          // ← image（string 形）
    ToolAssistanceImage image = ToolAssistanceImage::Keyboard;  // ← image（枚举形）
    std::string text;                               // ← text
    std::optional<ToolAssistanceKeyboardInfo> keyboardInfo;  // ← keyboardInfo?
    bool isNew = false;                             // ← isNew?
    ToolAssistanceInputMethod inputMethod = ToolAssistanceInputMethod::Both;  // ← inputMethod?
};

// ---------------------------------------------------------------------------
// ToolAssistanceSection — Ported from: itwinjs-core ToolAssistanceSection
//（ToolAssistance.ts:117-122）。
// ---------------------------------------------------------------------------
struct ToolAssistanceSection {
    std::vector<ToolAssistanceInstruction> instructions;  // ← instructions
    std::optional<std::string> label;                      // ← label?
};

// ---------------------------------------------------------------------------
// ToolAssistanceInstructions — Ported from: itwinjs-core
// ToolAssistanceInstructions（ToolAssistance.ts:128-133）。
// ---------------------------------------------------------------------------
struct ToolAssistanceInstructions {
    ToolAssistanceInstruction mainInstruction;              // ← mainInstruction
    std::optional<std::vector<ToolAssistanceSection>> sections;  // ← sections?
};

// ---------------------------------------------------------------------------
// ToolAssistance — Ported from: itwinjs-core ToolAssistance
//（ToolAssistance.ts:139-328——消费面工厂 + inputsLabel）。
// ---------------------------------------------------------------------------
class DQ_APP_EXPORT ToolAssistance {
public:
    // Up/Down/Left/Right key symbols（:142-148——⯅..⯈ 逐字）。
    static std::string const& upSymbol() noexcept
    {
        static std::string const k = "\xe2\xaf\x85";
        return k;
    }
    static std::string const& downSymbol() noexcept
    {
        static std::string const k = "\xe2\xaf\x86";
        return k;
    }
    static std::string const& leftSymbol() noexcept
    {
        static std::string const k = "\xe2\xaf\x87";
        return k;
    }
    static std::string const& rightSymbol() noexcept
    {
        static std::string const k = "\xe2\xaf\x88";
        return k;
    }

    // Keyboard info for Arrow keys（:151-154）。
    static ToolAssistanceKeyboardInfo arrowKeyboardInfo()
    {
        ToolAssistanceKeyboardInfo info;
        info.keys = {upSymbol()};
        info.bottomKeys = {leftSymbol(), downSymbol(), rightSymbol()};
        return info;
    }

    // Alt/Ctrl/Shift key text（:170-182——translateKey；en 实值
    // CoreTools.json toolAssistance.altKey/ctrlKey/shiftKey）。
    static std::string const& altKey() noexcept
    {
        static std::string const k = "Alt";
        return k;
    }
    static std::string const& ctrlKey() noexcept
    {
        static std::string const k = "Ctrl";
        return k;
    }
    static std::string const& shiftKey() noexcept
    {
        static std::string const k = "Shift";
        return k;
    }

    // ← inputsLabel（:185-187——translateKey("inputs")；DanQing 无 localization
    // 系统，en 值 "Inputs" 内嵌——CoreTools.json toolAssistance.inputs）。
    // EQUIVALENCE: 参考源=ToolAssistance.ts:156 translateKey 经
    // IModelApp.localization；发散=DanQing 恒 en（无 locale 面）；验证法=
    // ViewToolTest ToolAssistance 断言段 label 值。
    static std::string const& inputsLabel() noexcept
    {
        static std::string const kInputs = "Inputs";
        return kInputs;
    }

    // Ported from: itwinjs-core ToolAssistance.createInstruction（ToolAssistance
    // .ts:235-247——image 枚举形；inputMethod 缺省 Both :236-237）。
    static ToolAssistanceInstruction createInstruction(
        ToolAssistanceImage image, std::string const& text, bool isNew = false,
        ToolAssistanceInputMethod inputMethod = ToolAssistanceInputMethod::Both,
        ToolAssistanceKeyboardInfo keyboardInfo = {})
    {
        ToolAssistanceInstruction instruction;
        instruction.image = image;
        instruction.text = text;
        instruction.keyboardInfo = std::move(keyboardInfo);
        instruction.isNew = isNew;
        instruction.inputMethod = inputMethod;
        return instruction;
    }

    // ← createInstruction 的 image 字符串形重载（参考联合类型的另一支——
    // ViewTool.ts:631 `createInstruction(this.iconSpec, ...)` 即此形态）。
    static ToolAssistanceInstruction createInstruction(
        std::string const& imageSpec, std::string const& text, bool isNew = false,
        ToolAssistanceInputMethod inputMethod = ToolAssistanceInputMethod::Both)
    {
        ToolAssistanceInstruction instruction;
        instruction.imageSpec = imageSpec;
        instruction.text = text;
        instruction.isNew = isNew;
        instruction.inputMethod = inputMethod;
        return instruction;
    }

    // Ported from: ToolAssistance.createKeyboardInstruction（:251-263——
    // image=Keyboard + keyboardInfo；inputMethod 缺省 Mouse :253。
    // M-O(3) P1：Walk/Fly/LookAndMove 键盘指令段消费面）。
    static ToolAssistanceInstruction createKeyboardInstruction(
        ToolAssistanceKeyboardInfo keyboardInfo, std::string const& text,
        bool isNew = false,
        ToolAssistanceInputMethod inputMethod = ToolAssistanceInputMethod::Mouse)
    {
        ToolAssistanceInstruction instruction;
        instruction.image = ToolAssistanceImage::Keyboard;
        instruction.text = text;
        instruction.keyboardInfo = std::move(keyboardInfo);
        instruction.isNew = isNew;
        instruction.inputMethod = inputMethod;
        return instruction;
    }

    // Ported from: ToolAssistance.createModifierKeyInstruction（:267-282——
    // keyboardInfo={keys:[modifierKey]} + image 形。M-O(3) P1：Walk/Fly 的
    // shift/ctrl flyover 段消费面）。
    static ToolAssistanceInstruction createModifierKeyInstruction(
        std::string const& modifierKey, ToolAssistanceImage image,
        std::string const& text, bool isNew = false,
        ToolAssistanceInputMethod inputMethod = ToolAssistanceInputMethod::Both)
    {
        ToolAssistanceInstruction instruction;
        instruction.image = image;
        instruction.text = text;
        ToolAssistanceKeyboardInfo info;
        info.keys = {modifierKey};
        instruction.keyboardInfo = std::move(info);
        instruction.isNew = isNew;
        instruction.inputMethod = inputMethod;
        return instruction;
    }

    // Ported from: ToolAssistance.createKeyboardInfo（:286-292）。
    static ToolAssistanceKeyboardInfo createKeyboardInfo(
        std::vector<std::string> keys,
        std::optional<std::vector<std::string>> bottomKeys = std::nullopt)
    {
        return ToolAssistanceKeyboardInfo{std::move(keys), std::move(bottomKeys)};
    }

    // Ported from: itwinjs-core ToolAssistance.createSection（ToolAssistance
    // .ts:311-317）。
    static ToolAssistanceSection createSection(
        std::vector<ToolAssistanceInstruction> instructions,
        std::optional<std::string> label = std::nullopt)
    {
        ToolAssistanceSection section;
        section.instructions = std::move(instructions);
        section.label = std::move(label);
        return section;
    }

    // Ported from: itwinjs-core ToolAssistance.createInstructions（ToolAssistance
    // .ts:321-327）。
    static ToolAssistanceInstructions createInstructions(
        ToolAssistanceInstruction mainInstruction,
        std::optional<std::vector<ToolAssistanceSection>> sections = std::nullopt)
    {
        ToolAssistanceInstructions instructions;
        instructions.mainInstruction = std::move(mainInstruction);
        instructions.sections = std::move(sections);
        return instructions;
    }
};

}  // namespace dqApp
