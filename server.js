const WebSocket = require('ws');
const { SerialPort } = require('serialport');

const wss = new WebSocket.Server({ port: 8765 });

const port = new SerialPort({
    path: '/dev/ttyACM0',
    baudRate: 115200
});

wss.on('connection', function connection(ws) {
    console.log('A client connected via WebSocket.');

    port.on('data', function(data) {
        console.log('Data from serial port:', data.toString());
        ws.send(data.toString());
    });

    ws.on('message', function incoming(message) {
        console.log('received:', message);
        port.write(message + '\n', function(err) {
            if (err) {
                return console.log('Error on write:', err.message);
            }
            console.log('message written');
        });
    });

    ws.on('close', function() {
        console.log('WebSocket connection closed');
    });
});

console.log('Server listening on WebSocket port 8765');
