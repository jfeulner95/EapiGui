// Mock PICMG EAPI library for testing EapiGui without EAPI hardware.
// Build (x64 Developer Command Prompt):  cl /LD /EHsc /O2 MockEapi.cpp /Fe:EApi.dll
// Keeps state, so set functions can be read back. Deliberately leaves some
// features unsupported and does not export EApiI2CWriteReadRaw, so
// "Not supported" handling can be checked.
//
// Mock hardware: backlight 1 only, 1 KiB storage, GPIO bank 0 with pins 0-7
// (0-3 input only, 4-7 input/output), I2C EEPROM (256 bytes) at 7-bit address
// 0x50 on the external bus, watchdog up to 65535 ms.

#include <cstdint>
#include <cstring>

using Status = std::uint32_t;
constexpr Status SUCCESS = 0, UNSUPPORTED = 0xFFFFFCFF, MORE_DATA = 0xFFFFF9FF, INVALID_PARAMETER = 0xFFFFFEFF,
                 INVALID_BLOCK_LENGTH = 0xFFFFFEFD, INVALID_DIRECTION = 0xFFFFFEFC, INVALID_BITMASK = 0xFFFFFEFB,
                 RUNNING = 0xFFFFFEFA, NOT_FOUND = 0xFFFFFBFF, ERROR_ = 0xFFFFF0FF;
constexpr std::uint32_t BL_ON = 0, BL_OFF = 0xFFFFFFFF, GPIO_BANK0 = 0x10000, GPIO_IN = 0xFF, GPIO_OUT = 0xF0;
constexpr std::uint32_t EEPROM_ADDR = 0x50 << 1; // EAPI-encoded 7-bit address

#define EXPORT extern "C" __declspec(dllexport)

static bool g_init = false;
static std::uint32_t g_blEnable = BL_ON, g_blBright = 200;
static std::uint32_t g_gpioDir = 0x0F, g_gpioLevel = 0x50; // 1 = input
static std::uint8_t g_storage[1024];
static std::uint8_t g_eeprom[256];
static bool g_wdRunning = false;

EXPORT Status EApiLibInitialize()
{
    if (g_init) return 0xFFFFFFFE;
    g_init = true;
    for (int i = 0; i < 256; ++i) g_eeprom[i] = static_cast<std::uint8_t>(i);
    return SUCCESS;
}
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

// ---- Backlight (only ID 0) ----
EXPORT Status EApiVgaGetBacklightEnable(std::uint32_t id, std::uint32_t* e)
{
    if (id != 0) return UNSUPPORTED;
    *e = g_blEnable; return SUCCESS;
}
EXPORT Status EApiVgaSetBacklightEnable(std::uint32_t id, std::uint32_t e)
{
    if (id != 0) return UNSUPPORTED;
    if (e != BL_ON && e != BL_OFF) return INVALID_PARAMETER;
    g_blEnable = e; return SUCCESS;
}
EXPORT Status EApiVgaGetBacklightBrightness(std::uint32_t id, std::uint32_t* b)
{
    if (id != 0) return UNSUPPORTED;
    *b = g_blBright; return SUCCESS;
}
EXPORT Status EApiVgaSetBacklightBrightness(std::uint32_t id, std::uint32_t b)
{
    if (id != 0) return UNSUPPORTED;
    if (b > 255) return INVALID_PARAMETER;
    g_blBright = b; return SUCCESS;
}

// ---- Storage ----
EXPORT Status EApiStorageCap(std::uint32_t id, std::uint32_t* size, std::uint32_t* block)
{
    if (id != 0) return UNSUPPORTED;
    *size = sizeof(g_storage); *block = 16; return SUCCESS;
}
EXPORT Status EApiStorageAreaRead(std::uint32_t id, std::uint32_t off, void* buf, std::uint32_t bufLen, std::uint32_t cnt)
{
    if (id != 0) return UNSUPPORTED;
    if (!buf || !cnt || cnt > bufLen) return INVALID_PARAMETER;
    if (off >= sizeof(g_storage) || cnt > sizeof(g_storage) - off) return INVALID_BLOCK_LENGTH;
    std::memcpy(buf, g_storage + off, cnt); return SUCCESS;
}
EXPORT Status EApiStorageAreaWrite(std::uint32_t id, std::uint32_t off, void* buf, std::uint32_t cnt)
{
    if (id != 0) return UNSUPPORTED;
    if (!buf || !cnt) return INVALID_PARAMETER;
    if (off >= sizeof(g_storage) || cnt > sizeof(g_storage) - off) return INVALID_BLOCK_LENGTH;
    std::memcpy(g_storage + off, buf, cnt); return SUCCESS;
}

// ---- I2C (external bus only) ----
EXPORT Status EApiI2CGetBusCap(std::uint32_t id, std::uint32_t* maxLen)
{
    if (id != 0) return UNSUPPORTED;
    *maxLen = 32; return SUCCESS;
}
EXPORT Status EApiI2CProbeDevice(std::uint32_t id, std::uint32_t addr)
{
    if (id != 0) return UNSUPPORTED;
    return addr == EEPROM_ADDR ? SUCCESS : NOT_FOUND;
}
static Status EepromOffset(std::uint32_t id, std::uint32_t addr, std::uint32_t cmd, std::uint32_t& off)
{
    if (id != 0) return UNSUPPORTED;
    if (addr != EEPROM_ADDR) return NOT_FOUND;
    off = (cmd >> 30) == 1 ? 0 : (cmd & 0xFFFF); // NO_CMD starts at 0
    return SUCCESS;
}
EXPORT Status EApiI2CReadTransfer(std::uint32_t id, std::uint32_t addr, std::uint32_t cmd, void* buf, std::uint32_t bufLen, std::uint32_t cnt)
{
    std::uint32_t off = 0;
    if (Status st = EepromOffset(id, addr, cmd, off)) return st;
    if (!buf || !cnt || cnt > bufLen || cnt > 32) return INVALID_PARAMETER;
    for (std::uint32_t i = 0; i < cnt; ++i) static_cast<std::uint8_t*>(buf)[i] = g_eeprom[(off + i) & 0xFF];
    return SUCCESS;
}
EXPORT Status EApiI2CWriteTransfer(std::uint32_t id, std::uint32_t addr, std::uint32_t cmd, void* buf, std::uint32_t cnt)
{
    std::uint32_t off = 0;
    if (Status st = EepromOffset(id, addr, cmd, off)) return st;
    if (!buf || !cnt || cnt > 32) return INVALID_PARAMETER;
    for (std::uint32_t i = 0; i < cnt; ++i) g_eeprom[(off + i) & 0xFF] = static_cast<std::uint8_t*>(buf)[i];
    return SUCCESS;
}
// EApiI2CWriteReadRaw intentionally not exported.

// ---- GPIO (bank 0 only) ----
EXPORT Status EApiGPIOGetDirectionCaps(std::uint32_t id, std::uint32_t* in, std::uint32_t* out)
{
    if (id != GPIO_BANK0) return UNSUPPORTED;
    *in = GPIO_IN; *out = GPIO_OUT; return SUCCESS;
}
EXPORT Status EApiGPIOGetDirection(std::uint32_t id, std::uint32_t mask, std::uint32_t* dir)
{
    if (id != GPIO_BANK0) return UNSUPPORTED;
    if (mask & ~GPIO_IN) return INVALID_BITMASK;
    *dir = g_gpioDir & mask; return SUCCESS;
}
EXPORT Status EApiGPIOSetDirection(std::uint32_t id, std::uint32_t mask, std::uint32_t dir)
{
    if (id != GPIO_BANK0) return UNSUPPORTED;
    if (mask & ~GPIO_IN) return INVALID_BITMASK;
    if (mask & ~dir & ~GPIO_OUT) return INVALID_DIRECTION; // output requested on an input-only pin
    g_gpioDir = (g_gpioDir & ~mask) | (dir & mask); return SUCCESS;
}
EXPORT Status EApiGPIOGetLevel(std::uint32_t id, std::uint32_t mask, std::uint32_t* level)
{
    if (id != GPIO_BANK0) return UNSUPPORTED;
    if (mask & ~GPIO_IN) return INVALID_BITMASK;
    *level = g_gpioLevel & mask; return SUCCESS;
}
EXPORT Status EApiGPIOSetLevel(std::uint32_t id, std::uint32_t mask, std::uint32_t level)
{
    if (id != GPIO_BANK0) return UNSUPPORTED;
    if (mask & ~GPIO_IN) return INVALID_BITMASK;
    if (mask & g_gpioDir) return INVALID_DIRECTION; // pin is an input
    g_gpioLevel = (g_gpioLevel & ~mask) | (level & mask); return SUCCESS;
}

// ---- Watchdog ----
EXPORT Status EApiWDogGetCap(std::uint32_t* maxDelay, std::uint32_t* maxEvent, std::uint32_t* maxReset)
{
    *maxDelay = 65535; *maxEvent = 0; *maxReset = 65535; return SUCCESS;
}
EXPORT Status EApiWDogStart(std::uint32_t delay, std::uint32_t event, std::uint32_t reset)
{
    if (g_wdRunning) return RUNNING;
    if (delay > 65535 || event != 0 || reset == 0 || reset > 65535) return INVALID_PARAMETER;
    g_wdRunning = true; return SUCCESS;
}
EXPORT Status EApiWDogTrigger() { return g_wdRunning ? SUCCESS : ERROR_; }
EXPORT Status EApiWDogStop() { g_wdRunning = false; return SUCCESS; }
