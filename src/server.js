const WebSocket = require('ws');
const { SerialPort } = require('serialport');
const express = require('express');
const path = require('path');
const wifi = require('node-wifi');
const app = express();
const wss = new WebSocket.Server({ port: 8765 });

let port;
let selectedPort = '/dev/ttyACM1';

// Initialize wifi module
wifi.init({
    iface: null // network interface, choose a random wifi interface if set to null
});

// Serve static files from the "public" directory
app.use(express.static(path.join(__dirname, '../public')));

app.get('/ports', async (req, res) => {
    try {
        console.log('Listing serial ports...');
        const ports = await SerialPort.list();
        console.log('Available ports:', ports);
        res.json({ ports });
    } catch (error) {
        console.error('Error listing ports:', error);
        res.status(500).json({ error: 'Failed to list ports' });
    }
});

app.get('/connect', (req, res) => {
    const { port: portPath } = req.query;
    if (port) {
        port.close();
    }
    port = new SerialPort({ path: portPath, baudRate: 115200 }, (err) => {
        if (err) {
            res.json({ success: false });
        } else {
            selectedPort = portPath;
            res.json({ success: true });
        }
    });

    port.on('data', (data) => {
        wss.clients.forEach((client) => {
            if (client.readyState === WebSocket.OPEN) {
                client.send(data.toString());
            }
        });
    });

    port.on('error', (err) => {
        console.error('Serial port error:', err.message);
    });
});

// Endpoint to scan available Wi-Fi networks
app.get('/wifi-networks', (req, res) => {
    wifi.scan((err, networks) => {
        if (err) {
            console.error('Error scanning Wi-Fi networks:', err);
            res.status(500).json({ error: 'Failed to scan Wi-Fi networks' });
        } else {
            res.json({ networks });
        }
    });
});

// Serve the index.html file on the root URL
app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, '../public', 'index.html'));
});

wss.on('connection', function connection(ws) {
    console.log('A client connected via WebSocket.');

    ws.on('message', function incoming(message) {
        console.log('received:', message);
        if (port && port.isOpen) {
            port.write(message + '\n', function (err) {
                if (err) {
                    return console.log('Error on write:', err.message);
                }
                console.log('message written');
            });
        } else {
            console.log('Serial port not open');
        }
    });

    ws.on('close', function () {
        console.log('WebSocket connection closed');
    });
});

const server = app.listen(3000, () => {
    console.log('Express server listening on port 3000');
});

console.log('WebSocket server listening on port 8765');

module.exports = server;
