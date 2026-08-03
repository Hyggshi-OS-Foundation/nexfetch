# nexfetch

<p align="center">
  <img src="./logos/nexfetch.png" alt="nexfetch logo" width="120">
</p>

<p align="center">
  <strong>A modern, modular system information tool for Linux, macOS, and Windows.</strong>
</p>

<p align="center">
  <em>Similar to Neofetch and Fastfetch — but with auto-scaling logos, pluggable modules, and multiple presentation themes built in.</em>
</p>

<img src="Resources/screenshot1.png" width="48%" align="left" />
<img src="Resources/screenshot2.png" width="48%" align="left" />
<img src="Resources/screenshot3.png" width="48%" align="top" />
<img src="Resources/screenshot4.png" width="48%" align="top" />

---

## ✨ Features

- **Cross-platform** — Runs on Linux, macOS, and Windows (native + MinGW/MSYS/Cygwin)
- **Modular architecture** — 21 built-in modules, easily extended with dynamic plugins
- **Multiple themes** — `boxed` (default), `classic`, and `modern` presentation styles
- **Smart logo handling** — ASCII `.txt` logos and image logos (PNG/JPG/GIF) via [`chafa`](https://github.com/hpjansson/chafa)
- **Auto-scaling** — Automatically disables the logo when the terminal is too narrow
- **Configurable** — JSON config file + CLI flags for full control
- **Plugin system** — Load custom `.so`/`.dll` modules at runtime
- **Lightweight** — Written in pure C with minimal dependencies

## 📦 Installation

### Option 1: APT (Debian/Ubuntu — recommended)

Install nexfetch with a single command:

```bash
curl -sL https://raw.githubusercontent.com/Hyggshi-OS-Foundation/nexfetch/main/scripts/nexfetch-apt-setup.sh | sudo bash
```

Or manually add the repository and install:

```bash
# Install prerequisites
sudo apt install ca-certificates curl gnupg lsb-release

# Add the GPG key
sudo mkdir -p /etc/apt/keyrings
curl -fsSL https://hyggshi-os-foundation.github.io/nexfetch/apt/repo-key.gpg \
  | sudo gpg --dearmor -o /etc/apt/keyrings/nexfetch-archive-keyring.gpg

# Add the repository
echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/nexfetch-archive-keyring.gpg] https://hyggshi-os-foundation.github.io/nexfetch/apt $(lsb_release -cs) main" \
  | sudo tee /etc/apt/sources.list.d/nexfetch.list

# Install
sudo apt update
sudo apt install nexfetch
```

After installation, run `nexfetch` from anywhere:

```bash
nexfetch
```

To update:

```bash
sudo apt update && sudo apt upgrade nexfetch
```

To uninstall:

```bash
sudo apt remove nexfetch
```

### Option 2: Build from source

#### Prerequisites

- A C compiler (`gcc` or `clang`)
- `make`
- *(Optional)* [`chafa`](https://github.com/hpjansson/chafa) — for image-to-ANSI logo conversion

#### Build

```bash
git clone https://github.com/Hyggshi-OS-Foundation/nexfetch.git
cd nexfetch
make
```

This produces a `nexfetch` executable (or `nexfetch.exe` on Windows) in the project root.

#### System-wide install

```bash
sudo make install
```

This installs:
- `nexfetch` binary → `/usr/bin/nexfetch`
- Logos → `/usr/share/nexfetch/logos/`
- Default config → `/etc/nexfetch/config.json`

To uninstall:

```bash
sudo make uninstall
```

### Run

```bash
make run
# or directly:
./nexfetch
```

### Clean

```bash
make clean
```

## 🚀 Usage

```bash
nexfetch [options]
```

### Options

| Flag | Description |
| --- | --- |
| `-h`, `--help` | Show help and available options |
| `-v`, `--version` | Print version information |
| `--no-logo` | Disable the ASCII logo display |
| `--logo <path>` | Use a custom logo file (`.txt` or image) |
| `--theme <name>` | Set presentation theme: `boxed`, `classic`, or `modern` |
| `--list-modules` | List all registered modules |

### Examples

```bash
# Default run with auto-detected distro logo
./nexfetch

# Use the classic neofetch-style layout
./nexfetch --theme classic

# Use a custom image logo
./nexfetch --logo logos/Tux.png

# Run without the logo (info only)
./nexfetch --no-logo

# List all available modules
./nexfetch --list-modules
```

## ⚙️ Configuration

nexfetch reads `config/config.json` at startup. Below is the default configuration:

```json
{
  "show_logo": true,
  "color_blocks": true,
  "theme": "boxed",
  "logo": "",
  "logo_width": 16,
  "background_image": "",
  "plugins": [
    "plugins/myplugin.so"
  ],
  "modules": [
    "os", "kernel", "host", "uptime", "packages", "display",
    "shell", "de", "wm", "terminal", "cpu", "gpu", "memory",
    "disk", "swap", "battery", "network", "theme", "icons",
    "font", "locale"
  ]
}
```

| Key | Type | Description |
| --- | --- | --- |
| `show_logo` | boolean | Show/hide the ASCII logo |
| `color_blocks` | boolean | Show/hide the color palette bar |
| `theme` | string | Presentation theme: `boxed`, `classic`, or `modern` |
| `logo` | string | Path to a custom logo file (`.txt` or image) |
| `logo_width` | integer | Width in columns for image logos (used with `chafa`) |
| `background_image` | string | Path to an image rendered as full-terminal background (requires `chafa`) |
| `plugins` | array | Paths to dynamic plugin shared libraries (`.so`/`.dll`) to load |
| `modules` | array | Ordered list of module keys to display |

> **Note:** CLI flags override values set in `config.json`.

## 🧩 Modules

nexfetch ships with 21 built-in modules:

| Module | Key | Description |
| --- | --- | --- |
| OS | `os` | Operating system name and version |
| Kernel | `kernel` | Kernel release string |
| Host | `host` | Device/host model |
| Uptime | `uptime` | System uptime |
| Packages | `packages` | Installed package count |
| Display | `display` | Screen resolution(s) |
| Shell | `shell` | Current shell |
| DE | `de` | Desktop environment |
| WM | `wm` | Window manager |
| Terminal | `terminal` | Terminal emulator |
| CPU | `cpu` | CPU model and cores |
| GPU | `gpu` | GPU model |
| Memory | `memory` | RAM usage |
| Disk | `disk` | Disk usage |
| Swap | `swap` | Swap usage |
| Battery | `battery` | Battery status |
| Network | `network` | Network interface info |
| Theme | `theme` | Current theme |
| Icons | `icons` | Icon set |
| Font | `font` | Current font |
| Locale | `locale` | System locale |

### Writing a Plugin

Plugins are shared libraries (`.so` on Linux/macOS, `.dll` on Windows) that export three symbols:

```c
// my_plugin.c
#include <stdio.h>
#include <stddef.h>

const char *plugin_name = "MyModule";
const char *plugin_key  = "mymodule";

void plugin_detect(char *out, size_t max_len) {
    snprintf(out, max_len, "Hello from my plugin!");
}
```

Compile and load:

```bash
# Linux/macOS
gcc -shared -fPIC -o plugins/myplugin.so my_plugin.c

# Windows
gcc -shared -o plugins/myplugin.dll my_plugin.c
```

The plugin is loaded at runtime via `module_manager_load_plugin()`, which resolves `plugin_name`, `plugin_key`, and `plugin_detect`.

### Vision Plugin (Camera/Webcam)

nexfetch ships with a **Vision Plugin** that detects and displays camera/webcam information on your system:

```c
// plugins/vision.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <dirent.h>

const char *plugin_name = "Vision Camera";
const char *plugin_key  = "visioncamera";

static void trim(char *s)
{
    size_t len = strlen(s);
    while (len && (s[len - 1] == '\n' || s[len - 1] == '\r'))
        s[--len] = '\0';
}

/* Count the number of video devices in /sys/class/video4linux/ */
static int count_video_devices(void)
{
    DIR *dir = opendir("/sys/class/video4linux");
    if (!dir) return 0;

    int count = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strncmp(ent->d_name, "video", 5) == 0)
            count++;
    }
    closedir(dir);
    return count;
}

/* Get the first camera name from /sys/class/video4linux/videoX/name */
static void get_first_camera_name(char *out, size_t max_len)
{
    DIR *dir = opendir("/sys/class/video4linux");
    if (!dir) {
        snprintf(out, max_len, "Unknown");
        return;
    }

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strncmp(ent->d_name, "video", 5) != 0) continue;

        char path[512];
        snprintf(path, sizeof(path), "/sys/class/video4linux/%s/name", ent->d_name);

        FILE *fp = fopen(path, "r");
        if (fp) {
            if (fgets(out, (int)max_len, fp))
                trim(out);
            fclose(fp);
            closedir(dir);
            return;
        }
    }
    closedir(dir);
    snprintf(out, max_len, "Unknown");
}

void plugin_detect(char *out, size_t max_len)
{
    int count = count_video_devices();
    char name[128] = "Unknown";

    if (count > 0)
        get_first_camera_name(name, sizeof(name));

    if (count <= 0) {
        snprintf(out, max_len, "No camera detected");
        return;
    }

    if (count == 1) {
        snprintf(out, max_len, "%s (1 camera)", name);
    } else {
        snprintf(out, max_len, "%s (+%d more cameras)", name, count - 1);
    }
}
```

Compile the Vision Plugin:

```bash
# Linux/macOS
gcc -shared -fPIC -o plugins/vision.so plugins/vision.c

# Windows (requires V4L2 emulation layer)
gcc -shared -o plugins/vision.dll plugins/vision.c
```

Then register it in `config/config.json`:

```json
{
  "plugins": ["plugins/vision.so"],
  "modules": [ "os", "kernel", "...", "visioncamera" ]
}
```

**How it works:**

| Step | Description |
| --- | --- |
| Scan | `count_video_devices()` scans `/sys/class/video4linux/` counting `video*` nodes |
| Name | `get_first_camera_name()` reads the device name from `videoX/name` |
| Output | `Camera Name (N cameras)` — or `No camera detected` if none found |

**Example output:**

```
Vision Camera: Integrated_Webcam_HD: Integrate (+1 more cameras)
```

> **Note:** On Linux, a single physical webcam often exposes multiple `/dev/videoN` nodes (e.g. `video0` for capture, `video1` for metadata). The Vision Plugin counts them all.

### Vision for Nexfetch Plugin (Version)

A lightweight plugin that displays the **Vision for Nexfetch** version banner — dynamically resolved from the running `nexfetch` binary (no hardcoded version):

```c
// plugins/vision_nexfetch.c
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

const char *plugin_name = "Vision for Nexfetch";
const char *plugin_key  = "vision_nexfetch";

void plugin_detect(char *out, size_t max_len)
{
    const char *version = "unknown";

#ifdef _WIN32
    HMODULE host = GetModuleHandleA(NULL);
    if (host) {
        const char **ver_ptr = (const char **)GetProcAddress(host, "nexfetch_version");
        if (ver_ptr && *ver_ptr)
            version = *ver_ptr;
    }
#else
    const char **ver_ptr = (const char **)dlsym(RTLD_DEFAULT, "nexfetch_version");
    if (ver_ptr && *ver_ptr)
        version = *ver_ptr;
#endif

    snprintf(out, max_len, "v%s", version);
}
```

Compile and register:

```bash
gcc -shared -fPIC -o plugins/vision_nexfetch.so plugins/vision_nexfetch.c
```

```json
{
  "plugins": ["plugins/vision_nexfetch.so"],
  "modules": [ "os", "kernel", "...", "vision_nexfetch" ]
}
```

**Example output:**

```
Vision for Nexfetch: v1.1.0
```

> **Notes:**
> - The presenter automatically prefixes the value with the plugin name (`Vision for Nexfetch: <value>`), so `plugin_detect()` only writes the version string.
> - On Linux/macOS, nexfetch is built with `-rdynamic` and exports the `nexfetch_version` symbol (defined in `src/module_manager.c`). The plugin resolves it at runtime via `dlsym(RTLD_DEFAULT, "nexfetch_version")` — so the version always matches the running binary, no manual updates needed.
> - On Windows, the plugin reads the same symbol from the host executable via `GetProcAddress`.

To bump the nexfetch version, only `NEXFETCH_VERSION` in `include/nexfetch.h` needs to change — the plugin picks it up automatically.

## 🎨 Themes

| Theme | Description |
| --- | --- |
| `boxed` | Information displayed inside a rounded Unicode box (default) |
| `classic` | Traditional neofetch-style key: value layout |
| `modern` | Tree-style layout with `├─` / `╰─` connectors |

## 🖼️ Logos

Logos live in the `logos/` directory. nexfetch supports two formats:

- **ASCII text** (`.txt`) — Plain ANSI art, loaded directly
- **Images** (`.png`, `.jpg`, `.gif`, etc.) — Converted to ANSI art via `chafa`

### Logo resolution order

1. Custom path from `config.json` `"logo"` field or `--logo` CLI flag
2. `logos/<distro_id>.txt` (auto-detected from `/etc/os-release`)
3. `logos/tux.txt` (fallback)

### Built-in logos

| File | Distro |
| --- | --- |
| `nexfetch.png` | nexfetch (project logo, image) |
| `nexfetch.txt` | nexfetch (project logo, ASCII) |
| `alpine.txt` | Alpine Linux |
| `arch.txt` | Arch Linux |
| `debian.txt` | Debian |
| `fedora.txt` | Fedora |
| `hyggshi_OS.txt` | Hyggshi OS |
| `tux.txt` | Tux (generic Linux fallback) |
| `ubuntu.txt` | Ubuntu |
| `Tux.png` | Tux (image) |
| `Windows_logo_11.png` | Windows 11 (image) |

### Auto-scaling

If the combined width of the logo and information box exceeds the terminal width, nexfetch automatically hides the logo to keep the output readable.

## 📁 Project Structure

```
nexfetch/
├── config/
│   └── config.json          # Default configuration
├── include/                 # Public headers
│   ├── module.h             # Module system API
│   ├── nexfetch.h           # Core types, config, color macros
│   ├── platform.h           # Platform abstraction API
│   ├── presenter.h          # Theme/presenter API
│   └── util.h               # Utility functions
├── logos/                   # ASCII and image logos
├── modules/                 # Built-in module implementations
│   ├── ansi.c               # ANSI visible-length calculator
│   ├── battery.c
│   ├── color.c              # Color blocks bar
│   ├── cpu.c
│   ├── custom.c
│   ├── de.c
│   ├── disk.c
│   ├── display.c
│   ├── gpu.c
│   ├── host.c
│   ├── kernel.c
│   ├── locale.c
│   ├── logo.c               # Logo loader (txt + image via chafa)
│   ├── memory.c
│   ├── network.c
│   ├── os.c
│   ├── packages.c
│   ├── shell.c
│   ├── swap.c
│   ├── uptime.c
│   └── ...
├── platform/                # Platform-specific backends
│   ├── linux/               # Linux implementations
│   ├── macos/               # macOS implementation
│   └── windows/             # Windows implementation
├── plugins/                 # Drop-in plugin directory (shared libs)
├── src/                     # Core source files
│   ├── config.c             # JSON config parser
│   ├── main.c               # Entry point and CLI handling
│   ├── module_manager.c     # Module registry + plugin loader
│   ├── presenter.c          # Theme renderers
│   └── util.c               # Shared utilities
├── Makefile                 # Build system
├── LICENSE                  # GPL-3.0
└── README.md
```

## 🔧 Building

The `Makefile` auto-detects the platform and selects the appropriate source files:

```bash
make          # Build nexfetch
make run      # Build and run
make clean    # Remove build artifacts
```

### Platform notes

- **Linux** — Links with `-ldl` for dynamic plugin loading
- **macOS** — Uses `platform/macos/platform_macos.c`
- **Windows** — Uses `platform/windows/platform_windows.c`; builds `nexfetch.exe`

## 📜 License

This project is licensed under the **GNU General Public License v3.0** — see the [LICENSE](LICENSE) file for details.

## 🌐 Repository

- **Source:** [https://github.com/Hyggshi-OS-Foundation/nexfetch](https://github.com/Hyggshi-OS-Foundation/nexfetch)
- **Issues:** [https://github.com/Hyggshi-OS-Foundation/nexfetch/issues](https://github.com/Hyggshi-OS-Foundation/nexfetch/issues)

---

<p align="center">
  Made with ❤️ by the <a href="https://github.com/Hyggshi-OS-Foundation">Hyggshi OS Foundation</a> and <a href="https://github.com/Hyggshi-OS-Research-Technology">Hyggshi OS Research Technology</a>
</p>