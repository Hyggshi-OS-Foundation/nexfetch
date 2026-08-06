# nexfetch

<p align="center">
  <img src="./logos/nexfetch.png" alt="nexfetch logo" width="120">
</p>

<p align="center">
  <strong>一个可在 Linux、macOS 和 Windows 上运行的系统信息工具。</strong>
</p>

<p align="center">
  <em>和 Neofetch、Fastfetch 类似,不过logo 会根据终端宽度自动缩放,模块可以通过插件扩展,自带几种显示样式。</em>
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
> 这是一个独立项目,和 ghvbb/NexFetch 没有关系。

---

## 功能

- 支持 Linux、macOS、Windows(原生运行,也可通过 MinGW/MSYS/Cygwin)
- 21 个内置模块,可以用插件继续扩展
- 三种显示样式:`boxed`(默认)、`classic`、`modern`
- logo 支持 ASCII 文本(`.txt`)或图片(PNG/JPG/GIF),图片通过 [`chafa`](https://github.com/hpjansson/chafa) 转成 ANSI 字符
- 终端太窄时 logo 会自动隐藏,避免排版乱掉
- 用 JSON 文件或命令行参数配置
- 运行时可以加载 `.so`/`.dll` 插件
- 用 C 写的,依赖很少

## 安装

每次发布 `v*` 标签,[Releases 页面](https://github.com/Hyggshi-OS-Foundation/nexfetch/releases) 都会提供 `amd64`、`arm64`、`armhf` 的预编译包。根据自己的发行版选下面对应的方式,表里没有的就从源码构建。

| 发行版系列 | 例子 | 包格式 | 支持架构 |
| --- | --- | --- | --- |
| Debian 系 | Debian、Ubuntu、Linux Mint、Pop!_OS | `.deb` | amd64, arm64, armhf |
| RPM 系 | Fedora、openSUSE、RHEL、CentOS | `.rpm` | amd64, arm64, armhf |
| Arch 系 | Arch Linux、Manjaro、EndeavourOS | `.pkg.tar.zst` | 仅 x86_64 |
| 任意发行版 | — | `.AppImage` | amd64, arm64 |
| Gentoo、Void、Slackware 等 | — | — | 需要从源码构建 |

### Debian 系(.deb)

从 Releases 下载对应架构的 `.deb`,然后:

```bash
sudo dpkg -i nexfetch_<version>_<arch>.deb
sudo apt -f install   # 如果有依赖问题就跑这条
```

卸载:

```bash
sudo apt remove nexfetch
```

### RPM 系(.rpm)

```bash
sudo rpm -i nexfetch-<version>.<arch>.rpm
# 或者在 dnf/zypper 系统上:
sudo dnf install ./nexfetch-<version>.<arch>.rpm
```

卸载:

```bash
sudo rpm -e nexfetch
```

### Arch 系(.pkg.tar.zst)

只提供 `x86_64` 版本,因为上游的 `archlinux` 构建容器没有 `aarch64` 镜像。从 Releases 下载包后:

```bash
sudo pacman -U nexfetch-<version>-1-x86_64.pkg.tar.zst
```

卸载:

```bash
sudo pacman -R nexfetch
```

### AppImage(任意发行版,免安装)

```bash
chmod +x nexfetch-<version>-x86_64.AppImage
./nexfetch-<version>-x86_64.AppImage
```

*(ARM64 机器用 `aarch64` 版本)*

### 从源码构建

Gentoo、Void、Slackware 或者上面表格里没列出的发行版必须这样装,其他发行版不想用包管理器的话也可以用这种方式。

#### 需要准备

- C 编译器(`gcc` 或 `clang`)
- `make`
- `chafa`(可选,只有要用图片 logo 才需要)

#### 构建

```bash
git clone https://github.com/Hyggshi-OS-Foundation/nexfetch.git
cd nexfetch
make
```

构建完成后,项目根目录会生成 `nexfetch` 可执行文件(Windows 上是 `nexfetch.exe`)。

#### 安装到系统

```bash
sudo make install
```

这一步会把:
- `nexfetch` 二进制文件复制到 `/usr/bin/nexfetch`
- logo 复制到 `/usr/share/nexfetch/logos/`
- 默认配置复制到 `/etc/nexfetch/config.json`

卸载:

```bash
sudo make uninstall
```

### 运行

```bash
make run
# 或者直接:
./nexfetch
```

### 清理构建产物

```bash
make clean
```

## 用法

```bash
nexfetch [options]
```

### 参数

| 参数 | 说明 |
| --- | --- |
| `-h`, `--help` | 显示帮助和可用选项 |
| `-v`, `--version` | 打印版本信息 |
| `--no-logo` | 关闭 ASCII logo |
| `--logo <path>` | 使用自定义 logo 文件(`.txt` 或图片) |
| `--theme <name>` | 设置显示样式:`boxed`、`classic`、`modern` |
| `--list-modules` | 列出所有已注册的模块 |

### 例子

```bash
# 默认运行,logo 自动按发行版识别
./nexfetch

# 老式 neofetch 风格布局
./nexfetch --theme classic

# 使用自定义图片 logo
./nexfetch --logo logos/Tux.png

# 不显示 logo,只看信息
./nexfetch --no-logo

# 查看所有可用模块
./nexfetch --list-modules
```

## 配置

nexfetch 启动时会读取 `config/config.json`。默认配置如下:

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

| 字段 | 类型 | 说明 |
| --- | --- | --- |
| `show_logo` | boolean | 是否显示 ASCII logo |
| `color_blocks` | boolean | 是否显示颜色条 |
| `theme` | string | 显示样式:`boxed`、`classic`、`modern` |
| `logo` | string | 自定义 logo 文件路径(`.txt` 或图片) |
| `logo_width` | integer | 图片 logo 的列宽(配合 `chafa` 使用) |
| `background_image` | string | 铺满终端的背景图路径(需要 `chafa`) |
| `plugins` | array | 要加载的插件库路径(`.so`/`.dll`) |
| `modules` | array | 模块显示的顺序 |

> 命令行参数会覆盖 `config.json` 里的设置。

## 模块

nexfetch 自带 21 个模块:

| 模块 | Key | 说明 |
| --- | --- | --- |
| OS | `os` | 操作系统名称和版本 |
| Kernel | `kernel` | 内核版本字符串 |
| Host | `host` | 设备/主机型号 |
| Uptime | `uptime` | 系统运行时间 |
| Packages | `packages` | 已安装的软件包数量 |
| Display | `display` | 屏幕分辨率 |
| Shell | `shell` | 当前使用的 shell |
| DE | `de` | 桌面环境 |
| WM | `wm` | 窗口管理器 |
| Terminal | `terminal` | 终端模拟器 |
| CPU | `cpu` | CPU 型号和核心数 |
| GPU | `gpu` | GPU 型号 |
| Memory | `memory` | 内存使用情况 |
| Disk | `disk` | 磁盘使用情况 |
| Swap | `swap` | 交换分区使用情况 |
| Battery | `battery` | 电池状态 |
| Network | `network` | 网卡信息 |
| Theme | `theme` | 当前主题 |
| Icons | `icons` | 图标集 |
| Font | `font` | 当前字体 |
| Locale | `locale` | 系统区域设置 |

### 编写插件

插件是共享库(Linux/macOS 上是 `.so`,Windows 上是 `.dll`),需要导出三个符号:

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

编译并加载:

```bash
# Linux/macOS
gcc -shared -fPIC -o plugins/myplugin.so my_plugin.c

# Windows
gcc -shared -o plugins/myplugin.dll my_plugin.c
```

插件在运行时由 `module_manager_load_plugin()` 加载,它会解析 `plugin_name`、`plugin_key`、`plugin_detect` 这三个符号。

### Vision Plugin(摄像头)

nexfetch 自带一个 Vision Plugin,用来检测和显示摄像头信息:

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

/* 统计 /sys/class/video4linux/ 下的视频设备数量 */
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

/* 从 /sys/class/video4linux/videoX/name 读取第一个摄像头的名字 */
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

编译 Vision Plugin:

```bash
# Linux/macOS
gcc -shared -fPIC -o plugins/vision.so plugins/vision.c

# Windows(需要 V4L2 模拟层)
gcc -shared -o plugins/vision.dll plugins/vision.c
```

然后在 `config/config.json` 里注册:

```json
{
  "plugins": ["plugins/vision.so"],
  "modules": [ "os", "kernel", "...", "visioncamera" ]
}
```

**工作原理:**

| 步骤 | 说明 |
| --- | --- |
| 扫描 | `count_video_devices()` 扫描 `/sys/class/video4linux/`,统计 `video*` 节点数 |
| 取名字 | `get_first_camera_name()` 从 `videoX/name` 读取设备名称 |
| 输出 | 打印`摄像头名称(N 个摄像头)`,没找到就打印 `No camera detected` |

**输出示例:**

```
Vision Camera: Integrated_Webcam_HD: Integrate (+1 more cameras)
```

> 在 Linux 上,一个物理摄像头经常会同时暴露出多个 `/dev/videoN` 节点(比如 `video0` 用来采集画面,`video1` 用来读取元数据)。Vision Plugin 会把这些节点都算进去。

### Vision for Nexfetch Plugin(版本号)

一个很小的插件,用来显示 Vision for Nexfetch 的版本号,直接从正在运行的 `nexfetch` 二进制文件里读取,不是写死的:

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

编译并注册:

```bash
gcc -shared -fPIC -o plugins/vision_nexfetch.so plugins/vision_nexfetch.c
```

```json
{
  "plugins": ["plugins/vision_nexfetch.so"],
  "modules": [ "os", "kernel", "...", "vision_nexfetch" ]
}
```

**输出示例:**

```
Vision for Nexfetch: v1.1.0
```

> 说明:
> - presenter 会自动在值前面加上插件名前缀(`Vision for Nexfetch: <value>`),所以 `plugin_detect()` 只需要写版本号字符串就行。
> - Linux/macOS 上,nexfetch 编译时带 `-rdynamic`,会导出 `nexfetch_version` 符号(定义在 `src/module_manager.c` 里)。插件在运行时用 `dlsym(RTLD_DEFAULT, "nexfetch_version")` 读取这个符号,所以版本号永远和当前运行的二进制一致,不用手动改。
> - Windows 上,插件通过 `GetProcAddress` 从宿主可执行文件里读取同一个符号。

要升级 nexfetch 的版本号,只需要改 `include/nexfetch.h` 里的 `NEXFETCH_VERSION`,插件会自动读到新值。

## 主题

| 主题 | 说明 |
| --- | --- |
| `boxed` | 信息放在圆角 Unicode 边框里(默认) |
| `classic` | 传统 neofetch 风格的 key: value 布局 |
| `modern` | 树状布局,用 `├─` / `╰─` 连接 |

## Logo

logo 放在 `logos/` 目录下,支持两种格式:

- ASCII 文本(`.txt`)— 纯 ANSI 字符画,直接加载
- 图片(`.png`、`.jpg`、`.gif` 等)— 通过 `chafa` 转成 ANSI 字符画

### logo 查找顺序

1. `config.json` 的 `"logo"` 字段或 `--logo` 参数指定的路径
2. `logos/<distro_id>.txt`(从 `/etc/os-release` 自动识别)
3. `logos/tux.txt`(兜底方案)

### 内置 logo

| 文件 | 发行版 |
| --- | --- |
| `nexfetch.png` | nexfetch 项目 logo(图片) |
| `nexfetch.txt` | nexfetch 项目 logo(ASCII) |
| `alpine.txt` | Alpine Linux |
| `arch.txt` | Arch Linux |
| `debian.txt` | Debian |
| `fedora.txt` | Fedora |
| `hyggshi_OS.txt` | Hyggshi OS |
| `tux.txt` | Tux(通用 Linux 兜底 logo) |
| `ubuntu.txt` | Ubuntu |
| `Tux.png` | Tux(图片) |
| `Windows_logo_11.png` | Windows 11(图片) |

### 自动缩放

如果 logo 和信息框加起来超出终端宽度,nexfetch 会自动隐藏 logo,保证输出不会乱掉。

## 项目结构

```
nexfetch/
├── config/
│   └── config.json          # 默认配置
├── include/                 # 公共头文件
│   ├── module.h             # 模块系统 API
│   ├── nexfetch.h           # 核心类型、配置、颜色宏
│   ├── platform.h           # 平台抽象 API
│   ├── presenter.h          # 主题/presenter API
│   └── util.h               # 工具函数
├── logos/                   # ASCII 和图片 logo
├── modules/                 # 内置模块实现
│   ├── ansi.c               # ANSI 可见长度计算
│   ├── battery.c
│   ├── color.c              # 颜色条
│   ├── cpu.c
│   ├── custom.c
│   ├── de.c
│   ├── disk.c
│   ├── display.c
│   ├── gpu.c
│   ├── host.c
│   ├── kernel.c
│   ├── locale.c
│   ├── logo.c               # logo 加载(文本 + 通过 chafa 处理图片)
│   ├── memory.c
│   ├── network.c
│   ├── os.c
│   ├── packages.c
│   ├── shell.c
│   ├── swap.c
│   ├── uptime.c
│   └── ...
├── platform/                # 各平台专属实现
│   ├── linux/               # Linux 实现
│   ├── macos/                # macOS 实现
│   └── windows/              # Windows 实现
├── plugins/                 # 插件目录(共享库放这里)
├── src/                     # 核心源码
│   ├── config.c             # JSON 配置解析
│   ├── main.c                # 入口和 CLI 处理
│   ├── module_manager.c      # 模块注册 + 插件加载
│   ├── presenter.c           # 主题渲染
│   └── util.c                 # 通用工具函数
├── Makefile                 # 构建系统
├── LICENSE                  # GPL-3.0
└── README.md
```

## 构建

Makefile 会自动识别平台,选择对应的源文件:

```bash
make          # 构建 nexfetch
make run      # 构建后直接运行
make clean    # 清理构建产物
```

### 各平台说明

- Linux:链接 `-ldl` 来支持动态插件加载
- macOS:使用 `platform/macos/platform_macos.c`
- Windows:使用 `platform/windows/platform_windows.c`,构建出 `nexfetch.exe`

### CI/CD

每次推送 `v*` 标签都会触发 `.github/workflows/release.yml`,构建 `amd64`/`arm64`/`armhf`,打包出 `.deb`、`.rpm`、`x86_64` 的 Arch 包,以及 `amd64`/`arm64` 的 AppImage,然后统一发布到 GitHub Releases。

## Benchmark

在同一台机器上测的,每个工具都跑了多次并计时。数值越小越快。

### 默认配置

| 工具 | 平均值 | 中位数 | 标准差 | 最小值 | 最大值 |
| --- | --- | --- | --- | --- | --- |
| `./nexfetch` | 6.38ms | 6.29ms | 0.44ms | 5.76ms | 8.03ms |
| `./nexfetch-ghvbb` | 9.06ms | 8.91ms | 0.72ms | 8.33ms | 14.03ms |
| `fastfetch` | 34.28ms | 33.95ms | 1.34ms | 32.67ms | 39.37ms |
| `./neofetch` | 452.58ms | 446.10ms | 19.21ms | 431.15ms | 577.88ms |

<img src="Resources/benchmark_bar_normal.png"/>

### `--fast` 配置

| 工具 | 平均值 | 中位数 | 标准差 | 最小值 | 最大值 |
| --- | --- | --- | --- | --- | --- |
| `./nexfetch --fast` | 2.15ms | 2.08ms | 0.28ms | 1.81ms | 3.79ms |
| `./nexfetch-ghvbb` | 8.95ms | 8.93ms | 0.26ms | 8.45ms | 10.28ms |
| `fastfetch` | 34.97ms | 34.15ms | 3.26ms | 32.65ms | 61.37ms |
| `./neofetch --fast` | 445.92ms | 442.80ms | 10.68ms | 430.28ms | 487.43ms |

<img src="Resources/benchmark_bar.png"/>

两种配置下,nexfetch 比 fastfetch 快大约 5 倍,比 neofetch 快 70 到 200 倍左右。这里的 `nexfetch-ghvbb` 指的是 [ghvbb/NexFetch](https://github.com/ghvbb/NexFetch),一个名字相似但没有关系的项目,放进来只是方便对照,因为不少人会把两者搞混。

## 许可证

本项目使用 GNU General Public License v3.0,详见 [LICENSE](LICENSE) 文件。

## 仓库

- 源码:[https://github.com/Hyggshi-OS-Foundation/nexfetch](https://github.com/Hyggshi-OS-Foundation/nexfetch)
- Issues:[https://github.com/Hyggshi-OS-Foundation/nexfetch/issues](https://github.com/Hyggshi-OS-Foundation/nexfetch/issues)

---

<p align="center">
  由 <a href="https://github.com/Hyggshi-OS-Foundation">Hyggshi OS Foundation</a> 和 <a href="https://github.com/Hyggshi-OS-Research-Technology">Hyggshi OS Research Technology</a> 构建
</p>