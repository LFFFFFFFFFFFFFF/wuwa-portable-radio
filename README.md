# 鸣潮 · 随身电台

[English](README.en.md)

《鸣潮》Windows 随身电台实验 Mod，提供音源切换、播放控制、网易云歌曲／歌单链接导入和当前曲目信息显示。**摩托音乐下车后不中断是随身电台的一项功能。**

**状态：实验版本。下车续播和内置曲名接口已在一套 3.7 客户端观察到成功记录；v2.6 已适配本次审计的 3.7.18 客户端，界面、控制和背景音乐避让已离线验证，仍待游戏内确认。没有通用版本兼容性承诺。**

## 功能

| 按键 | 功能 |
| --- | --- |
| `-` | 上一首；内置音源调用官方切歌逻辑 |
| `=` | 下一首；内置音源调用官方切歌逻辑 |
| Esc 下方的反引号／`·` 键 | 打开官方电台界面 |

仅在游戏前台且没有 Ctrl/Alt/Shift 修饰键时响应，按住不会连续切歌。旧 F7—F11 默认绑定已停用。首次进入场景后仍需上、下摩托一次，让插件观察控制器线程。

打开界面直接使用游戏原有的专辑、歌曲列表和播放模式。右上角旧信息卡替换为青色细长播放条，可点击上一首／暂停／下一首，右键打开音源菜单；骑行时沿用官方播放条。官方电台界面显示时，播放条隐藏，频道标题右侧出现同风格的「＋ 添加歌曲」和「音源」附加按钮。附加按钮是独立窗口，按游戏可见视图状态定位，未修改或分发官方 UI 资源。

「＋ 添加歌曲」打开与游戏电台一致布局的导入面板（唱片卡片、半透明歌曲条、底部圆角操作栏）：粘贴网易云歌曲／公开歌单链接 → 读取预览 → 勾选要添加的歌曲 → 添加到电台。**追加保留旧列表并按歌曲 ID 去重**；添加到的是随身电台的网易云音源列表，不会伪装成官方已解锁曲目。通过「音源」或播放条右键菜单切到网易云播放。网易云模式会自动重载更新后的列表。

内置曲名和播放状态沿用游戏的曲目配置及本地化接口；缺配置或译文时显示编号并清除旧名称。网易云曲名来自客户端实际 SMTC 会话，切歌尚未确认时会明确提示。本地音乐显示文件名。显示需要窗口／无边框模式，独占全屏不保证可见。

网易云版的工作方式是：导入歌曲／公开歌单元数据，由**已登录的网易云官方客户端**负责音频播放。需要在网易云的「设置 → 系统」中开启 SMTC。账号的播放权限仍由官方客户端决定。项目不提取 Cookie、不解密下载缓存，也不提供受限歌曲解锁。

`-`／`=` 按 Mod 导入的列表切换曲目；自然结束时的后续播放由网易云客户端自己的队列负责。`Source=Local` 还可启用 MP3/WAV 文件夹播放。

### 播放时屏蔽背景音乐

在播放条右键菜单或「音源」菜单中勾选 **播放时屏蔽游戏背景音乐**，默认开启；也可设置 `[Audio] SuppressBackgroundMusic=0/1`，菜单更改会保存在 Mod 配置中。

网易云以 SMTC 实际 `Playing` 状态、本地音乐以播放器实际播放状态为准，播放时借用游戏官方 `system_motor_radio=playing` 的 BGM 避让路由。暂停、停止、客户端会话消失、关闭开关或退出场景后释放覆盖，恢复当前内置电台应有的路由。内置电台本身继续沿用官方避让规则。此开关控制外部音源的额外避让，不会关闭内置电台原有规则。

没有更改总音量、语音／音效／电台音量或官方音频配置；没有增加游戏方法挂钩。实际声音效果由游戏的混音规则决定，仍需进游戏确认。首次使用仍需上、下摩托一次以接入已观察到的游戏线程。

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

源码锁定了本次审计的程序集 MVID `630e3143-f5eb-42e7-940c-5f1af45f3fb6` 和目标方法标记；背景音乐接口额外校验 `Aki.Core` 的 MVID `f2c9e329-0f5e-4676-9aa1-d7c93bbdb1d1`。程序集、签名、字段或 JIT 函数体不匹配会拒绝挂钩。**不要为了套用到其他版本而直接删除这些校验。**

## 导入与配置

官方电台页面的「＋ 添加歌曲」→ 粘贴歌曲／公开歌单分享链接 → 读取预览 → 勾选要添加的歌曲 → 添加到电台；从音源菜单选择网易云播放。

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
