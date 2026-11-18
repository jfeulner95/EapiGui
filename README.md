# EapiGui

WinUI 3 / C++/WinRT desktop tool (Windows only) for a PICMG **EAPI 1.0** library (Embedded API for
COM Express, Qseven, SMARC modules). It reads all board information and can use every EAPI function,
including the set/write functions. Values the hardware does not provide are shown as **"Not supported"**.

## Pages

| Page | EAPI functions |
|---|---|
| Overview | `EApiLibInitialize`, `EApiBoardGetStringA`, `EApiBoardGetValue` (versions, board info, hardware monitor), plus the read-only state of all blocks below |
| Backlight | `EApiVgaGet/SetBacklightEnable`, `EApiVgaGet/SetBacklightBrightness` |
| GPIO | `EApiGPIOGetDirectionCaps`, `EApiGPIOGet/SetDirection`, `EApiGPIOGet/SetLevel` (bank IDs, or single-pin IDs 0-15 if no bank is supported) |
| Watchdog | `EApiWDogGetCap`, `EApiWDogStart`, `EApiWDogTrigger`, `EApiWDogStop` |
| Storage | `EApiStorageCap`, `EApiStorageAreaRead`, `EApiStorageAreaWrite` |
| I2C | `EApiI2CGetBusCap`, `EApiI2CProbeDevice`, `EApiI2CReadTransfer`, `EApiI2CWriteTransfer`, `EApiI2CWriteReadRaw` |

The Overview page never writes anything. On the other pages, GPIO and backlight changes are written
when you change them / press **Apply**; storage writes, I2C writes and starting the watchdog ask for
confirmation first. A started watchdog is **not** stopped when the tool closes.

A value is shown as **Not supported** when
- the function returns `EAPI_STATUS_UNSUPPORTED`, or
- the vendor DLL does not export the function at all.

Every other error status is shown in red as `Error: <STATUS>`. Hover a value for raw data / status code.

## How the EAPI DLL is found

The DLL is loaded at runtime (`LoadLibrary` + `GetProcAddress`), so no vendor header or `.lib` is needed.
On start the tool tries `EAPI.dll`, `EApi_1.dll`, `EApi64.dll` via the normal DLL search path
(exe folder, System32, `PATH`). If your vendor uses another name, use **File > Search for EAPI.dll...** (Ctrl+O).

The bitness must match: the x64 build needs a 64-bit EAPI DLL, the x86 build a 32-bit one.

## Build

Requirements: Visual Studio 2026 with "Desktop development with C++" (toolset v145) and Windows SDK 10.0.26100.

```bash
msbuild EapiGui.sln -t:restore -p:RestorePackagesConfig=true
```

```bash
msbuild EapiGui.sln -p:Configuration=Release -p:Platform=x64
```

Or open `EapiGui.sln` in Visual Studio and press F5 (packages are restored automatically).

Output: `x64\Release\EapiGui\`. The app is **unpackaged and self-contained** (Windows App SDK 1.7)
and links the C++ runtime statically (`/MT`). Copy the whole output folder to the target and start
`EapiGui.exe` – nothing has to be installed (no MSIX, no Windows App Runtime, no VC++ Redistributable).
Only the vendor's EAPI driver/DLL must be present. Requires Windows 10 1809 or later.

## Testing without hardware

`tools/MockEapi/MockEapi.cpp` is a fake EAPI DLL that keeps state (so set functions can be read back)
and leaves some features deliberately unsupported. Build it in an x64 Developer Command Prompt and
load it via **File > Search for EAPI.dll...**:

```bash
cl /LD /EHsc /O2 tools\MockEapi\MockEapi.cpp /Fe:EApi.dll
```

`tools/SelfTest/SelfTest.cpp` checks the backend (parsing, encodings, all set/get round trips) against
the mock. Run from the repository root after building the mock:

```bash
cl /EHsc /std:c++20 /utf-8 /I EapiGui tools\SelfTest\SelfTest.cpp EapiGui\EapiLibrary.cpp EapiGui\EapiReader.cpp user32.lib /Fe:SelfTest.exe
SelfTest.exe
```
