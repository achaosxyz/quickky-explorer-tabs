<p align="center">
  <img src="resources/logo.png" width="96" height="96" alt="QTabs 标志">
</p>

<h1 align="center">QTabs</h1>

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-blue" alt="MIT 许可证"></a>
  <img src="https://img.shields.io/badge/platform-Windows_x86__64-0078D4" alt="Windows x86_64">
  <a href="https://github.com/achaosxyz/quickky-explorer-tabs/actions/workflows/windows-smoke.yml?query=branch%3Adevelop" title="develop 分支测试状态与运行记录">
    <img src="https://github.com/achaosxyz/quickky-explorer-tabs/actions/workflows/windows-smoke.yml/badge.svg?branch=develop" alt="Windows smoke（develop）">
  </a>
</p>

<p align="center"><a href="README.md">English</a> · <strong>简体中文</strong></p>

QTabs 将新打开的文件夹窗口收纳到同一个标签工作区，保留熟悉的 Explorer，不增加独立浏览窗体。

## 功能

- Windows 11 使用 Explorer 原生标签，Windows 10 使用直角标签。
- 自动收纳新文件夹窗口，排除控制面板等系统管理页面。
- 点击标签切换文件夹，通过关闭按钮关闭标签。
- 点击 **+**，在活动标签的**当前目录**打开新标签。
- Windows 10 支持最大化、最小化和还原。
- 常驻系统托盘，单实例后台运行。

## 开始使用

1. 从 [Releases](https://github.com/achaosxyz/quickky-explorer-tabs/releases/latest) 下载 `QTabs_vX.X.X.zip`。
2. 完整解压，运行 `QTabs.exe`。
3. 右键点击 QTabs 托盘图标，勾选 **启用Tabs**。

压缩包已包含 Visual C++ 运行库，无需另行安装，也无需管理员权限。请将解压后的文件保持在一起。

### 托盘菜单

右键点击托盘图标：

- **开机自启动**：登录 Windows 时启动 QTabs。
- **启用Tabs**：开启或关闭窗口自动收纳。
- **兼容性检测**：查看 Explorer DLL 版本和支持状态。
- **关于**：查看作者、版本和构建信息。
- **退出**：退出 QTabs。

## 兼容性与限制

- 适用于 Windows x86_64，已在 Windows 10 构建 19045 和 Windows 11 构建 26300 验证；兼容性取决于 Windows 和 Explorer 版本。
- Windows 11 使用内置兼容性检测；不受支持的 Explorer 窗口保持独立。
- Windows 11 最多 128 个标签，Windows 10 最多 32 个；超出容量的窗口保持独立。
- 新窗口可能在收纳前短暂显示；原生合并可能重置文件选择和导航历史。
- 关闭标签功能或退出 QTabs 后，Windows 10 恢复独立窗口，Windows 11 原生标签仍保留在 Explorer 中。

## 问题反馈

欢迎[提交 Issue](https://github.com/achaosxyz/quickky-explorer-tabs/issues)，请附上 QTabs 版本、Windows 版本和复现步骤。分享截图或日志前，请遮蔽个人文件名和路径。

## 作者

**aCHAOS** · [GitHub](https://github.com/achaosxyz) · [achaosxyz@gmail.com](mailto:achaosxyz@gmail.com)

## 许可证

项目源码采用 [MIT](LICENSE) 许可证。随包附带的 Microsoft Visual C++ 运行库二进制保留其原有许可条款。
