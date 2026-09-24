// SPDX-License-Identifier: Apache-2.0
// DanQing dqRender — BatchTableHierarchy implementation
// Ported from: itwinjs-core core/frontend/src/internal/tile/BatchedTileIdMap.ts
#include "BatchTableHierarchy.h"

#include <cstring>
#include <sstream>

BEGIN_DQ_RENDER_NAMESPACE

uint64_t BatchTableHierarchy::getElementId(uint32_t batchId) const
{
    if (batchId >= instancesLength || batchId >= classIds.size())
        return 0;

    uint32_t classId = classIds[batchId];
    if (classId >= classes.size())
        return 0;

    auto const& cls = classes[classId];
    // Find the instance index within this class.
    // Count how many instances with the same classId appear before batchId.
    uint32_t instanceIndex = 0;
    for (uint32_t i = 0; i < batchId; ++i) {
        if (classIds[i] == classId)
            instanceIndex++;
    }

    if (instanceIndex >= cls.elementIds.size())
        return 0;

    return cls.elementIds[instanceIndex];
}

uint64_t BatchTableHierarchy::getSubCategoryId(uint32_t batchId) const
{
    if (batchId >= instancesLength || batchId >= classIds.size())
        return 0;

    uint32_t classId = classIds[batchId];
    if (classId >= classes.size())
        return 0;

    auto const& cls = classes[classId];
    uint32_t instanceIndex = 0;
    for (uint32_t i = 0; i < batchId; ++i) {
        if (classIds[i] == classId)
            instanceIndex++;
    }

    if (instanceIndex >= cls.subCategoryIds.size())
        return 0;

    return cls.subCategoryIds[instanceIndex];
}

bool BatchTableHierarchy::isValid() const
{
    return !classes.empty() && instancesLength > 0
        && classIds.size() == instancesLength;
}

// Simple JSON helpers (same style as RealityTileTree.cpp).
static std::string_view findJsonValue(std::string_view json, std::string_view key)
{
    std::string search = std::string("\"") + std::string(key) + "\"";
    auto pos = json.find(search);
    if (pos == std::string_view::npos)
        return {};

    pos = json.find(':', pos + search.size());
    if (pos == std::string_view::npos)
        return {};

    pos++;
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t' || json[pos] == '\n'))
        pos++;

    if (pos >= json.size())
        return {};

    if (json[pos] == '"') {
        auto end = json.find('"', pos + 1);
        return json.substr(pos + 1, end - pos - 1);
    }

    auto end = pos;
    while (end < json.size() && json[end] != ',' && json[end] != '}' && json[end] != '\n')
        end++;
    return json.substr(pos, end - pos);
}

static std::string_view extractJsonObject(std::string_view json, size_t startPos)
{
    auto objStart = json.find('{', startPos);
    if (objStart == std::string_view::npos)
        return {};

    int depth = 0;
    for (size_t i = objStart; i < json.size(); ++i) {
        if (json[i] == '{') depth++;
        else if (json[i] == '}') {
            depth--;
            if (depth == 0)
                return json.substr(objStart, i - objStart + 1);
        }
    }
    return {};
}

static std::string_view extractJsonArray(std::string_view json, size_t startPos)
{
    auto arrStart = json.find('[', startPos);
    if (arrStart == std::string_view::npos)
        return {};

    int depth = 0;
    for (size_t i = arrStart; i < json.size(); ++i) {
        if (json[i] == '[') depth++;
        else if (json[i] == ']') {
            depth--;
            if (depth == 0)
                return json.substr(arrStart, i - arrStart + 1);
        }
    }
    return {};
}

static std::vector<uint64_t> parseUint64Array(std::string_view arr)
{
    std::vector<uint64_t> result;
    // Remove brackets.
    if (arr.empty() || arr[0] != '[')
        return result;

    std::string_view content = arr.substr(1, arr.size() - 2);
    std::string str(content);
    std::stringstream ss(str);
    uint64_t val;
    while (ss >> val) {
        result.push_back(val);
        if (ss.peek() == ',') ss.ignore();
    }
    return result;
}

static std::vector<int32_t> parseInt32Array(std::string_view arr)
{
    std::vector<int32_t> result;
    if (arr.empty() || arr[0] != '[')
        return result;

    std::string_view content = arr.substr(1, arr.size() - 2);
    std::string str(content);
    std::stringstream ss(str);
    int32_t val;
    while (ss >> val) {
        result.push_back(val);
        if (ss.peek() == ',') ss.ignore();
    }
    return result;
}

static std::vector<uint32_t> parseUint32Array(std::string_view arr)
{
    std::vector<uint32_t> result;
    if (arr.empty() || arr[0] != '[')
        return result;

    std::string_view content = arr.substr(1, arr.size() - 2);
    std::string str(content);
    std::stringstream ss(str);
    uint32_t val;
    while (ss >> val) {
        result.push_back(val);
        if (ss.peek() == ',') ss.ignore();
    }
    return result;
}

BatchTableHierarchy parseBatchTableHierarchy(char const* json, size_t jsonSize)
{
    BatchTableHierarchy result;

    if (!json || jsonSize == 0)
        return result;

    std::string_view sv(json, jsonSize);

    // Find the extension object.
    auto extPos = sv.find("3DTILES_batch_table_hierarchy");
    if (extPos == std::string_view::npos)
        return result;

    auto extObj = extractJsonObject(sv, extPos);
    if (extObj.empty())
        return result;

    // Parse instancesLength.
    auto instLenStr = findJsonValue(extObj, "instancesLength");
    if (!instLenStr.empty())
        result.instancesLength = static_cast<uint32_t>(std::stoul(std::string(instLenStr)));

    // Parse classIds array.
    auto classIdsArr = extractJsonArray(extObj, extObj.find("\"classIds\""));
    if (!classIdsArr.empty())
        result.classIds = parseUint32Array(classIdsArr);

    // Parse parentIds array.
    auto parentIdsArr = extractJsonArray(extObj, extObj.find("\"parentIds\""));
    if (!parentIdsArr.empty())
        result.parentIds = parseInt32Array(parentIdsArr);

    // Parse classes array.
    auto classesPos = extObj.find("\"classes\"");
    if (classesPos == std::string_view::npos)
        return result;

    auto classesArr = extractJsonArray(extObj, classesPos);
    if (classesArr.empty())
        return result;

    // Parse each class object in the array.
    std::string_view arrContent = classesArr.substr(1, classesArr.size() - 2);
    size_t pos = 0;
    while (pos < arrContent.size()) {
        auto classObjStart = arrContent.find('{', pos);
        if (classObjStart == std::string_view::npos)
            break;

        auto classObj = extractJsonObject(arrContent, classObjStart);
        if (classObj.empty())
            break;

        BatchTableClass cls;
        cls.name = std::string(findJsonValue(classObj, "name"));

        // Parse instances object.
        auto instancesPos = classObj.find("\"instances\"");
        if (instancesPos != std::string_view::npos) {
            auto instancesObj = extractJsonObject(classObj, instancesPos);
            if (!instancesObj.empty()) {
                auto elemIdsArr = extractJsonArray(instancesObj, instancesObj.find("\"elementId\""));
                if (!elemIdsArr.empty())
                    cls.elementIds = parseUint64Array(elemIdsArr);

                auto subCatIdsArr = extractJsonArray(instancesObj, instancesObj.find("\"subCategoryId\""));
                if (!subCatIdsArr.empty())
                    cls.subCategoryIds = parseUint64Array(subCatIdsArr);
            }
        }

        result.classes.push_back(std::move(cls));
        pos = classObjStart + classObj.size();
    }

    return result;
}

END_DQ_RENDER_NAMESPACE
