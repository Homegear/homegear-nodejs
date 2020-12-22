'use strict'
var homegear = require('@homegear/homegear-nodejs');

function connected() {
    console.log("connected")
    try {
        hg.invoke('writeLog', ['My log entry', 4])
    } catch (e) {
        console.log(e)
    }
}

function disconnected() {
    console.log("disconnected")
}

function event(eventSource, peerId, channel, variableName, value) {
    console.log("event", eventSource, peerId, channel, variableName, value)
}

var hg = new homegear.Homegear('', connected, disconnected, event)

setTimeout(function(){ 
    console.log("Done"); 
}, 30000);
