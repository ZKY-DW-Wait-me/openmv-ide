import * as vscode from 'vscode';
import WebSocket from 'ws';

interface DiagnosticItem {
    line: number;       // 1-indexed
    column?: number;    // 1-indexed
    endLine?: number;
    endColumn?: number;
    severity: 'error' | 'warning' | 'info' | 'hint';
    message: string;
    source?: string;
}

interface DiagnosticsMessage {
    type: 'diagnostics';
    file: string;
    items: DiagnosticItem[];
}

interface SerialDataMessage {
    type: 'serial_data';
    payload: string;
    port?: string;
    connected?: boolean;
}

interface DeviceStatusMessage {
    type: 'device_status';
    connected: boolean;
    port?: string;
    board?: string;
}

interface NotificationMessage {
    type: 'notification';
    level: 'info' | 'warn' | 'error';
    message: string;
}

interface FileSavedInIdeMessage {
    type: 'file_saved_in_ide' | 'file_edited_in_ide';
    file: string;
    content?: string;
}

type BridgeMessage = DiagnosticsMessage | SerialDataMessage | DeviceStatusMessage | NotificationMessage | FileSavedInIdeMessage;

class OpenMVBridgeManager {
    private ws: WebSocket | null = null;
    private reconnectTimer: NodeJS.Timeout | null = null;
    private isDisposed = false;

    public isAutoSyncing: boolean = false;
    public isUserTyping: boolean = false;

    private statusBarItem: vscode.StatusBarItem;
    private diagnosticCollection: vscode.DiagnosticCollection;
    private outputChannel: vscode.OutputChannel;
    private terminalEmitter: vscode.EventEmitter<string> | null = null;
    private terminal: vscode.Terminal | null = null;

    private currentPort: string = '';
    private isDeviceConnected: boolean = false;

    constructor(private context: vscode.ExtensionContext) {
        // Initialize Status Bar
        this.statusBarItem = vscode.window.createStatusBarItem(vscode.StatusBarAlignment.Left, 100);
        this.statusBarItem.command = 'openmv-bridge.showMenu';
        this.context.subscriptions.push(this.statusBarItem);

        // Initialize Diagnostics Collection
        this.diagnosticCollection = vscode.languages.createDiagnosticCollection('openmv');
        this.context.subscriptions.push(this.diagnosticCollection);

        // Initialize Output Channel
        this.outputChannel = vscode.window.createOutputChannel('OpenMV Serial Monitor');
        this.context.subscriptions.push(this.outputChannel);

        // Initialize Terminal
        this.initPseudoterminal();

        this.updateStatusBar(false);
    }

    public start(): void {
        const config = vscode.workspace.getConfiguration('openmvBridge');
        const autoConnect = config.get<boolean>('autoConnect', true);

        if (autoConnect) {
            this.connect();
        }
    }

    public connect(): void {
        if (this.ws && (this.ws.readyState === WebSocket.OPEN || this.ws.readyState === WebSocket.CONNECTING)) {
            return;
        }

        const config = vscode.workspace.getConfiguration('openmvBridge');
        const serverUrl = config.get<string>('serverUrl', 'ws://127.0.0.1:23888');

        this.log(`[OpenMV Bridge] Connecting to ${serverUrl}...`);

        try {
            this.ws = new WebSocket(serverUrl);

            this.ws.on('open', () => {
                this.log(`[OpenMV Bridge] Connected to OpenMV IDE successfully!`);
                this.updateStatusBar(true);
                vscode.window.showInformationMessage('Connected to OpenMV IDE Bridge');
                
                if (this.reconnectTimer) {
                    clearTimeout(this.reconnectTimer);
                    this.reconnectTimer = null;
                }
            });

            this.ws.on('message', (data: WebSocket.RawData) => {
                try {
                    const text = data.toString('utf8');
                    const message = JSON.parse(text) as BridgeMessage;
                    this.handleMessage(message);
                } catch (err) {
                    this.log(`[OpenMV Bridge] Error parsing message: ${err}`);
                }
            });

            this.ws.on('close', () => {
                this.updateStatusBar(false);
                this.scheduleReconnect();
            });

            this.ws.on('error', (error) => {
                this.log(`[OpenMV Bridge] Connection status: ${error.message}`);
                this.updateStatusBar(false);
            });

        } catch (err: any) {
            this.log(`[OpenMV Bridge] WebSocket creation error: ${err?.message}`);
            this.scheduleReconnect();
        }
    }

    public disconnect(): void {
        if (this.reconnectTimer) {
            clearTimeout(this.reconnectTimer);
            this.reconnectTimer = null;
        }

        if (this.ws) {
            this.ws.removeAllListeners();
            this.ws.close();
            this.ws = null;
        }

        this.updateStatusBar(false);
        this.log(`[OpenMV Bridge] Disconnected.`);
    }

    private scheduleReconnect(): void {
        if (this.isDisposed) return;
        if (this.reconnectTimer) return;

        this.reconnectTimer = setTimeout(() => {
            this.reconnectTimer = null;
            const config = vscode.workspace.getConfiguration('openmvBridge');
            if (config.get<boolean>('autoConnect', true)) {
                this.connect();
            }
        }, 3000);
    }

    private handleMessage(message: BridgeMessage): void {
        const config = vscode.workspace.getConfiguration('openmvBridge');

        switch (message.type) {
            case 'diagnostics':
                if (config.get<boolean>('enableDiagnostics', true)) {
                    this.processDiagnostics(message);
                }
                break;

            case 'serial_data':
                if (config.get<boolean>('enableSerialMonitor', true)) {
                    this.processSerialData(message);
                }
                break;

            case 'device_status':
                this.processDeviceStatus(message);
                break;

            case 'file_saved_in_ide':
            case 'file_edited_in_ide':
                this.processFileSavedInIde(message);
                break;

            case 'notification':
                if (message.level === 'error') {
                    vscode.window.showErrorMessage(`[OpenMV] ${message.message}`);
                } else if (message.level === 'warn') {
                    vscode.window.showWarningMessage(`[OpenMV] ${message.message}`);
                } else {
                    vscode.window.showInformationMessage(`[OpenMV] ${message.message}`);
                }
                break;
        }
    }

    private processDiagnostics(msg: DiagnosticsMessage): void {
        try {
            let targetUri = vscode.Uri.file(msg.file);
            const targetNorm = targetUri.fsPath.replace(/\\/g, '/').toLowerCase();

            // Match exact workspace document URI to ensure Problems panel attaches properly
            for (const doc of vscode.workspace.textDocuments) {
                if (doc.uri.fsPath.replace(/\\/g, '/').toLowerCase() === targetNorm) {
                    targetUri = doc.uri;
                    break;
                }
            }

            const vsDiagnostics: vscode.Diagnostic[] = [];

            if (msg.items && Array.isArray(msg.items)) {
                for (const item of msg.items) {
                    const line = Math.max(0, (item.line || 1) - 1);
                    const col = Math.max(0, (item.column || 1) - 1);
                    const endLine = item.endLine ? Math.max(0, item.endLine - 1) : line;
                    const endCol = item.endColumn ? Math.max(0, item.endColumn - 1) : col + 50;

                    const range = new vscode.Range(line, col, endLine, endCol);

                    let severity = vscode.DiagnosticSeverity.Error;
                    if (item.severity === 'warning') severity = vscode.DiagnosticSeverity.Warning;
                    else if (item.severity === 'info') severity = vscode.DiagnosticSeverity.Information;
                    else if (item.severity === 'hint') severity = vscode.DiagnosticSeverity.Hint;

                    const diagnostic = new vscode.Diagnostic(range, item.message, severity);
                    diagnostic.source = item.source || 'OpenMV Linter';
                    vsDiagnostics.push(diagnostic);
                }
            }

            // Setting empty array automatically clears diagnostics for this file
            this.diagnosticCollection.set(targetUri, vsDiagnostics);
        } catch (err) {
            this.log(`[OpenMV Bridge] Error updating diagnostics: ${err}`);
        }
    }

    private processSerialData(msg: SerialDataMessage): void {
        if (!msg.payload) return;

        // 1. Stream to Output Channel
        this.outputChannel.append(msg.payload);

        // 2. Stream to Interactive Pseudoterminal
        if (this.terminalEmitter) {
            const formatted = msg.payload.replace(/\r?\n/g, '\r\n');
            this.terminalEmitter.fire(formatted);
        }
    }

    private processDeviceStatus(msg: DeviceStatusMessage): void {
        this.isDeviceConnected = msg.connected;
        this.currentPort = msg.port || '';
        this.updateStatusBar(this.ws?.readyState === WebSocket.OPEN);

        if (msg.connected) {
            this.log(`\n[OpenMV Bridge] Board connected on ${msg.port || 'USB'} (${msg.board || 'OpenMV Cam'})`);
            this.showSerial();
        } else {
            this.log(`\n[OpenMV Bridge] Board disconnected`);
        }
    }

    private async processFileSavedInIde(msg: FileSavedInIdeMessage): Promise<void> {
        if (!msg.file) return;
        // If user is actively typing in VS Code, do NOT overwrite or interrupt!
        if (this.isUserTyping) {
            return;
        }

        const normalizedTarget = msg.file.replace(/\\/g, '/').toLowerCase();

        for (const doc of vscode.workspace.textDocuments) {
            const docNorm = doc.uri.fsPath.replace(/\\/g, '/').toLowerCase();
            if (docNorm === normalizedTarget || docNorm.endsWith(normalizedTarget) || normalizedTarget.endsWith(docNorm)) {
                try {
                    let newText = msg.content;
                    if (newText === undefined) {
                        const fileData = await vscode.workspace.fs.readFile(doc.uri);
                        newText = Buffer.from(fileData).toString('utf8');
                    }

                    if (doc.getText() !== newText && !this.isUserTyping) {
                        this.isAutoSyncing = true;
                        const edit = new vscode.WorkspaceEdit();
                        const fullRange = new vscode.Range(
                            doc.positionAt(0),
                            doc.positionAt(doc.getText().length)
                        );
                        edit.replace(doc.uri, fullRange, newText);
                        await vscode.workspace.applyEdit(edit);
                        if (msg.type === 'file_saved_in_ide') {
                            await doc.save();
                        }
                        this.log(`[OpenMV Bridge] Live synchronized changes for ${doc.fileName} from OpenMV IDE into VS Code.`);
                    }
                } catch (err) {
                    this.log(`[OpenMV Bridge] Auto-sync error: ${err}`);
                } finally {
                    setTimeout(() => {
                        this.isAutoSyncing = false;
                    }, 200);
                }
                return;
            }
        }
    }

    private log(message: string): void {
        this.outputChannel.appendLine(message);
        if (this.terminalEmitter) {
            this.terminalEmitter.fire(`${message}\r\n`);
        }
    }

    private updateStatusBar(isServerConnected: boolean): void {
        if (!isServerConnected) {
            this.statusBarItem.text = `$(debug-disconnect) OpenMV: Disconnected`;
            this.statusBarItem.tooltip = 'Click to connect to OpenMV IDE';
            this.statusBarItem.backgroundColor = new vscode.ThemeColor('statusBarItem.warningBackground');
        } else {
            if (this.isDeviceConnected) {
                this.statusBarItem.text = `$(circuit-board) OpenMV: ${this.currentPort || 'Connected'}`;
                this.statusBarItem.tooltip = `OpenMV IDE Connected | Cam: ${this.currentPort}\nClick for OpenMV menu`;
                this.statusBarItem.backgroundColor = undefined;
            } else {
                this.statusBarItem.text = `$(plug) OpenMV: IDE Ready`;
                this.statusBarItem.tooltip = `Connected to OpenMV IDE (No Cam Connected)\nClick for OpenMV menu`;
                this.statusBarItem.backgroundColor = undefined;
            }
        }
        this.statusBarItem.show();
    }

    public showSerial(): void {
        if (this.terminal) {
            this.terminal.show(true);
        }
        this.outputChannel.show(true);
    }

    public clearSerial(): void {
        this.outputChannel.clear();
        if (this.terminalEmitter) {
            this.terminalEmitter.fire('\x1b[2J\x1b[0;0H');
        }
    }

    public clearDiagnostics(): void {
        this.diagnosticCollection.clear();
    }

    public sendCommand(type: string, data: any = {}): void {
        if (this.ws && this.ws.readyState === WebSocket.OPEN) {
            this.ws.send(JSON.stringify({ type, ...data }));
        }
    }

    private initPseudoterminal(): void {
        if (this.terminal) return;

        this.terminalEmitter = new vscode.EventEmitter<string>();
        const pty: vscode.Pseudoterminal = {
            onDidWrite: this.terminalEmitter.event,
            open: () => {
                this.terminalEmitter?.fire('\x1b[36m=== OpenMV Cam Serial Console ===\x1b[0m\r\n');
            },
            close: () => {
                this.terminal = null;
            },
            handleInput: (data: string) => {
                // Send keystrokes directly to OpenMV Cam REPL
                this.sendCommand('serial_input', { data });
            }
        };

        this.terminal = vscode.window.createTerminal({
            name: 'OpenMV Terminal',
            pty
        });
    }

    public dispose(): void {
        this.isDisposed = true;
        this.disconnect();
        this.diagnosticCollection.dispose();
        this.outputChannel.dispose();
        this.statusBarItem.dispose();
        if (this.terminal) {
            this.terminal.dispose();
        }
    }
}

export function activate(context: vscode.ExtensionContext) {
    const manager = new OpenMVBridgeManager(context);
    manager.start();

    // Register Commands
    context.subscriptions.push(
        vscode.commands.registerCommand('openmv-bridge.connect', () => manager.connect()),
        vscode.commands.registerCommand('openmv-bridge.disconnect', () => manager.disconnect()),
        vscode.commands.registerCommand('openmv-bridge.showSerial', () => manager.showSerial()),
        vscode.commands.registerCommand('openmv-bridge.clearSerial', () => manager.clearSerial()),
        vscode.commands.registerCommand('openmv-bridge.clearDiagnostics', () => manager.clearDiagnostics()),
        vscode.commands.registerCommand('openmv-bridge.showMenu', async () => {
            const items: vscode.QuickPickItem[] = [
                { label: '$(terminal) Show OpenMV Terminal', description: 'Open the OpenMV serial terminal' },
                { label: '$(sync) Reconnect to OpenMV IDE', description: 'Force reconnection to OpenMV IDE server' },
                { label: '$(clear-all) Clear Problems / Diagnostics', description: 'Clear all diagnostic markers' },
                { label: '$(trash) Clear Serial Output', description: 'Clear serial terminal console buffer' }
            ];

            const selected = await vscode.window.showQuickPick(items, {
                placeHolder: 'OpenMV Bridge Quick Actions'
            });

            if (selected) {
                if (selected.label.includes('Show OpenMV Terminal')) manager.showSerial();
                else if (selected.label.includes('Reconnect')) manager.connect();
                else if (selected.label.includes('Clear Problems')) manager.clearDiagnostics();
                else if (selected.label.includes('Clear Serial')) manager.clearSerial();
            }
        })
    );

    // Watch for active file typing changes in VS Code to trigger OpenMV live sync (debounced to 800ms IDLE)
    let liveEditTimer: NodeJS.Timeout | null = null;
    let typingReleaseTimer: NodeJS.Timeout | null = null;

    context.subscriptions.push(
        vscode.workspace.onDidChangeTextDocument((event) => {
            if (manager.isAutoSyncing) return;
            const doc = event.document;
            if (doc.languageId === 'python' || doc.fileName.endsWith('.py')) {
                // Lock out incoming sync while user is typing in VS Code
                manager.isUserTyping = true;

                if (typingReleaseTimer) clearTimeout(typingReleaseTimer);
                typingReleaseTimer = setTimeout(() => {
                    manager.isUserTyping = false;
                }, 1000);

                if (liveEditTimer) clearTimeout(liveEditTimer);
                liveEditTimer = setTimeout(() => {
                    manager.sendCommand('sync_file_content', {
                        path: doc.fileName,
                        content: doc.getText()
                    });
                }, 800);
            }
        })
    );

    // Watch for active file saves in VS Code
    context.subscriptions.push(
        vscode.workspace.onDidSaveTextDocument((doc) => {
            if (manager.isAutoSyncing) return;
            if (doc.languageId === 'python' || doc.fileName.endsWith('.py')) {
                manager.sendCommand('sync_file_content', {
                    path: doc.fileName,
                    content: doc.getText()
                });
                manager.sendCommand('reload_file', { path: doc.fileName });
            }
        })
    );

    context.subscriptions.push(manager);
}

export function deactivate() {}
