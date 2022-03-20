import * as vscode from 'vscode';
import { WARDuinoDebugSession } from './DebugSession';
import { ProviderResult } from 'vscode';
import { ErrorReporter } from './ErrorReporter';

export class InlineDebugAdapterFactory implements vscode.DebugAdapterDescriptorFactory {
	notifier: vscode.StatusBarItem;
	errorReporter: ErrorReporter;
	warduino : WARDuinoDebugSession | undefined;

	constructor(notifier: vscode.StatusBarItem, reporter: ErrorReporter) {
		this.notifier = notifier;
		this.errorReporter = reporter;
	}

	createDebugAdapterDescriptor(_session: vscode.DebugSession): ProviderResult<vscode.DebugAdapterDescriptor> {
		this.warduino = new WARDuinoDebugSession(this.notifier, this.errorReporter);
		return new vscode.DebugAdapterInlineImplementation(this.warduino);
	}
}
