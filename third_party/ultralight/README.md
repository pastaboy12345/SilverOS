## Ultralight SDK (vendored)

This project integrates the **Ultralight** HTML renderer into SilverOS.

Ultralight distributes prebuilt SDK archives via their website (account required).
Download an SDK that matches:

- **Platform**: Linux x64
- **C/C++ ABI**: libstdc++ (GCC)
- **Runtime model**: prebuilt SDK packages target Linux userspace, not a freestanding kernel

### Expected layout

Place the Ultralight SDK contents into:

`SilverOS/third_party/ultralight/sdk/`

With at least:

- `sdk/include/` (Ultralight headers)
- `sdk/lib/` (Ultralight libraries)
- `sdk/resources/` (Ultralight resources, eg. ICU/dat files if included)

### Note on feasibility

SilverOS is a freestanding kernel. The current repo integration wires an Ultralight
app into the SilverOS desktop and exposes a status panel in the OS, but the vendored
Linux SDK still cannot execute directly inside the kernel.

To get real HTML rendering inside SilverOS, we will need one of:

- a dedicated host bridge that renders with Ultralight in userspace and streams frames/input
- a custom port of the runtime dependencies needed by Ultralight for bare-metal execution
