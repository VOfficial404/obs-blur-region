<div align="center">

# Blur Region for OBS

**游戏没有主播模式？在 OBS 里自己盖。**

一个 OBS Studio 来源插件：在游戏画面上拖出一块区域，实时高斯模糊、打马赛克或用纯色遮挡，<br>
用来盖住聊天框、玩家 ID、坐标、好友列表这些不想让观众看到的东西。

[![版本](https://img.shields.io/github/v/release/VOfficial404/obs-blur-region?color=5b8def&label=版本)](https://github.com/VOfficial404/obs-blur-region/releases/latest)
[![构建](https://img.shields.io/github/actions/workflow/status/VOfficial404/obs-blur-region/release.yaml?label=构建)](https://github.com/VOfficial404/obs-blur-region/actions)
[![许可](https://img.shields.io/github/license/VOfficial404/obs-blur-region?label=许可)](LICENSE)

[下载](https://github.com/VOfficial404/obs-blur-region/releases/latest) · [安装](#安装) · [使用](#使用) · [常见问题](#常见问题) · [English](#english)

</div>

## 为什么需要它

很多游戏没有「主播模式」，聊天框、私聊、玩家名字会直接出现在直播画面里。OBS 自带的滤镜只能作用于整个来源，想只糊掉一小块，要么复制一份游戏采集再裁剪加模糊，要么叠一张图片，位置一变就得重新对。

这个插件把「一块要糊掉的区域」做成一个独立的来源：放在游戏采集上方，在预览里拖动、拉伸红框就能调整位置和大小，所见即所得。

## 功能

- **可视化调整**：区域就是一个普通来源，用 OBS 自带的变换框拖动、缩放、旋转，也能锁定、设置显示/隐藏快捷键。
- **三种效果**：高斯模糊、马赛克、纯色遮挡。
- **可调参数**：模糊强度、马赛克块大小、颜色叠加（例如在模糊上压一层 30% 的黑色）、不透明度、圆角、边缘羽化。
- **始终对准**：游戏采集被移动、缩放、旋转或裁剪后，区域里显示的依然是它正下方那块画面。
- **想盖几处就加几个**：每个区域互不影响。
- **开销小**：只处理区域周围的一小块画面，大半径模糊还会先降采样，强度开到最大也只是几次小纹理的绘制。
- 中文和英文界面跟随 OBS 的语言设置。

## 安装

需要 OBS Studio 31 或更新版本，**官网版、Steam 版、便携版都支持**。到 [Releases](https://github.com/VOfficial404/obs-blur-region/releases/latest) 下载对应系统的安装包：

| 系统 | 文件 | 安装方式 |
|---|---|---|
| Windows（官网版、Steam 版） | `obs-blur-region-*-windows-x64-setup.exe` | **双击安装**，一路下一步；升级时直接运行新版安装程序 |
| Windows（便携版） | 同上，或 `obs-blur-region-*-windows-x64.zip` | 安装时把目录改成 OBS 文件夹下的 `plugins\obs-blur-region`；用 zip 时解压到同一位置 |
| macOS（官网版、Steam 版） | `obs-blur-region-*-macos-universal.pkg` | 双击安装 |
| Ubuntu 24.04 | `obs-blur-region-*-x86_64-linux-gnu.deb` | `sudo apt install ./obs-blur-region-*.deb` |

Windows 安装程序默认装到 `C:\ProgramData\obs-studio\plugins\obs-blur-region`，官网版和 Steam 版的 OBS 都会从这里加载插件。Steam 版请不要把插件放进 Steam 的 OBS 安装目录。

## 使用

1. 在场景的 **来源** 里点 `+` → **模糊区域**。
2. 在来源列表里把它拖到游戏采集的**上方**。
3. 在预览窗口里拖动、拉伸红框，盖住聊天框。
4. 双击这个来源打开属性，选效果、调强度。

需要遮挡多处，就再添加几个「模糊区域」。

### 选项

| 选项 | 说明 |
|---|---|
| 效果 | 高斯模糊 / 马赛克 / 纯色遮挡 |
| 要模糊的源 | 默认「自动」：同一场景中位于本区域下方、覆盖到区域中心的最近的游戏 / 窗口 / 显示器采集；没有采集时用最近的可见来源。也可以手动指定某个来源（改名后会自动跟上），或一个嵌套场景 |
| 模糊强度 | 1–300，越大越糊 |
| 马赛克块大小 | 以游戏画面的像素计 |
| 颜色 / 颜色叠加强度 | 在模糊或马赛克上叠一层颜色；纯色遮挡时就是遮挡用的颜色 |
| 整体不透明度、圆角半径、边缘羽化 | 外观 |
| 基础宽度 / 高度 | 区域的原始尺寸。平时直接在预览里拉伸即可，不用改这里 |

### 小技巧

- **快速开关**：在「设置 → 快捷键」里给每个区域设置「显示 / 隐藏」，打游戏时一键盖上或掀开。
- **防误拖**：调好后点来源列表里的小锁锁定。
- **完全看不清**：轻度高斯模糊下，大号文字有时还能认出来。遮聊天建议用马赛克（块 ≥ 15 px）、模糊强度 ≥ 40，或者再叠 30% 以上的颜色。

## 常见问题

**在「滤镜」里找不到**
它不是滤镜，而是来源：在主界面左下角的 **来源** 面板点 `+` → **模糊区域**。看不到「来源」面板时，在菜单栏 **停靠窗口** 里勾选「来源」。

**用采集卡（视频采集设备）玩主机游戏**
自动模式只认游戏、窗口和显示器采集，采集卡上方如果还叠着透明的边框图片，可能会选中那张图片。在属性里把「要模糊的源」手动选成采集卡即可。

**区域是透明的，什么都没显示**
区域和要模糊的来源必须在同一个场景（或同一个分组）里，并且来源在区域**下方**。如果游戏采集放在一个嵌套场景里，在属性里把「要模糊的源」选成那个场景。游戏还没被捕获到画面（黑屏）时，区域也是空的。

**区域盖住了摄像头 / 其他叠加层**
区域显示的是游戏画面，会盖住它和游戏之间的所有东西。把摄像头等来源移到区域上方即可。

**同一个区域想在多个场景里用**
用「粘贴（复制）」而不是「粘贴（引用）」，每个场景一个独立的区域。引用同一个区域时，它会跟随正在直播的那个场景。

**会不会很吃性能**
每个区域每帧额外绘制一次游戏画面和几次缩小后的模糊，开几个区域对显卡几乎没有影响。

## 从源码构建

本项目基于 [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate)，只依赖 libobs。

```sh
# Windows（Visual Studio 2022）
cmake --preset windows-x64
cmake --build --preset windows-x64

# macOS（Xcode 16）
cmake --preset macos
cmake --build --preset macos

# Ubuntu 24.04
cmake --preset ubuntu-x86_64
cmake --build --preset ubuntu-x86_64
```

单元测试覆盖区域到游戏画面的坐标换算（平移、缩放、旋转、裁剪）：

```sh
cmake --preset ubuntu-x86_64 -DENABLE_TESTS=ON
cmake --build --preset ubuntu-x86_64 --target region-map-tests
ctest --test-dir build_x86_64
```

更多说明见 [CONTRIBUTING.md](CONTRIBUTING.md)。

## 许可

[GPL-2.0-or-later](LICENSE)

---

## English

**Blur Region for OBS** adds a *Blur Region* source to OBS Studio. Put it above your game capture, drag and resize it in the preview like any other source, and it shows a blurred, pixelated or solid-color copy of the game underneath — handy for hiding chat boxes, player names and coordinates in games that have no streamer mode.

Options: effect (Gaussian blur, pixelate, solid color), blur strength, pixel block size, tint color and amount, opacity, corner radius and edge feather. The region stays aligned when the game capture is moved, scaled, rotated or cropped. Add as many regions as you need.

Install the package for your platform from [Releases](https://github.com/VOfficial404/obs-blur-region/releases/latest) and restart OBS. Requires OBS Studio 31 or newer; the website, Steam and portable builds are all supported. On Windows (website and Steam builds) the installer puts the plugin in `C:\ProgramData\obs-studio\plugins\obs-blur-region`.

The region and the source it blurs must be in the same scene (or group), with the source below the region. If the game sits in a nested scene, pick that scene as the source to blur.
