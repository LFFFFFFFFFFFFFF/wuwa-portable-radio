# 鸣潮 · 随身电台

[English](README.en.md)

《鸣潮》Windows 随身电台实验 Mod，提供音源切换、播放控制、网易云歌曲／歌单链接导入和当前曲目信息显示。**摩托音乐下车后不中断是随身电台的一项功能。**

**状态：实验版本。下车续播已在一套 3.7 客户端实测；网易云导入与客户端控制已单独验证，新快捷键与曲目浮层仍待游戏内确认。没有通用版本兼容性承诺。**

## 功能

| 按键 | 功能 |
| --- | --- |
| F7 | 显示当前音源、歌名、歌手和播放状态 |
| F8 | 暂停／继续当前来源 |
| F9 | 游戏内置电台／网易云切换 |
| F10 | 已导入列表的下一首 |
| F11 | 打开歌曲／公开歌单链接导入窗口 |

快捷键仅在游戏窗口处于前台时响应。首次进入场景后先上、下摩托一次，让插件观察控制器线程。

切歌、暂停或切换音源时，右上角自动显示曲目信息约 6 秒；F7 可再次查看。网易云曲名来自客户端实际 SMTC 媒体会话；发送切歌请求后会先显示「切歌待确认」，不会把请求目标当成已经播放的歌曲。客户端自动换歌也会更新提示。本地音乐显示文件名。**内置摩托电台目前显示音源，下车续播仍可用，内置曲名暂不能读取。**

显示由独立的 `RadioHud.exe` 完成，不抢焦点、不拦截鼠标；切出游戏时隐藏，游戏退出后关闭。适用于窗口／无边框模式，独占全屏下不保证可见。在 `[HUD]` 中可设置 `Enabled=0` 关闭、`AlwaysVisible=1` 常显，或用 `ShowSeconds` 修改提示时长。旧配置不补写也会使用默认值。

网易云版的工作方式是：导入歌曲／公开歌单元数据，由**已登录的网易云官方客户端**负责音频播放。需要在网易云的「设置 → 系统」中开启 SMTC。账号的播放权限仍由官方客户端决定。项目不提取 Cookie、不解密下载缓存，也不提供受限歌曲解锁。

F10 按 Mod 导入的列表切换曲目；自然结束时的后续播放由网易云客户端自己的队列负责。`Source=Local` 还可启用 MP3/WAV 文件夹播放。

## 构建

需要 Windows x64、PowerShell 7、Visual Studio Build Tools 的「使用 C++ 的桌面开发」、Windows 10/11 SDK（包含 C++/WinRT）和 .NET Framework 4.x 编译器。

```powershell
pwsh -NoProfile -File scripts/build.ps1
```

输出位于 `build/`：

- `PortableRadioNative-loadlate.asi`
- `NetEaseImporter.exe`
- `RadioHud.exe`
- `MinHook-LICENSE.txt`

构建脚本自动查找 MSVC 和 SDK，不需要修改用户目录或编译器版本。每次构建会运行无网络的导入器自测。可加 `-WithTests` 编译合成控制器和原生测试程序；详见 [验证范围](docs/VALIDATION.md)。

## 安装到匹配的客户端

本项目未附带加载器。当前接入方式依赖已有、可用且支持 ASI 的 [OptiScaler](https://github.com/optiscaler/OptiScaler)。**不要覆盖已经存在的 `dxgi.dll`。**

1. 完全退出游戏，备份现有 `OptiScaler.ini`。
2. 在 `Client/Binaries/Win64/plugins/` 放入构建出的 ASI、导入器、`RadioHud.exe` 及第三方许可证。升级时也要复制新增的显示程序。
3. 在 OptiScaler 的 `[Plugins]` 下启用 `LoadAsiPlugins=true`，并把 `Path` 指向该插件目录；确认没有意外启用目录中的其他 ASI。
4. 将 `PortableRadio.ini.example` 复制到 `Client/Saved/Config/PortableRadio.ini`。如写入中文路径，请用 UTF-16 保存。
5. 保持网易云客户端登录并开启 SMTC，正常启动游戏。插件依赖延后加载，并会在接入前再次等待。

安装只新增自己的插件文件和配置。这里没有官方游戏文件、音频文件、运行库、登录状态或个人安装备份。

源码锁定了本次审计的程序集 MVID `398499d1-a649-4e50-8ac2-270d7cbbc858` 和目标方法标记。程序集、签名、字段或 JIT 函数体不匹配会拒绝挂钩。**不要为了套用到其他版本而直接删除这些校验。**

## 导入与配置

F11 → 粘贴网易云歌曲／公开歌单分享链接 → 读取并检查曲目 → 导入为当前电台列表。导入成功后切回游戏，重新切换 F9 读取列表。

支持 `music.163.com` 及其子域名下的歌曲／歌单链接，包括 `#/song`、`#/playlist` 网页地址。私密歌单和未公开的账号列表不在此版本范围内。公开网页接口发生变化时，导入器可能需要适配。

| 文件 | 用途 |
| --- | --- |
| `Client/Saved/Config/PortableRadio.ini` | 快捷键、来源和本地音量 |
| `Client/Saved/Config/PortableRadioNetEase.ini` | Mod 的曲目列表 |
| `Client/Saved/Logs/PortableRadioNative.log` | 初始化和控制诊断 |
| `Client/Saved/Config/PortableRadioStatus-<pid>.ini` | 当前曲目临时状态，由浮层读取；不上传 |

用导入器单独运行时，可显式指定列表路径：

```powershell
NetEaseImporter.exe --library "<your-library-path>"
```

## 卸载

完全退出游戏后删除本项目的 ASI、导入器和 `RadioHud.exe`，并恢复备份的 OptiScaler 插件设置。请保留自己的音乐文件与自定义配置，除非你确实想删除它们。

## 代码与许可

- `src/`：原生 ASI、电台控制、Windows 媒体控制与链接导入器。
- `tests/`：手写的合成控制器及独立测试程序，**不是游戏源码**。
- `third_party/minhook/`：保留原许可的 MinHook v1.3.4。

原创代码使用 [MIT](LICENSE)，第三方代码见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。项目与 Kuro Games、网易云和 OptiScaler 均无隶属或官方支持关系。
