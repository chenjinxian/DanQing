// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — iModel tile tree (per-model production)
// Ported from: itwinjs-core core/frontend/src/internal/tile/IModelTileTree.ts
//              (tree shell + maxDepth=32 :418)
//              core/common/src/tile/TileMetadata.ts computeChildTileProps
//              (:777-853 — pure-frontend child subdivision) and ContentIdProvider
//              V1 scheme (:665-674 — "d/i/j/k/mult" components).
#pragma once

#include "../Export.h"
#include "ImdlHeader.h"  // CurrentImdlVersion（TileOptions 缺省值引用）
#include "Tile.h"
#include "TileDrawArgs.h"
#include "TileTree.h"

#include <dqGeom/Range3d.h>

#include <memory>
#include <string>
#include <vector>

#ifndef BEGIN_DQ_RENDER_NAMESPACE
#define BEGIN_DQ_RENDER_NAMESPACE namespace dqRender {
#define END_DQ_RENDER_NAMESPACE }
#endif

BEGIN_DQ_RENDER_NAMESPACE

// Tile metadata for child computation (the subset the subdivision reads).
// Ported from: itwinjs-core TileMetadata (TileMetadata.ts:750-775 subset).
struct ImdlTileMetadata {
    std::string contentId;
    dqGeom::Range3d range;
    dqGeom::Range3d contentRange;
    bool isLeaf = false;
    double sizeMultiplier = 0.0;   // 0 = not set (magnification branch off)
    uint32_t emptySubRangeMask = 0;
};

// Tree-level metadata for child computation.
// Ported from: itwinjs-core TileTreeMetadata (:760-775 subset).
struct ImdlTreeMetadata {
    dqGeom::Range3d contentRange;  // model range (empty = unknown)
    uint32_t tileScreenSize = 512;  // TileProps.ts:66 default
    bool is2d = false;
    // IModelTileTreeProps.formatVersion (TileProps.ts:65) — the maximum
    // major+minor version the backend supplies ((major<<0x10)|minor).
    // Consumed by the ImdlTileTree constructor to select the content Id
    // scheme (ContentIdProvider.create — IModelTileTree.ts:396-398).
    // 0 = not carried: DanQing's legacy V1 id helpers (parseImdlContentId/
    // formatImdlContentId — base-10, multiplier omitted when 0) stay in
    // charge. EQUIVALENCE (registered, §11.10): the reference maps an absent
    // props.formatVersion to maxMajorVersion = CurrentImdlVersion.Major → V4
    // (ContentIdProvider.create, TileMetadata.ts:649-651 — formatVersion ?? );
    // DanQing keeps the legacy path there because its pre-existing id-encoded
    // tree assembly (PrimaryTileTreeSupplier) and offline assets/tests speak
    // the legacy id form — switching them would be a breaking id-scheme flip
    // with no fetcher on the other side. Divergence = legacy-path trees format
    // ids base-10/4-segment; verification = this header's provider tests +
    // TileTreeRender pixel locks (legacy) + RpcDumpRender (V4 path).
    uint32_t formatVersion = 0;
    // IModelTileTreeProps.maxInitialTilesToSkip (TileProps.ts:63 — "If
    // defined, specifies the number of levels of the tile tree that can be
    // skipped when selecting tiles."). Consumed by the ImdlTileTree
    // constructor (IModelTileTree.ts:390 `params.maxInitialTilesToSkip ?? 0`
    // — 0 = absent, the ?? 0 default). The SelectParent protocol's initial
    // skip budget (IModelTile.ts:265/:269 — tiles whose depth is below it are
    // always skippable, so a NotFound root does not block descent; M-G(2)
    // RED forensics: the drill dump has no root bytes by collection
    // provenance, and without this carrier the replay stalled at the root).
    uint32_t maxInitialTilesToSkip = 0;
};

// Content-id components — V1 scheme (ContentIdProvider, TileMetadata.ts:665-674:
// "d"epth / "i","j","k" sub-volume indices / "mult"iplier, "/"-separated).
struct DQ_RENDER_EXPORT ImdlContentIdSpec {
    uint32_t depth = 0;
    uint32_t i = 0, j = 0, k = 0;
    uint32_t mult = 0;  // 0 = no multiplier component
};

// Compute the children of a tile: magnification branch (one child, same
// volume, 2x multiplier) or 3d/2d bisection (8/4 children with empty-mask
// and model-range rejection).
// Ported from: computeChildTileProps (TileMetadata.ts:777-853).
struct DQ_RENDER_EXPORT ImdlChildTileProps {
    std::string contentId;
    dqGeom::Range3d range;
    bool isLeaf = false;
    double sizeMultiplier = 0.0;
    // Every child's maximumSize = root.tileScreenSize (:795 magnification /
    // :847 subdivision — the SSE criterion's per-tile operand,
    // Tile.ts:459-463).
    double maximumSize = 0.0;
};

// ---------------------------------------------------------------------------
// ContentFlags / TileOptions / getMaximumMajorTileFormatVersion /
// ContentIdProvider — the content Id scheme machinery.
// Ported from: itwinjs-core core/common/src/tile/TileMetadata.ts
//              (ContentFlags :564-571, TileOptions :86-91 +
//              defaultTileOptions :314-331, getMaximumMajorTileFormatVersion
//              :412-427, ContentIdProvider :596-638, ContentIdV1Provider
//              :665-673, ContentIdV2Provider :680-691, ContentIdV4Provider
//              :699-713).
// ---------------------------------------------------------------------------

// Flags controlling how tile content is produced (part of the content Id).
// Ported from: ContentFlags (TileMetadata.ts:564-571).
enum class ContentFlags : uint32_t {
    None = 0,
    AllowInstancing = 1u << 0,
    ImprovedElision = 1u << 1,
    IgnoreAreaPatterns = 1u << 2,
    ExternalTextures = 1u << 3,
};

// TileOptions — the subset ContentIdProvider.create consumes. Ported from:
// TileOptions (TileMetadata.ts:86-91) with the defaultTileOptions defaults
// (:314-331). §3.4 registered subset adaptation: only the fields the provider
// reads have a consumer here — the remaining TileOptions fields (useProject-
// Extents/edgeOptions/…) are carried by their own features' ports.
struct TileOptions {
    uint32_t maximumMajorTileFormatVersion = CurrentImdlVersion::Major;
    bool enableInstancing = true;
    bool enableImprovedElision = true;
    bool ignoreAreaPatterns = false;
    bool enableExternalTextures = true;
};

// The major tile format version to request: the backend's formatVersion
// clamped by the app-configured maximum and the currently supported version.
// Ported from: getMaximumMajorTileFormatVersion (TileMetadata.ts:412-427);
// formatVersion 0 stands for the reference's `undefined` (backend version
// unknown — :417's guard).
uint32_t DQ_RENDER_EXPORT getMaximumMajorTileFormatVersion(
    uint32_t maxMajorVersion, uint32_t formatVersion);

// ContentIdProvider — content Id composition/parsing for the negotiated tile
// format scheme. Ported from: ContentIdProvider (TileMetadata.ts:596-638
// abstract base; concrete methods rootContentId/idFromParentAndMultiplier/
// specFromId/idFromSpec are virtual in DanQing so the §3.4 legacy-id adapter
// in computeImdlChildTileProps can substitute the pre-existing id form —
// the reference providers' behavior is the default implementation).
class DQ_RENDER_EXPORT ContentIdProvider {
public:
    virtual ~ContentIdProvider();

    uint32_t const majorFormatVersion;  // :597
    ContentFlags const contentFlags;    // :598

    // (:605-607) — computeId(0, 0, 0, 0, 1)
    virtual std::string rootContentId() const;
    // (:609-613) — replace the id's last separator component with the
    // multiplier (hex).
    virtual std::string idFromParentAndMultiplier(std::string const& parentId,
                                                  uint32_t multiplier) const;
    // (:615-627) — split on the scheme separator and parse the trailing five
    // hex components. Registered divergence: ids with fewer than five
    // components parse best-effort from the front instead of the reference's
    // assert (DanQing compiles -fno-exceptions without assert-driven flows;
    // callers only round-trip provider-produced ids).
    virtual ImdlContentIdSpec specFromId(std::string const& id) const;
    // (:629-631)
    virtual std::string idFromSpec(ImdlContentIdSpec const& spec) const;

    // (:640-661) — scheme selection by the negotiated major version:
    // 0/1 → V1, 2/3 → V2, ≥4 → V4. `allowInstancing` is the tree options'
    // allowInstancing (PrimaryTileTree.ts:70 — static-primary derivation).
    static std::unique_ptr<ContentIdProvider> create(bool allowInstancing,
                                                     TileOptions const& options,
                                                     uint32_t formatVersion);

protected:
    ContentIdProvider(uint32_t majorVersion, ContentFlags flags);
    virtual char separator() const noexcept = 0;  // :637
    virtual std::string computeId(uint32_t depth, uint32_t i, uint32_t j,
                                  uint32_t k, uint32_t mult) const = 0;  // :638
    // (:633-636) — the five components hex-joined with the scheme separator.
    std::string join(uint32_t depth, uint32_t i, uint32_t j, uint32_t k,
                     uint32_t mult) const;
};

// V1 scheme "depth/i/j/k/multiplier". Ported from: ContentIdV1Provider
// (TileMetadata.ts:665-673).
class DQ_RENDER_EXPORT ContentIdV1Provider final : public ContentIdProvider {
public:
    explicit ContentIdV1Provider(uint32_t majorVersion);

protected:
    char separator() const noexcept override;
    std::string computeId(uint32_t depth, uint32_t i, uint32_t j, uint32_t k,
                          uint32_t mult) const override;
};

// V2/V3 scheme "_majorVersion_flags_depth_i_j_k_multiplier". Ported from:
// ContentIdV2Provider (TileMetadata.ts:680-691).
class DQ_RENDER_EXPORT ContentIdV2Provider final : public ContentIdProvider {
public:
    ContentIdV2Provider(uint32_t majorVersion, bool allowInstancing,
                        TileOptions const& options);

protected:
    char separator() const noexcept override;
    std::string computeId(uint32_t depth, uint32_t i, uint32_t j, uint32_t k,
                          uint32_t mult) const override;

private:
    std::string m_prefix;  // _prefix (:681)
};

// V4+ scheme "-flags-depth-i-j-k-multiplier" (the version lives in the tree
// Id). Ported from: ContentIdV4Provider (TileMetadata.ts:699-713).
class DQ_RENDER_EXPORT ContentIdV4Provider final : public ContentIdProvider {
public:
    ContentIdV4Provider(bool allowInstancing, TileOptions const& options,
                        uint32_t majorVersion);

protected:
    char separator() const noexcept override;
    std::string computeId(uint32_t depth, uint32_t i, uint32_t j, uint32_t k,
                          uint32_t mult) const override;

private:
    std::string m_prefix;  // _prefix (:700)
};

std::vector<ImdlChildTileProps> DQ_RENDER_EXPORT
computeImdlChildTileProps(ImdlTileMetadata const& parent,
                          ImdlTreeMetadata const& root);

// The reference-signature form: the tree's content Id provider feeds child Id
// composition (:777 parameter order — IModelTile._loadChildren passes
// tree.contentIdProvider, IModelTile.ts:153-169). Request keys come out in the
// negotiated scheme (V4 "-flags-depth-i-j-k-multiplier" for the RPC dumps' 37.0
// domain) — the key domain the captured manifest speaks.
std::vector<ImdlChildTileProps> DQ_RENDER_EXPORT
computeImdlChildTileProps(ImdlTileMetadata const& parent,
                          ContentIdProvider const& idProvider,
                          ImdlTreeMetadata const& root);

// Parse/format a V1 content id ("<depth>/<i>/<j>/<k>" [+ "/<mult>"]).
// Ported from: ContentIdProvider V1 (TileMetadata.ts:665-674).
ImdlContentIdSpec DQ_RENDER_EXPORT parseImdlContentId(std::string const& id);
std::string DQ_RENDER_EXPORT formatImdlContentId(ImdlContentIdSpec const& spec);

// ---------------------------------------------------------------------------
// ImdlTile — a tile of an ImdlTileTree.
// Ported from: itwinjs-core IModelTile (IModelTile.ts:56 — contentId-carrying
// tile; content acquisition goes through the TileAdmin fetcher with a URL
// composed from treeId + contentId — the RPC layer's TODO stands in for the
// reference's generateTileContent).
// ---------------------------------------------------------------------------
class DQ_RENDER_EXPORT ImdlTileTree;

class DQ_RENDER_EXPORT ImdlTile : public Tile {
public:
    ImdlTile(ImdlTileTree& tree, Tile* parent,
             std::string contentId, dqGeom::Range3d const& range,
             double sizeMultiplier, double maximumSize);

    std::string const& getContentId() const noexcept { return m_contentId; }
    double getSizeMultiplier() const noexcept { return m_sizeMultiplier; }

    /// Mask of known-empty sub-volumes (subdivision skips them,
    /// computeChildTileProps TileMetadata.ts:826-830). Assigned from content
    /// in setContent.
    /// Ported from: IModelTile.emptySubRangeMask (IModelTile.ts:78).
    uint32_t getEmptySubRangeMask() const noexcept { return m_emptySubRangeMask; }

    /// This tile's tree, typed as the iModel tree (the SelectParent protocol's
    /// skip-budget access goes through it).
    /// Ported from: IModelTile.iModelTree (IModelTile.ts:76 —
    /// `return this.tree as IModelTileTree`). Defined in the .cpp (the
    /// reference cast needs the complete ImdlTileTree type).
    ImdlTileTree& iModelTree() const noexcept;

    // Ported from: itwinjs-core IModelTile.maximumSize (IModelTile.ts:81-83)
    // — super.maximumSize * (this.sizeMultiplier ?? 1.0). The reference's
    // sizeMultiplier is `number | undefined`; DanQing's 0.0 stands for
    // "not set" (ImdlTileMetadata.sizeMultiplier same convention).
    double getMaximumSize() const noexcept override
    {
        return Tile::getMaximumSize()
               * (m_sizeMultiplier > 0.0 ? m_sizeMultiplier : 1.0);
    }

    // --- Tile overrides ---
    bool requestContent() override;
    TileContent readContent(uint8_t const* data, size_t dataSize) override;

    /// IModelTile.setContent 的 :134-148 语义：emptySubRangeMask 赋值 (:136)、
    /// maximumSize 回填 (:140-142)、sizeMultiplier 升门控赋值 (:145-146) +
    /// contentId 覆写 (:147) + 子代 >1 时 disposeChildren (:148)。
    /// Ported from: itwinjs-core IModelTile.setContent (IModelTile.ts:134-148)。
    void setContent(TileContent content) override;

    void loadChildren() override;
    bool hasContent() const noexcept override { return !m_contentId.empty(); }

    /// The SelectParent selection protocol: skip counting
    /// (maxInitialTilesToSkip/maxTilesToSkip), NotFound fallback,
    /// parent/children exclusivity, undisplayable-root special case.
    /// Ported from: IModelTile.selectTiles (IModelTile.ts:205-334).
    SelectParent selectTiles(std::vector<Tile*>& selected, TileDrawArgs& args,
                             uint32_t numSkipped) override;

private:
    std::string m_contentId;
    double m_sizeMultiplier = 0.0;
    // 参考 _emptySubRangeMask (IModelTile.ts:58)；0 = 未赋值（参考 undefined，
    // IModelTile.ts:78 的 `?? 0`）。
    uint32_t m_emptySubRangeMask = 0;
};

// ---------------------------------------------------------------------------
// ImdlTileTree — per-model imdl tree.
// Ported from: IModelTileTree (IModelTileTree.ts:354-460 subset — the
// constructor receives root tile props; maxDepth 32).
// ---------------------------------------------------------------------------
class DQ_RENDER_EXPORT ImdlTileTree : public TileTree {
public:
    static constexpr uint32_t kMaxDepth = 32;  // IModelTileTree.ts:418

    // treeId identifies the tree (model + options); the root's content id
    // comes from the tree props (the reference's requestTileTreeProps RPC —
    // DanQing's props arrive via the supplier's id encoding).
    ImdlTileTree(std::string treeId, std::string rootContentId,
                 dqGeom::Range3d const& rootRange,
                 ImdlTreeMetadata treeMetadata);

    std::string const& getTreeId() const noexcept { return m_treeId; }
    ImdlTreeMetadata const& metadata() const noexcept { return m_metadata; }

    /// How many levels may be skipped past while selecting until
    /// maxInitialTilesToSkip is exhausted (the SelectParent protocol's
    /// per-tree budgets).
    /// Ported from: IModelTree.maxInitialTilesToSkip / maxTilesToSkip
    /// (IModelTileTree.ts:361-362 — maxInitialTilesToSkip = tree-props field
    /// ?? 0 (:390); maxTilesToSkip = TileAdmin.maximumLevelsToSkip (:391)).
    /// The props carrier is ImdlTreeMetadata::maxInitialTilesToSkip
    /// (TileProps.ts:63) — wired since M-G(2); before that the member existed
    /// but the props value could not reach it, so the ?? 0 default held even
    /// where the RPC dump props carry the field (e.g. compatseed 6,
    /// instances60 3 — registered gap, closed by the metadata carrier).
    uint32_t getMaxInitialTilesToSkip() const noexcept
    {
        return m_maxInitialTilesToSkip;
    }
    uint32_t getMaxTilesToSkip() const noexcept { return m_maxTilesToSkip; }

    TileVisibility computeVisibility(TileDrawArgs& args, Tile* tile) override;

    // Child computation for a tile of this tree (computeImdlChildTileProps
    // with this tree's metadata + content Id provider; IModelTile._loadChildren
    // :153-169 passes tree.contentIdProvider — the negotiated scheme's request
    // keys; the legacy 2-arg form keeps the pre-provider id contract).
    std::vector<ImdlChildTileProps> childPropsFor(ImdlTileMetadata const& parent) const
    {
        if (m_contentIdProvider)
            return computeImdlChildTileProps(parent, *m_contentIdProvider,
                                             m_metadata);
        return computeImdlChildTileProps(parent, m_metadata);
    }

    /// The tree's content Id provider (IModelTileTree.contentIdProvider —
    /// :396-398 contentIdProvider.create). Null on the legacy path (props
    /// carry no formatVersion — the legacy V1 id helpers stay in charge, see
    /// ImdlTreeMetadata::formatVersion). Consumed by ImdlTile::setContent's
    /// contentId rewrite (IModelTile.ts:147).
    ContentIdProvider const* contentIdProvider() const noexcept
    {
        return m_contentIdProvider.get();
    }

    // Content URL: <treeId>/<contentId> (the fetch layer's composition point —
    // the reference routes through generateTileContent RPC / tile cache).
    std::string contentUrl(std::string const& contentId) const;

private:
    std::string m_treeId;
    ImdlTreeMetadata m_metadata;
    // SelectParent 协议的树级跳级预算（IModelTileTree.ts:361-362/:390-391）。
    uint32_t m_maxInitialTilesToSkip = 0;  // :390 — props ?? 0（载体
                                           // ImdlTreeMetadata::maxInitialTilesToSkip，
                                           // 构造器初始化列表消费）
    uint32_t m_maxTilesToSkip = 1;         // :391 — TileAdmin.maximumLevelsToSkip
    // The tree's content Id scheme (:396-398 contentIdProvider.create +
    // :398 rootContentId override) — null
    // while the tree props carry no formatVersion (DanQing's legacy V1 id
    // helpers path, see ImdlTreeMetadata::formatVersion).
    std::unique_ptr<ContentIdProvider> const m_contentIdProvider;
};

END_DQ_RENDER_NAMESPACE
