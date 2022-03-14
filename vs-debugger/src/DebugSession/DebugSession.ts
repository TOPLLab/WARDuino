import {DebugProtocol} from 'vscode-debugprotocol';
import {basename} from 'path-browserify';
import * as vscode from 'vscode';

import {
    InitializedEvent,
    LoggingDebugSession,
    Source,
    StackFrame,
    StoppedEvent,
    TerminatedEvent,
    Thread
} from 'vscode-debugadapter';
import {CompileTimeError} from "../CompilerBridges/CompileTimeError";
import {ErrorReporter} from "./ErrorReporter";
import {DebugBridge} from '../DebugBridges/DebugBridge';
import {DebugBridgeFactory} from '../DebugBridges/DebugBridgeFactory';
import {RunTimeTarget} from "../DebugBridges/RunTimeTarget";
import {CompileBridgeFactory} from "../CompilerBridges/CompileBridgeFactory";
import {CompileBridge} from "../CompilerBridges/CompileBridge";
import {SourceMap} from "../CompilerBridges/SourceMap";

// Interface between the debugger and the VS runtime 
export class WARDuinoDebugSession extends LoggingDebugSession {
    private sourceMap?: SourceMap = undefined;
    private program: string = "";
    private THREAD_ID: number = 42;
    private testCurrentLine = 0;
    //private compiler? : WASMCompilerBridge;
    private debugBridge?: DebugBridge;
    private notifier: vscode.StatusBarItem;
    private reporter: ErrorReporter;

    public constructor(notifier: vscode.StatusBarItem, reporter: ErrorReporter) {
        super("debug_log.txt");
        this.notifier = notifier;
        this.reporter = reporter;
    }

    protected initializeRequest(response: DebugProtocol.InitializeResponse, args: DebugProtocol.InitializeRequestArguments): void {
        // build and return the capabilities of this debug adapter:
        response.body = response.body || {};

        // the adapter implements the configurationDone request.
        response.body.supportsConfigurationDoneRequest = true;

        // make VS Code use 'evaluate' when hovering over source
        response.body.supportsEvaluateForHovers = false;

        // make VS Code show a 'step back' button
        response.body.supportsStepBack = false;

        // make VS Code support data breakpoints
        response.body.supportsDataBreakpoints = false;

        // make VS Code support completion in REPL
        response.body.supportsCompletionsRequest = false;
        response.body.completionTriggerCharacters = [".", "["];

        // make VS Code send cancel request
        response.body.supportsCancelRequest = false;

        // make VS Code send the breakpointLocations request
        response.body.supportsBreakpointLocationsRequest = true;

        // make VS Code provide "Step in Target" functionality
        response.body.supportsStepInTargetsRequest = true;

        // the adapter defines two exceptions filters, one with support for conditions.
        response.body.supportsExceptionFilterOptions = false;


        // make VS Code send exceptionInfo request
        response.body.supportsExceptionInfoRequest = false;

        // make VS Code send setVariable request
        response.body.supportsSetVariable = false;

        // make VS Code send setExpression request
        response.body.supportsSetExpression = false;

        // make VS Code send disassemble request
        response.body.supportsDisassembleRequest = false;
        response.body.supportsSteppingGranularity = false;
        response.body.supportsInstructionBreakpoints = false;


        this.sendResponse(response);
        this.sendEvent(new InitializedEvent());
    }

    protected configurationDoneRequest(response: DebugProtocol.ConfigurationDoneResponse, args: DebugProtocol.ConfigurationDoneArguments): void {
        super.configurationDoneRequest(response, args);
    }

    protected async launchRequest(response: DebugProtocol.LaunchResponse, args: any) {
        console.log(args.program);
        console.log(process.cwd());
        this.reporter.clear();
        this.program = args.program;

        let compiler: CompileBridge = CompileBridgeFactory.makeCompileBridge(args.program);

        let sourceMap = await compiler.compile().catch((reason) => this.handleCompileError(reason));
        if (sourceMap) {
            this.sourceMap = sourceMap;
        }
        let that = this;
        this.debugBridge = DebugBridgeFactory.makeDebugBridge(args.program,
            RunTimeTarget.embedded,
            {   // VS Code Interface
                notifyError(): void {

                },
                connected(): void {

                },
                disconnected(): void {

                },
                notifyProgress(message: string): void {
                    that.notifier.text = message;
                }
            }
        );

        this.sendResponse(response);
        this.sendEvent(new StoppedEvent('entry', this.THREAD_ID));
    }

    private handleCompileError(handleCompileError: CompileTimeError) {
        let range = new vscode.Range(handleCompileError.lineInfo.line - 1,
            handleCompileError.lineInfo.column,
            handleCompileError.lineInfo.line - 1,
            handleCompileError.lineInfo.column);
        this.reporter.report(range, this.program, handleCompileError.message);
        this.sendEvent(new TerminatedEvent());
    }

    protected breakpointLocationsRequest(response: DebugProtocol.BreakpointLocationsResponse, args: DebugProtocol.BreakpointLocationsArguments, request?: DebugProtocol.Request): void {
        console.log("breakpointLocationsRequest");

    }

    protected setBreakPointsRequest(response: DebugProtocol.SetBreakpointsResponse, args: DebugProtocol.SetBreakpointsArguments, request?: DebugProtocol.Request): void {
        console.log("setBreakPointsRequest");

    }

    protected setInstructionBreakpointsRequest(response: DebugProtocol.SetInstructionBreakpointsResponse, args: DebugProtocol.SetInstructionBreakpointsArguments) {
        console.log("setInstructionBreakpointsRequest");
        response.body = {
            breakpoints: []
        };
        this.sendResponse(response);
    }

    protected threadsRequest(response: DebugProtocol.ThreadsResponse): void {
        response.body = {
            threads: [new Thread(this.THREAD_ID, "WARDuino Debug Thread")]
        };
        this.sendResponse(response);
    }

    private setLineNumberFromPC(pc: number) {
        this.sourceMap?.lineInfoPairs.forEach((info) => {
            const address = parseInt("0x" + info.lineAddress);
            if (Math.abs(pc - address) === 0) {
                this.testCurrentLine = info.lineInfo.line - 1;
            }
        });
    }

    protected stackTraceRequest(response: DebugProtocol.StackTraceResponse, args: DebugProtocol.StackTraceArguments): void {
        const pc = this.debugBridge!.getProgramCounter();
        this.setLineNumberFromPC(pc);

        const sf: DebugProtocol.StackFrame = new StackFrame(0,
            'Top Frame',
            this.createSource(this.program),
            this.convertDebuggerLineToClient(this.testCurrentLine));

        response.body = {
            stackFrames: [sf],
            totalFrames: 1
        };

        this.sendResponse(response);
    }

    private createSource(filePath: string): Source {
        return new Source(basename(filePath), this.convertDebuggerPathToClient(filePath), undefined, undefined, 'mock-adapter-data');
    }

    protected nextRequest(response: DebugProtocol.NextResponse, args: DebugProtocol.NextArguments): void {
        console.log("nextRequest");
        this.sendResponse(response);
        this.debugBridge?.step();
        this.notifyStopped();
    }

    override shutdown(): void {
        console.log("Shutting the debugger down");
        this.debugBridge?.disconnect();
    }

    public notifyStopped() {
        this.sendEvent(new StoppedEvent('step', this.THREAD_ID));
    }

}