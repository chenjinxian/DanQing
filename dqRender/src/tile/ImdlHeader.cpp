// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — imdl header deserialization
// Ported from: itwinjs-core core/common/src/tile/IModelTileIO.ts
#include "dqRender/tile/ImdlHeader.h"

BEGIN_DQ_RENDER_NAMESPACE

ImdlHeader ImdlHeader::readFrom(ImdlByteStream& stream)
{
    // Ported from: ImdlHeader constructor (IModelTileIO.ts:77-101).
    ImdlHeader h;
    h.format = stream.readUint32();
    h.version = stream.readUint32();
    h.headerLength = stream.readUint32();
    h.flags = static_cast<ImdlFlags>(stream.readUint32());

    double low[3], high[3];
    stream.readPoint3d64(low);
    stream.readPoint3d64(high);
    h.contentRange = dqGeom::Range3d::CreateXYZXYZ(low[0], low[1], low[2],
                                                   high[0], high[1], high[2]);

    h.tolerance = stream.readFloat64();
    h.numElementsIncluded = stream.readUint32();
    h.numElementsExcluded = stream.readUint32();
    h.tileLength = stream.readUint32();

    // Empty sub-volume bit field introduced in format v02.00 (:91-92).
    h.emptySubRanges = h.versionMajor() >= 2 ? stream.readUint32() : 0;

    // Skip any unprocessed bytes in the header (:94-97).
    size_t const remainingHeaderBytes =
        h.headerLength > stream.curPos() ? h.headerLength - stream.curPos() : 0;
    stream.advance(remainingHeaderBytes);

    if (stream.isPastTheEnd())
        h.format = 0;  // invalidate()

    return h;
}

bool ImdlFeatureTableHeader::readFrom(ImdlByteStream& stream, ImdlFeatureTableHeader& out)
{
    // Ported from: FeatureTableHeader.readFrom (IModelTileIO.ts:116-121).
    out.length = stream.readUint32();
    out.numSubCategories = stream.readUint32();
    out.count = stream.readUint32();
    return !stream.isPastTheEnd();
}

END_DQ_RENDER_NAMESPACE
