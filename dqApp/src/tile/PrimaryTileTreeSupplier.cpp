// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — primary tile tree supplier implementation
// Ported from: itwinjs-core core/frontend/src/internal/tile/PrimaryTileTree.ts
#include "dqApp/tile/PrimaryTileTreeSupplier.h"

#include "dqApp/IModelConnection.h"
#include <dqRender/tile/ImdlTileTree.h>

#include <cstdlib>
#include <sstream>
#include <vector>

BEGIN_DQ_APP_NAMESPACE

int PrimaryTileTreeSupplier::compareTileTreeIds(TileTreeId const& lhs, TileTreeId const& rhs) const
{
    return lhs < rhs ? -1 : (lhs > rhs ? 1 : 0);
}

std::unique_ptr<dqRender::TileTree> PrimaryTileTreeSupplier::createTileTree(
    TileTreeId const& id, IModelConnection&)
{
    // Decode the id-encoded root props: modelId|contentId|x0,y0,z0,x1,y1,z1|content-range|screenSize|is2d
    // (see header note — the encoding stands in for requestTileTreeProps).
    std::vector<std::string> parts;
    std::istringstream ss(id);
    std::string token;
    while (std::getline(ss, token, '|'))
        parts.push_back(token);
    if (parts.size() < 6)
        return nullptr;

    auto parseDoubles = [](std::string const& s, double* out, int n) {
        std::istringstream ds(s);
        for (int i = 0; i < n; ++i) {
            std::string v;
            if (!std::getline(ds, v, ','))
                return false;
            out[i] = std::strtod(v.c_str(), nullptr);
        }
        return true;
    };

    double root[6];
    if (!parseDoubles(parts[2], root, 6))
        return nullptr;
    double content[6];
    if (!parseDoubles(parts[3], content, 6))
        return nullptr;

    dqRender::ImdlTreeMetadata meta;
    meta.contentRange = dqGeom::Range3d::CreateXYZXYZ(
        content[0], content[1], content[2], content[3], content[4], content[5]);
    meta.tileScreenSize = static_cast<uint32_t>(std::strtoul(parts[4].c_str(), nullptr, 10));
    meta.is2d = parts[5] == "2d";

    return std::make_unique<dqRender::ImdlTileTree>(
        parts[0], parts[1],
        dqGeom::Range3d::CreateXYZXYZ(root[0], root[1], root[2], root[3], root[4], root[5]),
        meta);
}

END_DQ_APP_NAMESPACE
