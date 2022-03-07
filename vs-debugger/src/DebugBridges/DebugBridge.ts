export interface DebugBridge {
    connect(): void;
    getProgramCounter(): number;
    step(): void;
    disconnect(): void;
}