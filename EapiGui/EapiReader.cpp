#include "pch.h"
#include "EapiReader.h"
#include "EapiLibrary.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <functional>

namespace eapi
{
    namespace
    {
        std::wstring Format(wchar_t const* fmt, ...)
        {
            wchar_t buffer[256];
            va_list args;
            va_start(args, fmt);
            int const n = _vsnwprintf_s(buffer, _countof(buffer), _TRUNCATE, fmt, args);
            va_end(args);
            return n < 0 ? std::wstring(buffer) : std::wstring(buffer, static_cast<size_t>(n));
        }

        std::wstring Hex(std::uint32_t v) { return Format(L"0x%08X", v); }

        std::wstring Widen(std::string const& s)
        {
            if (s.empty()) return {};
            int const n = ::MultiByteToWideChar(CP_ACP, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
            std::wstring w(static_cast<size_t>(n), L'\0');
            ::MultiByteToWideChar(CP_ACP, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
            return w;
        }

        Entry Ok(std::wstring name, std::wstring value, std::wstring detail = {})
        {
            return { std::move(name), std::move(value), EntryState::Ok, std::move(detail) };
        }

        Entry Unsupported(std::wstring name, std::wstring detail)
        {
            return { std::move(name), NotSupportedText, EntryState::NotSupported, std::move(detail) };
        }

        Entry Missing(std::wstring name, char const* function)
        {
            return Unsupported(std::move(name), L"Function " + Widen(function) + L" is not exported by the EAPI library");
        }

        // Maps a failed status to an entry: EAPI_STATUS_UNSUPPORTED -> "Not supported", rest -> error.
        Entry Failed(std::wstring name, Status status)
        {
            if (status == EAPI_STATUS_UNSUPPORTED)
                return Unsupported(std::move(name), L"EAPI_STATUS_UNSUPPORTED (" + Hex(status) + L")");
            return { std::move(name), L"Error: " + StatusToString(status), EntryState::Error, Hex(status) };
        }

        // ---- value formatters ----------------------------------------------------
        using Formatter = std::function<std::wstring(std::uint32_t)>;

        std::wstring FmtNumber(std::uint32_t v) { return std::to_wstring(v); }

        std::wstring FmtSpecVersion(std::uint32_t v) { return Format(L"%u.%u", VerMajor(v), VerMinor(v)); }

        std::wstring FmtVersion(std::uint32_t v) { return Format(L"%u.%u.%u", VerMajor(v), VerMinor(v), VerBuild(v)); }

        std::wstring FmtTemperature(std::uint32_t v)
        {
            // EAPI encodes temperatures in 0.1 Kelvin (EAPI_KELVINS_OFFSET = 2731).
            double const celsius = (static_cast<double>(v) - 2731.0) / 10.0;
            return Format(L"%.1f °C", celsius);
        }

        std::wstring FmtVoltage(std::uint32_t v) { return Format(L"%.3f V", v / 1000.0); }

        std::wstring FmtRpm(std::uint32_t v) { return Format(L"%u RPM", v); }

        std::wstring FmtMinutes(std::uint32_t v)
        {
            return Format(L"%u min (%u d %u h %u min)", v, v / 1440, (v / 60) % 24, v % 60);
        }

        std::wstring FmtPnpId(std::uint32_t v)
        {
            // Compressed EISA ID, stored byte-swapped in the low 16 bits.
            std::uint32_t const w = ((v & 0xFF) << 8) | ((v >> 8) & 0xFF);
            wchar_t const c1 = static_cast<wchar_t>(L'A' - 1 + ((w >> 10) & 0x1F));
            wchar_t const c2 = static_cast<wchar_t>(L'A' - 1 + ((w >> 5) & 0x1F));
            wchar_t const c3 = static_cast<wchar_t>(L'A' - 1 + (w & 0x1F));
            auto valid = [](wchar_t c) { return c >= L'A' && c <= L'Z'; };
            if (valid(c1) && valid(c2) && valid(c3))
                return Format(L"%c%c%c (0x%04X)", c1, c2, c3, v & 0xFFFF);
            return Hex(v);
        }

        // ---- readers -------------------------------------------------------------
        Entry ReadString(Library const& lib, Id id, std::wstring name)
        {
            if (!lib.BoardGetStringA)
                return Missing(std::move(name), "EApiBoardGetStringA");

            std::string buffer(256, '\0');
            for (int attempt = 0; attempt < 3; ++attempt)
            {
                auto len = static_cast<std::uint32_t>(buffer.size());
                Status const status = lib.BoardGetStringA(id, buffer.data(), &len);
                if (status == EAPI_STATUS_MORE_DATA && len > buffer.size())
                {
                    buffer.assign(len, '\0');
                    continue;
                }
                if (status != EAPI_STATUS_SUCCESS)
                    return Failed(std::move(name), status);

                buffer.resize(strnlen(buffer.data(), buffer.size()));
                return Ok(std::move(name), Widen(buffer));
            }
            return Failed(std::move(name), EAPI_STATUS_MORE_DATA);
        }

        Entry ReadValue(Library const& lib, Id id, std::wstring name, Formatter const& fmt)
        {
            if (!lib.BoardGetValue)
                return Missing(std::move(name), "EApiBoardGetValue");

            std::uint32_t value = 0;
            Status const status = lib.BoardGetValue(id, &value);
            if (status != EAPI_STATUS_SUCCESS)
                return Failed(std::move(name), status);
            return Ok(std::move(name), fmt(value), L"Raw: " + Hex(value));
        }

        Section ReadLibrarySection(Library const& lib)
        {
            Section s{ L"EAPI Library", {} };
            s.entries.push_back(Ok(L"Library path", lib.LoadedPath()));
            s.entries.push_back(Ok(L"EApiLibInitialize",
                lib.InitStatus() == EAPI_STATUS_INITIALIZED ? L"Success (already initialized)" : L"Success"));
            s.entries.push_back(ReadValue(lib, ID_GET_EAPI_SPEC_VERSION, L"EAPI specification version", FmtSpecVersion));
            s.entries.push_back(ReadValue(lib, ID_BOARD_LIB_VERSION_VAL, L"Library version", FmtVersion));
            s.entries.push_back(ReadValue(lib, ID_BOARD_DRIVER_VERSION_VAL, L"Driver version", FmtVersion));
            return s;
        }

        Section ReadBoardSection(Library const& lib)
        {
            Section s{ L"Board Information", {} };
            s.entries.push_back(ReadString(lib, ID_BOARD_MANUFACTURER_STR, L"Manufacturer"));
            s.entries.push_back(ReadString(lib, ID_BOARD_NAME_STR, L"Board name"));
            s.entries.push_back(ReadString(lib, ID_BOARD_REVISION_STR, L"Board revision"));
            s.entries.push_back(ReadString(lib, ID_BOARD_SERIAL_STR, L"Serial number"));
            s.entries.push_back(ReadString(lib, ID_BOARD_BIOS_REVISION_STR, L"BIOS revision"));
            s.entries.push_back(ReadString(lib, ID_BOARD_HW_REVISION_STR, L"Hardware revision"));
            s.entries.push_back(ReadString(lib, ID_BOARD_PLATFORM_TYPE_STR, L"Platform type"));
            s.entries.push_back(ReadValue(lib, ID_BOARD_PLATFORM_REV_VAL, L"Platform specification revision", FmtSpecVersion));
            s.entries.push_back(ReadValue(lib, ID_BOARD_PNPID_VAL, L"PNP ID", FmtPnpId));
            s.entries.push_back(ReadValue(lib, ID_BOARD_BOOT_COUNTER_VAL, L"Boot counter", FmtNumber));
            s.entries.push_back(ReadValue(lib, ID_BOARD_RUNNING_TIME_METER_VAL, L"Running time meter", FmtMinutes));
            return s;
        }

        Section ReadHwMonSection(Library const& lib)
        {
            Section s{ L"Hardware Monitor", {} };
            s.entries.push_back(ReadValue(lib, ID_HWMON_CPU_TEMP, L"CPU temperature", FmtTemperature));
            s.entries.push_back(ReadValue(lib, ID_HWMON_CHIPSET_TEMP, L"Chipset temperature", FmtTemperature));
            s.entries.push_back(ReadValue(lib, ID_HWMON_SYSTEM_TEMP, L"System temperature", FmtTemperature));
            s.entries.push_back(ReadValue(lib, ID_HWMON_VOLTAGE_VCORE, L"Voltage VCore", FmtVoltage));
            s.entries.push_back(ReadValue(lib, ID_HWMON_VOLTAGE_2V5, L"Voltage 2.5 V", FmtVoltage));
            s.entries.push_back(ReadValue(lib, ID_HWMON_VOLTAGE_3V3, L"Voltage 3.3 V", FmtVoltage));
            s.entries.push_back(ReadValue(lib, ID_HWMON_VOLTAGE_VBAT, L"Voltage VBAT", FmtVoltage));
            s.entries.push_back(ReadValue(lib, ID_HWMON_VOLTAGE_5V, L"Voltage 5 V", FmtVoltage));
            s.entries.push_back(ReadValue(lib, ID_HWMON_VOLTAGE_5VSB, L"Voltage 5 V standby", FmtVoltage));
            s.entries.push_back(ReadValue(lib, ID_HWMON_VOLTAGE_12V, L"Voltage 12 V", FmtVoltage));
            s.entries.push_back(ReadValue(lib, ID_HWMON_FAN_CPU, L"CPU fan", FmtRpm));
            s.entries.push_back(ReadValue(lib, ID_HWMON_FAN_SYSTEM, L"System fan", FmtRpm));
            return s;
        }

        Section ReadBacklightSection(Library const& lib)
        {
            Section s{ L"Backlight", {} };
            for (Id id : { ID_BACKLIGHT_1, ID_BACKLIGHT_2, ID_BACKLIGHT_3 })
            {
                std::wstring const prefix = Format(L"Backlight %u", id + 1);

                if (!lib.VgaGetBacklightEnable)
                    s.entries.push_back(Missing(prefix + L" enabled", "EApiVgaGetBacklightEnable"));
                else
                {
                    std::uint32_t enable = 0;
                    Status const st = lib.VgaGetBacklightEnable(id, &enable);
                    s.entries.push_back(st == EAPI_STATUS_SUCCESS
                        ? Ok(prefix + L" enabled", enable ? L"On" : L"Off", L"Raw: " + Hex(enable))
                        : Failed(prefix + L" enabled", st));
                }

                if (!lib.VgaGetBacklightBrightness)
                    s.entries.push_back(Missing(prefix + L" brightness", "EApiVgaGetBacklightBrightness"));
                else
                {
                    std::uint32_t bright = 0;
                    Status const st = lib.VgaGetBacklightBrightness(id, &bright);
                    s.entries.push_back(st == EAPI_STATUS_SUCCESS
                        ? Ok(prefix + L" brightness", Format(L"%u / 255 (%u %%)", bright, (bright * 100 + 127) / 255))
                        : Failed(prefix + L" brightness", st));
                }
            }
            return s;
        }

        Section ReadStorageSection(Library const& lib)
        {
            Section s{ L"User Storage (EEPROM)", {} };
            if (!lib.StorageCap)
            {
                s.entries.push_back(Missing(L"Storage size", "EApiStorageCap"));
                s.entries.push_back(Missing(L"Block length", "EApiStorageCap"));
                return s;
            }
            std::uint32_t size = 0, block = 0;
            Status const st = lib.StorageCap(ID_STORAGE_STD, &size, &block);
            if (st != EAPI_STATUS_SUCCESS)
            {
                s.entries.push_back(Failed(L"Storage size", st));
                s.entries.push_back(Failed(L"Block length", st));
                return s;
            }
            s.entries.push_back(Ok(L"Storage size", Format(L"%u bytes", size)));
            s.entries.push_back(Ok(L"Block length", Format(L"%u bytes", block)));
            return s;
        }

        Section ReadI2CSection(Library const& lib)
        {
            Section s{ L"I2C Buses", {} };
            struct Bus { Id id; wchar_t const* name; };
            for (Bus const bus : { Bus{ ID_I2C_EXTERNAL, L"External I2C" },
                                   Bus{ ID_I2C_LVDS_1, L"LVDS 1 (DDC)" },
                                   Bus{ ID_I2C_LVDS_2, L"LVDS 2 (DDC)" } })
            {
                std::wstring name = std::wstring(bus.name) + L" max. block length";
                if (!lib.I2CGetBusCap)
                {
                    s.entries.push_back(Missing(std::move(name), "EApiI2CGetBusCap"));
                    continue;
                }
                std::uint32_t maxLen = 0;
                Status const st = lib.I2CGetBusCap(bus.id, &maxLen);
                s.entries.push_back(st == EAPI_STATUS_SUCCESS
                    ? Ok(std::move(name), Format(L"%u bytes", maxLen))
                    : Failed(std::move(name), st));
            }
            return s;
        }

        Section ReadGpioSection(Library const& lib)
        {
            Section s{ L"GPIO", {} };
            if (!lib.GPIOGetDirectionCaps)
            {
                s.entries.push_back(Missing(L"GPIO bank 0", "EApiGPIOGetDirectionCaps"));
                return s;
            }

            constexpr std::uint32_t maxBanks = 4;
            for (std::uint32_t bank = 0; bank < maxBanks; ++bank)
            {
                Id const bankId = GpioBankId(bank);
                std::wstring const bankName = Format(L"Bank %u", bank);

                std::uint32_t inputs = 0, outputs = 0;
                Status const st = lib.GPIOGetDirectionCaps(bankId, &inputs, &outputs);
                if (st != EAPI_STATUS_SUCCESS)
                {
                    // Bank 0 is always shown; further banks only when present.
                    if (bank == 0)
                        s.entries.push_back(Failed(bankName + L" capabilities", st));
                    break;
                }

                std::uint32_t const mask = inputs | outputs;
                s.entries.push_back(Ok(bankName + L" input capable", Hex(inputs)));
                s.entries.push_back(Ok(bankName + L" output capable", Hex(outputs)));
                if (mask == 0)
                    continue;

                std::uint32_t direction = 0, level = 0;
                Status dirSt = lib.GPIOGetDirection ? lib.GPIOGetDirection(bankId, mask, &direction) : EAPI_STATUS_UNSUPPORTED;
                Status lvlSt = lib.GPIOGetLevel ? lib.GPIOGetLevel(bankId, mask, &level) : EAPI_STATUS_UNSUPPORTED;

                for (std::uint32_t pin = 0; pin < 32; ++pin)
                {
                    std::uint32_t const bit = 1u << pin;
                    if (!(mask & bit))
                        continue;

                    std::wstring const pinName = Format(L"GPIO %u", bank * 32 + pin);
                    if (dirSt != EAPI_STATUS_SUCCESS && lvlSt != EAPI_STATUS_SUCCESS)
                    {
                        s.entries.push_back(Failed(pinName, dirSt));
                        continue;
                    }
                    // EAPI: direction bit 1 = input, 0 = output.
                    std::wstring const dir = dirSt == EAPI_STATUS_SUCCESS ? ((direction & bit) ? L"Input" : L"Output") : L"Direction ?";
                    std::wstring const lvl = lvlSt == EAPI_STATUS_SUCCESS ? ((level & bit) ? L"High" : L"Low") : L"Level ?";
                    s.entries.push_back(Ok(pinName, dir + L", " + lvl));
                }
            }
            return s;
        }

        Section ReadWatchdogSection(Library const& lib)
        {
            Section s{ L"Watchdog", {} };
            if (!lib.WDogGetCap)
            {
                s.entries.push_back(Missing(L"Watchdog", "EApiWDogGetCap"));
                return s;
            }
            std::uint32_t maxDelay = 0, maxEvent = 0, maxReset = 0;
            Status const st = lib.WDogGetCap(&maxDelay, &maxEvent, &maxReset);
            if (st != EAPI_STATUS_SUCCESS)
            {
                s.entries.push_back(Failed(L"Watchdog", st));
                return s;
            }
            s.entries.push_back(Ok(L"Max. delay", Format(L"%u ms", maxDelay)));
            s.entries.push_back(Ok(L"Max. event timeout", Format(L"%u ms", maxEvent)));
            s.entries.push_back(Ok(L"Max. reset timeout", Format(L"%u ms", maxReset)));
            return s;
        }
    }

    std::wstring StatusToString(Status status)
    {
        switch (status)
        {
        case EAPI_STATUS_SUCCESS:                 return L"SUCCESS";
        case EAPI_STATUS_NOT_INITIALIZED:         return L"NOT_INITIALIZED";
        case EAPI_STATUS_INITIALIZED:             return L"INITIALIZED";
        case EAPI_STATUS_ALLOC_ERROR:             return L"ALLOC_ERROR";
        case EAPI_STATUS_DRIVER_TIMEOUT:          return L"DRIVER_TIMEOUT";
        case EAPI_STATUS_INVALID_PARAMETER:       return L"INVALID_PARAMETER";
        case EAPI_STATUS_INVALID_BLOCK_ALIGNMENT: return L"INVALID_BLOCK_ALIGNMENT";
        case EAPI_STATUS_INVALID_BLOCK_LENGTH:    return L"INVALID_BLOCK_LENGTH";
        case EAPI_STATUS_INVALID_DIRECTION:       return L"INVALID_DIRECTION";
        case EAPI_STATUS_INVALID_BITMASK:         return L"INVALID_BITMASK";
        case EAPI_STATUS_RUNNING:                 return L"RUNNING";
        case EAPI_STATUS_UNSUPPORTED:             return L"UNSUPPORTED";
        case EAPI_STATUS_NOT_FOUND:               return L"NOT_FOUND";
        case EAPI_STATUS_TIMEOUT:                 return L"TIMEOUT";
        case EAPI_STATUS_BUSY_COLLISION:          return L"BUSY_COLLISION";
        case EAPI_STATUS_READ_ERROR:              return L"READ_ERROR";
        case EAPI_STATUS_WRITE_ERROR:             return L"WRITE_ERROR";
        case EAPI_STATUS_MORE_DATA:               return L"MORE_DATA";
        case EAPI_STATUS_ERROR:                   return L"ERROR";
        default:                             return L"Unknown status " + Hex(status);
        }
    }

    std::vector<Section> ReadAll(Library const& lib)
    {
        std::vector<Section> sections;
        if (!lib.IsLoaded())
            return sections;
        sections.push_back(ReadLibrarySection(lib));
        sections.push_back(ReadBoardSection(lib));
        sections.push_back(ReadHwMonSection(lib));
        sections.push_back(ReadBacklightSection(lib));
        sections.push_back(ReadStorageSection(lib));
        sections.push_back(ReadI2CSection(lib));
        sections.push_back(ReadGpioSection(lib));
        sections.push_back(ReadWatchdogSection(lib));
        return sections;
    }

    std::wstring ToText(std::vector<Section> const& sections)
    {
        std::wstring text;
        for (auto const& section : sections)
        {
            text += L"[" + section.title + L"]\r\n";
            for (auto const& e : section.entries)
                text += e.name + L": " + e.value + L"\r\n";
            text += L"\r\n";
        }
        return text;
    }
}
