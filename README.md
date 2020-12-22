# homegear-nodejs

`homegear-nodejs` is a Node.js binding to connect Node.js to a local Homegear over IPC Sockets (= Unix Domain Sockets). It supports

* invoking RPC methods in Homegear and
* it receives live variable updates from Homegear.

## Installation

You can install `homegear-nodejs` using `npm`:

```bash
npm i @homegear/homegear-nodejs
```

Alternatively clone this repository and execute:

```bash
node-gyp rebuild
```

## Usage

First we need load the module:

```javascript
var hg = require('@homegear/homegear-nodejs');
```

Now we can create a new `Homegear` object. The constructor accepts up to 4 arguments and has the following signature:

```javascript
Homegear(string socketPath, function connected, function disconnected, function event)
```

| Argument       | Type       | Description                                                  |
| -------------- | ---------- | ------------------------------------------------------------ |
| `socketPath`   | `string`   | The path to the Homegear socket file. The default path is `/var/run/homegear/homegearIPC.sock`. When an empty `string` is passed, this default path is used. |
| `connected`    | `function` | Callback method that is executed when a connection to Homegear was successfully established. `connected()` has no arguments. |
| `disconnected` | `function` | Callback method that is executed when the connection to Homegear is closed. The module automatically tries to reconnect and calls `connected()` again once the connection is reestablished. `disconnected()` has no arguments. |
| `event`        | `function` | Callback method that is executed for every Homegear variable update.  Five arguments are passed to `event()`. Please see the next table for a description. |

### Arguments to `event()`

| Argument       | Type      | Description                                                 |
| -------------- | --------- | ----------------------------------------------------------- |
| `eventSource`  | `string`  | A `string` describing the entity that updated the variable. |
| `peerId`       | `number`  | The peer ID of the updated variable.                        |
| `channel`      | `number`  | The peer channel of the updated variable.                   |
| `variableName` | `string`  | The name of the updated variable.                           |
| `value`        | `variant` | The new value of the variable.                              |

For more information about `event()` please see the Homegear reference: https://ref.homegear.eu/rpc.html#eventEvent

### Example

```javascript
'use strict'
var homegear = require('@homegear/homegear-nodejs');

function connected() {
    console.log("connected")
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
```



### Invoking Homegear RPC methods

In addition to these callback methods, the Homegear object has one method: `invoke()`. `invoke()` has the following signature:

```javascript
variant Homegear.invoke(string methodName, array parameters)
```

| Argument     | Type     | Description                                              |
| ------------ | -------- | -------------------------------------------------------- |
| `methodName` | `string` | The name of the RPC method to call.                      |
| `parameters` | `array`  | An array with the parameters required by the RPC method. |

On error an exception is thrown.

Please visit https://ref.homegear.eu/rpc.html for more information about the supported RPC methods.

#### Example

```javascript
'use strict'
var homegear = require('@homegear/homegear-nodejs');

function connected() {
    try {
        hg.invoke('writeLog', ['My log entry', 4])
    } catch (e) {
        console.log(e)
    }
}

var hg = new homegear.Homegear('', connected)
```