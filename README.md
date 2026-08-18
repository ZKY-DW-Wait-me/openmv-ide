<img width="480" src="https://raw.githubusercontent.com/openmv/openmv-media/master/logos/openmv-logo/logo.png">

# OpenMV IDE & VS Code Bridge (Enhanced Edition)

[![License: MIT/GPL-3.0](https://img.shields.io/badge/License-GPL%203.0%20%2F%20MIT-blue.svg)](LICENSE)
[![Platform: Windows](https://img.shields.io/badge/Platform-Windows%20x64-brightgreen.svg)]()
[![VS Code Extension](https://img.shields.io/badge/VS%20Code%20Extension-v1.0.1-purple.svg)]()

🚀 **OpenMV IDE + VS Code 跨编辑器实时双向协同系统**。  
将 **VS Code 强大的现代化代码编辑生态** 与 **OpenMV IDE 专业的机器视觉硬件连接、实时图像预览、直方图分析及固件烧录能力** 深度融合。

---

## 🌟 核心特性 (Features)

### 1. 🔄 内存级双向代码实时同步 (Two-Way Live Code Synchronization)
- **输入空闲检测（Typing Idle Lockout）**：键盘高速输入时自动进入独占保护状态，停笔后（800ms）平滑同步，彻底杜绝打字被覆盖、丢字乱跳的问题。
- **光标与视口平滑保持（Smooth View Preservation）**：在 OpenMV IDE 接收外部代码更新时，自动锁定当前光标字符索引与滚动条高度，屏幕视口零跳动、零晃动。
- **无需手动保存**：直接监听内存编辑缓冲区，在 OpenMV IDE 或 VS Code 任意一处改动代码，另一处即刻生效。

### 2. 🚫 彻底消除弹窗干扰 (Silent Sync & No Reload Prompts)
- 源码底层移除了 Qt Creator 默认的 "File Changed on Disk" 模态冲突对话框，所有的双向同步完全在后台静默、平滑、无感地完成。

### 3. 📟 串口终端双向数据流与 REPL 交互 (Real-time Serial Terminal)
- **实时串口转发**：OpenMV Cam 的 `print()` 输出直接流式转发到 VS Code 的 **OpenMV Terminal (伪终端)** 与 **Output Channel**。
- **双向交互**：支持直接在 VS Code 终端中输入指令，与板载 MicroPython REPL 进行交互调试。

### 4. 🔍 MicroPython 语法诊断与错误实时投射 (Diagnostics & Problems Sync)
- OpenMV 内置的 MicroPython 语法检查器（LSP/Linter）生成的警告和报错，会自动同步渲染到 VS Code 的 **Problems（问题面板）** 与行内高亮波浪线。

---

## 📥 快速下载与使用 (Quick Start)

### 1. 获取安装包 (Releases)
从项目 `release/` 目录或 Release 发布页下载最新预编译成品：
- 🛠️ **OpenMV IDE 安装程序**：`openmv-ide-windows-5.0.1.exe`
- 🧩 **VS Code 桥接扩展**：`openmv-bridge-1.0.1.vsix`

### 2. 安装 VS Code 扩展
1. 打开 VS Code；
2. 进入扩展面板（`Ctrl+Shift+X`），点击右上角的 `...` 菜单；
3. 选择 **“从 VSIX 安装... (Install from VSIX...)”** 并选择 `openmv-bridge-1.0.1.vsix`；
4. 或在命令行直接执行：
   ```bash
   code --install-extension openmv-bridge-1.0.1.vsix
   ```

### 3. 运行与协同开发流程
1. 启动 **OpenMV IDE** 并连接您的 OpenMV 摄像头（OpenMV IDE 会在后台自动开启 WebSocket 桥接服务器：`ws://127.0.0.1:23888`）；
2. 在 **VS Code** 中打开包含 OpenMV 脚本（如 `main.py`）的工作区；
3. VS Code 状态栏左侧会显示 `$(circuit-board) OpenMV: Connected`；
4. 此时您可以在 VS Code 中编写 Python 代码，OpenMV IDE 负责实时显示帧缓冲区图像和视觉调试，两边代码毫秒级双向同步！

---

## 🛠️ 从源码编译 (Build from Source)

### 编译 OpenMV IDE (Windows)
要求环境：Qt 6.5.3 (MinGW 64-bit), CMake, Ninja, Python 3.8+。
```powershell
git clone --recursive https://github.com/ZKY-DW-Wait-me/openmv-ide.git
cd openmv-ide
python make.py --no-sign-application --no-sign-installer
```
编译产物位于 `build/install/bin/openmvide.exe` 及 `build/openmv-ide-windows-5.0.1.exe`。

### 编译 VS Code 扩展
```powershell
cd vscode-openmv-bridge
npm install
npm run compile
npx vsce package --allow-missing-repository
```

---

## 📄 开源许可证 (License)
本项目继承 OpenMV IDE 的 GPL-3.0 许可证，VS Code 桥接扩展遵循 MIT 许可证。
