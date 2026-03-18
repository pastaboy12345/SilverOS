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

### SilverOS status

SilverOS is a freestanding kernel and cannot directly execute the vendored Linux
Ultralight runtime in-kernel from this package.

What is in this SDK drop:

- headers under `sdk/include/`
- prebuilt Linux shared libraries under `sdk/bin/`
- samples and tools source code

What is not in this SDK drop:

- engine source for `Ultralight`, `UltralightCore`, `WebCore`, or `AppCore`
- a freestanding/static SilverOS build of those libraries

That means a real SilverOS port requires new vendor artifacts:

1. Ultralight engine source, or
2. a static/freestanding build provided by Ultralight for SilverOS-like targets.

The SilverOS desktop app now reports this limitation directly instead of pretending native support exists.

### Optional host-side experiments

`tools/ultralight_capture_frame.cpp` can still be used for host-side rendering experiments, but it is not a native SilverOS port.
