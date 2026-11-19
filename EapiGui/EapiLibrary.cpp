#include "EapiLibrary.h"
#include "EapiReader.h"

namespace eapi
{
    namespace
    {
        template <typename T>
        T Resolve(HMODULE module, char const* name) noexcept
        {
            return reinterpret_cast<T>(::GetProcAddress(module, name));
        }

        std::wstring LastErrorText(DWORD code)
        {
            wchar_t* buffer = nullptr;
            DWORD const len = ::FormatMessageW(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr, code, 0, reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
            std::wstring text = len ? std::wstring(buffer, len) : L"Win32 error " + std::to_wstring(code);
            if (buffer) ::LocalFree(buffer);
            while (!text.empty() && (text.back() == L'\r' || text.back() == L'\n' || text.back() == L' '))
                text.pop_back();
            return text;
        }

        std::wstring ModulePath(HMODULE module)
        {
            std::wstring path(MAX_PATH, L'\0');
            for (;;)
            {
                DWORD const n = ::GetModuleFileNameW(module, path.data(), static_cast<DWORD>(path.size()));
                if (n < path.size()) { path.resize(n); return path; }
                path.resize(path.size() * 2);
            }
        }
    }

    Library::~Library()
    {
        Unload();
    }

    std::vector<std::wstring> const& Library::DefaultDllNames()
    {
        // Common file names of PICMG EAPI implementations. Vendors differ, so the
        // GUI also accepts an explicit path.
        static std::vector<std::wstring> const names{ L"EAPI.dll", L"EApi_1.dll", L"EApi64.dll" };
        return names;
    }

    bool Library::Load(std::wstring const& path, std::wstring& error)
    {
        Unload();
        if (!path.empty())
            return TryLoad(path, error);

        std::wstring tried;
        for (auto const& name : DefaultDllNames())
        {
            std::wstring ignored;
            if (TryLoad(name, ignored))
                return true;
            tried += (tried.empty() ? L"" : L", ") + name;
        }
        error = L"No EAPI library found (tried " + tried + L"). Select the vendor's EAPI DLL.";
        return false;
    }

    bool Library::TryLoad(std::wstring const& path, std::wstring& error)
    {
        HMODULE module = ::LoadLibraryW(path.c_str());
        if (!module)
        {
            DWORD const code = ::GetLastError();
            error = L"Cannot load \"" + path + L"\": " + LastErrorText(code);
            if (code == ERROR_BAD_EXE_FORMAT)
                error += L" (32/64-bit mismatch between this application and the DLL?)";
            return false;
        }

        auto const libInitialize = Resolve<PfnLibInitialize>(module, "EApiLibInitialize");
        if (!libInitialize)
        {
            ::FreeLibrary(module);
            error = L"\"" + path + L"\" does not export EApiLibInitialize - not an EAPI library.";
            return false;
        }

        Status const status = libInitialize();
        if (status != EAPI_STATUS_SUCCESS && status != EAPI_STATUS_INITIALIZED)
        {
            ::FreeLibrary(module);
            error = L"EApiLibInitialize failed: " + StatusToString(status);
            return false;
        }

        m_module = module;
        m_path = ModulePath(module);
        m_initStatus = status;
        m_missing.clear();
#define EAPI_RESOLVE(name) \
        if (!(name = Resolve<decltype(name)>(module, "EApi" #name))) m_missing += (m_missing.empty() ? L"" : L", ") + std::wstring(L"EApi" #name)
        EAPI_RESOLVE(LibUnInitialize);
        EAPI_RESOLVE(BoardGetStringA);
        EAPI_RESOLVE(BoardGetValue);
        EAPI_RESOLVE(VgaGetBacklightEnable);
        EAPI_RESOLVE(VgaSetBacklightEnable);
        EAPI_RESOLVE(VgaGetBacklightBrightness);
        EAPI_RESOLVE(VgaSetBacklightBrightness);
        EAPI_RESOLVE(StorageCap);
        EAPI_RESOLVE(StorageAreaRead);
        EAPI_RESOLVE(StorageAreaWrite);
        EAPI_RESOLVE(I2CGetBusCap);
        EAPI_RESOLVE(I2CWriteReadRaw);
        EAPI_RESOLVE(I2CReadTransfer);
        EAPI_RESOLVE(I2CWriteTransfer);
        EAPI_RESOLVE(I2CProbeDevice);
        EAPI_RESOLVE(WDogGetCap);
        EAPI_RESOLVE(WDogStart);
        EAPI_RESOLVE(WDogTrigger);
        EAPI_RESOLVE(WDogStop);
        EAPI_RESOLVE(GPIOGetDirectionCaps);
        EAPI_RESOLVE(GPIOGetDirection);
        EAPI_RESOLVE(GPIOSetDirection);
        EAPI_RESOLVE(GPIOGetLevel);
        EAPI_RESOLVE(GPIOSetLevel);
#undef EAPI_RESOLVE
        return true;
    }

    void Library::Unload()
    {
        if (!m_module)
            return;
        // Only uninitialize what we initialized ourselves.
        if (LibUnInitialize && m_initStatus == EAPI_STATUS_SUCCESS)
            LibUnInitialize();
        ::FreeLibrary(m_module);
        m_module = nullptr;
        m_path.clear();
        m_missing.clear();
        m_initStatus = EAPI_STATUS_NOT_INITIALIZED;
        static_cast<Functions&>(*this) = {};
    }

}
