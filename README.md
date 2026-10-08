# Snim

Screenshots, screen recordings, OCR and uploads in one small app for Linux, macOS and Windows. The name comes from the Macedonian "snimi": capture.

[Download](https://snim.dev/download) · [Website](https://snim.dev) · [Ideas and roadmap](https://snim.dev/ideas)

## Features

- **Capture** an area, a window or the whole screen from the tray or a global hotkey.
- **Annotate** with arrows, shapes, freehand, text, highlight, blur and step numbers, then frame the shot with padding, rounded corners and a shadow.
- **Record** the screen with an optional webcam bubble, trim the result and export it as a video, GIF or WebP.
- **Grab text** from anything on screen with OCR, straight to the clipboard.
- **Upload** to your own S3-compatible storage, SFTP or FTP server, with saved profiles.
- **Instant screenshots on KDE Plasma**, with no permission dialog.

## Install

The [download page](https://snim.dev/download) has step-by-step instructions. Every file is also on the [Releases](https://github.com/snimdev/snim/releases) page.

**macOS** (Apple Silicon, macOS 15 or newer): open the DMG and drag Snim into Applications, or use Homebrew:

```bash
brew install --cask snimdev/tap/snim
```

**Windows** (Windows 10 2004 or newer, x64): run the `-setup.exe`, which installs for your user without admin rights, or unzip the portable `.zip` anywhere. The builds are not code-signed yet, so SmartScreen asks first: click **More info**, then **Run anyway**.

**Linux** (x86_64 and arm64): install the Flatpak on any distribution:

```bash
flatpak install --user https://dl.snim.dev/flatpak/snim.flatpakref
```

Or take the `.deb` (Debian, Ubuntu) or `.rpm` (Fedora) from Releases, or install without root under `~/.local`:

```bash
curl -fsSL https://snim.dev/install.sh | bash
```

Screen recording on Linux needs PipeWire and `xdg-desktop-portal`.

## Build from source

You need CMake 3.31 or newer, Ninja, a C++20 compiler and the Qt 6 development packages.

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cmake --build build --target check   # build and run the tests
./build/snim                         # macOS: ./build/Snim.app/Contents/MacOS/Snim
```

- **Optional dependencies** are detected automatically: Tesseract (OCR), GStreamer (recording on Linux), LayerShellQt (the recording frame on Wayland), libcurl (FTP uploads) and libssh2 (SFTP uploads). Without one, its feature is left out and the build still works.
- **macOS with Homebrew Qt**: add `-DCMAKE_PREFIX_PATH="$(brew --prefix qt)"` when configuring.
- **Windows**: dependencies come from vcpkg. From an x64 Native Tools prompt, with `VCPKG_ROOT` set and Qt 6.11 `msvc2022_64` on `CMAKE_PREFIX_PATH`, run `cmake --preset windows-msvc`, then `cmake --build --preset windows-msvc`.

## Contributing

Bug reports, ideas and pull requests are all welcome.

- **Found a bug?** [Open an issue](https://github.com/snimdev/snim/issues/new/choose). The form asks for your system details, which are usually what it takes to reproduce a problem.
- **Have an idea?** Post it or vote on others on the [ideas board](https://snim.dev/ideas).
- **Security issue?** Please [report it privately](https://github.com/snimdev/snim/security/advisories/new), not in a public issue.

To send a pull request:

1. Fork the repository and branch from `main`.
2. Keep each pull request to one change, and add or update tests when behaviour changes.
3. Run `cmake --build build --target check` before you push.
4. Write each commit message as one short line, `Area: What it does`, for example `Editor: Name Snim in the editor window titles`.

Pull requests from forks are built and tested on Linux. The macOS and Windows builds run on the maintainer's own machines, so they only run once a change is merged: if you change code for those platforms, say how you tested it. A first-time contributor's CI run waits for a maintainer to approve it.

By contributing, you agree that your work is released under the same license as Snim.

## License

Snim is free software under the GNU General Public License, version 3 or (at your option) any later version. See [LICENSE](LICENSE).
