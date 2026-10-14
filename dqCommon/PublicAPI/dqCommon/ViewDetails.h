// SPDX-License-Identifier: Apache-2.0
// DanQing dqCommon — ViewDetails（子集：maxSkew 静态家园）
//
// Ported from: itwinjs-core core/common/src/ViewDetails.ts
//
// 子集声明：参考 ViewDetails 是 core-common 的整类（viewFlags/clipVector/
// aspectSkew/grid/orientation 等 JSON 承载 + 访问器族）。DanQing 的既有承载面
// 按"就地"模式散落于 dqApp ViewState（clipVector 双态 M-P P-B 先例、
// ViewDetailsProps 结构 ViewStateProps.h:126）——本头只承载**静态成员**
// maxSkew（AspectRatioSkew 消费面，M-W W-p1），为 `ViewDetails.maxSkew` 参考
// 引用面提供 1:1 命名家园。其余 ViewDetails 面随消费者出现再扩（TODO 锚：
// core/common/src/ViewDetails.ts 全文）。
#pragma once

namespace dqCommon {

// Ported from: itwinjs-core ViewDetails（core/common/src/ViewDetails.ts）。
class ViewDetails {
public:
    /// Maximum aspect ratio skew. Apps can override this by changing its value.
    /// （ViewDetails.ts:95——TS `static maxSkew = 25` 可变静态的 C++ 承载。）
    inline static double maxSkew = 25.0;
};

} // namespace dqCommon
