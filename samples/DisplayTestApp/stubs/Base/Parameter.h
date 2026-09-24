// Stub: FreeCAD Base::Parameter
// Backed by QSettings for persistence
#pragma once

#include <string>
#include <vector>
#include <map>
#include <QSettings>
#include <Base/Handle.h>

namespace Base {

class ParameterManager;

class ParameterGrp : public Handled {
public:
    using handle = Reference<ParameterGrp>;

    ParameterGrp() = default;
    explicit ParameterGrp(class ParameterManager* mgr) : m_manager(mgr) {}

    void setGroupPath(const std::string& path) { m_groupPath = path; }

    int GetInt(const char* name, int defaultVal = 0) {
        QSettings s("DanQing", "DisplayTestApp");
        s.beginGroup(QString::fromStdString(m_groupPath));
        int val = s.value(QString::fromLatin1(name), defaultVal).toInt();
        s.endGroup();
        return val;
    }

    bool GetBool(const char* name, bool defaultVal = false) {
        QSettings s("DanQing", "DisplayTestApp");
        s.beginGroup(QString::fromStdString(m_groupPath));
        bool val = s.value(QString::fromLatin1(name), defaultVal).toBool();
        s.endGroup();
        return val;
    }

    std::string GetASCII(const char* name, const char* defaultVal = "") {
        QSettings s("DanQing", "DisplayTestApp");
        s.beginGroup(QString::fromStdString(m_groupPath));
        QString val = s.value(QString::fromLatin1(name),
                              QString::fromLatin1(defaultVal ? defaultVal : "")).toString();
        s.endGroup();
        return val.toStdString();
    }

    void SetBool(const char* name, bool val) {
        QSettings s("DanQing", "DisplayTestApp");
        s.beginGroup(QString::fromStdString(m_groupPath));
        s.setValue(QString::fromLatin1(name), val);
        s.endGroup();
    }

    void SetInt(const char* name, int val) {
        QSettings s("DanQing", "DisplayTestApp");
        s.beginGroup(QString::fromStdString(m_groupPath));
        s.setValue(QString::fromLatin1(name), val);
        s.endGroup();
    }

    void SetASCII(const char* name, const char* sValue) {
        QSettings s("DanQing", "DisplayTestApp");
        s.beginGroup(QString::fromStdString(m_groupPath));
        s.setValue(QString::fromLatin1(name), QString::fromUtf8(sValue));
        s.endGroup();
    }

    void Clear() {
        QSettings s("DanQing", "DisplayTestApp");
        s.beginGroup(QString::fromStdString(m_groupPath));
        s.remove("");
        s.endGroup();
    }

    // Ported from: FreeCAD src/Base/Parameter.h:396
    std::vector<std::string> GetASCIIs(const char* sFilter = nullptr) const {
        QSettings s("DanQing", "DisplayTestApp");
        s.beginGroup(QString::fromStdString(m_groupPath));
        QStringList keys = s.childKeys();
        s.endGroup();
        std::vector<std::string> result;
        QString filter = sFilter ? QString::fromLatin1(sFilter) : QString();
        for (const auto& key : keys) {
            if (!filter.isEmpty() && !key.startsWith(filter)) continue;
            QSettings s2("DanQing", "DisplayTestApp");
            s2.beginGroup(QString::fromStdString(m_groupPath));
            QString val = s2.value(key).toString();
            s2.endGroup();
            if (!val.isEmpty()) {
                result.push_back(val.toStdString());
            }
        }
        return result;
    }

    Reference<ParameterGrp> GetGroup(const char* path) {
        auto* grp = new ParameterGrp(m_manager);
        grp->setGroupPath(m_groupPath + "/" + path);
        return Reference<ParameterGrp>(grp);
    }

    ParameterManager* Manager() { return m_manager; }

private:
    class ParameterManager* m_manager = nullptr;
    std::string m_groupPath;
};

class ParameterManager : public ParameterGrp {
public:
    ParameterManager() : ParameterGrp(this) {}
    static Reference<ParameterManager> Create() {
        return Reference<ParameterManager>(new ParameterManager());
    }
    Reference<ParameterGrp> GetGroup(const char* path) {
        auto* grp = new ParameterGrp(this);
        grp->setGroupPath(path);
        return Reference<ParameterGrp>(grp);
    }
};

using handle = Reference<ParameterGrp>;

} // namespace Base

using Base::ParameterGrp;
using Base::ParameterManager;
