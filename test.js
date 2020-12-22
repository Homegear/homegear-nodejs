'use strict';
var homegear = require('./build/Release/homegear');

var hg = new homegear.Homegear("/var/lib/homegear/homegearIPC.sock", function() {
	try {
		console.log(hg.invoke("logLevel", []));
	} catch (e) {
		console.log(e);
	}
}, function() {
	console.log("disconnected");
}, function(eventSource, peerId, channel, variableName, value) {
	console.log("event", eventSource, peerId, channel, variableName, value);
});

setTimeout(function(){ 
    console.log("This is the second statement"); 
}, 30000);
