// Stub: App::Application for FreeCAD UI shell
// Bridges to dqApp::Application for real initialization.
// Original: /Users/xunzhang/Documents/GitHub/FreeCAD/src/App/Application.h
#pragma once

#include <FCGlobal.h>
#include <Base/Parameter.h>
#include <string>
#include <map>

#include <dqApp/Application.h>

namespace App {

class AppExport Application
{
public:
    static Application& GetApplication() { return *_pcSingleton; }

    Base::ParameterManager& GetUserParameter() { return *m_userParam; }
    Base::Reference<Base::ParameterGrp> GetParameterGroupByPath(const char* sName) {
        return m_userParam->GetGroup(sName);
    }
    static std::map<std::string, Base::Reference<Base::ParameterManager>>& GetParameterSetList() {
        static std::map<std::string, Base::Reference<Base::ParameterManager>> list;
        return list;
    }

    static std::string getExecutableName() { return "DisplayTestApp"; }

    Application() {
        m_userParam = Base::ParameterManager::Create();

        // Bridge to dqApp::Application — real initialization.
        // Ported from: itwinjs-core App.ts DisplayTestApp.startup()
        //              calls IModelApp.startup(opts)
        if (!dqApp::Application::Get().isInitialized()) {
            dqApp::Application::Options opts;
            opts.applicationId = "DisplayTestApp";
            opts.applicationVersion = "1.0.0";
            opts.renderSystemOptions.dpiAwareViewports = true;
            opts.renderSystemOptions.antialiasSamples = 4;
            dqApp::Application::Get().Startup(opts);
        }
    }
    virtual ~Application() = default;

    static Application* _pcSingleton;
    Base::Reference<Base::ParameterManager> m_userParam;
    static std::map<std::string, std::string> m_config;
};

inline Application& GetApplication() { return Application::GetApplication(); }

} // namespace App
