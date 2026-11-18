// Assert-based self-check of the EAPI backend against tools/MockEapi.
// Build and run from the repository root (x64 Developer Command Prompt):
//   cl /nologo /LD /EHsc /O2 tools\MockEapi\MockEapi.cpp /Fe:EApi.dll
//   cl /nologo /EHsc /std:c++20 /utf-8 /I EapiGui tools\SelfTest\SelfTest.cpp EapiGui\EapiLibrary.cpp EapiGui\EapiReader.cpp user32.lib /Fe:SelfTest.exe
//   SelfTest.exe

#undef NDEBUG
#include "EapiLibrary.h"
#include "EapiReader.h"
#include <cassert>
#include <cstdio>

using namespace eapi;

static std::wstring ValueOf(std::vector<Section> const& sections, std::wstring const& name)
{
    for (auto const& s : sections)
        for (auto const& e : s.entries)
            if (e.name == name) return e.value;
    return L"<missing>";
}

int main()
{
    // ---- text helpers ----
    assert(ParseHex32(L" 0x50 ") == 0x50u);
    assert(ParseHex32(L"3ff") == 0x3FFu);
    assert(!ParseHex32(L""));
    assert(!ParseHex32(L"0x"));
    assert(!ParseHex32(L"12g"));
    assert(!ParseHex32(L"123456789"));
    assert((ParseHexBytes(L"01 0xff,a 1234") == std::vector<std::uint8_t>{ 0x01, 0xFF, 0x0A, 0x12, 0x34 }));
    assert(ParseHexBytes(L"  ")->empty());
    assert(!ParseHexBytes(L"123"));
    assert(!ParseHexBytes(L"zz"));
    assert(HexDump({ 0x41, 0x00 }, 0x10) == L"00000010  41 00                                            |A.|\r\n");

    // ---- spec encodings ----
    static_assert(I2CEncode7BitAddr(0x50) == 0xA0);
    static_assert(I2CEncode10BitAddr(0x3FF) == 0xF6FF);
    static_assert(I2CEncodeExtCmd(0x1234) == 0x80001234);

    // ---- set/get round trips against the mock ----
    Library lib;
    std::wstring error;
    assert(lib.Load(L"EApi.dll", error));
    std::uint32_t v = 0, w = 0;

    assert(Call(lib.VgaSetBacklightEnable, ID_BACKLIGHT_1, BACKLIGHT_SET_OFF) == EAPI_STATUS_SUCCESS);
    assert(Call(lib.VgaSetBacklightBrightness, ID_BACKLIGHT_1, 17u) == EAPI_STATUS_SUCCESS);
    assert(Call(lib.VgaGetBacklightBrightness, ID_BACKLIGHT_1, &v) == EAPI_STATUS_SUCCESS && v == 17);
    assert(ValueOf(ReadAll(lib), L"Backlight 1 enabled") == L"Off");
    assert(Call(lib.VgaSetBacklightEnable, ID_BACKLIGHT_1, BACKLIGHT_SET_ON) == EAPI_STATUS_SUCCESS);
    assert(ValueOf(ReadAll(lib), L"Backlight 1 enabled") == L"On");

    std::vector<std::uint8_t> data{ 1, 2, 3, 4 }, back(4);
    assert(Call(lib.StorageAreaWrite, ID_STORAGE_STD, 16u, data.data(), 4u) == EAPI_STATUS_SUCCESS);
    assert(Call(lib.StorageAreaRead, ID_STORAGE_STD, 16u, back.data(), 4u, 4u) == EAPI_STATUS_SUCCESS && back == data);

    std::uint32_t const eeprom = I2CEncode7BitAddr(0x50);
    assert(Call(lib.I2CProbeDevice, ID_I2C_EXTERNAL, eeprom) == EAPI_STATUS_SUCCESS);
    assert(Call(lib.I2CProbeDevice, ID_I2C_EXTERNAL, I2CEncode7BitAddr(0x51)) == EAPI_STATUS_NOT_FOUND);
    assert(Call(lib.I2CWriteTransfer, ID_I2C_EXTERNAL, eeprom, I2CEncodeStdCmd(0x20), data.data(), 4u) == EAPI_STATUS_SUCCESS);
    assert(Call(lib.I2CReadTransfer, ID_I2C_EXTERNAL, eeprom, I2CEncodeStdCmd(0x20), back.data(), 4u, 4u) == EAPI_STATUS_SUCCESS && back == data);
    // Not exported by the mock -> unsupported instead of a crash.
    assert(!lib.I2CWriteReadRaw);
    assert(Call(lib.I2CWriteReadRaw, ID_I2C_EXTERNAL, eeprom, data.data(), 5u, back.data(), 4u, 5u) == EAPI_STATUS_UNSUPPORTED);

    std::vector<GpioPin> pins;
    assert(ReadGpio(lib, pins) == EAPI_STATUS_SUCCESS && pins.size() == 8);
    assert(pins[4].number == 4 && pins[4].canOutput && !pins[4].isInput);
    assert(Call(lib.GPIOSetLevel, pins[4].id, pins[4].bit, pins[4].bit) == EAPI_STATUS_SUCCESS);
    assert(Call(lib.GPIOSetDirection, pins[0].id, pins[0].bit, 0u) == EAPI_STATUS_INVALID_DIRECTION); // input only
    assert(Call(lib.GPIOSetDirection, pins[5].id, pins[5].bit, pins[5].bit) == EAPI_STATUS_SUCCESS);  // -> input
    assert(ReadGpio(lib, pins) == EAPI_STATUS_SUCCESS && pins[4].isHigh && pins[5].isInput);

    assert(Call(lib.WDogGetCap, &v, &w, &v) == EAPI_STATUS_SUCCESS);
    assert(Call(lib.WDogStart, 0u, 0u, 1000u) == EAPI_STATUS_SUCCESS);
    assert(Call(lib.WDogStart, 0u, 0u, 1000u) == EAPI_STATUS_RUNNING);
    assert(Call(lib.WDogTrigger) == EAPI_STATUS_SUCCESS);
    assert(Call(lib.WDogStop) == EAPI_STATUS_SUCCESS);

    lib.Unload();
    assert(!lib.IsLoaded() && !lib.BoardGetValue);
    std::puts("SelfTest OK");
    return 0;
}
