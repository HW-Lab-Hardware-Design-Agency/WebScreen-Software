// ws_elk_ble.h — fragment of the WebScreen Elk/LVGL bridge.
//
// NOT a standalone header: it is included exactly once, in order, by
// lvgl_elk.h (which is itself included only by webscreen_runtime.cpp).
// Symbols here may depend on every fragment included before it.
// Split from the former 3,700-line lvgl_elk.h monolith; see lvgl_elk.h
// for the include order.

// ~~~~~~~~~~~~~~~~~~~~~~~~~~~ 5) Basic BLE bridging ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Example usage from JS:
//   ble_init("ESP32-S3 Demo", "4fafc201-1fb5-459e-8fcc-c5c9c331914b", "beb5483e-36e1-4688-b7f5-ea07361b26a8");
//   ble_write("Hello from JS!");
//   if( ble_is_connected() ) { ... }

// Callbacks for NimBLE
class MyServerCallbacks : public NimBLEServerCallbacks {
public:
  void onConnect(NimBLEServer *pServer, NimBLEConnInfo &info) override {
    g_bleConnected = true;
    LOG("BLE device connected");
  }

  void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &info, int reason) override {
    g_bleConnected = false;
    LOG("BLE device disconnected");
    pServer->startAdvertising();
  }
};

class MyCharCallbacks : public NimBLECharacteristicCallbacks {
public:
  void onWrite(NimBLECharacteristic *pCharacteristic, NimBLEConnInfo &info) override {
    std::string rxData = pCharacteristic->getValue();
    LOGF("BLE Received: %s\n", rxData.c_str());
  }
};

// ble_init(devName, serviceUUID, charUUID)
static jsval_t js_ble_init(struct js *js, jsval_t *args, int nargs) {
  // Identity captured at the first successful init. NimBLE (~70-100KB
  // internal heap) can never be deinit'd, so the stack keeps this identity
  // until reboot; repeated init must compare against it (matters for the
  // in-place app switch, where a new script may request different UUIDs).
  static char s_bleName[64] = { 0 };
  static char s_bleSvcUUID[40] = { 0 };
  static char s_bleCharUUID[40] = { 0 };

  if (nargs < 3) return js_mkfalse();
  String name = js_arg_str(js, args[0]);
  String service = js_arg_str(js, args[1]);
  String characteristic = js_arg_str(js, args[2]);
  if (name.isEmpty() || name.length() >= sizeof(s_bleName) ||
      service.isEmpty() || service.length() >= sizeof(s_bleSvcUUID) ||
      characteristic.isEmpty() || characteristic.length() >= sizeof(s_bleCharUUID)) return js_mkfalse();
  const char *devName = name.c_str();
  const char *svcUUID = service.c_str();
  const char *charUUID = characteristic.c_str();

  // Idempotent: repeated init leaked fresh callback objects each call, and
  // NimBLE can never be deinit'd — reuse the first server/service/
  // characteristic, but only when the requested identity matches the live one.
  if (g_bleServer != nullptr && g_bleChar != nullptr) {
    if (strcmp(devName, s_bleName) == 0
        && strcmp(svcUUID, s_bleSvcUUID) == 0
        && strcmp(charUUID, s_bleCharUUID) == 0) {
      LOG("BLE already initialized");
      return js_mktrue();
    }
    LOG("BLE already initialized with different identity — reboot to change UUIDs");
    return js_mkfalse();
  }

  // Initialize NimBLE
  NimBLEDevice::init(devName);

  // Create server
  g_bleServer = NimBLEDevice::createServer();
  g_bleServer->setCallbacks(new MyServerCallbacks());

  // Create a BLE service
  NimBLEService *pService = g_bleServer->createService(svcUUID);

  // Create a BLE Characteristic
  g_bleChar = pService->createCharacteristic(
    charUUID,
    NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::NOTIFY);
  g_bleChar->setCallbacks(new MyCharCallbacks());

  // Start the service
  pService->start();

  // Start advertising
  g_bleServer->getAdvertising()->start();
  LOG("NimBLE advertising started");

  // Record the live identity for the idempotency check above.
  snprintf(s_bleName, sizeof(s_bleName), "%s", devName);
  snprintf(s_bleSvcUUID, sizeof(s_bleSvcUUID), "%s", svcUUID);
  snprintf(s_bleCharUUID, sizeof(s_bleCharUUID), "%s", charUUID);
  return js_mktrue();
}

// ble_is_connected() => bool
static jsval_t js_ble_is_connected(struct js *js, jsval_t *args, int nargs) {
  return g_bleConnected ? js_mktrue() : js_mkfalse();
}

// ble_write(str)
static jsval_t js_ble_write(struct js *js, jsval_t *args, int nargs) {
  if (!g_bleChar) return js_mkfalse();
  if (nargs < 1) return js_mkfalse();
  String data = js_arg_str(js, args[0]);
  g_bleChar->setValue((const uint8_t *)data.c_str(), data.length());
  g_bleChar->notify();
  return js_mktrue();
}
