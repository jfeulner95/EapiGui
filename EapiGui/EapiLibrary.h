#pragma once
// Runtime binding to the vendor's EAPI DLL (LoadLibrary + GetProcAddress).
// Functions the DLL does not export stay nullptr and are reported as
// "Not supported" by the reader, so the tool works with partial implementations.

#include "EapiDefs.h"
#include <string>
#include <vector>
#include <windows.h>

namespace eapi
{
    class Library
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

        PfnBoardGetStringA           BoardGetStringA{};
        PfnBoardGetValue             BoardGetValue{};
        PfnVgaGetBacklightEnable     VgaGetBacklightEnable{};
        PfnVgaGetBacklightBrightness VgaGetBacklightBrightness{};
        PfnStorageCap                StorageCap{};
        PfnI2CGetBusCap              I2CGetBusCap{};
        PfnGPIOGetDirectionCaps      GPIOGetDirectionCaps{};
        PfnGPIOGetDirection          GPIOGetDirection{};
        PfnGPIOGetLevel              GPIOGetLevel{};
        PfnWDogGetCap                WDogGetCap{};

    private:
        bool TryLoad(std::wstring const& path, std::wstring& error);
        void ResetPointers() noexcept;

        HMODULE m_module{};
        std::wstring m_path;
        Status m_initStatus{ EAPI_STATUS_NOT_INITIALIZED };
        PfnLibUnInitialize m_libUnInitialize{};
    };
}
