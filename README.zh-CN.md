<p align="center">
  <img src="resources/logo.png" width="96" height="96" alt="Quickky Explorer Tabs 标志">
</p>

<h1 align="center">Quickky Explorer Tabs</h1>

<p align="center">熟悉的资源管理器，统一的工作区，自有的标签。</p>

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-blue" alt="MIT 许可证"></a>
  <a href="VERSION"><img src="https://img.shields.io/badge/preview-experimental-orange" alt="实验预览版"></a>
  <img src="https://img.shields.io/badge/platform-Windows_x86__64-0078D4" alt="Windows x86_64">
</p>

<p align="center"><a href="README.md">English</a> · <strong>简体中文</strong></p>

Quickky 将新打开的文件资源管理器窗口，直接收纳为 Explorer 内的标签。继续使用熟悉的文件管理器，不增加独立浏览窗体，也不需要另一套文件操作方式。

> **当前为实验预览版，暂不适合日常使用。** 已在 Windows 10 构建 19045 和一套 Windows 11 环境中验证核心分组功能；普遍兼容性尚未验证。不支持的配置不会被接管，请勿使用重要窗口测试。

## 功能

- Windows 11 保留原生标签栏，不额外绘制重叠标签栏。
- Windows 10 自绘直角标签，支持最大化、最小化、还原及窗口恢复。
- 自动将符合条件的新文件夹窗口收纳到同一标签组。
- 排除控制面板、打印机、网络连接等系统管理页面，不将其收纳为标签。
- 点击标签切换文件夹，点击关闭按钮关闭对应标签。
- 点击 **+**，在活动标签的**当前目录**打开并收纳新窗口。
- Windows 11 关闭标签功能或退出 Quickky 后，已合并标签仍由 Explorer 保留。
- 常驻系统托盘，不显示独立的文件浏览窗体。
- 托盘菜单支持英文和简体中文，跟随 Windows 界面语言。

## 开始使用

完整解压 `QTabs_vX.X.X.zip`，所需的 Visual C++ 运行库 DLL 已包含在压缩包中，无需另行安装。将所有解压出的 DLL 与 `QTabs.exe` 保持在一起，正常启动 EXE 即可，不请求管理员权限。标签功能默认关闭；此前主动启用过的设置会被保留。启动后可在系统托盘中找到 Quickky 图标；如果图标被隐藏，请展开托盘折叠区域。

### 托盘菜单

右键点击托盘图标：

- **开机自启动**：设置或取消当前 Windows 账户的登录自启动。
- **启用 tabs**：收纳符合条件的窗口；取消勾选时停止接管。
- **关于**：查看作者、版本、构建时间和 GitHub 链接。
- **退出**：移除 Quickky 接管并退出托盘程序。

勾选状态分别反映实际的自启动注册状态和标签启用状态。自启动仅作用于当前账户；Quickky 不安装系统服务，也不修改 Explorer 的全局设置。

### 隔离试用

先创建专用目录，再运行 `QTabs.exe --capture-test-root "C:\QuickkyTabs"`。此模式仅收纳该目录内的文件夹窗口，不保存标签开关设置。在该目录下打开文件夹后，可点击标签切换，或点击 **+** 在活动标签的当前目录再打开一个视图。关闭标签功能或退出程序后，恢复普通 Explorer 行为；Windows 11 已合并的标签不会拆回独立窗口。请勿用预览版管理重要或无关的窗口。

## 兼容性与限制

| 项目 | 当前预览版 |
| --- | --- |
| 架构 | x86_64 |
| Windows 11 | 已在构建 26300 验证原生合并、当前目录 + / Ctrl+T、退出保留标签 |
| Windows 10 | 已在构建 19045 验证接管、当前目录 +、最大化/还原及故障恢复 |
| 已有原生多标签窗口 | 符合条件的源窗口逐个合并标签，不关闭无关标签 |
| 原生工作区容量 | 最多 128 个标签，额外窗口保持独立 |
| Windows 10 工作区容量 | 最多 32 个标签，额外窗口保持独立 |
| 迁移状态 | 遵循原生拖放行为，选择状态和导航历史可能重置 |
| 无障碍、主题、多显示器及模态窗口 | 尚未完整验证 |

项目面向 x86_64 平台的 Windows 10/11，不接管桌面、任务栏、打开/保存对话框或其他虚拟桌面的窗口。
当前原生接管仅支持已验证的 Explorer 组件版本 `10.0.26100.9949`；Windows 更新后可能暂不接管，待重新验证兼容性。不支持的经典标题栏布局保留独立窗口，不覆盖 Explorer 文件区域。结果不确定时不自动反复迁移。超时不代表操作已撤销，已启动的原生操作仍可能完成。无需管理员权限。

收纳不是瞬时完成：需要等待 Explorer 的文件夹视图就绪，Explorer 繁忙时仍可能有延迟，并短暂显示独立窗口。

## 问题反馈

发现问题或有功能建议？欢迎[提交 Issue](https://github.com/achaosxyz/quickky-explorer-tabs/issues)。请附上“关于”中的版本和构建时间、Windows 版本、复现步骤以及期望行为。分享截图或诊断信息前，请遮蔽个人文件名和路径。

欢迎反馈缺陷、提出功能想法或修正翻译。涉及 Explorer 行为的大幅修改，请先讨论方案。

## 作者

**aCHAOS** · [GitHub](https://github.com/achaosxyz) · [achaosxyz@gmail.com](mailto:achaosxyz@gmail.com)

## 许可证

项目源码采用 [MIT](LICENSE) 许可证。随包附带的 Microsoft Visual C++ 运行库二进制保留其原有许可条款。
