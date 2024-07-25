// public/settings.js

function saveWiFiSettings() {
  const ssid = document.getElementById('wifiSSID').value;
  const password = document.getElementById('wifiPassword').value;
  // Add your logic to handle Wi-Fi settings here
  console.log('Wi-Fi Settings Saved:', { ssid, password });
  alert('Wi-Fi settings saved!');
}

function saveBluetoothSettings() {
  const deviceName = document.getElementById('bluetoothDevice').value;
  // Add your logic to handle Bluetooth settings here
  console.log('Bluetooth Settings Saved:', { deviceName });
  alert('Bluetooth settings saved!');
}

function fetchWiFiNetworks() {
  fetch('/wifi-networks')
    .then(response => response.json())
    .then(data => {
      const selectBox = document.getElementById('wifiSSID');
      selectBox.innerHTML = ''; // Clear existing options
      data.networks.forEach(network => {
        const option = document.createElement('option');
        option.value = network.ssid;
        option.textContent = network.ssid;
        selectBox.appendChild(option);
      });
    })
    .catch(error => console.error('Error fetching Wi-Fi networks:', error));
}

window.onload = function () {
  fetchWiFiNetworks();
  fetchSerialPorts();
};
