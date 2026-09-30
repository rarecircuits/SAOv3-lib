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

**Note:** the CH32 port needs the vendor SPL headers (`ch32x035.h` and friends), so it works with the `noneos-sdk`,
`freertos`, `rt-thread`, `harmony-liteos` and `tencent-os` frameworks, but not with `ch32v003fun`.
