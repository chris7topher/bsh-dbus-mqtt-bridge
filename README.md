# Siemens Washing Machine MQTT Logger

This project is based on [hn/bsh-home-appliances](https://github.com/hn/bsh-home-appliances).

It allows you to read status and remaining time messages from a Siemens washing machine via the D-Bus and transmit them via MQTT. The MQTT messages can be processed by any home automation system or custom application that supports MQTT.

**Tested on**: Siemens WM14N0A1 - All examples and message codes in this documentation are specific to this model. Other washing machine models may behave differently and use different message structures.

## Hardware

- **ESP12F**: The microcontroller reads the D-Bus and communicates via MQTT.
- **Step-Down Converter (MP1584EN)**: Since the D-Bus operates at 9V and the ESP12F only supports 3.3V, an MP1584EN step-down converter is used to safely reduce the voltage to 3.3V.
- **Direct D-Bus Connection**: The connection is made directly, without additional adapters.

## Features

The ESP12F firmware provides the following functionality:

### Core Features
- **D-Bus Communication**: Reads messages from the washing machine's D-Bus interface with CRC16 integrity checking
- **MQTT Publishing**: Publishes valid D-Bus messages as hex strings to configurable MQTT topics
- **WiFi Connectivity**: Uses AutoConnect for easy WiFi setup and management

### Web Interface
- **Configuration Portal**: Web-based MQTT settings configuration (server, port, credentials)
- **Real-time Status**: Displays current MQTT connection status
- **Parameter Management**: Save/load configuration from SPIFFS filesystem

### Over-the-Air Updates
- **OTA Support**: Firmware updates over WiFi without physical access
- **Progress Monitoring**: Real-time update progress reporting via serial output

### MQTT Topics
- `washingmachine/dbus`: Raw D-Bus messages as hex strings

## Message Processing

The firmware outputs raw hex messages that can be processed by any MQTT-capable system. The messages contain information about:
- Door status (open/closed)
- Remaining wash time (note: the washing machine only broadcasts the remaining time occasionally, not continuously)
- Program completion
- Various other status information

**Important Note**: Since the washing machine only sends remaining time updates sporadically during the wash cycle, any processing system needs to implement its own countdown timer to track the remaining time between updates.

**Message Compatibility**: The message codes and formats shown in this project are specific to the Siemens WM14N0A1 washing machine model. Different washing machine models or brands may use different message structures and codes. To adapt this for your machine, monitor the incoming MQTT messages and compare them with the actual display on your washing machine to identify the correct patterns.

## Example: ioBroker Integration

As an example of how to process the MQTT messages, here's an ioBroker JavaScript that processes the received D-Bus messages and calculates the remaining time of the washing machine.
Various statuses are detected (door open/closed, remaining time, washing machine finished) and corresponding actions are triggered (e.g., Telegram notification).

```javascript
// Configuration
const CONFIG = {
    objectFolder: 'javascript.0.washingMachine.',
    mqttTopic: 'mqtt.0.washingmachine.dbus',
    telegramInstance: 'telegram.0',
    messages: {
        doorClosed: '10002906',
        doorOpen: '10002903',
        finished: '10010500',
        timePrefix: '1002'
    }
};

// Global variables
let remainingTimeInterval = null;

// Initialize states
createState(`${CONFIG.objectFolder}RemainingTime`, 0, false);
createState(`${CONFIG.objectFolder}RemainingTimeReadable`, '', false);
createState(`${CONFIG.objectFolder}DoorStatus`, 'unknown', false);
createState(`${CONFIG.objectFolder}MachineStatus`, 'unknown', false);

// Helper functions
function clearRemainingTimeTimer() {
    if (remainingTimeInterval) {
        clearInterval(remainingTimeInterval);
        remainingTimeInterval = null;
        log('Remaining time timer stopped.');
    }
}

function formatTime(minutes) {
    const hours = Math.floor(minutes / 60);
    const mins = minutes % 60;
    return `${hours}:${String(mins).padStart(2, '0')}`;
}

function hexToDuration(hexStr) {
    const base = 0x10020000;
    const value = parseInt(hexStr, 16);
    return Math.round((value - base) / 60);
}

function startCountdownTimer(totalMinutes) {
    clearRemainingTimeTimer();
    setState(`${CONFIG.objectFolder}RemainingTime`, totalMinutes);

    if (totalMinutes <= 0) return;

    remainingTimeInterval = setInterval(() => {
        let currentMinutes = getState(`${CONFIG.objectFolder}RemainingTime`).val;
        currentMinutes--;

        setState(`${CONFIG.objectFolder}RemainingTime`, currentMinutes);

        if (currentMinutes <= 0) {
            clearRemainingTimeTimer();
            log('Countdown finished - Machine should be done!');
        }
    }, 60 * 1000);
}

function sendFinishedNotification() {
    sendTo(CONFIG.telegramInstance, {
        text: 'Washing machine is finished!',
        caption: 'Washing Machine Finished',
        disable_notification: false
    });
}

// Event handlers
on({ id: `${CONFIG.objectFolder}RemainingTime`, change: "ne" }, function (obj) {
    const remaining = +obj.state.val;
    const readable = formatTime(remaining);
    log(`Remaining time: ${readable}`);
    setState(`${CONFIG.objectFolder}RemainingTimeReadable`, readable);
});

on({ id: CONFIG.mqttTopic, change: "ne" }, function (obj) {
    const message = `${obj.state.val}`;

    if (!message || message.length <= 6) return;

    const content = message.slice(4, -4); // Extract content (remove length, address, checksum)
    const address = message.slice(2, 4);

    // Process different message types
    switch (content) {
        case CONFIG.messages.doorClosed:
            log('Door closed');
            setState(`${CONFIG.objectFolder}DoorStatus`, 'closed');
            break;

        case CONFIG.messages.doorOpen:
            log('Door open');
            setState(`${CONFIG.objectFolder}DoorStatus`, 'open');
            break;

        default:
            if (content.startsWith(CONFIG.messages.timePrefix)) {
                // Remaining time message
                const totalMinutes = hexToDuration(content);
                log(`New remaining time: ${formatTime(totalMinutes)}`);
                setState(`${CONFIG.objectFolder}MachineStatus`, 'running');
                startCountdownTimer(totalMinutes);

            } else if (content.startsWith(CONFIG.messages.finished)) {
                // Machine finished
                log('Washing machine finished!');
                setState(`${CONFIG.objectFolder}MachineStatus`, 'finished');
                clearRemainingTimeTimer();
                setState(`${CONFIG.objectFolder}RemainingTime`, 0);
                sendFinishedNotification();

            } else {
                // Unknown message - log for debugging
                log(`Unknown message from ${address}: ${content}`);
            }
            break;
    }
});
```

## Alternative Processing

The MQTT messages can be processed by any system that supports MQTT subscriptions:

- **Home Assistant**: Using MQTT sensors and automations
- **Node-RED**: Visual flow-based processing
- **OpenHAB**: Rules and item definitions
- **Custom Applications**: Any programming language with MQTT client library
- **Cloud Services**: AWS IoT, Azure IoT Hub, Google Cloud IoT

### Example Messages (Specific to Siemens WM14N0A1)

- `10022328` → 2h 30min remaining
- `10022D78` → 3h 14min remaining
- `10002906` → Door closed
- `10002903` → Door open
- `10010500` → Washing machine finished

**Note**: These message codes are specific to the Siemens WM14N0A1 washing machine model. Other models may behave differently. To figure out the remaining time for other machines, you'll need to:
1. Monitor the raw MQTT messages during operation
2. Compare the received messages with the time displayed on your machine
3. Calculate the hex representation of the displayed time in minutes
4. Identify the pattern and adjust the message codes accordingly

## Technical Notes

- CRC16 checking is performed directly on the ESP12F to ensure message integrity
- MQTT messages are transmitted as hex strings for maximum compatibility
- The device creates a WiFi access point for initial configuration if no known network is available
- Configuration is stored in SPIFFS and persists across reboots
- OTA updates allow remote firmware updates without physical access

## License

This project is licensed under the MIT License. See the LICENSE file for details.