export interface DebugBridge {
    connect(): Promise<string>;
    getProgramCounter(): number;
    setProgramCounter(pc: number): void;
    step(): void;
    refresh(): void;
    disconnect(): void;
}