// Ported from: FreeCAD src/Gui/Language/Translator.h
// Stub: minimal for DTA FirstStart — only the API surface consumed by
// StartGui::GeneralSettingsWidget is provided. The real FreeCAD Translator
// is a QObject singleton that installs QTranslator objects at runtime; this
// stub returns a fixed locale/language map so the Language combo can populate.
//
// Deviations from reference (§0 backend-deferred):
//   - No Qt QTranslator installation; activateLanguage() only records the choice.
//   - supportedLocales() returns a fixed TStringMap built from the same
//     language→locale table FreeCAD ships (Translator.cpp:211-257).
#pragma once

#include <FCGlobal.h>
#include <map>
#include <string>
#include <vector>

namespace Gui
{

using TLanguageList = std::vector<std::string>;
using TStringMap = std::map<std::string, std::string>;

// Ported from: FreeCAD src/Gui/Language/Translator.h:52-117
class GuiExport Translator
{
public:
    // Ported from: FreeCAD src/Gui/Language/Translator.cpp:152-156 (instance)
    static Translator* instance()
    {
        static Translator s_instance;
        return &s_instance;
    }

    // Ported from: FreeCAD src/Gui/Language/Translator.cpp:335-338 (activeLanguage)
    std::string activeLanguage() const { return m_activeLanguage; }

    // Ported from: FreeCAD src/Gui/Language/Translator.cpp:325-333 (activateLanguage)
    // Stub: does not install QTranslator objects; only records the choice.
    void activateLanguage(const char* lang) { m_activeLanguage = lang; }

    // Ported from: FreeCAD src/Gui/Language/Translator.cpp:286-295 (supportedLanguages)
    TLanguageList supportedLanguages() const
    {
        TLanguageList languages;
        const TStringMap locales = supportedLocales();
        languages.reserve(locales.size());
        for (const auto& it : locales) {
            languages.emplace_back(it.first);
        }
        return languages;
    }

    // Ported from: FreeCAD src/Gui/Language/Translator.cpp:297-323 (supportedLocales)
    // Stub: FreeCAD enumerates *_*.qm files in the translate directories and
    // intersects with mapLanguageTopLevelDomain; here we return the full
    // language→locale table FreeCAD ships (Translator.cpp:211-257) as if all
    // qm files were present.
    TStringMap supportedLocales() const
    {
        return {
            {"Afrikaans",              "af"},
            {"Arabic",                 "ar"},
            {"Basque",                 "eu"},
            {"Belarusian",             "be"},
            {"Bulgarian",              "bg"},
            {"Catalan",                "ca"},
            {"Chinese (Simplified)",   "zh-CN"},
            {"Chinese (Traditional)",  "zh-TW"},
            {"Croatian",               "hr"},
            {"Czech",                  "cs"},
            {"Danish",                 "da"},
            {"Dutch",                  "nl"},
            {"English",                "en"},
            {"Filipino",               "fil"},
            {"Finnish",                "fi"},
            {"French",                 "fr"},
            {"Galician",               "gl"},
            {"Georgian",               "ka"},
            {"German",                 "de"},
            {"Greek",                  "el"},
            {"Hungarian",              "hu"},
            {"Indonesian",             "id"},
            {"Italian",                "it"},
            {"Japanese",               "ja"},
            {"Kabyle",                 "kab"},
            {"Korean",                 "ko"},
            {"Lithuanian",             "lt"},
            {"Norwegian",              "no"},
            {"Polish",                 "pl"},
            {"Portuguese (Brazilian)", "pt-BR"},
            {"Portuguese",             "pt-PT"},
            {"Romanian",               "ro"},
            {"Russian",                "ru"},
            {"Serbian",                "sr"},
            {"Serbian (Latin)",        "sr-CS"},
            {"Slovak",                 "sk"},
            {"Slovenian",              "sl"},
            {"Spanish",                "es-ES"},
            {"Spanish (Argentina)",    "es-AR"},
            {"Swedish",                "sv"},
            {"Turkish",                "tr"},
            {"Ukrainian",              "uk"},
            {"Valencian",              "val-ES"},
            {"Vietnamese",             "vi"},
            {"Malay",                  "ms"},
            {"Tamil",                  "ta"},
            {"Irish",                  "ga-IE"},
        };
    }

private:
    Translator() = default;
    ~Translator() = default;
    Translator(const Translator&) = delete;
    Translator& operator=(const Translator&) = delete;

    std::string m_activeLanguage {"English"};
};

}  // namespace Gui
