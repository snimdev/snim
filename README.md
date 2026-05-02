# Snim

Screenshot, screen recording, OCR and upload tool for Linux, macOS and Windows, built with Qt 6. From Macedonian "snimi": capture.

## Features

- Annotation editor with arrows, shapes, freehand, text, highlight, blur and step numbers.
- Beautify backdrops with padding, rounded corners, shadow and saved presets.
- Screen recording with a trim editor and GIF export.
- OCR text snip that copies recognized text straight to the clipboard.
- Uploads to S3-compatible storage, SFTP and FTP, with named destination profiles.
- Global hotkeys for every capture action, bound to the same actions as the tray menu.

## Screenshots

<!-- TODO: add screenshots of the annotation editor, the backdrop panel and the trim editor. -->

## Building

Requires CMake 3.31+, Qt 6 development packages, a C++20 compiler, and Ninja:

```bash
# Configure, build, then test (CTest does not build).
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure --timeout 120
# Alternatively, build and run all tests together.
cmake --build build --target check
# Run a single test.
ctest --test-dir build -R tst_recordinggeometry --output-on-failure
# Run locally: Linux / macOS, respectively.
./build/snim
./build/Snim.app/Contents/MacOS/Snim
```

For Homebrew Qt, add `-DCMAKE_PREFIX_PATH="$(brew --prefix qt)"` when configuring. Optional dependencies (Tesseract, GStreamer, LayerShellQt, libcurl, libssh2) are auto-detected; missing ones disable features but never break the build.

On Windows, dependencies come from vcpkg: from an x64 Native Tools prompt, with `VCPKG_ROOT` set and Qt 6.11 `msvc2022_64` on `CMAKE_PREFIX_PATH`, run `cmake --preset windows-msvc` then `cmake --build --preset windows-msvc`.

## License

Snim is free software released under the GNU General Public License version 3 (see [LICENSE](LICENSE)).

## Links

- Website: https://snim.dev
- Source: https://github.com/snimdev/snim
