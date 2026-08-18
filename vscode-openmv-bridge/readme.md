# OpenMV IDE Bridge for VS Code

Seamlessly bridge **OpenMV IDE** with **VS Code** to enable live AI-assisted development, error diagnosis, and real-time serial streaming.

## Features

1. **Live Syntax & Diagnostics Sync**: Automatically reflects OpenMV IDE's syntax errors, warnings, and MicroPython exceptions directly in the VS Code **Problems** panel and in-editor wavy lines.
2. **Real-Time Serial Monitor**: Streams all camera prints (`print(...)`), REPL interactions, and runtime Tracebacks from OpenMV Cam into VS Code's **Output Channel** or **Integrated Terminal**.
3. **Smart File Sync**: Works with OpenMV IDE's auto-reload engine so that edits saved in VS Code are instantly reloaded in OpenMV IDE without requiring manual focus clicks.
4. **AI-Ready Context**: Allows AI coding assistants (e.g. Copilot, Cursor, Roo, Cline, Coding extensions) to directly read diagnostics from the Problems panel and serial outputs from the terminal to guide intelligent code suggestions.

## Configuration

* `openmvBridge.serverUrl`: WebSocket URL of OpenMV IDE bridge (default: `ws://127.0.0.1:23888`).
* `openmvBridge.autoConnect`: Auto-connect to OpenMV IDE on launch (default: `true`).
* `openmvBridge.enableDiagnostics`: Populate VS Code Problems panel with OpenMV diagnostics (default: `true`).
* `openmvBridge.enableSerialMonitor`: Stream serial output into the OpenMV Serial Monitor (default: `true`).
* `openmvBridge.useTerminalView`: Use interactive Pseudoterminal instead of Output Channel (default: `false`).

## Quick Commands

Press `Ctrl+Shift+P` and type:
* `OpenMV: Show Serial Terminal` - Focus the live serial output
* `OpenMV: Connect to OpenMV IDE` - Reconnect to the bridge server
* `OpenMV: Clear Serial Output` - Clear the console buffer
