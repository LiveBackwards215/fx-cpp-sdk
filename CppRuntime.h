#pragma once

#include "cfx/fxScripting.h"
#include "cfx/core.h"
#include "resource_sdk/Resource.h"
#include "resource_sdk/msgpack.h"

#include <string>
#include <atomic>
#include <cstdlib>

FX_DEFINE_GUID(CLSID_CppRuntime, 0xF3A7B9, 0x241D, 0x5E4C, 0x8A, 0x93, 0x2F, 0xA1, 0xB2, 0xC3, 0xD4, 0xE5);

class CppRuntime final : public fx::OMClass<CppRuntime, IScriptRuntime, IScriptTickRuntime, IScriptEventRuntime, IScriptFileHandlingRuntime>
{
public:
    CppRuntime();
    ~CppRuntime();
    result_t OM_DECL Create (IScriptHost* host) override;
    result_t OM_DECL Destroy() override;
    void* OM_DECL GetParentObject() override { return m_parentObject; }
    void OM_DECL SetParentObject(void*) override;
    int32_t OM_DECL GetInstanceId() override { return m_instanceId; }
    result_t OM_DECL Tick() override;
    result_t OM_DECL TriggerEvent(char* eventName, char* argsSerialized, uint32_t serializedSize, char* sourceId) override;
    int32_t OM_DECL HandlesFile(char* scriptFile, IScriptHostWithResourceData* metadata) override;
    result_t OM_DECL LoadFile(char* scriptFile) override;

private:
    IScriptHost* m_host = nullptr;
    void* m_parentObject = nullptr;
    int32_t m_instanceId = 0;
    void* m_libHandle = nullptr;
    fx::ResourceContext* m_ctx = nullptr;
    std::string m_resourceName;
};
