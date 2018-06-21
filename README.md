lyclMiner
===============

lyclMiner is a high performance OpenCL Lyra2REv2 miner for AMD GCN GPUs.

**Developer:** CryptoGraphics ( CrGraphics@protonmail.com )

Stratum and WorkIO implementations are based on [cpuminer-multi](https://github.com/tpruvot/cpuminer-multi)  
Some kernels(Skein and BMW) are based on cuda and OpenCL kernels from (ccminer and sgminer projects respectively).

This open source release was made possible thanks to [Vertcoin project](https://vertcoin.org) and its community.

## Supported hardware
AMD GPU GCN 1.0 or later.

## Supported platforms
- Windows (Radeon Software Adrenalin Edition)
- Linux (AMDGPU-Pro and ROCm)

Mesa Gallium Compute and macOS are not supported.

## Download
* Binary releases: https://github.com/CryptoGraphics/lyclMiner/releases
* Clone with `git clone https://github.com/CryptoGraphics/lyclMiner.git`  
 Follow [Building lyclMiner](#building).

## Quick start guide

All miner settings are managed through the configuration file.

1. **Generating a configuration file.**
   - Config file can be generated using the following command inside cmd/terminal:  
`./lyclMiner -g lyclMiner.conf`

   - Alternative (Windows).  
   Create a file `GenerateConfig.bat` in the same folder as `lyclMiner.exe` with the following content:  
`lyclMiner -g lyclMiner.conf`
   - **Additional notes:**
     - Configuration file is generated specifically for your GPU and driver setup.
     - Configuration file must be re-generated every time you add/remove a new Device to/from the PCIe slot.
     - If configuration file name is different from `lyclMiner.conf`, then it must be specified manually when starting a miner:
Example: `lyclMiner your_configuration_file`
     - By default miner looks for `lyclMiner.conf` in the same directory as lyclMiner executable.

2. **Configuring a miner.**
Open `lyclMiner.conf` using any text editor and edit `"Url"`, `"Username"` and `"Password"` fields inside a `"Connection"` block.
Additional notes:  
   - It is recommended to adjust `WorkSize` parameter for each `Device` to get better performance.

3. **Start a** `lyclMiner` **executable.**


## There is more

### Comments inside a config file

- Comments can be in C format, e.g. `/* some stuff */`, with a `//` at the start of the line, or in shell format (`#`). 

### Selecting specific devices

By default, all devices are used. However it is possible to select specific ones using a `PCIeBusId` option.
```
#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#
# Device config:
#
# Available platforms:
#
# 1. Platform name: Advanced Micro Devices, Inc.
#    Index: 0
#
# Available devices:
#
# 1. Device: gfx900
#    Board name: Radeon RX Vega
#    PCIe bus ID: 1
#    Available platforms indices: 0
#
# 2. Device: Ellesmere
#    Board name: Radeon RX 580 Series
#    PCIe bus ID: 3
#    Available platforms indices: 0
#
# 3. Device: Ellesmere
#    Board name: Radeon RX 580 Series
#    PCIe bus ID: 5
#    Available platforms indices: 0
#
# 4. Device: Ellesmere
#    Board name: Radeon RX 580 Series
#    PCIe bus ID: 7
#    Available platforms indices: 0
#
#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#-#

<Device0 PCIeBusId = "1" PlatformIndex = "0" BinaryFormat = "amdcl2" AsmProgram = "gfx9" WorkSize = "16777216">
<Device1 PCIeBusId = "3" PlatformIndex = "0" BinaryFormat = "amdcl2" AsmProgram = "gfx8" WorkSize = "8388608">
<Device2 PCIeBusId = "5" PlatformIndex = "0" BinaryFormat = "amdcl2" AsmProgram = "gfx8" WorkSize = "2097152">
<Device3 PCIeBusId = "7" PlatformIndex = "0" BinaryFormat = "amdcl2" AsmProgram = "gfx8" WorkSize = "4194304">
```

For example: We want to use devices only with `PCIeBusId` 3 and 7.  
Comment/backup the original list.
```
/*
<Device0 PCIeBusId = "1" PlatformIndex = "0" BinaryFormat = "amdcl2" AsmProgram = "gfx9" WorkSize = "16777216">
<Device1 PCIeBusId = "3" PlatformIndex = "0" BinaryFormat = "amdcl2" AsmProgram = "gfx8" WorkSize = "8388608">
<Device2 PCIeBusId = "5" PlatformIndex = "0" BinaryFormat = "amdcl2" AsmProgram = "gfx8" WorkSize = "2097152">
<Device3 PCIeBusId = "7" PlatformIndex = "0" BinaryFormat = "amdcl2" AsmProgram = "gfx8" WorkSize = "4194304">
*/
```

Copy selected device configurations and rename blocks, so they will start from 0. e.g `<Device0>`, `<Device1>` ...
```
<Device0 PCIeBusId = "3" PlatformIndex = "0" BinaryFormat = "amdcl2" AsmProgram = "gfx8" WorkSize = "8388608">
<Device1 PCIeBusId = "7" PlatformIndex = "0" BinaryFormat = "amdcl2" AsmProgram = "gfx8" WorkSize = "4194304">
```

### Per device configuration:

- **Device configuration block**  
`<Device>` block contains all settings for selected device. Blocks must start from 0, e.g `<Device0>`, `<Device1>` ...
`PCIeBusId` field is the only required setting. It is a connection between device and its configuration.
All other settings are optional, but recommended for the best performance.

- **PlatformIndex**  
Selects a specific OpenCL platform/driver to use. Every device has its own `Available platforms indices`.

- **BinaryFormat**  
Specifies a binary format for asm programs. Asm programs are faster than high-level OpenCL ones. Performance difference is GPU driver dependant.
  - `amdcl2` (for Windows and Linux)
  - `ROCm` (Linux only)
  - `none` (Disable asm programs. `AsmProgram` option will be ignored.)  

- **AsmProgram**  
  - `gfx7` (GCN 2nd generation, not available on ROCm)  
  - `gfx8` (GCN 3rd and 4th generations)  
  - `gfx9` (GCN 5nd generation)  

- **WorkSize**  
Possible values: Minimal value is 256. Must be multiple of 256.  
Specifies a number of hashes to compute per run(batch), before returning result to the host(CPU).  
The higher the number the larger the size of work(GPU memory usage) and potentially higher hashrate.  
Too high work sizes can produce errors, including miner crashes and other issues.  
This value is GPU and driver specific.  
To get started, adjust `WorkSize` parameter according to hash rate with the default value(`1048576`)
  - less than 6mh/s: `524288`
  - 6-19mh/s: `524288`, `1048576`
  - 20-30mh/s: `1048576`, `2097152`
  - 32-40mh/s: `2097152`, `3145728`, `4194304`
  - 41-59mh/s: `4194304`, `6291456`, `8388608`
  - more than 60mh/s: `8388608`, `12582912`, `16777216`


## Building lyclMiner

Make sure that OpenCL drivers are installed. See [Supported platforms](#supported platforms).
lyclMiner uses [premake5](https://premake.github.io/) to build platform specific projects. Download it and make sure it's available on your path, or copy `premake5` executable to the same directory as `premake5.lua` file.

### Compilers
GCC (Linux) / MinGW-w64 (Windows)

### Dependencies
- OpenCL
- OpenSSL
- libcurl
- jansson

### Linux
1. Install all dependencies according to your distro's guidelines.  
e.g(on Ubuntu 16.04 LTS): `sudo apt-get install libcurl4-openssl-dev libssl-dev libjansson-dev opencl-headers`
2. run `premake5 gmake` from the same directory as `premake5.lua` file.
3. `cd build` then `make`. If there were no errors, a compiled binary will inside a newly created folder `bin`(same directory as `premake5.lua` file)
4. Copy `kernels` folder to the same directory as compiled `lyclMiner` executable.
    
### Windows
 1. Install [MinGW-w64](https://sourceforge.net/projects/mingw-w64/) with these settings: 
    - Architecture: x86_64
    - Threads: posix
    - Exception: seh
 2. Add `MinGW installation/mingw-w64/bin` to your "PATH"
 3. Download or compile [libcurl](https://curl.haxx.se/download.html), [jansson](https://github.com/akheron/jansson), [OpenSSL](https://slproweb.com/products/Win32OpenSSL.html)
 6. Install [AMD OpenCL app SDK](http://amd-dev.wpengine.netdna-cdn.com/app-sdk/installers/APPSDKInstaller/3.0.130.135-GA/full/AMD-APP-SDKInstaller-v3.0.130.135-GA-windows-F-x64.exe)
    or use Kronos OpenCL headers and ICD.
 7. Edit `setupJansson()`, `setupCurl()`, `setupOpenCL()` functions inside a `premake5.lua` file. Specify `includedirs` and `libdirs` for all dependencies.
 8. run `premake5 gmake` from the same folder as `premake5.lua` file.
 9. `cd build` then `mingw32-make`. If there were no errors, a compiled binary will inside a newly created folder `bin`(same directory as `premake5.lua` file)
 10. Copy `kernels` folder and all required dlls to the same directory as compiled `lyclMiner` executable.
