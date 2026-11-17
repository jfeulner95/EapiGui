// Mock PICMG EAPI library for testing EapiGui without EAPI hardware.
// Build (x64 Developer Command Prompt):  cl /LD /EHsc /O2 MockEapi.cpp /Fe:EApi.dll
// Deliberately leaves some features unsupported and does not export the
// watchdog functions, so "Not supported" handling can be checked.

#include <cstdint>
#include <cstring>

using Status = std::uint32_t;
constexpr Status SUCCESS = 0, UNSUPPORTED = 0xFFFFFCFF, MORE_DATA = 0xFFFFF9FF, INVALID_PARAMETER = 0xFFFFFEFF;

#define EXPORT extern "C" __declspec(dllexport)

static bool g_init = false;

EXPORT Status EApiLibInitialize() { if (g_init) return 0xFFFFFFFE; g_init = true; return SUCCESS; }
EXPORT Status EApiLibUnInitialize() { g_init = false; return SUCCESS; }

EXPORT Status EApiBoardGetStringA(std::uint32_t id, char* buf, std::uint32_t* len)
{
    if (!len) return INVALID_PARAMETER;
    char const* s = nullptr;
    switch (id)
    {
    case 0: s = "Mock Embedded GmbH"; break;
    case 1: s = "COMe-Mock-TGL"; break;
    case 2: s = "A2"; break;
    case 3: s = "SN1234567890-THIS-IS-A-LONG-SERIAL-NUMBER-TO-TEST-THE-MORE-DATA-PATH-OF-THE-READER-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-0123456789-END"; break;
    case 4: s = "BIOS R1.05"; break;
    case 6: s = "COM Express Type 6"; break;
    default: return UNSUPPORTED; // 5 = HW revision unsupported
    }
    std::uint32_t const need = static_cast<std::uint32_t>(std::strlen(s) + 1);
    if (!buf || *len < need) { *len = need; return MORE_DATA; }
    std::memcpy(buf, s, need);
    *len = need;
    return SUCCESS;
}

EXPORT Status EApiBoardGetValue(std::uint32_t id, std::uint32_t* v)
{
    switch (id)
    {
    case 0:       *v = 0x01000000; return SUCCESS;            // spec 1.0
    case 1:       *v = 4711; return SUCCESS;                  // boot counter
    case 2:       *v = 98765; return SUCCESS;                 // minutes
    case 3:       *v = 0xA741; return SUCCESS;                // "PMG" byte-swapped
    case 4:       *v = 0x03000000; return SUCCESS;            // platform rev 3.0
    case 0x10000: *v = 0x02010017; return SUCCESS;            // driver 2.1.23
    case 0x10001: *v = 0x01030005; return SUCCESS;            // lib 1.3.5
    case 0x20000: *v = 2731 + 523; return SUCCESS;            // 52.3 °C
    case 0x20002: *v = 2731 + 381; return SUCCESS;            // 38.1 °C
    case 0x21004: *v = 1050; return SUCCESS;
    case 0x2100C: *v = 3312; return SUCCESS;
    case 0x21010: *v = 3021; return SUCCESS;
    case 0x21014: *v = 5040; return SUCCESS;
    case 0x2101C: *v = 12110; return SUCCESS;
    case 0x22000: *v = 2350; return SUCCESS;
    default:      return UNSUPPORTED;
    }
}

EXPORT Status EApiVgaGetBacklightEnable(std::uint32_t id, std::uint32_t* e)
{
    if (id != 0) return UNSUPPORTED;
    *e = 1; return SUCCESS;
}

EXPORT Status EApiVgaGetBacklightBrightness(std::uint32_t id, std::uint32_t* b)
{
    if (id != 0) return UNSUPPORTED;
    *b = 200; return SUCCESS;
}

EXPORT Status EApiStorageCap(std::uint32_t id, std::uint32_t* size, std::uint32_t* block)
{
    if (id != 0) return UNSUPPORTED;
    *size = 1024; *block = 16; return SUCCESS;
}

EXPORT Status EApiI2CGetBusCap(std::uint32_t id, std::uint32_t* maxLen)
{
    if (id != 0) return UNSUPPORTED;
    *maxLen = 32; return SUCCESS;
}

EXPORT Status EApiGPIOGetDirectionCaps(std::uint32_t id, std::uint32_t* in, std::uint32_t* out)
{
    if (id != 0x10000) return UNSUPPORTED;
    *in = 0xFF; *out = 0xF0; return SUCCESS;
}

EXPORT Status EApiGPIOGetDirection(std::uint32_t id, std::uint32_t mask, std::uint32_t* dir)
{
    if (id != 0x10000) return UNSUPPORTED;
    *dir = 0x0F & mask; return SUCCESS; // 0-3 input, 4-7 output
}

EXPORT Status EApiGPIOGetLevel(std::uint32_t id, std::uint32_t mask, std::uint32_t* level)
{
    if (id != 0x10000) return UNSUPPORTED;
    *level = 0x5A & mask; return SUCCESS;
}

// EApiWDogGetCap intentionally not exported.
