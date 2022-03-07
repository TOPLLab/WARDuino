import { WARDuinoDebugSession }	 from './DebugSession/DebugSession';
    
// start a single session that communicates via stdin/stdout
/* const session = new WARDuinoDebugSession({});
    process.on('SIGTERM', () => {
	session.shutdown();
});*/

//session.start(process.stdin, process.stdout);