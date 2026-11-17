# EapiGui

WinUI 3 / C++/WinRT desktop tool (Windows only) that reads all information exposed by a
PICMG **EAPI** library (Embedded API for COM Express, Qseven, SMARC modules) and shows it in a GUI.
Values the hardware does not provide are shown as **"Not supported"**.

## What is read

| Section | EAPI functions |
|---|---|
| EAPI Library | `EApiLibInitialize`, `EApiBoardGetValue` (spec / library / driver version) |
| Board Information | `EApiBoardGetStringA` (manufacturer, name, revision, serial, BIOS, HW revision, platform) and `EApiBoardGetValue` (boot counter, running time, PNP ID, platform revision) |
| Hardware Monitor | `EApiBoardGetValue` (CPU/chipset/system temperature, voltages, fans) |
| Backlight | `EApiVgaGetBacklightEnable`, `EApiVgaGetBacklightBrightness` |
| User Storage | `EApiStorageCap` |
| I2C Buses | `EApiI2CGetBusCap` |
| GPIO | `EApiGPIOGetDirectionCaps`, `EApiGPIOGetDirection`, `EApiGPIOGetLevel` |
| Watchdog | `EApiWDogGetCap` |

Read-only: the tool never writes GPIOs, storage, backlight or starts the watchdog.

A value is shown as **Not supported** when
- the function returns `EAPI_STATUS_UNSUPPORTED`, or
- the vendor DLL does not export the function at all.

Every other error status is shown in red as `Error: <STATUS>`. Hover a value for raw data / status code.

## How the EAPI DLL is found

The DLL is loaded at runtime (`LoadLibrary` + `GetProcAddress`), so no vendor header or `.lib` is needed.
On start the tool tries `EAPI.dll`, `EApi_1.dll`, `EApi64.dll` via the normal DLL search path
(exe folder, System32, `PATH`). If your vendor uses another name, enter the path or use **Browse...**.

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

`tools/MockEapi/MockEapi.cpp` is a fake EAPI DLL with some features deliberately unsupported:

```bash
cl /LD /EHsc /O2 MockEapi.cpp /Fe:EApi.dll
```

Build it in an x64 Developer Command Prompt and select it via **Browse...**.
