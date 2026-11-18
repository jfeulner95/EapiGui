#pragma once
// Runtime binding to the vendor's EAPI DLL (LoadLibrary + GetProcAddress).
// Functions the DLL does not export stay nullptr; Call() reports them as
// EAPI_STATUS_UNSUPPORTED, so the tool works with partial implementations.

#include "EapiDefs.h"
#include <string>
#include <vector>
#include <windows.h>

namespace eapi
{
    // Every EAPI 1.0 function except EApiLibInitialize (called by Library::Load).
    struct Functions
    {
        PfnLibUnInitialize           LibUnInitialize{};
        PfnBoardGetStringA           BoardGetStringA{};
        PfnBoardGetValue             BoardGetValue{};
        PfnVgaGetBacklightEnable     VgaGetBacklightEnable{};
        PfnVgaSetBacklightEnable     VgaSetBacklightEnable{};
        PfnVgaGetBacklightBrightness VgaGetBacklightBrightness{};
        PfnVgaSetBacklightBrightness VgaSetBacklightBrightness{};
        PfnStorageCap                StorageCap{};
        PfnStorageAreaRead           StorageAreaRead{};
        PfnStorageAreaWrite          StorageAreaWrite{};
        PfnI2CGetBusCap              I2CGetBusCap{};
        PfnI2CWriteReadRaw           I2CWriteReadRaw{};
        PfnI2CReadTransfer           I2CReadTransfer{};
        PfnI2CWriteTransfer          I2CWriteTransfer{};
        PfnI2CProbeDevice            I2CProbeDevice{};
        PfnWDogGetCap                WDogGetCap{};
        PfnWDogStart                 WDogStart{};
        PfnWDogTrigger               WDogTrigger{};
        PfnWDogStop                  WDogStop{};
        PfnGPIOGetDirectionCaps      GPIOGetDirectionCaps{};
        PfnGPIOGetDirection          GPIOGetDirection{};
        PfnGPIOSetDirection          GPIOSetDirection{};
        PfnGPIOGetLevel              GPIOGetLevel{};
        PfnGPIOSetLevel              GPIOSetLevel{};
    };

    class Library : public Functions
    {
    public:
        Library() = default;
        ~Library();
        Library(Library const&) = delete;
        Library& operator=(Library const&) = delete;

        // Loads the DLL and calls EApiLibInitialize. An empty path tries
        // DefaultDllNames() in order. Returns false and fills `error` on failure.
        bool Load(std::wstring const& path, std::wstring& error);
        void Unload();

        bool IsLoaded() const noexcept { return m_module != nullptr; }
        std::wstring const& LoadedPath() const noexcept { return m_path; }
        Status InitStatus() const noexcept { return m_initStatus; }

        static std::vector<std::wstring> const& DefaultDllNames();

    private:
        bool TryLoad(std::wstring const& path, std::wstring& error);

        HMODULE m_module{};
        std::wstring m_path;
        Status m_initStatus{ EAPI_STATUS_NOT_INITIALIZED };
    };

    // Calls an EAPI function pointer; a function the DLL does not export counts as unsupported.
    template <typename Fn, typename... Args>
    Status Call(Fn fn, Args... args)
    {
        return fn ? fn(args...) : EAPI_STATUS_UNSUPPORTED;
    }
}
