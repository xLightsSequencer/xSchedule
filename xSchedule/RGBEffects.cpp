#include "RGBEffects.h"

#include <wx/filename.h>
#include "xScheduleApp.h"
#include "xScheduleMain.h"

RGBEffects::RGBEffects() {
    std::string showDir = wxGetApp().GetFrame()->GetShowDir();

    _rgbEffects = new wxXmlDocument();
    wxString file = showDir + "/xlights_rgbeffects.xml";
    if (wxFileName::FileExists(file)) {
        _rgbEffects->Load(file);
    }
}

long RGBEffects::GetLong(const wxXmlNode* model, const wxString& name, const wxString& legacyName, long def) {
    wxString value;
    if (!model->GetAttribute(name, &value) && !model->GetAttribute(legacyName, &value)) {
        return def;
    }
    return wxAtol(value);
}

bool RGBEffects::IsVertical(const wxXmlNode* model) {
    wxString displayAs = model->GetAttribute("DisplayAs");
    if (displayAs == "Horiz Matrix") return false;
    if (displayAs == "Matrix") return model->GetAttribute("Vertical", "false") == "true";
    if (displayAs.StartsWith("Tree")) return model->GetAttribute("StrandDir", "Vertical") != "Horizontal";
    return true;
}

std::list<std::string> RGBEffects::GetModels(const std::string& ofType) {
    std::list<std::string> models;
    wxXmlNode* root = _rgbEffects->GetRoot();
    if (root == nullptr) return models;
    for (wxXmlNode* n = root->GetChildren(); n != nullptr; n = n->GetNext()) {
        if (n->GetName() == "models") {
            for (wxXmlNode* nn = n->GetChildren(); nn != nullptr; nn = nn->GetNext()) {
                if (nn->GetName() == "model") {
                    std::string displayAs = nn->GetAttribute("DisplayAs").ToStdString();
                    bool match = (displayAs == ofType);
                    // Handle legacy DisplayAs values: "Horiz Matrix"/"Vert Matrix" -> "Matrix",
                    // "Tree 360"/"Tree Flat"/"Tree Ribbon"/etc. -> "Tree"
                    if (!match && ofType == "Matrix")
                        match = (displayAs == "Horiz Matrix" || displayAs == "Vert Matrix");
                    else if (!match && ofType == "Tree")
                        match = (displayAs.size() > 4 && displayAs.rfind("Tree", 0) == 0 && displayAs[4] == ' ');
                    if (match)
                        models.push_back(nn->GetAttribute("name").ToStdString());
                }
            }
        }
    }
    return models;
}

wxXmlNode* RGBEffects::GetModel(const std::string& model) {
    wxXmlNode* root = _rgbEffects->GetRoot();
    if (root == nullptr) return nullptr;
    for (wxXmlNode* n = root->GetChildren(); n != nullptr; n = n->GetNext()) {
        if (n->GetName() == "models") {
            for (wxXmlNode* nn = n->GetChildren(); nn != nullptr; nn = nn->GetNext()) {
                if (nn->GetName() == "model") {
                    if (nn->GetAttribute("name") == model) {
                        return nn;
                    }
                }
            }
        }
    }
    return nullptr;
}
