// public/client.js

var ws;

function startWebSocket() {
  ws = new WebSocket('ws://localhost:8765/');
  ws.onopen = function () {
    console.log('WebSocket connection established');
  };
  ws.onmessage = function (event) {
    console.log('Message from ESP32:', event.data);
    document.getElementById('serverResponse').textContent = event.data;
  };
  ws.onerror = function (error) {
    console.error('WebSocket error:', error);
  };
}

async function fetchWifiNetworks() {
  try {
    const response = await fetch('/wifi-networks');
    const data = await response.json();
    const networks = data.networks;
    const wifiNetworksList = document.getElementById('wifiNetworksList');
    wifiNetworksList.innerHTML = '';

    networks.forEach(network => {
      const div = document.createElement('div');
      div.classList.add('wifi-network');
      div.textContent = network.ssid;
      div.onclick = () => {
        document.getElementById('wifiSSID').value = network.ssid;
      };
      wifiNetworksList.appendChild(div);
    });
  } catch (error) {
    console.error('Error fetching WiFi networks:', error);
  }
}

function fetchSerialPorts() {
  fetch('/ports')
    .then(response => response.json())
    .then(data => {
      const selectBox = document.getElementById('portSelect');
      data.ports.forEach(port => {
        const option = document.createElement('option');
        option.value = port.path;
        option.textContent = port.path;
        selectBox.appendChild(option);
      });
    })
    .catch(error => console.error('Error fetching serial ports:', error));
}

function connectSerialPort() {
  const selectedPort = document.getElementById('portSelect').value;
  fetch(`/connect?port=${selectedPort}`)
    .then(response => response.json())
    .then(data => {
      if (data.success) {
        console.log('Connected to serial port:', selectedPort);
        document.getElementById('connectionStatus').textContent = `Connected to ${selectedPort}`;
        startWebSocket();
      } else {
        console.error('Failed to connect to serial port');
      }
    })
    .catch(error => console.error('Error connecting to serial port:', error));
}

function sendMessage() {
  var message = document.getElementById('messageInput').value;
  ws.send(message);
  console.log('Sent:', message);
}


window.onload = function () {
  //fetchWiFiNetworks();
  fetchSerialPorts();
};
