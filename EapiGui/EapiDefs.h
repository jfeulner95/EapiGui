#pragma once
// Constants and function signatures of the PICMG EAPI 1.0 specification
// (Embedded Application Programming Interface, COM Express / Qseven / SMARC).
// The vendor DLL is loaded at runtime, so no vendor header or import library is required.

#include <cstdint>

namespace eapi
{
    using Status = std::uint32_t;
    using Id = std::uint32_t;

    // ---- Status codes -------------------------------------------------------
    constexpr Status EAPI_STATUS_SUCCESS                 = 0x00000000;
    constexpr Status EAPI_STATUS_NOT_INITIALIZED         = 0xFFFFFFFF;
    constexpr Status EAPI_STATUS_INITIALIZED             = 0xFFFFFFFE;
    constexpr Status EAPI_STATUS_ALLOC_ERROR             = 0xFFFFFFFD;
    constexpr Status EAPI_STATUS_DRIVER_TIMEOUT          = 0xFFFFFFFC;
    constexpr Status EAPI_STATUS_INVALID_PARAMETER       = 0xFFFFFEFF;
    constexpr Status EAPI_STATUS_INVALID_BLOCK_ALIGNMENT = 0xFFFFFEFE;
    constexpr Status EAPI_STATUS_INVALID_BLOCK_LENGTH    = 0xFFFFFEFD;
    constexpr Status EAPI_STATUS_INVALID_DIRECTION       = 0xFFFFFEFC;
    constexpr Status EAPI_STATUS_INVALID_BITMASK         = 0xFFFFFEFB;
    constexpr Status EAPI_STATUS_RUNNING                 = 0xFFFFFEFA;
    constexpr Status EAPI_STATUS_UNSUPPORTED             = 0xFFFFFCFF;
    constexpr Status EAPI_STATUS_NOT_FOUND               = 0xFFFFFBFF;
    constexpr Status EAPI_STATUS_TIMEOUT                 = 0xFFFFFBFE;
    constexpr Status EAPI_STATUS_BUSY_COLLISION          = 0xFFFFFBFD;
    constexpr Status EAPI_STATUS_READ_ERROR              = 0xFFFFFAFF;
    constexpr Status EAPI_STATUS_WRITE_ERROR             = 0xFFFFFAFE;
    constexpr Status EAPI_STATUS_MORE_DATA               = 0xFFFFF9FF;
    constexpr Status EAPI_STATUS_ERROR                   = 0xFFFFF0FF;

    // ---- Board information strings (EApiBoardGetStringA) ---------------------
    constexpr Id ID_BOARD_MANUFACTURER_STR  = 0;
    constexpr Id ID_BOARD_NAME_STR          = 1;
    constexpr Id ID_BOARD_REVISION_STR      = 2;
    constexpr Id ID_BOARD_SERIAL_STR        = 3;
    constexpr Id ID_BOARD_BIOS_REVISION_STR = 4;
    constexpr Id ID_BOARD_HW_REVISION_STR   = 5;
    constexpr Id ID_BOARD_PLATFORM_TYPE_STR = 6;

    // ---- Board information values (EApiBoardGetValue) ------------------------
    constexpr Id ID_GET_EAPI_SPEC_VERSION        = 0;
    constexpr Id ID_BOARD_BOOT_COUNTER_VAL       = 1;
    constexpr Id ID_BOARD_RUNNING_TIME_METER_VAL = 2;
    constexpr Id ID_BOARD_PNPID_VAL              = 3;
    constexpr Id ID_BOARD_PLATFORM_REV_VAL       = 4;
    constexpr Id ID_BOARD_DRIVER_VERSION_VAL     = 0x10000;
    constexpr Id ID_BOARD_LIB_VERSION_VAL        = 0x10001;

    // Hardware monitor (also EApiBoardGetValue)
    constexpr Id ID_HWMON_CPU_TEMP        = 0x20000; // 0.1 Kelvin
    constexpr Id ID_HWMON_CHIPSET_TEMP    = 0x20001;
    constexpr Id ID_HWMON_SYSTEM_TEMP     = 0x20002;
    constexpr Id ID_HWMON_VOLTAGE_VCORE   = 0x21004; // millivolt
    constexpr Id ID_HWMON_VOLTAGE_2V5     = 0x21008;
    constexpr Id ID_HWMON_VOLTAGE_3V3     = 0x2100C;
    constexpr Id ID_HWMON_VOLTAGE_VBAT    = 0x21010;
    constexpr Id ID_HWMON_VOLTAGE_5V      = 0x21014;
    constexpr Id ID_HWMON_VOLTAGE_5VSB    = 0x21018;
    constexpr Id ID_HWMON_VOLTAGE_12V     = 0x2101C;
    constexpr Id ID_HWMON_FAN_CPU         = 0x22000; // RPM
    constexpr Id ID_HWMON_FAN_SYSTEM      = 0x22001;

    // ---- Backlight ----------------------------------------------------------
    constexpr Id ID_BACKLIGHT_1 = 0;
    constexpr Id ID_BACKLIGHT_2 = 1;
    constexpr Id ID_BACKLIGHT_3 = 2;
    constexpr std::uint32_t BACKLIGHT_SET_ON  = 0; // yes, ON is 0 in the spec
    constexpr std::uint32_t BACKLIGHT_SET_OFF = 0xFFFFFFFF;
    constexpr std::uint32_t BACKLIGHT_BRIGHTEST = 255;

    // ---- Storage ------------------------------------------------------------
    constexpr Id ID_STORAGE_STD = 0;

    // ---- I2C ----------------------------------------------------------------
    constexpr Id ID_I2C_EXTERNAL = 0;
    constexpr Id ID_I2C_LVDS_1   = 1;
    constexpr Id ID_I2C_LVDS_2   = 2;

    constexpr std::uint32_t I2C_NO_CMD = 1u << 30;
    constexpr std::uint32_t I2CEncode7BitAddr(std::uint32_t a) { return (a & 0x7F) << 1; }
    constexpr std::uint32_t I2CEncode10BitAddr(std::uint32_t a) { return (a & 0xFF) | ((a & 0x300) << 1) | 0xF000; }
    constexpr std::uint32_t I2CEncodeStdCmd(std::uint32_t c) { return c & 0xFF; }
    constexpr std::uint32_t I2CEncodeExtCmd(std::uint32_t c) { return (c & 0xFFFF) | (2u << 30); }

    // ---- GPIO ---------------------------------------------------------------
    // IDs 0..15 address single pins (bitmask bit 0 selects the pin); bank IDs address 32 pins.
    constexpr Id GpioBankId(std::uint32_t bank) { return 0x10000u | bank; }
    constexpr std::uint32_t GPIO_INPUT = 1; // direction bit: 1 = input, 0 = output

    // ---- Helpers --------------------------------------------------------------
    // Version encoding: VV.RR.BBBB  (bits 31..24 version, 23..16 revision, 15..0 build)
    constexpr std::uint32_t VerMajor(std::uint32_t v) { return (v >> 24) & 0xFF; }
    constexpr std::uint32_t VerMinor(std::uint32_t v) { return (v >> 16) & 0xFF; }
    constexpr std::uint32_t VerBuild(std::uint32_t v) { return v & 0xFFFF; }

    // ---- Function signatures (all exported with C linkage) --------------------
#if defined(_M_IX86)
#define EAPI_CALLTYPE __stdcall
#else
#define EAPI_CALLTYPE
#endif

    using U32 = std::uint32_t;
    using PfnLibInitialize              = Status(EAPI_CALLTYPE*)();
    using PfnLibUnInitialize            = Status(EAPI_CALLTYPE*)();
    using PfnBoardGetStringA            = Status(EAPI_CALLTYPE*)(Id, char*, U32*);
    using PfnBoardGetValue              = Status(EAPI_CALLTYPE*)(Id, U32*);
    using PfnVgaGetBacklightEnable      = Status(EAPI_CALLTYPE*)(Id, U32*);
    using PfnVgaSetBacklightEnable      = Status(EAPI_CALLTYPE*)(Id, U32);
    using PfnVgaGetBacklightBrightness  = Status(EAPI_CALLTYPE*)(Id, U32*);
    using PfnVgaSetBacklightBrightness  = Status(EAPI_CALLTYPE*)(Id, U32);
    using PfnStorageCap                 = Status(EAPI_CALLTYPE*)(Id, U32*, U32*);
    using PfnStorageAreaRead            = Status(EAPI_CALLTYPE*)(Id, U32 offset, void* buffer, U32 bufLen, U32 byteCnt);
    using PfnStorageAreaWrite           = Status(EAPI_CALLTYPE*)(Id, U32 offset, void* buffer, U32 byteCnt);
    using PfnI2CGetBusCap               = Status(EAPI_CALLTYPE*)(Id, U32*);
    // WriteBCnt / ReadBCnt are "number of bytes plus 1" (the address byte), per spec.
    using PfnI2CWriteReadRaw            = Status(EAPI_CALLTYPE*)(Id, U32 addr, void* wBuf, U32 writeBCnt, void* rBuf, U32 rBufLen, U32 readBCnt);
    using PfnI2CReadTransfer            = Status(EAPI_CALLTYPE*)(Id, U32 addr, U32 cmd, void* buffer, U32 bufLen, U32 byteCnt);
    using PfnI2CWriteTransfer           = Status(EAPI_CALLTYPE*)(Id, U32 addr, U32 cmd, void* buffer, U32 byteCnt);
    using PfnI2CProbeDevice             = Status(EAPI_CALLTYPE*)(Id, U32 addr);
    using PfnWDogGetCap                 = Status(EAPI_CALLTYPE*)(U32* maxDelay, U32* maxEventTimeout, U32* maxResetTimeout);
    using PfnWDogStart                  = Status(EAPI_CALLTYPE*)(U32 delay, U32 eventTimeout, U32 resetTimeout);
    using PfnWDogTrigger                = Status(EAPI_CALLTYPE*)();
    using PfnWDogStop                   = Status(EAPI_CALLTYPE*)();
    using PfnGPIOGetDirectionCaps       = Status(EAPI_CALLTYPE*)(Id, U32* inputs, U32* outputs);
    using PfnGPIOGetDirection           = Status(EAPI_CALLTYPE*)(Id, U32 bitmask, U32* direction);
    using PfnGPIOSetDirection           = Status(EAPI_CALLTYPE*)(Id, U32 bitmask, U32 direction);
    using PfnGPIOGetLevel               = Status(EAPI_CALLTYPE*)(Id, U32 bitmask, U32* level);
    using PfnGPIOSetLevel               = Status(EAPI_CALLTYPE*)(Id, U32 bitmask, U32 level);
}
