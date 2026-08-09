# nexfetch

<p align="center">
  <img src="./logos/nexfetch.png" alt="nexfetch logo" width="120">
</p>

<p align="center">
  <strong>Công cụ xem thông tin hệ thống cho Linux, macOS và Windows.</strong>
</p>

<p align="center">
  <em>Giống Neofetch và Fastfetch, nhưng logo tự co giãn theo terminal, module có thể gắn thêm qua plugin, và có sẵn vài kiểu hiển thị khác nhau.</em>
</p>

| Language | README |
|----------|--------|
|  English | [English](README.md) |
|  Tiếng Việt | [Tiếng Việt](README_VI.md) |
|  简体中文 | [简体中文](README_zh.md) |

<img src="Resources/screenshot1.png" width="48%" align="left" />
<img src="Resources/screenshot2.png" width="48%" align="left" />
<img src="Resources/screenshot3.png" width="48%" align="top" />
<img src="Resources/screenshot4.png" width="48%" align="top" />

> [!WARNING]
> Đây là dự án độc lập, không liên quan gì đến ghvbb/NexFetch.

---

## Tính năng

- Chạy được trên Linux, macOS và Windows (bản gốc lẫn qua MinGW/MSYS/Cygwin)
- 21 module dựng sẵn, mở rộng bằng plugin động khi cần thêm
- Ba kiểu hiển thị: `boxed` (mặc định), `classic`, `modern`
- Logo dùng file `.txt` ASCII hoặc ảnh PNG/JPG/GIF, ảnh được chuyển sang ANSI qua [`chafa`](https://github.com/hpjansson/chafa)
- Terminal hẹp quá thì logo tự tắt để phần thông tin không bị vỡ layout
- Cấu hình qua file JSON hoặc cờ dòng lệnh
- Nạp module `.so`/`.dll` tùy chỉnh lúc chạy
- Viết bằng C thuần, ít phụ thuộc
- Chế độ kiểm tra bảo mật (`--security`, chỉ Linux) kiểm tra Secure Boot, kernel lockdown, firewall, MAC (SELinux/AppArmor), ASLR, chính sách core dump, và các cổng đang lắng nghe có thể truy cập từ bên ngoài

## Cài đặt

Mỗi lần gắn tag `v*` mới, [trang Releases](https://github.com/Hyggshi-OS-Foundation/nexfetch/releases) sẽ có sẵn gói build cho `amd64`, `arm64`, `armhf`. Chọn cách phù hợp với distro bên dưới, nếu distro không nằm trong bảng thì build từ source.

| Nhóm distro | Ví dụ | Gói | Kiến trúc |
| --- | --- | --- | --- |
| Debian | Debian, Ubuntu, Linux Mint, Pop!_OS | `.deb` | amd64, arm64, armhf |
| RPM | Fedora, openSUSE, RHEL, CentOS | `.rpm` | amd64, arm64, armhf |
| Arch | Arch Linux, Manjaro, EndeavourOS | `.pkg.tar.zst` | chỉ x86_64 |
| Bất kỳ | — | `.AppImage` | amd64, arm64 |
| Gentoo, Void, Slackware và các distro khác | — | — | build từ source (cần compiler) |

### Debian (.deb)

Tải file `.deb` đúng kiến trúc từ Releases rồi chạy:

```bash
sudo dpkg -i nexfetch_<version>_<arch>.deb
sudo apt -f install   # gỡ lỗi dependency nếu cần
```

Gỡ cài đặt:

```bash
sudo apt remove nexfetch
```

### RPM (.rpm)

```bash
sudo rpm -i nexfetch-<version>.<arch>.rpm
# hoặc trên hệ dnf/zypper:
sudo dnf install ./nexfetch-<version>.<arch>.rpm
```

Gỡ cài đặt:

```bash
sudo rpm -e nexfetch
```

### Arch (.pkg.tar.zst)

Chỉ có bản `x86_64` vì container build `archlinux` gốc không có image `aarch64`. Tải gói từ Releases rồi:

```bash
sudo pacman -U nexfetch-<version>-1-x86_64.pkg.tar.zst
```

Gỡ cài đặt:

```bash
sudo pacman -R nexfetch
```

### AppImage (chạy trên mọi distro, không cần cài)

```bash
chmod +x nexfetch-<version>-x86_64.AppImage
./nexfetch-<version>-x86_64.AppImage
```

*(dùng bản `aarch64` cho máy ARM64)*

### Build từ source

Bắt buộc với Gentoo, Void, Slackware hoặc distro nào không có gói dựng sẵn ở trên — và cũng dùng được trên các distro khác nếu không muốn qua package manager.

#### Cần có

- Compiler C (`gcc` hoặc `clang`)
- `make`
- `chafa` (tùy chọn, chỉ cần nếu muốn dùng logo ảnh)

#### Build

```bash
git clone https://github.com/Hyggshi-OS-Foundation/nexfetch.git
cd nexfetch
make
```

Lệnh trên tạo ra file thực thi `nexfetch` (hoặc `nexfetch.exe` trên Windows) ngay trong thư mục dự án.

#### Cài toàn hệ thống

```bash
sudo make install
```

Lệnh này sẽ:
- Copy binary `nexfetch` vào `/usr/bin/nexfetch`
- Copy logo vào `/usr/share/nexfetch/logos/`
- Copy config mặc định vào `/etc/nexfetch/config.json`

Gỡ cài đặt:

```bash
sudo make uninstall
```

### Chạy

```bash
make run
# hoặc chạy trực tiếp:
./nexfetch
```

### Dọn build

```bash
make clean
```

## Cách dùng

```bash
nexfetch [options]
```

### Các cờ

| Cờ | Mô tả |
| --- | --- |
| `-h`, `--help` | Xem hướng dẫn và danh sách tùy chọn |
| `-v`, `--version` | In thông tin phiên bản |
| `--no-logo` | Tắt hiển thị logo ASCII |
| `--logo <path>` | Dùng file logo tùy chỉnh (`.txt` hoặc ảnh) |
| `--theme <name>` | Chọn kiểu hiển thị: `boxed`, `classic`, `modern` |
| `--list-modules` | Liệt kê các module đã đăng ký |
| `--security` | Chạy kiểm tra bảo mật độc lập rồi thoát (chỉ Linux, xem [Kiểm tra bảo mật](#kiểm-tra-bảo-mật)) |

### Ví dụ

```bash
# Chạy mặc định, logo tự nhận theo distro
./nexfetch

# Layout kiểu neofetch cũ
./nexfetch --theme classic

# Dùng logo ảnh tùy chỉnh
./nexfetch --logo logos/Tux.png

# Chạy không kèm logo, chỉ hiện thông tin
./nexfetch --no-logo

# Xem hết các module có sẵn
./nexfetch --list-modules

# Chạy kiểm tra bảo mật
./nexfetch --security
```

## Kiểm tra bảo mật

`--security` chạy kiểm tra bảo mật riêng cho Linux, in ra bảng có tính điểm thay vì thông tin hệ thống thông thường:

```
Nexfetch Security Audit

  ✓ Secure Boot       Enabled
  ✓ Kernel Lockdown   Enabled
  ✓ Firewall          Active
  ✓ MAC               Active
  ✓ ASLR              Enabled
  ⚠ Core Dumps        Enabled (piped)
  ⚠ Open Ports        2

  Security Score: 5/7
```

| Mục kiểm tra | Đọc từ đâu | Ghi chú |
| --- | --- | --- |
| Secure Boot | `/sys/firmware/efi/efivars/SecureBoot-...` | Báo "Not found" trên máy không dùng UEFI |
| Kernel Lockdown | `/sys/kernel/security/lockdown` | Bật khi ở chế độ `[integrity]` hoặc `[confidentiality]` |
| Firewall | `systemctl is-active` cho ufw/firewalld/iptables/nftables, sau đó đếm rule nếu chạy root | Báo "Unknown" thay vì đoán bừa nếu không đủ quyền kiểm tra |
| MAC | `/sys/fs/selinux/enforce`, `/sys/module/apparmor/parameters/enabled` | Dùng cờ kernel ai cũng đọc được, không dùng danh sách profile chỉ root mới đọc, nên chạy không cần `sudo` |
| ASLR | `/proc/sys/kernel/randomize_va_space` | 2 = đầy đủ, 1 = một phần, 0 = tắt |
| Core Dumps | `/proc/sys/kernel/core_pattern` | Core dump dạng piped/enabled chỉ là cảnh báo, không phải lỗi |
| Open Ports | `/proc/net/tcp` + `/proc/net/udp`, đã lọc bỏ dải `127.0.0.0/8` | Chỉ đếm socket có thể truy cập từ bên ngoài máy; các service chỉ bind loopback (DNS stub resolver, database nội bộ...) không được tính |

Một vài mục (chủ yếu Firewall và danh sách profile SELinux/AppArmor) sẽ chính xác hơn khi chạy `sudo ./nexfetch --security`, vì đếm rule và đọc profile trực tiếp cần quyền root. Các mục còn lại hoạt động giống nhau dù có sudo hay không.

Hiện tại tính năng bảo mật chỉ hỗ trợ Linux; macOS và Windows có thể được bổ sung sau.

## Cấu hình

nexfetch đọc file `config/config.json` khi khởi động. Đây là cấu hình mặc định:

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

| Trường | Kiểu | Mô tả |
| --- | --- | --- |
| `show_logo` | boolean | Hiện/ẩn logo ASCII |
| `color_blocks` | boolean | Hiện/ẩn thanh màu |
| `theme` | string | Kiểu hiển thị: `boxed`, `classic`, `modern` |
| `logo` | string | Đường dẫn tới file logo tùy chỉnh (`.txt` hoặc ảnh) |
| `logo_width` | integer | Bề rộng cột cho logo ảnh (dùng cùng `chafa`) |
| `background_image` | string | Đường dẫn ảnh nền phủ toàn terminal (cần `chafa`) |
| `plugins` | array | Đường dẫn tới các thư viện plugin (`.so`/`.dll`) cần nạp |
| `modules` | array | Thứ tự các module sẽ hiển thị |

> Cờ dòng lệnh sẽ ghi đè giá trị trong `config.json`.

## Module

nexfetch có sẵn 21 module:

| Module | Key | Mô tả |
| --- | --- | --- |
| OS | `os` | Tên và phiên bản hệ điều hành |
| Kernel | `kernel` | Chuỗi kernel release |
| Host | `host` | Model máy/thiết bị |
| Uptime | `uptime` | Thời gian máy đã chạy |
| Packages | `packages` | Số package đã cài |
| Display | `display` | Độ phân giải màn hình |
| Shell | `shell` | Shell đang dùng |
| DE | `de` | Desktop environment |
| WM | `wm` | Window manager |
| Terminal | `terminal` | Trình giả lập terminal |
| CPU | `cpu` | Model và số nhân CPU |
| GPU | `gpu` | Model GPU |
| Memory | `memory` | RAM đang dùng |
| Disk | `disk` | Dung lượng ổ đĩa đang dùng |
| Swap | `swap` | Swap đang dùng |
| Battery | `battery` | Trạng thái pin |
| Network | `network` | Thông tin card mạng |
| Theme | `theme` | Theme hiện tại |
| Icons | `icons` | Bộ icon |
| Font | `font` | Font hiện tại |
| Locale | `locale` | Locale hệ thống |

### Viết plugin

Plugin là thư viện dùng chung (`.so` trên Linux/macOS, `.dll` trên Windows), cần export 3 ký hiệu:

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

Biên dịch và nạp:

```bash
# Linux/macOS
gcc -shared -fPIC -o plugins/myplugin.so my_plugin.c

# Windows
gcc -shared -o plugins/myplugin.dll my_plugin.c
```

Plugin được nạp lúc chạy qua `module_manager_load_plugin()`, hàm này sẽ tìm `plugin_name`, `plugin_key`, và `plugin_detect`.

### Vision Plugin (Camera/Webcam)

nexfetch có sẵn Vision Plugin, dùng để phát hiện và hiện thông tin camera/webcam trên máy:

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

/* Đếm số thiết bị video trong /sys/class/video4linux/ */
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

/* Lấy tên camera đầu tiên từ /sys/class/video4linux/videoX/name */
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

Biên dịch Vision Plugin:

```bash
# Linux/macOS
gcc -shared -fPIC -o plugins/vision.so plugins/vision.c

# Windows (cần lớp giả lập V4L2)
gcc -shared -o plugins/vision.dll plugins/vision.c
```

Rồi đăng ký trong `config/config.json`:

```json
{
  "plugins": ["plugins/vision.so"],
  "modules": [ "os", "kernel", "...", "visioncamera" ]
}
```

**Cách hoạt động:**

| Bước | Mô tả |
| --- | --- |
| Quét | `count_video_devices()` quét `/sys/class/video4linux/`, đếm các node `video*` |
| Lấy tên | `get_first_camera_name()` đọc tên thiết bị từ `videoX/name` |
| Kết quả | In ra `Tên camera (N camera)`, hoặc `No camera detected` nếu không tìm thấy |

**Ví dụ output:**

```
Vision Camera: Integrated_Webcam_HD: Integrate (+1 more cameras)
```

> Trên Linux, một webcam vật lý thường tạo ra nhiều node `/dev/videoN` cùng lúc (ví dụ `video0` để capture, `video1` để lấy metadata). Vision Plugin đếm hết tất cả các node đó.

### Vision for Nexfetch Plugin (phiên bản)

Plugin nhỏ hiện dòng phiên bản của Vision for Nexfetch, lấy trực tiếp từ binary `nexfetch` đang chạy, không hardcode:

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

Biên dịch và đăng ký:

```bash
gcc -shared -fPIC -o plugins/vision_nexfetch.so plugins/vision_nexfetch.c
```

```json
{
  "plugins": ["plugins/vision_nexfetch.so"],
  "modules": [ "os", "kernel", "...", "vision_nexfetch" ]
}
```

**Ví dụ output:**

```
Vision for Nexfetch: v1.1.0
```

> Ghi chú:
> - Presenter tự thêm tiền tố tên plugin vào trước giá trị (`Vision for Nexfetch: <value>`), nên `plugin_detect()` chỉ cần ghi ra chuỗi phiên bản.
> - Trên Linux/macOS, nexfetch được build với `-rdynamic` và export symbol `nexfetch_version` (định nghĩa trong `src/module_manager.c`). Plugin lấy symbol này lúc chạy qua `dlsym(RTLD_DEFAULT, "nexfetch_version")`, nên phiên bản luôn khớp với binary đang chạy, không cần sửa tay.
> - Trên Windows, plugin đọc cùng symbol đó từ file thực thi host qua `GetProcAddress`.

Muốn tăng version của nexfetch chỉ cần sửa `NEXFETCH_VERSION` trong `include/nexfetch.h`, plugin sẽ tự lấy giá trị mới.

## Theme

| Theme | Mô tả |
| --- | --- |
| `boxed` | Thông tin nằm trong khung Unicode bo góc (mặc định) |
| `classic` | Layout key: value kiểu neofetch truyền thống |
| `modern` | Layout dạng cây với nối `├─` / `╰─` |

## Logo

Logo nằm trong thư mục `logos/`. nexfetch hỗ trợ hai dạng:

- ASCII text (`.txt`) — ANSI art thuần, nạp trực tiếp
- Ảnh (`.png`, `.jpg`, `.gif`,...) — chuyển sang ANSI art qua `chafa`

### Thứ tự tìm logo

1. Đường dẫn tùy chỉnh từ `config.json` trường `"logo"` hoặc cờ `--logo`
2. `logos/<distro_id>.txt` (tự nhận từ `/etc/os-release`)
3. `logos/tux.txt` (mặc định dự phòng)

### Logo có sẵn

| File | Distro |
| --- | --- |
| `nexfetch.png` | Logo dự án nexfetch (ảnh) |
| `nexfetch.txt` | Logo dự án nexfetch (ASCII) |
| `alpine.txt` | Alpine Linux |
| `arch.txt` | Arch Linux |
| `debian.txt` | Debian |
| `fedora.txt` | Fedora |
| `hyggshi_OS.txt` | Hyggshi OS |
| `tux.txt` | Tux (fallback chung cho Linux) |
| `ubuntu.txt` | Ubuntu |
| `Tux.png` | Tux (ảnh) |
| `Windows_logo_11.png` | Windows 11 (ảnh) |

### Tự co giãn

Nếu tổng bề rộng logo cộng khung thông tin vượt quá bề rộng terminal, nexfetch tự ẩn logo để output không bị vỡ.

## Cấu trúc dự án

```
nexfetch/
├── config/
│   └── config.json          # Cấu hình mặc định
├── include/                 # Header công khai
│   ├── module.h             # API hệ thống module
│   ├── nexfetch.h           # Kiểu dữ liệu, config, macro màu
│   ├── platform.h           # API trừu tượng hóa platform
│   ├── presenter.h          # API theme/presenter
│   └── util.h               # Hàm tiện ích
├── logos/                   # Logo ASCII và ảnh
├── modules/                 # Các module dựng sẵn
│   ├── ansi.c               # Tính độ dài hiển thị ANSI
│   ├── battery.c
│   ├── color.c              # Thanh màu
│   ├── cpu.c
│   ├── custom.c
│   ├── de.c
│   ├── disk.c
│   ├── display.c
│   ├── gpu.c
│   ├── host.c
│   ├── kernel.c
│   ├── locale.c
│   ├── logo.c               # Nạp logo (txt + ảnh qua chafa)
│   ├── memory.c
│   ├── network.c
│   ├── os.c
│   ├── packages.c
│   ├── shell.c
│   ├── swap.c
│   ├── uptime.c
│   └── ...
├── platform/                # Backend riêng cho từng platform
│   ├── linux/               # Cài đặt cho Linux
│   ├── macos/                # Cài đặt cho macOS
│   └── windows/              # Cài đặt cho Windows
├── plugins/                 # Thư mục thả plugin (shared lib)
├── src/                     # Source chính
│   ├── config.c             # Parse config JSON
│   ├── main.c                # Entry point, xử lý CLI
│   ├── module_manager.c      # Đăng ký module + nạp plugin
│   ├── presenter.c           # Render theme
│   └── util.c                 # Hàm dùng chung
├── Makefile                 # Build system
├── LICENSE                  # GPL-3.0
└── README.md
```

## Build

`Makefile` tự nhận diện platform và chọn source phù hợp:

```bash
make          # Build nexfetch
make run      # Build rồi chạy luôn
make clean    # Xóa artifact build
```

### Ghi chú theo platform

- Linux: link với `-ldl` để nạp plugin động
- macOS: dùng `platform/macos/platform_macos.c`
- Windows: dùng `platform/windows/platform_windows.c`, build ra `nexfetch.exe`

### CI/CD

Mỗi lần push tag `v*`, workflow `.github/workflows/release.yml` sẽ chạy: build `amd64`/`arm64`/`armhf`, đóng gói `.deb`, `.rpm`, gói Arch `x86_64`, AppImage `amd64`/`arm64`, rồi đẩy hết lên GitHub Releases.

## Benchmark

Đo trên cùng một máy, mỗi công cụ chạy nhiều lần và tính thời gian. Số càng nhỏ càng nhanh.

### Cấu hình mặc định

| Công cụ | Trung bình | Trung vị | Độ lệch chuẩn | Nhỏ nhất | Lớn nhất |
| --- | --- | --- | --- | --- | --- |
| `./nexfetch` | 6.38ms | 6.29ms | 0.44ms | 5.76ms | 8.03ms |
| `./nexfetch-ghvbb` | 9.06ms | 8.91ms | 0.72ms | 8.33ms | 14.03ms |
| `fastfetch` | 34.28ms | 33.95ms | 1.34ms | 32.67ms | 39.37ms |
| `./neofetch` | 452.58ms | 446.10ms | 19.21ms | 431.15ms | 577.88ms |

<img src="Resources/benchmark_bar_normal.png"/>

### Cấu hình `--fast`

| Công cụ | Trung bình | Trung vị | Độ lệch chuẩn | Nhỏ nhất | Lớn nhất |
| --- | --- | --- | --- | --- | --- |
| `./nexfetch --fast` | 2.15ms | 2.08ms | 0.28ms | 1.81ms | 3.79ms |
| `./nexfetch-ghvbb` | 8.95ms | 8.93ms | 0.26ms | 8.45ms | 10.28ms |
| `fastfetch` | 34.97ms | 34.15ms | 3.26ms | 32.65ms | 61.37ms |
| `./neofetch --fast` | 445.92ms | 442.80ms | 10.68ms | 430.28ms | 487.43ms |

<img src="Resources/benchmark_bar.png"/>

Ở cả hai cấu hình, nexfetch nhanh hơn fastfetch khoảng 5 lần, nhanh hơn neofetch khoảng 70-200 lần. `nexfetch-ghvbb` ở đây là [ghvbb/NexFetch](https://github.com/ghvbb/NexFetch), một dự án khác không liên quan nhưng trùng tên, đưa vào để đối chiếu vì nhiều người hay nhầm hai project này với nhau.

## Giấy phép

Dự án dùng giấy phép GNU General Public License v3.0, xem chi tiết trong file [LICENSE](LICENSE).

## Repository

- Source: [https://github.com/Hyggshi-OS-Foundation/nexfetch](https://github.com/Hyggshi-OS-Foundation/nexfetch)
- Issues: [https://github.com/Hyggshi-OS-Foundation/nexfetch/issues](https://github.com/Hyggshi-OS-Foundation/nexfetch/issues)

---

<p align="center">
  Được làm bởi <a href="https://github.com/Hyggshi-OS-Foundation">Hyggshi OS Foundation</a> và <a href="https://github.com/Hyggshi-OS-Research-Technology">Hyggshi OS Research Technology</a>
</p>