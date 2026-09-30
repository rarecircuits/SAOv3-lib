# SAOv3 Library Code

Holds all shared library code for SAOv3.
Split into SAO (Target) and Badge (Host) side code.

## Device Library Ports

The device library core is portable; each supported microcontroller family gets a small port that maps its I2C
peripheral onto the core's SMBus handlers.

| Port | Macro | Targets | Entry points |
| --- | --- | --- | --- |
| ATTiny | `SAOD_PORT_ATTINY` | ATTiny1616 | `saod_attiny_init()` / `saod_attiny_tick()` |
| CH32 | `SAOD_PORT_CH32` | WCH CH32X035, CH32V003, CH32V20x, CH32V30x | `saod_ch32_init()` / `saod_ch32_tick()` |

Port selection happens in the preprocessor (`include/saod_port_cfg.h`), auto-detected from the chip macros the
toolchain or board definition already provides. Every port source compiles on every target and all but the selected
one preprocess away to nothing, so build systems that compile the whole `src/` tree unconditionally need no source
filters. To select a port explicitly, define its macro to 1 on the command line.

Port-specific configuration lives in `include/port/<port>/`, and is applied after your `saod_user_cfg.h`, so anything
it sets can still be overridden per project.

## Using with PlatformIO

`device/` is a PlatformIO library. Drop it into your project's `lib/`, or point `lib_deps` at this repo. The only
project-specific build setting needed is making your `saod_user_cfg.h` visible to the library:

```ini
build_flags = -Isrc/
```

See `device/examples/ch32-sao/` for a complete example targeting the CH32X035 and CH32V003.

## Using with ch32fun

The CH32 port only needs the I2C register definitions and GPIO struct, which ch32fun's hardware headers provide under
the same names as the vendor SPL. The one extra setting is telling the port to include `ch32fun.h` instead of the SPL
header for the chip, in your `saod_user_cfg.h`:

```c
#define SAOD_CH32_DEVICE_HEADER "ch32fun.h"
```

Then add the library to the build. With ch32fun's own Makefile:

```make
ADDITIONAL_C_FILES += $(wildcard saov3-lib/device/src/*.c) saov3-lib/device/src/port/ch32/saod_ch32.c
EXTRA_CFLAGS += -Isaov3-lib/device/include
```

Under PlatformIO with ch32fun, point `lib_deps` at `device/` as above and add `-I` flags for ch32fun's headers and the
directory holding `funconfig.h` and `saod_user_cfg.h`.

Port auto-detection works unchanged, since ch32fun's build passes the same chip macros. With `SAOD_CFG_ENABLE_LOGGING`
set, log output goes through `printf`, which ch32fun supplies when its debug printf is enabled in `funconfig.h`.
