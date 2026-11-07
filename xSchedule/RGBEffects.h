#pragma once

#include <list>
#include <string>

#include <wx/xml/xml.h>

class RGBEffects {
    wxXmlDocument* _rgbEffects = nullptr;

public:
    RGBEffects();
    ~RGBEffects() {
        if (_rgbEffects != nullptr) {
            delete _rgbEffects;
        }
    }
    std::list<std::string> GetModels(const std::string& ofType);
    wxXmlNode* GetModel(const std::string& model);

    // xLights renamed some model settings in 2026: parm1/2/3 became NumStrings, NodesPerString and StrandsPerString
    // on matrix, tree and sphere models, and CustomWidth/CustomHeight on custom models. Layouts saved before that
    // only have the parm names.
    static long GetStrings(const wxXmlNode* model) { return GetLong(model, "NumStrings", "parm1", 0); }
    static long GetNodesPerString(const wxXmlNode* model) { return GetLong(model, "NodesPerString", "parm2", 0); }
    static long GetStrandsPerString(const wxXmlNode* model) { return GetLong(model, "StrandsPerString", "parm3", 1); }
    static long GetCustomWidth(const wxXmlNode* model) { return GetLong(model, "CustomWidth", "parm1", 0); }
    static long GetCustomHeight(const wxXmlNode* model) { return GetLong(model, "CustomHeight", "parm2", 0); }
    // True when the model's strings run up and down rather than across
    static bool IsVertical(const wxXmlNode* model);

private:
    static long GetLong(const wxXmlNode* model, const wxString& name, const wxString& legacyName, long def);
};
