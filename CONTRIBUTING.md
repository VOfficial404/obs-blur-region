# 参与贡献

感谢你愿意改进 Blur Region for OBS。提交 Issue 或 Pull Request 前，请先读完这一页。

## 报告问题

- 先搜一下 [Issues](https://github.com/VOfficial404/obs-blur-region/issues)，看看是不是已经有人报告过。
- 附上 OBS 版本、操作系统和插件版本。
- 附上 OBS 日志（「帮助 → 日志文件 → 上传当前日志文件」），插件的日志行以 `[obs-blur-region]` 开头。
- 区域显示不对时，附一张场景的截图，并说明区域和游戏采集在来源列表里的位置。

## 开发环境

项目基于 [obs-plugintemplate](https://github.com/obsproject/obs-plugintemplate)，CMake 预设会自动下载对应版本的 OBS 源码和依赖。

| 平台 | 工具 |
|---|---|
| Windows | Visual Studio 2022、CMake 3.30 |
| macOS | Xcode 16、CMake 3.30 |
| Ubuntu 24.04 | CMake 3.28、`ninja-build`、`build-essential`、`libsimde-dev`、OBS PPA 的 `obs-studio` |

```sh
cmake --preset windows-x64      # 或 macos / ubuntu-x86_64
cmake --build --preset windows-x64
```

## 代码结构

```
src/
  plugin-main.c     模块入口，注册来源
  blur-region.c     「模糊区域」来源：定位所在场景和目标、渲染、属性
  region-map.*      区域像素到目标像素的仿射换算（不依赖 OBS）
data/
  effects/blur-region.effect  降采样、高斯模糊、最终合成和纯色遮挡的着色器
  locale/           界面文字
tests/              region-map 的单元测试
```

## 测试

```sh
cmake --preset ubuntu-x86_64 -DENABLE_TESTS=ON
cmake --build --preset ubuntu-x86_64 --target region-map-tests
ctest --test-dir build_x86_64 --output-on-failure
```

改了坐标换算，请补上对应的测试。着色器和渲染的改动请在至少一个平台的 OBS 里实际看过效果。

## 代码风格

- C17，格式由 `.clang-format`（clang-format 19）和 `.gersemirc`（CMake）决定，CI 会检查。
- 提交前运行 `./build-aux/run-clang-format` 和 `./build-aux/run-gersemi`。
- 界面文字放进 `data/locale/*.ini`，`zh-CN.ini` 和 `en-US.ini` 同时更新。

## 提交 Pull Request

- 一个 PR 只做一件事，标题说明做了什么。
- 用户能感觉到的变化，写进 `CHANGELOG.md` 的 `[Unreleased]` 一节。
- 提交 PR 即表示你同意以 [GPL-2.0-or-later](LICENSE) 授权你的代码。
