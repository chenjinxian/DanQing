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

#include "render/Graphic.h"               // Branch（location 包裹——M-H Task 3）
#include "render/RenderGraphicAdapter.h"  // OwningRenderGraphicAdapter（同上）

#include <algorithm>
#include <cstdio>
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

// ---------------------------------------------------------------------------
// ContentIdProvider — content Id scheme machinery.
// Ported from: itwinjs-core core/common/src/tile/TileMetadata.ts
//              (getMaximumMajorTileFormatVersion :412-427, ContentIdProvider
//              :596-638, ContentIdV1Provider :665-673, ContentIdV2Provider
//              :680-691, ContentIdV4Provider :699-713).
// ---------------------------------------------------------------------------

namespace {

std::string toHex(uint32_t value)
{
    // toString(16) — lowercase, unpadded.
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%x", value);
    return buf;
}

uint32_t fromHex(std::string const& text)
{
    return static_cast<uint32_t>(std::strtoul(text.c_str(), nullptr, 16));
}

}  // namespace

uint32_t getMaximumMajorTileFormatVersion(uint32_t maxMajorVersion,
                                          uint32_t formatVersion)
{
    // Ported from: TileMetadata.ts:412-427 — clamp the backend's version by
    // the app-configured maximum and the currently supported major version.
    uint32_t majorVersion = maxMajorVersion;
    if (formatVersion != 0)  // `undefined !== formatVersion` (:417-419); 0 = undefined
        majorVersion = std::min(formatVersion >> 0x10, majorVersion);

    // Version number less than 1 is invalid - ignore (:421-422).
    majorVersion = std::max(majorVersion, 1u);

    // Version number greater than current known version ignored (:423-424).
    majorVersion = std::min(majorVersion,
                            static_cast<uint32_t>(CurrentImdlVersion::Major));

    // Version numbers are integers - round down (:426-427).
    return std::max(majorVersion, 1u);
}

ContentIdProvider::~ContentIdProvider() = default;

ContentIdProvider::ContentIdProvider(uint32_t majorVersion, ContentFlags flags)
    : majorFormatVersion(majorVersion)
    , contentFlags(flags)
{
}

std::string ContentIdProvider::rootContentId() const
{
    // Ported from: TileMetadata.ts:605-607.
    return computeId(0, 0, 0, 0, 1);
}

std::string ContentIdProvider::idFromParentAndMultiplier(
    std::string const& parentId, uint32_t multiplier) const
{
    // Ported from: TileMetadata.ts:609-613 — keep everything up to and
    // including the last separator, replace the multiplier component.
    size_t const lastSepPos = parentId.rfind(separator());
    if (lastSepPos == std::string::npos)
        return parentId;  // reference asserts (-fno-exceptions port: keep the id)
    return parentId.substr(0, lastSepPos + 1) + toHex(multiplier);
}

ImdlContentIdSpec ContentIdProvider::specFromId(std::string const& id) const
{
    // Ported from: TileMetadata.ts:615-627 — split on the separator, parse the
    // trailing five components as hex.
    ImdlContentIdSpec spec;
    std::vector<std::string> parts;
    size_t start = 0;
    while (true) {
        size_t const sep = id.find(separator(), start);
        if (sep == std::string::npos) {
            parts.push_back(id.substr(start));
            break;
        }
        parts.push_back(id.substr(start, sep - start));
        start = sep + 1;
    }
    // (:616-617 assert len >= 5) — registered best-effort divergence: short
    // ids parse from the front instead of asserting.
    size_t const len = parts.size();
    if (len >= 5) {
        spec.depth = fromHex(parts[len - 5]);
        spec.i = fromHex(parts[len - 4]);
        spec.j = fromHex(parts[len - 3]);
        spec.k = fromHex(parts[len - 2]);
        spec.mult = fromHex(parts[len - 1]);
    } else {
        size_t const n = std::min(len, size_t{5});
        uint32_t* fields[] = {&spec.depth, &spec.i, &spec.j, &spec.k, &spec.mult};
        for (size_t idx = 0; idx < n; ++idx)
            *fields[idx] = fromHex(parts[idx]);
    }
    return spec;
}

std::string ContentIdProvider::idFromSpec(ImdlContentIdSpec const& spec) const
{
    // Ported from: TileMetadata.ts:629-631.
    return computeId(spec.depth, spec.i, spec.j, spec.k, spec.mult);
}

std::string ContentIdProvider::join(uint32_t depth, uint32_t i, uint32_t j,
                                    uint32_t k, uint32_t mult) const
{
    // Ported from: TileMetadata.ts:633-636.
    std::string out = toHex(depth);
    out += separator();
    out += toHex(i);
    out += separator();
    out += toHex(j);
    out += separator();
    out += toHex(k);
    out += separator();
    out += toHex(mult);
    return out;
}

// ---------------------------------------------------------------------------
// edgeOptionsToString / iModelTileTreeIdToString — tile tree Id 派生
// （M-H Task 3；参考 PrimaryTreeSupplier.createTileTree 的调用点
// PrimaryTileTree.ts:65——requestTileTreeProps RPC 的键）。
// ---------------------------------------------------------------------------

std::string edgeOptionsToString(std::optional<EdgeOptions> const& options)
{
    // Ported from: TileMetadata.ts:399-409（逐分支 1:1）。
    if (!options.has_value())
        return "E:0_";  // :400-401

    switch (options->type) {
        case TileEdgeType::NonIndexed:
            return options->smooth ? "E:3_" : "";          // :404
        case TileEdgeType::Indexed:
            return options->smooth ? "E:4_" : "E:2_";      // :405
        case TileEdgeType::Compact:
            return options->smooth ? "E:6_" : "E:5_";      // :406
    }
    return "";  // :407-408 throw 分支不可达（enum 穷举）——§3.4 禁异常适配
}

namespace {

// animationIdToString — Ported from: TileMetadata.ts:475-477。
std::string animationIdToString(std::string const& animationId)
{
    return "A:" + animationId + "_";
}

// toFixed(6) 的 C++ 等价（TileMetadata.ts:513 expansion.toFixed(6)——
// JS toFixed 对有限小数与 printf %.6f 同产出：1→"1.000000"、0.25→"0.250000"、
// 12.00001234→"12.000012"、123456789→"123456789.000000"——参考测试钉值四例）。
std::string expansionToString(double expansion)
{
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.6f", expansion);
    return buf;
}

// uint32 → 小写 hex（:527-528 version.toString(16)/flags.toString(16)）。
std::string treeIdHex(uint32_t v)
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%x", v);
    return buf;
}

}  // namespace

std::string iModelTileTreeIdToString(std::string const& modelId,
                                     IModelTileTreeId const& treeId,
                                     TileOptions const& options)
{
    // Ported from: TileMetadata.ts:487-532（逐行 1:1）。
    std::string idStr;
    // :488-500 —— flags 合成。
    uint32_t flags = options.useProjectExtents
        ? static_cast<uint32_t>(TreeFlags::UseProjectExtents)
        : static_cast<uint32_t>(TreeFlags::None);
    if (options.optimizeBRepProcessing)
        flags |= static_cast<uint32_t>(TreeFlags::OptimizeBRepProcessing);
    if (options.disablePolyfaceDecimation)
        flags |= static_cast<uint32_t>(TreeFlags::DisablePolyfaceDecimation);
    if (options.useLargerTiles)
        flags |= static_cast<uint32_t>(TreeFlags::UseLargerTiles);
    if (options.expandProjectExtents)
        flags |= static_cast<uint32_t>(TreeFlags::ExpandProjectExtents);

    if (auto const* primary = std::get_if<PrimaryTileTreeId>(&treeId)) {
        // :502-510 —— BatchType.Primary 分支。
        if (primary->animationId.has_value())
            idStr += animationIdToString(*primary->animationId);       // :503-504
        else if (primary->enforceDisplayPriority)                      // :505-506
            flags |= static_cast<uint32_t>(TreeFlags::EnforceDisplayPriority);

        std::string const edges = edgeOptionsToString(primary->edges);  // :508
        // :509 —— sectionCut ? `S${sectionCut}s` : ""
        std::string const sectionCut = primary->sectionCut.has_value()
            ? "S" + *primary->sectionCut + "s"
            : "";
        idStr = idStr + edges + sectionCut;                             // :510
    } else {
        auto const& classifier = std::get<ClassifierTileTreeId>(treeId);
        // :511-523 —— classifier 分支。
        std::string const typeStr =
            dqCommon::BatchType::PlanarClassifier == classifier.type ? "CP" : "C";  // :512
        idStr = idStr + typeStr + ":" + expansionToString(classifier.expansion) + "_";  // :513

        if (dqCommon::BatchType::VolumeClassifier == classifier.type) {
            // Volume classifiers always use the exact project extents. (:515-519)
            flags |= static_cast<uint32_t>(TreeFlags::UseProjectExtents);
            flags &= ~static_cast<uint32_t>(TreeFlags::ExpandProjectExtents);
        }

        if (classifier.animationId.has_value())
            idStr += animationIdToString(*classifier.animationId);      // :521-522
    }

    // :525-529 —— V4+ 前缀（version_flags-）。
    uint32_t const version = getMaximumMajorTileFormatVersion(
        options.maximumMajorTileFormatVersion, /*formatVersion=*/0);
    if (version >= 4) {
        std::string const prefix = treeIdHex(version) + "_" + treeIdHex(flags) + "-";
        idStr = prefix + idStr;
    }

    return idStr + modelId;  // :531
}

std::unique_ptr<ContentIdProvider> ContentIdProvider::create(
    bool allowInstancing, TileOptions const& options, uint32_t formatVersion)
{
    // Ported from: TileMetadata.ts:640-661.
    uint32_t const majorVersion = getMaximumMajorTileFormatVersion(
        options.maximumMajorTileFormatVersion, formatVersion);
    switch (majorVersion) {
        case 0:
        case 1:
            return std::make_unique<ContentIdV1Provider>(majorVersion);
        case 2:
        case 3:
            return std::make_unique<ContentIdV2Provider>(majorVersion,
                                                         allowInstancing,
                                                         options);
        default:
            return std::make_unique<ContentIdV4Provider>(allowInstancing,
                                                         options,
                                                         majorVersion);
    }
}

ContentIdV1Provider::ContentIdV1Provider(uint32_t majorVersion)
    : ContentIdProvider(majorVersion, ContentFlags::None)
{
    // Ported from: TileMetadata.ts:667-670.
}

char ContentIdV1Provider::separator() const noexcept
{
    return '/';
}

std::string ContentIdV1Provider::computeId(uint32_t depth, uint32_t i,
                                           uint32_t j, uint32_t k,
                                           uint32_t mult) const
{
    // Ported from: TileMetadata.ts:671-673.
    return join(depth, i, j, k, mult);
}

namespace {

// The V4 flags composition (:703-710) — hoisted so the const base member can
// initialize directly from it.
ContentFlags v4ContentFlags(bool allowInstancing, TileOptions const& options)
{
    uint32_t flags = static_cast<uint32_t>(ContentFlags::None);
    if (allowInstancing && options.enableInstancing)
        flags |= static_cast<uint32_t>(ContentFlags::AllowInstancing);
    if (options.enableImprovedElision)
        flags |= static_cast<uint32_t>(ContentFlags::ImprovedElision);
    if (options.ignoreAreaPatterns)
        flags |= static_cast<uint32_t>(ContentFlags::IgnoreAreaPatterns);
    if (options.enableExternalTextures)
        flags |= static_cast<uint32_t>(ContentFlags::ExternalTextures);
    return static_cast<ContentFlags>(flags);
}

}  // namespace

ContentIdV2Provider::ContentIdV2Provider(uint32_t majorVersion,
                                         bool allowInstancing,
                                         TileOptions const& options)
    : ContentIdProvider(
          majorVersion,
          (allowInstancing && options.enableInstancing) ? ContentFlags::AllowInstancing
                                                        : ContentFlags::None)
{
    // Ported from: TileMetadata.ts:688 — _prefix = separator + majorVersion
    // hex + separator + flags hex + separator.
    m_prefix = std::string(1, separator()) + toHex(majorFormatVersion)
               + std::string(1, separator())
               + toHex(static_cast<uint32_t>(contentFlags))
               + std::string(1, separator());
}

char ContentIdV2Provider::separator() const noexcept
{
    return '_';
}

std::string ContentIdV2Provider::computeId(uint32_t depth, uint32_t i,
                                           uint32_t j, uint32_t k,
                                           uint32_t mult) const
{
    // Ported from: TileMetadata.ts:689-691.
    return m_prefix + join(depth, i, j, k, mult);
}

ContentIdV4Provider::ContentIdV4Provider(bool allowInstancing,
                                         TileOptions const& options,
                                         uint32_t majorVersion)
    : ContentIdProvider(majorVersion, v4ContentFlags(allowInstancing, options))
{
    // Ported from: TileMetadata.ts:711-712 — _prefix = separator + flags hex
    // + separator.
    m_prefix = std::string(1, separator())
               + toHex(static_cast<uint32_t>(contentFlags))
               + std::string(1, separator());
}

char ContentIdV4Provider::separator() const noexcept
{
    return '-';
}

std::string ContentIdV4Provider::computeId(uint32_t depth, uint32_t i,
                                           uint32_t j, uint32_t k,
                                           uint32_t mult) const
{
    // Ported from: TileMetadata.ts:712-713.
    return m_prefix + join(depth, i, j, k, mult);
}

// ---------------------------------------------------------------------------
// computeImdlChildTileProps — the provider-based reference-signature form.
// ---------------------------------------------------------------------------

std::vector<ImdlChildTileProps> computeImdlChildTileProps(
    ImdlTileMetadata const& parent, ContentIdProvider const& idProvider,
    ImdlTreeMetadata const& root)
{
    // Ported from: computeChildTileProps (TileMetadata.ts:777-853).
    std::vector<ImdlChildTileProps> children;
    if (parent.isLeaf)
        return children;

    // Magnification: one child, same volume, doubled multiplier (:785-799).
    if (parent.sizeMultiplier > 0.0) {
        double const multiplier = parent.sizeMultiplier * 2.0;
        ImdlChildTileProps child;
        child.contentId = idProvider.idFromParentAndMultiplier(
            parent.contentId, static_cast<uint32_t>(multiplier));  // :788
        child.range = parent.range;
        child.sizeMultiplier = multiplier;
        child.isLeaf = false;
        child.maximumSize = static_cast<double>(root.tileScreenSize);  // :795
        children.push_back(std::move(child));
        return children;
    }

    // Sub-divide into 4 (2d) or 8 (3d) children (:801-848).
    ImdlContentIdSpec parentSpec = idProvider.specFromId(parent.contentId);
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
                child.contentId = idProvider.idFromSpec(childSpec);  // :846
                child.range = range;
                child.maximumSize = static_cast<double>(root.tileScreenSize);  // :847
                children.push_back(std::move(child));
            }
        }
    }

    return children;
}

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

namespace {

// DanQing legacy V1 id 约定（base-10、mult=0 时省略——parseImdlContentId/
// formatImdlContentId 的既有行为）的 provider 适配器：2 参
// computeImdlChildTileProps 的输出合同（id-encoded 装配 + 离线资产/tests 的
// 既存行为）经它冻结到与参考 :777-853 单一的细分主体上。
// Authored: 兼容 shim，参考无对应物——参考的 ContentIdV1Provider 是 hex +
// 恒 5 段；两 id 形态并存的登记见 ImdlTreeMetadata::formatVersion 的
// EQUIVALENCE 注（ImdlTileTree.h）。
class LegacyContentIdAdapter final : public ContentIdProvider {
public:
    LegacyContentIdAdapter()
        : ContentIdProvider(1, ContentFlags::None)
    {
    }

    ImdlContentIdSpec specFromId(std::string const& id) const override
    {
        return parseImdlContentId(id);
    }

    std::string idFromParentAndMultiplier(std::string const& parentId,
                                          uint32_t multiplier) const override
    {
        ImdlContentIdSpec spec = parseImdlContentId(parentId);
        spec.mult = multiplier;
        return formatImdlContentId(spec);
    }

protected:
    std::string computeId(uint32_t depth, uint32_t i, uint32_t j, uint32_t k,
                          uint32_t mult) const override
    {
        return formatImdlContentId(ImdlContentIdSpec{depth, i, j, k, mult});
    }

private:
    char separator() const noexcept override { return '/'; }
};

LegacyContentIdAdapter const s_legacyContentIdAdapter;

}  // namespace

std::vector<ImdlChildTileProps> computeImdlChildTileProps(
    ImdlTileMetadata const& parent, ImdlTreeMetadata const& root)
{
    // DanQing legacy 签名：id 形态合同经适配器冻结后走参考 :777-853 的同一
    // 细分主体（provider 形态 = 本文件上方的 3 参定义）。
    return computeImdlChildTileProps(parent, s_legacyContentIdAdapter, root);
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
    // decode 产物的全字段接线：sizeMultiplier/emptySubRangeMask 经 TileContent
    // 进 setContent 的 :134-148 消费（M-F(1) 根因修复——此前丢失，根瓦恒
    // m_sizeMultiplier=0，放大-细分混合树走错分支）。desc 语义对齐参考
    // decodeTileContentDescription 的 `sizeMultiplier?: number`
    //（TileMetadata.ts:875——0.0 = undefined，DanQing 约定）。
    content.sizeMultiplier = desc->sizeMultiplier;
    content.emptySubRangeMask = desc->emptySubRangeMask;

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

    // maximumSize 回填已归位 ImdlTile::setContent（IModelTile.ts:140-142）——
    // setContent 现为 virtual，参考的 IModelTile.setContent 覆写有 C++ 分派
    // 路径（TileAdmin::deliverTileContent 经 Tile& 调用，M-F(1)）。

    // location 消费（M-H Task 3）：树 iModelTransform 非恒等 → 瓦 graphic
    // 包进带 localToWorld 的 Branch（变换经 BranchState.fromBranch 组合——
    // Graphic.h:220-221 注释；OwningRenderGraphicAdapter 持所有权——被包的
    // graphic 可能是公共 GraphicBranch（createGraphicList 多 mesh 返回，
    // 非内部 Graphic））。
    // EQUIVALENCE: 参考源=TileDrawArgs.produceGraphics 的 draw 时包裹
    //   （TileDrawArgs.ts:360-385——createGraphicBranch(graphics,
    //   this.location) :373，location=tree.iModelTransform）；发散=DanQing
    //   创建时包裹（树 location 装载期一次性 setIModelTransform、之后不变
    //   ——参考 draw 时逐帧读 tree.iModelTransform 的其余动机字段
    //   transformFromIModel/clipVolume/animationTransformNodeId 在 DanQing
    //   无载体）；验证法=DumpOpenChain 两锁的 saved 视图像素锚（location
    //   平移后内容落 saved 视域——修复前内容在 iModel 坐标域出视域）。
    // 既有锁零影响：不设 location 的树（DumpMount 通道）恒等 → 跳过包裹。
    if (content.graphic) {
        auto const& xf = getTree().getIModelTransform();
        if (!xf.IsIdentity()) {
            auto branch = std::make_unique<Branch>();
            branch->setLocalToWorld(xf);
            branch->setChild(std::make_unique<OwningRenderGraphicAdapter>(
                content.graphic.release()));
            content.graphic.reset(branch.release());
        }
    }
    return content;
}

void ImdlTile::setContent(TileContent content)
{
    // rvalue 顺序：TileContent 经基类按值参 move 走 graphic——参考语义是
    // "读 content 字段后 super.setContent"（IModelTile.ts:135-147），DanQing
    // 基类先 move，故标量字段须在 move 前抓本地再消费。
    double const sizeMultLocal = content.sizeMultiplier;
    uint32_t const emptyMaskLocal = content.emptySubRangeMask;

    Tile::setContent(std::move(content));

    // Ported from: IModelTile.setContent (IModelTile.ts:134-148) 逐行：
    // this._emptySubRangeMask = content.emptySubRangeMask;  (:136)
    m_emptySubRangeMask = emptyMaskLocal;

    // NB: If this tile has no graphics, it may or may not have children - but
    // we don't want to load the children until this tile is too coarse for
    // view based on its size in pixels.
    // That is different than an "undisplayable" tile (maximumSize=0) whose
    // children should be loaded immediately.  (:138-141)
    if (getGraphic() && 0.0 == getMaximumSize())
        m_maximumSize = static_cast<double>(iModelTree().metadata().tileScreenSize);

    // const sizeMult = content.sizeMultiplier;  (:144)
    double const sizeMult = sizeMultLocal;
    // undefined !== sizeMult && (undefined === this._sizeMultiplier ||
    //                            sizeMult > this._sizeMultiplier)  (:145)
    // ——DanQing 的 0=undefined 约定：> 0.0 即"已设"。
    if (sizeMult > 0.0 && (!(m_sizeMultiplier > 0.0) || sizeMult > m_sizeMultiplier)) {
        m_sizeMultiplier = sizeMult;                                            // :146
        // this._contentId = this.iModelTree.contentIdProvider
        //     .idFromParentAndMultiplier(this.contentId, sizeMult);  (:147)
        if (auto const* provider = iModelTree().contentIdProvider()) {
            m_contentId = provider->idFromParentAndMultiplier(
                m_contentId, static_cast<uint32_t>(sizeMult));
        } else {
            // legacy V1 id 路径（props 无 formatVersion）：mult 形态经
            // LegacyContentIdAdapter 重写（与 computeImdlChildTileProps 的
            // 2 参形态同一 id 合同，本文件匿名命名空间）。
            m_contentId = s_legacyContentIdAdapter.idFromParentAndMultiplier(
                m_contentId, static_cast<uint32_t>(sizeMult));
        }
        // if (undefined !== this.children && this.children.length > 1)
        //     this.disposeChildren();  (:148) —— disposeChildren 等价 =
        // 释放子代（Tile.ts:378-385 逐子 dispose；DanQing setChildren({}) 释放
        // m_ownedChildren）。
        if (getChildren().size() > 1)
            setChildren({});
    }
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
    // 参考的 parent 实参即 this（IModelTile.ts:156 computeChildTileProps(this,
    // ...)）——setContent :136 写入的 _emptySubRangeMask 随瓦进 computeChild-
    // TileProps 的跳过逻辑（TileMetadata.ts:808/:826-830）。
    parent.emptySubRangeMask = m_emptySubRangeMask;

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
//   另五：:232-241 钻取循环的可达性随 maxInitialTilesToSkip 载体接线
//   （M-G(2)，ImdlTreeMetadata::maxInitialTilesToSkip——TileProps.ts:63 →
//   IModelTileTree.ts:390）成立：props 携带预算的树（RPC dump 域——
//   compatseed 6 / instances60 3 / mirukuru 2）depth < 预算的瓦进入该循环；
//   默认构造的元数据（存量 TileTreeRender/单测域）仍为 0，循环保持死分支
//   形态。参考原形忠实保留。
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
      // maxInitialTilesToSkip = params.maxInitialTilesToSkip ?? 0 (the props
      // carrier is ImdlTreeMetadata::maxInitialTilesToSkip, TileProps.ts:63 —
      // the reference receives it via IModelTileTreeParams destructure
      // (:51/:76); DanQing's metadata struct is the props-injection path,
      // wired M-G(2)); maxTilesToSkip = TileAdmin.maximumLevelsToSkip.
    , m_maxInitialTilesToSkip(treeMetadata.maxInitialTilesToSkip)
    , m_maxTilesToSkip(TileAdmin::instance().maximumLevelsToSkip())
    // Ported from: IModelTileTree.ts:396-398 — contentIdProvider =
    // ContentIdProvider.create(params.options.allowInstancing, tileAdmin,
    // params.formatVersion). DanQing: allowInstancing = true (the
    // static-primary derivation, PrimaryTileTree.ts:70 — the animated/
    // priority/sectionCut carriers don't exist here yet, registered) and
    // defaultTileOptions (TileOptions{} — TileMetadata.ts:314-331; flags 0xb
    // for formatVersion 37.0, which is exactly the captured request-key
    // prefix "-b-" of the RPC dumps).
    , m_contentIdProvider(treeMetadata.formatVersion != 0
                              ? ContentIdProvider::create(
                                    /*allowInstancing=*/true, TileOptions{},
                                    treeMetadata.formatVersion)
                              : nullptr)
{
    // Ported from: IModelTileTree.ts:398 —
    // `params.rootTile.contentId = this.contentIdProvider.rootContentId;`
    // (the props' scheme-agnostic root id — V1 form from the backend — is
    // overridden with the negotiated scheme's root form; the request key must
    // match the backend's key domain).
    if (m_contentIdProvider)
        rootContentId = m_contentIdProvider->rootContentId();

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

    // [SEL] 剔除轨迹（DANQING_SEL_TRACE=1 门控，默认零开销——M-K(2)
    // housemodel 透视 saved 视图零请求取证的插桩面；§13.1 诊断表）。
    static bool const s_selTrace = [] {
        return std::getenv("DANQING_SEL_TRACE") != nullptr;
    }();

    if (args.frustumPlanes.isValid()) {
        auto const& bs = tile->getBoundingSphere();
        // Ported from: Tile.isRegionCulled/isFrustumCulled（Tile.ts:391-411）
        // ——range 与 boundingSphere 先经 args.location（treeToWorld =
        // tree.iModelTransform）变换到 world 域再剔除（`box.transformBy
        // (args.location)` / `sphere?.transformBy(args.location)` :406-407——
        // 球半径 × maxColumnScale，BoundingSphere.transformBy）。
        // 移植缺口修复（M-H Task 3）：原实现把 iModel 坐标域的 range/球直接
        // 喂 world 域 frustumPlanes——location 非恒等的树（RPC dump 的
        // iModel→world 平移）域错位剔除（saved 视域内内容被整树剔成
        // OutsideFrustum——[SEL] 取证实锤）；恒等 location 下两形同值
        //（M-E/M-F/M-G 锁全绿的既有面不受影响）。
        // EQUIVALENCE 登记（M-H(3) 终审 Minor 清偿）：参考 Frustum.transformBy
        // 对 range 做 8 角精确变换；此处 MultiplyRange 变换 8 角后取 AABB
        // 并集重装箱——旋转型 location 下为保守超集（少剔→可能多请求，不破
        // 正确性）。发散：旋转 location 出现时请求面可能与参考不同构。
        // 验证法：当前 21 棵采集树 location 全为纯平移（逐树解析核实）不可
        // 观测；旋转型 location 资产出现时以请求面对账锁复核。
        dqGeom::Range3d const worldRange =
            args.treeToWorld.MultiplyRange(tile->getRange());
        dqGeom::Point3d const worldCenter = args.treeToWorld.MultiplyPoint3d(
            dqGeom::Point3d(bs.center[0], bs.center[1], bs.center[2]));
        auto const& m = args.treeToWorld.GetMatrix();
        double const maxScale = (std::max)(
            {m.ColumnXMagnitude(), m.ColumnYMagnitude(), m.ColumnZMagnitude()});
        double const worldRadius = static_cast<double>(bs.radius) * maxScale;
        if (args.frustumPlanes.computeContainment(worldRange, &worldCenter,
                                                  worldRadius)
            == dqCommon::FrustumPlanes::Containment::Outside)
        {
            if (s_selTrace)
                printf("[SEL] cull depth=%u range=(%.1f,%.1f,%.1f)-(%.1f,%.1f,%.1f) "
                       "sphere=(%.1f,%.1f,%.1f)r%.1f\n",
                       tile->getDepth(),
                       worldRange.low.x, worldRange.low.y, worldRange.low.z,
                       worldRange.high.x, worldRange.high.y, worldRange.high.z,
                       worldCenter.x, worldCenter.y, worldCenter.z, worldRadius);
            return TileVisibility::OutsideFrustum;
        }
        if (s_selTrace)
            printf("[SEL] pass depth=%u range=(%.1f,%.1f,%.1f)-(%.1f,%.1f,%.1f) "
                   "sphere=(%.1f,%.1f,%.1f)r%.1f\n",
                   tile->getDepth(),
                   worldRange.low.x, worldRange.low.y, worldRange.low.z,
                   worldRange.high.x, worldRange.high.y, worldRange.high.z,
                   worldCenter.x, worldCenter.y, worldCenter.z, worldRadius);
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
    if (s_selTrace)
        printf("[SEL] sse depth=%u radius=%.1f pixelSize=%.1f maxSize=%.1f -> %s\n",
               tile->getDepth(), tile->getBoundingSphere().radius,
               pixelSize, maxSize, pixelSize <= maxSize ? "Visible" : "TooCoarse");
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
