#pragma once

template <typename T>
inline bool initFormFromEditorId(const char* editorId, T*& instance) {
    auto form = RE::TESForm::LookupByEditorID(editorId);
    if (!form) {
        logger::error("Failed to load in {}.", editorId);
        return false;
    }
    instance = form->As<T>();
    if (instance == nullptr) {
        logger::error(
            "Found {} form is not the appropriate {} type. Got: {}. Check xEdit for conflicts with editor id."
            "clashes.",
            editorId, typeid(T).name(), form->GetFormType());
        return false;
    }
    return true;
}

inline bool initGSFromEditorId(const char* editorId, RE::Setting*& instance) {
    auto gsCollection = RE::GameSettingCollection::GetSingleton();
    if (gsCollection == nullptr) {
        logger::error("Failed to get Game Setting collection during Game setting load.");
        return false;
    }
    instance = gsCollection->GetSetting(editorId);
    if (instance == nullptr) {
        logger::error("Failed to get Failed to load {} from game settings during Hit registration.", editorId);
        return false;
    }
    return true;
}