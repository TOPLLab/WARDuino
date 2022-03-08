import * as assert from 'assert';
import * as vscode from 'vscode';
import {WASMCompilerBridge} from  '../../CompilerBridges/WASMCompilerBridge';

suite('Extension Test Suite', () => {

	vscode.window.showInformationMessage('Start all tests.');

	test('TestCompileBridge', () => {
		let compilerBridge = new WASMCompilerBridge("","");
		compilerBridge.compile();
	});




});
