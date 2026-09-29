# drmshot

`drmshot` reads the current framebuffer from an active DRM/KMS CRTC and writes
it to a 24-bit BMP image.

## Requirements

- C compiler
- CMake 3.16 or newer
- pkg-config
- libdrm development files

On Debian or Ubuntu, install the dependencies with:

```sh
sudo apt install build-essential cmake pkg-config libdrm-dev
```

## Build

```sh
cmake -S . -B build
cmake --build build
```

The executable is created at `build/drmshot`.

## Usage

```text
drmshot [-c <crtc-id>] [-o <output.bmp>] [-d </dev/dri/cardN>]
```

- `-c <crtc-id>` selects a CRTC by ID. Without it, the first active CRTC is
  selected.
- `-o <output.bmp>` sets the output file. The default is `screenshot.bmp`.
- `-d </dev/dri/cardN>` selects the DRM device. The default is
  `/dev/dri/card0`.

For example:

```sh
./build/drmshot -d /dev/dri/card0 -o screenshot.bmp
./build/drmshot -c 42 -o display.bmp
```

The process needs permission to access the DRM device. Capturing a framebuffer
also depends on the driver allowing its framebuffer handle to be exported and
mapped; some devices or configurations may not support this. The BMP writer
handles framebuffers reported as 24 or 32 bits per pixel.