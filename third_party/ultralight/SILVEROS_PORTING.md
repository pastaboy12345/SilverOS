# Ultralight Porting Notes for SilverOS

SilverOS is a freestanding x86_64 kernel. The vendored Ultralight SDK package in `sdk/` is not
the engine source tree.

What is present:

- public headers
- samples and tools source code
- prebuilt Linux shared libraries in `sdk/bin/`

What is missing for a native SilverOS port:

- source for `Ultralight`
- source for `UltralightCore`
- source for `WebCore`
- source for `AppCore`
- a static or freestanding vendor build for those libraries

Evidence in this repo:

- `sdk/bin/libUltralight.so`
- `sdk/bin/libUltralightCore.so`
- `sdk/bin/libWebCore.so`
- `sdk/bin/libAppCore.so`
- no engine `.c/.cc/.cpp` sources exist outside `sdk/samples/` and `sdk/tools/`

Current SilverOS behavior:

- the desktop app exposes Ultralight as unavailable rather than pretending native support exists

To do a real port, we need one of:

1. vendor engine source access, then write SilverOS platform/file/font/surface/thread/time backends
2. vendor-provided static libraries built for a freestanding SilverOS ABI
