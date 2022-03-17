export interface DebugBridgeListener {
    connected(): void;
    disconnected(): void;
    notifyError(message: string): void;
    notifyProgress(message: string): void;
    notifyStateUpdate(): void;
    notifyPaused():void;
}