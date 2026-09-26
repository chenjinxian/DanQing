// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — iModel tile tree implementation
// Ported from: itwinjs-core core/common/src/tile/TileMetadata.ts
//              (computeChildTileProps :777-853, ContentIdProvider V1
//              :665-674, bisectTileRange3d/2d :724-743)
//              core/frontend/src/internal/tile/IModelTile.ts (:153-169
//              _loadChildren; :90-131 request/read content)
#include "dqRender/tile/ImdlTileTree.h"

#include <dqCommon/PackedFeatureTable.h>

#include "dqRender/RenderGraphic.h"
#include "dqRender/RenderSystem.h"
#include "dqRender/tile/ImdlHeader.h"
#include "dqRender/tile/TileFormat.h"

#include "dqRender/tile/ImdlDocument.h"
#include "dqRender/tile/ITileFetcher.h"
#include "dqRender/tile/TileAdmin.h"

#include <cstdlib>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

BEGIN_DQ_RENDER_NAMESPACE

namespace {

// Bisect a range along one axis. Ported from: bisectTileRange3d/2d
// (TileMetadata.ts:724-743 — the axis' low/high midpoint split, keep half).
void bisectRange(dqGeom::Range3d& range, int axis, bool keepLow)
{
    double* lo = &range.low.x;
    double* hi = &range.high.x;
    double const mid = 0.5 * (lo[axis] + hi[axis]);
    if (keepLow)
        hi[axis] = mid;
    else
        lo[axis] = mid;
}

}  // namespace

ImdlContentIdSpec parseImdlContentId(std::string const& id)
{
    // V1: "<depth>/<i>/<j>/<k>" (+"[/<mult>]" for magnification,
    // TileMetadata.ts:665-674).
    ImdlContentIdSpec spec;
    std::istringstream ss(id);
    std::string token;
    uint32_t values[5] = {0, 0, 0, 0, 0};
    int n = 0;
    while (n < 5 && std::getline(ss, token, '/')) {
        values[n++] = static_cast<uint32_t>(std::strtoul(token.c_str(), nullptr, 10));
    }
    if (n >= 4) {
        spec.depth = values[0];
        spec.i = values[1];
        spec.j = values[2];
        spec.k = values[3];
        if (n >= 5)
            spec.mult = values[4];
    }
    return spec;
}

std::string formatImdlContentId(ImdlContentIdSpec const& spec)
{
    std::ostringstream ss;
    ss << spec.depth << '/' << spec.i << '/' << spec.j << '/' << spec.k;
    if (spec.mult != 0)
        ss << '/' << spec.mult;
    return ss.str();
}

std::vector<ImdlChildTileProps> computeImdlChildTileProps(
    ImdlTileMetadata const& parent, ImdlTreeMetadata const& root)
{
    // Ported from: computeChildTileProps (TileMetadata.ts:777-853).
    std::vector<ImdlChildTileProps> children;
    if (parent.isLeaf)
        return children;

    // Magnification: one child, same volume, doubled multiplier (:785-799).
    if (parent.sizeMultiplier > 0.0) {
        double const multiplier = parent.sizeMultiplier * 2.0;
        ImdlContentIdSpec spec = parseImdlContentId(parent.contentId);
        spec.mult = static_cast<uint32_t>(multiplier);
        ImdlChildTileProps child;
        child.contentId = formatImdlContentId(spec);
        child.range = parent.range;
        child.sizeMultiplier = multiplier;
        child.isLeaf = false;
        child.maximumSize = static_cast<double>(root.tileScreenSize);  // :795
        children.push_back(std::move(child));
        return children;
    }

    // Sub-divide into 4 (2d) or 8 (3d) children (:801-848).
    ImdlContentIdSpec parentSpec = parseImdlContentId(parent.contentId);
    uint32_t const emptyMask = parent.emptySubRangeMask;

    // Model-range rejection (:816-820): only test children when the parent
    // is not wholly inside the model range.
    bool testContentRange = !root.contentRange.isNull()
                            && !root.contentRange.ContainsRange(parent.range);

    for (uint32_t i = 0; i < 2; ++i) {
        for (uint32_t j = 0; j < 2; ++j) {
            for (uint32_t k = 0; k < (root.is2d ? 1u : 2u); ++k) {
                uint32_t const emptyBit = 1u << (i + j * 2 + k * 4);
                if (0 != (emptyMask & emptyBit))
                    continue;  // known-empty sub-volume (:826-830)

                dqGeom::Range3d range = parent.range;
                bisectRange(range, 0, 0 == i);
                bisectRange(range, 1, 0 == j);
                if (!root.is2d)
                    bisectRange(range, 2, 0 == k);

                if (testContentRange && !range.IntersectsRange(root.contentRange))
                    continue;  // outside model range (:836-840)

                ImdlContentIdSpec childSpec = parentSpec;
                childSpec.depth = parentSpec.depth + 1;
                childSpec.i = parentSpec.i * 2 + i;
                childSpec.j = parentSpec.j * 2 + j;
                childSpec.k = parentSpec.k * 2 + k;

                ImdlChildTileProps child;
                child.contentId = formatImdlContentId(childSpec);
                child.range = range;
                child.maximumSize = static_cast<double>(root.tileScreenSize);  // :847
                children.push_back(std::move(child));
            }
        }
    }

    return children;
}

// ---------------------------------------------------------------------------
// ImdlTile
// ---------------------------------------------------------------------------

ImdlTile::ImdlTile(ImdlTileTree& tree, Tile* parent,
                   std::string contentId, dqGeom::Range3d const& range,
                   double sizeMultiplier, double maximumSize)
    : Tile(tree, parent, range, parent ? parent->getDepth() + 1 : 0, maximumSize)
    , m_contentId(std::move(contentId))
    , m_sizeMultiplier(sizeMultiplier)
{
    if (m_contentId.empty())
        setIsLeaf(true);
}

ImdlTileTree& ImdlTile::iModelTree() const noexcept
{
    // Ported from: IModelTile.iModelTree (IModelTile.ts:76 —
    // `return this.tree as IModelTileTree`).
    return static_cast<ImdlTileTree&>(getTree());
}

bool ImdlTile::requestContent()
{
    // Content acquisition: the reference routes through TileAdmin.
    // generateTileContent → RPC/cache (TileAdmin.ts:684-702); DanQing composes
    // a treeId/contentId URL for the injected fetcher (the production seam).
    if (m_contentId.empty())
        return false;

    auto& tree = static_cast<ImdlTileTree&>(getTree());
    std::string const url = tree.contentUrl(m_contentId);
    auto& fetcher = TileAdmin::instance().getFetcher();
    fetcher.fetch(
        url, *this,
        [](Tile& tile, std::vector<uint8_t> const& data) {
            TileAdmin::instance().deliverTileContent(tile, data);
        },
        [](Tile& tile, std::string const& error) {
            TileAdmin::instance().reportTileFetchError(tile, error);
        });
    return true;
}

TileContent ImdlTile::readContent(uint8_t const* data, size_t dataSize)
{
    // Ported from: IModelTile.readContent (IModelTile.ts:90-131) + ImdlReader
    // (.ts:104-123 — decode → convertFeatureTable → createBatch). DanQing
    // synchronous subset: no worker/async; rtcCenter not consumed this pass
    // (the offline fixtures carry none — JSON scene has no rtcCenter field;
    // TODO: rtcCenter branch, ImdlReader.ts:126-130).
    TileContent content;
    if (!data || dataSize < 4)
        return content;
    if (DetectTileFormat(data, dataSize) != TileFormat::IModel)
        return content;

    ImdlByteStream stream(data, dataSize);
    auto const header = ImdlHeader::readFrom(stream);
    if (!header.isValid())
        return content;

    // Feature table: read the 12-byte header + the packed words (3×u32/feature
    // + 2×u32/subcategory tail) instead of skipping them — the stream ends up
    // at the same position as the reference's skip-then-seek-back.
    // Ported from: ParseImdlDocument.ts:1278-1284 (ftStartPos +
    // FeatureTableHeader.readFrom + seek to ftStartPos + length) +
    // FeatureTableHeader.ts layout (length includes the 12-byte header).
    ImdlFeatureTableHeader ftHeader;
    if (!ImdlFeatureTableHeader::readFrom(stream, ftHeader))
        return content;  // InvalidFeatureTable (ParseImdlDocument.ts:1281-1282)
    std::vector<uint32_t> featureWords;
    size_t const ftBodyBytes = ftHeader.length >= ImdlFeatureTableHeader::sizeInBytes
        ? ftHeader.length - ImdlFeatureTableHeader::sizeInBytes : 0;
    if (ftBodyBytes > 0) {
        featureWords.resize(ftBodyBytes / sizeof(uint32_t));
        stream.readBytes(featureWords.data(), featureWords.size() * sizeof(uint32_t));
        if (stream.isPastTheEnd())
            return content;
    }

    // Header-only description (the caller consumed the feature table — the
    // leaf heuristic derives purely from the imdl header).
    auto const desc = decodeImdlContentDescriptionHeaderOnly(header);
    if (!desc.has_value())
        return content;
    content.contentRange = desc->contentRange;
    content.isLeaf = desc->isLeaf;

    auto doc = parseImdlDocument(stream, &ftHeader, &featureWords);
    if (!doc.has_value())
        return content;

    RenderSystem* system = getTree().getRenderSystem();
    if (!system)
        return content;

    // imdl 顶点消费主路径：LUT 直传（U7 归位，createImdlLutGraphics）。
    // EQUIVALENCE: 参考源=itwinjs-core VertexLUT.ts:93-99（线上顶点表直传纹理）+
    //   Vertex.ts computeVertexPosition（shader 侧 f32 解量化）。发散=旧路径 CPU
    //   f64 解量化后截 f32，新路径 shader f32 解量化，量化域内差异 ≤1ulp、
    //   屏幕像素不可辨；验证法=TileTreeRender 8 项像素锁全绿 +
    //   LutPathUploadsVertexTableVerbatim 的字节级直传断言。
    std::vector<RenderGraphic*> graphics;
    if (system->driver()) {
        graphics = createImdlLutGraphics(*doc, *system);
    } else {
        // 无 GL 桩系统（dqRenderTest 的 ReadContentStubSystem）回退：polyface
        // 形态保留为对照/调试通道（旧 decodeImdlGraphics——ImdlGraphics 既有
        // 测试亦直接测它）。
        auto meshes = decodeImdlGraphics(*doc);
        for (auto& polyface : meshes) {
            if (auto* graphic = system->createGraphicFromPolyface(polyface.Get(), 0xFFFFFFFFu, 0))
                graphics.push_back(graphic);
        }
    }
    if (graphics.empty())
        return content;
    RenderGraphic* list = system->createGraphicList(std::move(graphics));

    // convertFeatureTable (ParseImdlDocument.ts:1259-1266): on-the-wire words
    // → PackedFeatureTable (DanQing single-model subset: BatchType::Primary,
    // modelId 0 — offline tilesets carry no model semantics).
    // TODO(deferred): MultiModelPackedFeatureTable — when
    // ImdlFlags::MultiModelFeatureTable is set the reference builds a
    // multi-model table; until then such tiles fall through to the unbatched
    // path below (registered, mirrors the reference's branch at
    // ParseImdlDocument.ts:1260-1262).
    bool const multiModel =
        0 != (static_cast<uint32_t>(header.flags)
              & static_cast<uint32_t>(ImdlFlags::MultiModelFeatureTable));
    if (!multiModel && doc->featureCount > 0 && !doc->featureData.empty()) {
        dqCommon::PackedFeatureTable packed(doc->featureData, /*modelId*/ 0,
                                            doc->featureCount,
                                            dqCommon::BatchType::Primary);
        content.featureTable =
            std::make_unique<dqCommon::FeatureTable>(packed.unpack());
        // ImdlReader.ts:122-123: graphic = system.createBatch(graphic,
        // featureTable, content.contentRange).
        content.graphic.reset(system->createBatch(list, content.featureTable.get(),
                                                  content.contentRange));
    } else {
        content.graphic.reset(list);
    }

    // maximumSize 回填：内容到达且从未获得 maximumSize（0 = undisplayable）
    // 时取树的 tileScreenSize。
    // Ported from: IModelTile.setContent (IModelTile.ts:140-142):
    //   if (undefined !== content.graphic && 0 === this.maximumSize)
    //     this._maximumSize = this.iModelTree.tileScreenSize;
    // DanQing adaptation: the backfill lives here (readContent) — TileAdmin::
    // deliverTileContent runs readContent immediately before setContent and
    // setContent is non-virtual, so the reference's IModelTile.setContent
    // override has no C++ dispatch path; the observable state at the point of
    // consumption is the same.
    if (content.graphic && 0.0 == getMaximumSize()) {
        auto& imdlTree = static_cast<ImdlTileTree&>(getTree());
        m_maximumSize = static_cast<double>(imdlTree.metadata().tileScreenSize);
    }
    return content;
}

void ImdlTile::loadChildren()
{
    // 重入门（评审回合 1 Critical 修复）：children 已加载（或已定形为 leaf）
    // 即短路。参考 :282 对 loadChildren 的无条件调用以基类重入门为前提——
    // Ported from: Tile.loadChildren (Tile.ts:353-356,
    // `if (this._childrenLoadStatus !== TileTreeLoadStatus.NotLoaded)
    // return this._childrenLoadStatus;`)。DanQing 无 _childrenLoadStatus 态
    // 机，折叠为两态：Loaded-有子（hasLoadedChildren）/ Loaded-空（loadChildren
    // 空结果置 leaf，Tile.ts:361-364 同型）；Loading 态同步执行不可达；
    // NotFound 子代态 DanQing 无表示（登记）。无此门时每选择趟重算子代并
    // setChildren 替换——销毁已加载 graphic、LRU onTileContentDisposed 震荡、
    // 在途请求的 Tile* 悬空（RepeatedSelectionKeepsChildIdentity 锁）。
    if (isLeaf() || hasLoadedChildren())
        return;

    // Ported from: IModelTile._loadChildren (IModelTile.ts:153-169) — pure
    // frontend computation, no request.
    auto& tree = static_cast<ImdlTileTree&>(getTree());
    ImdlTileMetadata parent;
    parent.contentId = m_contentId;
    parent.range = getRange();
    parent.contentRange = getRange();
    parent.isLeaf = isLeaf();
    parent.sizeMultiplier = m_sizeMultiplier;

    auto children = tree.childPropsFor(parent);
    if (children.empty()) {
        setIsLeaf(true);
        return;
    }

    std::vector<std::unique_ptr<Tile>> childTiles;
    for (auto const& child : children) {
        childTiles.push_back(std::make_unique<ImdlTile>(
            tree, this, child.contentId, child.range, child.sizeMultiplier,
            child.maximumSize));
    }
    setChildren(std::move(childTiles));
}

// SelectParent 选择协议（U9(3)）。
// Ported from: IModelTile.selectTiles (IModelTile.ts:205-334) — SelectParent
// 协议：跳级计数（maxInitialTilesToSkip/maxTilesToSkip）、NotFound 回退、
// 父子独占回滚、undisplayable root 特例。
// EQUIVALENCE: 参考源=IModelTile.ts:276 loadChildren 异步（返回 Promise +
//   TileTreeLoadStatus.Loading 态）；发散=DanQing loadChildren 同步执行
//   （无异步源，Loading 态不可达）→ :282-287 的 markChildrenLoading/
//   markUsed 分支以 canSkipThisTile 结构位保留（原门的前半——
//   `canSkipThisTile && Loading`），markChildrenLoading 无调用点；验证法=
//   SelectTilesProtocol 场景矩阵 + TileTreeRender 像素锁。
//   另：参考 :211-213 的 debugMaxDepth 门（IModelTree.debugMaxDepth）DanQing
//   无 debug 面——省略登记。
//   另二：参考 :206 `this.computeVisibility(args)`（Tile 侧虚方法）；DanQing
//   的 computeVisibility 在树侧（TileTree.h computeVisibility 纯虚，:91，
//   前序任务的登记形态）——
//   getTree().computeVisibility(args, this) 为同一判定的在仓宿主。
//   另三：isDisplayable 位点（:235/:237/:270）取参考语义 `0 < maximumSize`
//  （Tile.ts:231）——DanQing 预存 Tile::isDisplayable()（Ready && graphic，
//   Tile.h DIVERGENCE 注）语义漂移，协议不得经它（场景 3 的计数路径即本
//   登记的回归锁）。参考 :272 hasSizeMultiplier（`_sizeMultiplier !==
//   undefined`，IModelTile.ts:81）以 DanQing 的 0=未设约定表达
//  （m_sizeMultiplier > 0.0，ImdlTileTree.h:35 同约定）。
//   另四：参考 :227-229 `iModelChildren === undefined`（children 未加载态）
//   以 hasLoadedChildren() 表达——成立前提是 loadChildren 的重入门已移植
//  （Tile.ts:353-356 → ImdlTile::loadChildren 头部折叠门：isLeaf() ||
//   hasLoadedChildren() 短路；折叠登记见该函数头：Loading 同步不可达、
//   NotFound 子代态无表示）。残留发散："已加载但空数组"（isLeaf=true，参考
//   Tile.ts:361-364）塌缩为"未加载"→ :228-229 返 Yes 而非 markUsed+返 No
//   ——不可达（leaf → computeVisibility 叶分支 → Visible → :214 段）。
//   另五：:232-241 钻取循环当前死分支——maxInitialTilesToSkip 恒 0（树
//   props 无该字段载体、无 setter，IModelTileTree.ts:390 的 ?? 0 缺省），
//   循环体不可进入；参考原形忠实保留，props 载体/setter 归后续里程碑。
// MSVC：参考 :232-241 的循环体两条路径都在首孩子上 return——C4702（代码
// 生成期告警，pragma 须在函数入口前生效）把 range-for 的隐藏推进判为
// unreachable。保留参考原形，函数级豁免 4702（登记：语义与参考逐字一致）。
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4702)
#endif
SelectParent ImdlTile::selectTiles(std::vector<Tile*>& selected, TileDrawArgs& args,
                                   uint32_t numSkipped)
{
    TileVisibility vis = getTree().computeVisibility(args, this);
    if (vis == TileVisibility::OutsideFrustum)
        return SelectParent::No;

    if (vis == TileVisibility::Visible) {
        // This tile is of appropriate resolution to draw. If need loading or
        // refinement, enqueue. (:214-217)
        if (!isReady())
            args.insertMissing(this);

        if (hasGraphics()) {
            // It can be drawn - select it. (:219-222)
            args.markReady(this);
            selected.push_back(this);
        } else if (!isReady()) {
            // It can't be drawn. Try to draw children in its place; otherwise
            // draw the parent. Do not load/request the children for this
            // purpose. (:223-229)
            size_t const initialSize = selected.size();
            if (!hasLoadedChildren())
                return SelectParent::Yes;

            // 参考 :227-237 的循环形态逐字保留（含"首孩子后无条件 return
            // No"的参考原形——不"修正"它，§0）：(:231-241)。
            if (getDepth() < iModelTree().getMaxInitialTilesToSkip()) {
                for (auto* kid : getChildren()) {
                    if (kid->selectTiles(selected, args, numSkipped) == SelectParent::Yes) {
                        selected.resize(initialSize);
                        return SelectParent::Yes;
                    }
                    return SelectParent::No;
                }
            }

            // If all visible direct children can be drawn, draw them. (:243-253)
            for (auto* kid : getChildren()) {
                if (getTree().computeVisibility(args, kid) != TileVisibility::OutsideFrustum) {
                    if (!kid->hasGraphics()) {
                        selected.resize(initialSize);
                        return SelectParent::Yes;
                    }
                    selected.push_back(kid);
                }
            }
            args.markUsed(this);
        }

        // We're drawing either this tile, or its direct children. (:258-259)
        return SelectParent::No;
    }

    // This tile is too coarse to draw. Try to draw something more appropriate.
    // If it is not ready to draw, we may want to skip loading in favor of
    // loading its descendants. If we previously loaded and later unloaded
    // content for this tile to free memory, don't force it to reload its
    // content - proceed to children. (:262-265)
    bool canSkipThisTile = (m_hadGraphics && !hasGraphics())
                           || getDepth() < iModelTree().getMaxInitialTilesToSkip();
    if (canSkipThisTile) {
        numSkipped = 1;                                                       // :267
    } else {
        canSkipThisTile = isReady() || isParentDisplayable()
                          || getDepth() < iModelTree().getMaxInitialTilesToSkip(); // :269
        if (canSkipThisTile && 0.0 < getMaximumSize()) {
            // skipping an undisplayable tile doesn't count toward the maximum
            // (:270; isDisplayable = `0 < maximumSize`，Tile.ts:231 —— 见函数
            // 头 EQUIVALENCE 另三)。
            // Some tiles do not sub-divide - they only facet the same geometry
            // to a higher resolution. We can skip directly to the correct
            // resolution. (:271-272)
            bool const isNotReady = !isReady() && !hasGraphics()
                                    && !(m_sizeMultiplier > 0.0);
            if (isNotReady) {
                if (numSkipped >= iModelTree().getMaxTilesToSkip())
                    canSkipThisTile = false;                                  // :275
                else
                    numSkipped += 1;                                          // :277
            }
        }
    }

    // :282-287 —— loadChildren 的 Loading 分支以结构位保留（函数头
    // EQUIVALENCE 登记：DanQing loadChildren 同步，Loading 不可达）。
    loadChildren();   // NB: synchronous（参考 :282 asynchronous）
    bool const haveChildren = canSkipThisTile && hasLoadedChildren();   // :283
    if (canSkipThisTile /* && TileTreeLoadStatus::Loading == childrenLoadStatus
                           —— 不可达，保留结构位 */)
        args.markUsed(this);                                            // :286

    if (haveChildren) {
        // If we are the root tile and we are not displayable, then we want to
        // draw *any* currently available children in our place, or else we
        // would draw nothing. Otherwise, if we want to draw children in our
        // place, we should wait for *all* of them to load, or else we would
        // show missing chunks where not-yet-loaded children belong. (:289-291)
        bool const undisplayableRoot = isUndisplayableRootTile();          // :292
        args.markUsed(this);                                               // :293
        bool drawChildren = true;
        size_t const initialSize = selected.size();
        for (auto* child : getChildren()) {
            // NB: We must continue iterating children so that they can be
            // requested if missing. (:297)
            if (child->selectTiles(selected, args, numSkipped) == SelectParent::Yes) {
                if (child->getLoadStatus() == TileLoadStatus::NotFound) {
                    // At least one child we want to draw failed to load. e.g.,
                    // we reached max depth of map tile tree. Draw parent
                    // instead. (:299-301)
                    drawChildren = canSkipThisTile = false;
                } else {
                    // At least one child we want to draw is not yet loaded.
                    // Wait for it to load before drawing it and its siblings,
                    // unless we have nothing to draw in their place. (:302-305)
                    drawChildren = undisplayableRoot;
                }
            }
        }

        if (drawChildren)
            return SelectParent::No;                                       // :309-310

        // Some types of tiles (like maps) allow the ready children to be drawn
        // on top of the parent while other children are not yet loaded. (:312-314)
        if (args.parentsAndChildrenExclusive)
            selected.resize(initialSize);
    }

    if (isReady()) {                                                       // :317-327
        if (hasGraphics()) {
            selected.push_back(this);
            if (!canSkipThisTile) {
                // This tile is too coarse, but we require loading it before we
                // can start loading higher-res children. (:321)
                args.markReady(this);
            }
        }

        return SelectParent::No;
    }

    // This tile is not ready to be drawn. Request it *only* if we cannot skip
    // it. (:329-331)
    if (!canSkipThisTile)
        args.insertMissing(this);
    return isParentDisplayable() ? SelectParent::Yes : SelectParent::No;   // :333
}
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

// ---------------------------------------------------------------------------
// ImdlTileTree
// ---------------------------------------------------------------------------

ImdlTileTree::ImdlTileTree(std::string treeId, std::string rootContentId,
                           dqGeom::Range3d const& rootRange,
                           ImdlTreeMetadata treeMetadata)
    : TileTree(nullptr)
    , m_treeId(std::move(treeId))
    , m_metadata(treeMetadata)
      // Ported from: IModelTileTree constructor (IModelTileTree.ts:390-391) —
      // maxInitialTilesToSkip = params.maxInitialTilesToSkip ?? 0 (DanQing's
      // offline tilesets carry no such props field → the ?? 0 default);
      // maxTilesToSkip = IModelApp.tileAdmin.maximumLevelsToSkip.
    , m_maxInitialTilesToSkip(0)
    , m_maxTilesToSkip(TileAdmin::instance().maximumLevelsToSkip())
{
    // Root tile from the tree props (the reference's requestTileTreeProps
    // rootTile; IModelTileTree constructor :396-410). maximumSize =
    // tileScreenSize: the children fill the same value (TileMetadata.ts:795/
    // :847 maximumSize: root.tileScreenSize) and the reference's root props
    // carry it via requestTileTreeProps — DanQing's id-encoded props have no
    // separate carrier for a root maximumSize, so tileScreenSize stands in
    // (the same value IModelTile.setContent backfills, IModelTile.ts:140-142).
    auto root = std::make_unique<ImdlTile>(*this, nullptr,
                                           std::move(rootContentId),
                                           rootRange, 0.0,
                                           static_cast<double>(treeMetadata.tileScreenSize));
    setRootTile(std::move(root));
}

TileVisibility ImdlTileTree::computeVisibility(TileDrawArgs& args, Tile* tile)
{
    if (!tile)
        return TileVisibility::OutsideFrustum;
    if (!tile->hasContent())
        return TileVisibility::TooCoarse;

    if (args.frustumPlanes.isValid()) {
        auto const& bs = tile->getBoundingSphere();
        dqGeom::Point3d const center(bs.center[0], bs.center[1], bs.center[2]);
        if (args.frustumPlanes.computeContainment(
                tile->getRange(), &center, static_cast<double>(bs.radius))
            == dqCommon::FrustumPlanes::Containment::Outside)
            return TileVisibility::OutsideFrustum;
    }

    // Leaf branch retained per reference computeVisibility (Tile.ts:445-449):
    // a leaf passes culling → Visible (after content culling, which DanQing's
    // subset does not port yet) — it never takes the SSE branch.
    if (tile->isLeaf())
        return TileVisibility::Visible;

    // Ported from: Tile.ts meetsScreenSpaceError (:459-463):
    //   pixelSize = args.getPixelSize(this) * args.pixelSizeScaleFactor;
    //   maxSize = this.maximumSize * args.tileSizeModifier;
    //   return pixelSize <= maxSize;
    double const pixelSize = args.getPixelSize(*tile) * args.pixelSizeScaleFactor;
    double const maxSize = tile->getMaximumSize() * args.tileSizeModifier;
    return pixelSize <= maxSize
        ? TileVisibility::Visible
        : TileVisibility::TooCoarse;
}

std::string ImdlTileTree::contentUrl(std::string const& contentId) const
{
    // Composition point for the fetch layer (FileTileFetcher resolves local
    // paths; the production RPC/cache routing is the registered TODO).
    return m_treeId + "/" + contentId;
}

END_DQ_RENDER_NAMESPACE
