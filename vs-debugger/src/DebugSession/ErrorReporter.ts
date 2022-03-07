import * as vscode from 'vscode';
import { DiagnosticSeverity } from 'vscode';


export class ErrorReporter {
	diagnosticCollection: vscode.DiagnosticCollection;
	diagnostics: vscode.Diagnostic[] = [];

	constructor(context: vscode.ExtensionContext) {
		this.diagnosticCollection = vscode.languages.createDiagnosticCollection('webassembly');
		context.subscriptions.push(this.diagnosticCollection);
	}

	report(range: vscode.Range, file: string, message: string) {
		this.diagnostics.push(new vscode.Diagnostic(range, message, DiagnosticSeverity.Error));
		this.diagnosticCollection.set(vscode.Uri.parse(file), this.diagnostics);
	}

	clear() {
		this.diagnostics = [];
		this.diagnosticCollection.clear();
	}
}
