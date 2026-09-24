// SPDX-License-Identifier: Apache-2.0
// DanQing dqApp — EntityState and ModelState
//
// Ported from: itwinjs-core core/frontend/src/EntityState.ts
//              core/frontend/src/ModelState.ts
// State objects representing entities and models in the frontend.
#pragma once

#include "Export.h"

#include <dqBase/DqId.h>
#include <dqGeom/Range3d.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace dqApp {

// Base class for entity state.
// Ported from: itwinjs-core EntityState
class DQ_APP_EXPORT EntityState {
public:
    virtual ~EntityState() = default;

    // The Id of this entity.
    dqBase::DqId GetId() const noexcept { return m_id; }

    // The full class name (schema:class).
    const std::string& GetClassFullName() const noexcept { return m_classFullName; }

    // Equality.
    virtual bool equals(const EntityState& other) const noexcept
    {
        return m_id == other.m_id && m_classFullName == other.m_classFullName;
    }

protected:
    EntityState(const dqBase::DqId& id, const std::string& classFullName)
        : m_id(id), m_classFullName(classFullName)
    {
    }

private:
    dqBase::DqId m_id;
    std::string m_classFullName;
};

// Element state.
// Ported from: itwinjs-core ElementState
class DQ_APP_EXPORT ElementState : public EntityState {
public:
    static constexpr const char* ClassName = "Element";

    ElementState(const dqBase::DqId& id, const dqBase::DqId& modelId, const std::string& classFullName = "BisCore:Element")
        : EntityState(id, classFullName), m_modelId(modelId)
    {
    }

    // The model containing this element.
    dqBase::DqId getModelId() const noexcept { return m_modelId; }

    // User label.
    const std::string& GetUserLabel() const noexcept { return m_userLabel; }
    void SetUserLabel(const std::string& label) { m_userLabel = label; }

private:
    dqBase::DqId m_modelId;
    std::string m_userLabel;
};

// Model state.
// Ported from: itwinjs-core ModelState
class DQ_APP_EXPORT ModelState : public EntityState {
public:
    static constexpr const char* ClassName = "Model";

    ModelState(const dqBase::DqId& id, const std::string& name, const std::string& classFullName = "BisCore:Model")
        : EntityState(id, classFullName), m_name(name)
    {
    }

    // Model name.
    const std::string& GetName() const noexcept { return m_name; }

    // Whether this is a geometric model.
    virtual bool IsGeometricModel() const noexcept { return false; }

    // Whether this is a 3d model.
    virtual bool is3d() const noexcept { return false; }

    // Whether this is a 2d model.
    bool Is2d() const noexcept { return !is3d(); }

    // Parent model Id.
    dqBase::DqId GetParentModelId() const noexcept { return m_parentModelId; }
    void SetParentModelId(const dqBase::DqId& id) { m_parentModelId = id; }

private:
    std::string m_name;
    dqBase::DqId m_parentModelId;
};

// Geometric model state (abstract).
// Ported from: itwinjs-core GeometricModelState
class DQ_APP_EXPORT GeometricModelState : public ModelState {
public:
    GeometricModelState(const dqBase::DqId& id, const std::string& name, const std::string& classFullName = "BisCore:GeometricModel")
        : ModelState(id, name, classFullName)
    {
    }

    bool IsGeometricModel() const noexcept override { return true; }

    // The range of geometry in this model.
    const dqGeom::Range3d& GetModelRange() const noexcept { return m_modelRange; }
    void SetModelRange(const dqGeom::Range3d& range) { m_modelRange = range; }

private:
    dqGeom::Range3d m_modelRange;
};

// 3D geometric model state.
// Ported from: itwinjs-core GeometricModel3dState
class DQ_APP_EXPORT GeometricModel3dState : public GeometricModelState {
public:
    GeometricModel3dState(const dqBase::DqId& id, const std::string& name)
        : GeometricModelState(id, name, "BisCore:GeometricModel3d")
    {
    }

    bool is3d() const noexcept override { return true; }
};

// 2D geometric model state.
// Ported from: itwinjs-core GeometricModel2dState
class DQ_APP_EXPORT GeometricModel2dState : public GeometricModelState {
public:
    GeometricModel2dState(const dqBase::DqId& id, const std::string& name)
        : GeometricModelState(id, name, "BisCore:GeometricModel2d")
    {
    }

    bool is3d() const noexcept override { return false; }

    // The origin of the 2D model.
    double GetOriginX() const noexcept { return m_originX; }
    double GetOriginY() const noexcept { return m_originY; }
    void SetOrigin(double x, double y) { m_originX = x; m_originY = y; }

    // The delta (extents) of the 2D model.
    double GetDeltaX() const noexcept { return m_deltaX; }
    double GetDeltaY() const noexcept { return m_deltaY; }
    void SetDelta(double dx, double dy) { m_deltaX = dx; m_deltaY = dy; }

private:
    double m_originX = 0.0, m_originY = 0.0;
    double m_deltaX = 1000.0, m_deltaY = 1000.0;
};

// Spatial model state (3D model in spatial context).
// Ported from: itwinjs-core SpatialModelState
class DQ_APP_EXPORT SpatialModelState : public GeometricModel3dState {
public:
    SpatialModelState(const dqBase::DqId& id, const std::string& name)
        : GeometricModel3dState(id, name)
    {
    }

    // Whether this is a spatial model.
    bool IsSpatialModel() const noexcept { return true; }
};

// Category selector state.
// Ported from: itwinjs-core CategorySelectorState
class DQ_APP_EXPORT CategorySelectorState : public EntityState {
public:
    static constexpr const char* ClassName = "CategorySelector";

    CategorySelectorState(const dqBase::DqId& id, const std::string& name = "")
        : EntityState(id, "BisCore:CategorySelector"), m_name(name)
    {
    }

    // The name of this selector.
    const std::string& GetName() const noexcept { return m_name; }

    // The selected category Ids.
    const std::vector<dqBase::DqId>& GetCategories() const noexcept { return m_categories; }

    // add a category.
    void AddCategory(const dqBase::DqId& categoryId) { m_categories.push_back(categoryId); }

    // Check if a category is selected.
    bool Contains(const dqBase::DqId& categoryId) const
    {
        for (const auto& id : m_categories) {
            if (id == categoryId)
                return true;
        }
        return false;
    }

private:
    std::string m_name;
    std::vector<dqBase::DqId> m_categories;
};

// Model selector state.
// Ported from: itwinjs-core ModelSelectorState
class DQ_APP_EXPORT ModelSelectorState : public EntityState {
public:
    static constexpr const char* ClassName = "ModelSelector";

    ModelSelectorState(const dqBase::DqId& id, const std::string& name = "")
        : EntityState(id, "BisCore:ModelSelector"), m_name(name)
    {
    }

    // The name of this selector.
    const std::string& GetName() const noexcept { return m_name; }

    // The selected model Ids.
    const std::vector<dqBase::DqId>& getModels() const noexcept { return m_models; }

    // add a model.
    void addModel(const dqBase::DqId& modelId) { m_models.push_back(modelId); }

    // Check if a model is selected.
    bool Contains(const dqBase::DqId& modelId) const
    {
        for (const auto& id : m_models) {
            if (id == modelId)
                return true;
        }
        return false;
    }

private:
    std::string m_name;
    std::vector<dqBase::DqId> m_models;
};

}  // namespace dqApp
