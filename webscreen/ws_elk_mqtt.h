// ws_elk_mqtt.h — fragment of the WebScreen Elk/LVGL bridge.
//
// NOT a standalone header: it is included exactly once, in order, by
// lvgl_elk.h (which is itself included only by webscreen_runtime.cpp).
// Symbols here may depend on every fragment included before it.
// Split from the former 3,700-line lvgl_elk.h monolith; see lvgl_elk.h
// for the include order.

// MQTT message callback from PubSubClient

// Messages lost to single-slot overwrite or payload truncation — JS: mqtt_dropped()
static uint32_t g_mqttDroppedCount = 0;

// Credentials of the last successful connect + last subscription, used by
// elk_mqtt_try_reconnect() after a broker drop. Empty user => anonymous.
static char g_mqttLastClientId[64] = "";
static char g_mqttLastUser[64] = "";
static char g_mqttLastPass[64] = "";
static bool g_mqttHaveCreds = false;
static char g_mqttLastSubTopic[128] = "";

void onMqttMessage(char *topic, byte *payload, unsigned int length) {
  LOGF("[MQTT] Message arrived on topic '%s'\n", topic);

  // Queue the message for processing from the C++ main loop.
  // We must NOT call js_eval here because this callback fires inside
  // g_mqttClient.loop(), which may be called from JS via mqtt_loop().
  // Elk is not re-entrant — calling js_eval inside another js_eval
  // corrupts the parser state.
  if (g_mqttCallbackName[0] != '\0') {
    if (g_mqttMsgPending || g_mqttMsgReady) {
      g_mqttDroppedCount++;  // Single slot still occupied — previous message is lost
    }
    strncpy(g_mqttMsgTopic, topic, sizeof(g_mqttMsgTopic) - 1);
    g_mqttMsgTopic[sizeof(g_mqttMsgTopic) - 1] = '\0';

    unsigned int copyLen = length < sizeof(g_mqttMsgPayload) - 1 ? length : sizeof(g_mqttMsgPayload) - 1;
    if (copyLen < length) {
      g_mqttDroppedCount++;  // Payload truncated to fit the slot
    }
    memcpy(g_mqttMsgPayload, payload, copyLen);
    g_mqttMsgPayload[copyLen] = '\0';

    g_mqttMsgPending = true;
  }
}

// Relay raw payload to JS — no C++ JSON parsing, minimal stack usage.
void processPendingMqttMessage() {
  if (!g_mqttMsgPending) return;
  g_mqttMsgPending = false;
  g_mqttMsgReady = true;
  LOGF("[MQTT] Message ready for JS (%d bytes)\n", strlen(g_mqttMsgPayload));
}

// JS-callable functions to poll for MQTT messages from timer callback
static jsval_t js_mqtt_has_message(struct js *js, jsval_t *args, int nargs) {
  return g_mqttMsgReady ? js_mktrue() : js_mkfalse();
}

static jsval_t js_mqtt_get_payload(struct js *js, jsval_t *args, int nargs) {
  return js_mkstr(js, g_mqttMsgPayload, strlen(g_mqttMsgPayload));
}

static jsval_t js_mqtt_msg_clear(struct js *js, jsval_t *args, int nargs) {
  g_mqttMsgReady = false;
  return js_mknull();
}

// mqtt_dropped() => count of messages lost to slot overwrite or truncation
static jsval_t js_mqtt_dropped(struct js *js, jsval_t *args, int nargs) {
  return js_mknum((double)g_mqttDroppedCount);
}
// JavaScript-exposed bridging functions
// mqtt_init(broker, port)
static jsval_t js_mqtt_init(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mkfalse();

  size_t broker_len = 0;
  char *broker = js_getstr(js, args[0], &broker_len);
  int port = (int)js_getnum(args[1]);

  if (!broker || broker_len == 0 || port <= 0) return js_mkfalse();

  // Copy broker string to persistent buffer — PubSubClient::setServer()
  // stores only the pointer, not a copy. The Elk JS heap pointer becomes
  // dangling after the function returns.
  size_t copy_len = broker_len < sizeof(g_mqttBrokerCopy) - 1 ? broker_len : sizeof(g_mqttBrokerCopy) - 1;
  memcpy(g_mqttBrokerCopy, broker, copy_len);
  g_mqttBrokerCopy[copy_len] = '\0';

  g_mqttClient.setServer(g_mqttBrokerCopy, port);
  g_mqttClient.setCallback(onMqttMessage);
  g_mqttClient.setBufferSize(WEBSCREEN_MQTT_MAX_PACKET_SIZE);
  g_mqttClient.setSocketTimeout(5);  // 5 second MQTT handshake timeout (in seconds)
  g_wifiClient.setTimeout(3000);  // 3 second TCP connect timeout (in milliseconds)

  LOGF("[MQTT] init => broker=%s port=%d\n", g_mqttBrokerCopy, port);

  return js_mktrue();
}

// mqtt_connect(clientID, user, pass)
static jsval_t js_mqtt_connect(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 1) return js_mkfalse();

  // Verify WiFi is connected before attempting MQTT
  if (WiFi.status() != WL_CONNECTED) {
    LOG("[MQTT] Connect skipped - WiFi not connected");
    return js_mkfalse();
  }

  size_t clientID_len = 0;
  char *clientID = js_getstr(js, args[0], &clientID_len);

  size_t user_len = 0, pass_len = 0;
  char *user = (nargs >= 2) ? js_getstr(js, args[1], &user_len) : nullptr;
  char *pass = (nargs >= 3) ? js_getstr(js, args[2], &pass_len) : nullptr;

  // Copy clientID to stack buffer since Elk heap pointers may not persist
  char clientID_buf[64];
  if (clientID && clientID_len > 0) {
    size_t clen = clientID_len < sizeof(clientID_buf) - 1 ? clientID_len : sizeof(clientID_buf) - 1;
    memcpy(clientID_buf, clientID, clen);
    clientID_buf[clen] = '\0';
  } else {
    strcpy(clientID_buf, "webscreen");
  }

  LOGF("[MQTT] Connecting as '%s' to %s...\n", clientID_buf, g_mqttBrokerCopy);

  bool ok = false;
  char user_buf[64] = "";
  char pass_buf[64] = "";
  if (user && pass && user_len > 0 && pass_len > 0) {
    // Copy user/pass to local buffers too
    size_t ulen = user_len < sizeof(user_buf) - 1 ? user_len : sizeof(user_buf) - 1;
    memcpy(user_buf, user, ulen); user_buf[ulen] = '\0';
    size_t plen = pass_len < sizeof(pass_buf) - 1 ? pass_len : sizeof(pass_buf) - 1;
    memcpy(pass_buf, pass, plen); pass_buf[plen] = '\0';
    ok = g_mqttClient.connect(clientID_buf, user_buf, pass_buf);
  } else {
    ok = g_mqttClient.connect(clientID_buf);
  }

  if (ok) {
    // Remember working credentials so the maintain loop can reconnect after
    // a broker drop (elk_mqtt_try_reconnect)
    strncpy(g_mqttLastClientId, clientID_buf, sizeof(g_mqttLastClientId) - 1);
    g_mqttLastClientId[sizeof(g_mqttLastClientId) - 1] = '\0';
    strncpy(g_mqttLastUser, user_buf, sizeof(g_mqttLastUser) - 1);
    g_mqttLastUser[sizeof(g_mqttLastUser) - 1] = '\0';
    strncpy(g_mqttLastPass, pass_buf, sizeof(g_mqttLastPass) - 1);
    g_mqttLastPass[sizeof(g_mqttLastPass) - 1] = '\0';
    g_mqttHaveCreds = true;
    LOG("[MQTT] Connected successfully");
    return js_mktrue();
  } else {
    LOGF("[MQTT] Connect failed, rc=%d\n", g_mqttClient.state());
    return js_mkfalse();
  }
}

// mqtt_publish(topic, message)
static jsval_t js_mqtt_publish(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mkfalse();
  size_t topic_len = 0, msg_len = 0;
  char *topic = js_getstr(js, args[0], &topic_len);
  char *message = js_getstr(js, args[1], &msg_len);
  if (!topic || !message || topic_len == 0) return js_mkfalse();

  // Copy to stack buffers since PubSubClient needs null-terminated C strings
  char topic_buf[128], msg_buf[512];
  size_t tlen = topic_len < sizeof(topic_buf) - 1 ? topic_len : sizeof(topic_buf) - 1;
  memcpy(topic_buf, topic, tlen); topic_buf[tlen] = '\0';
  size_t mlen = msg_len < sizeof(msg_buf) - 1 ? msg_len : sizeof(msg_buf) - 1;
  memcpy(msg_buf, message, mlen); msg_buf[mlen] = '\0';

  bool ok = g_mqttClient.publish(topic_buf, msg_buf);
  return ok ? js_mktrue() : js_mkfalse();
}

// mqtt_subscribe(topic)
static jsval_t js_mqtt_subscribe(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 1) return js_mkfalse();
  size_t topic_len = 0;
  char *topic = js_getstr(js, args[0], &topic_len);
  if (!topic || topic_len == 0) return js_mkfalse();

  char topic_buf[128];
  size_t tlen = topic_len < sizeof(topic_buf) - 1 ? topic_len : sizeof(topic_buf) - 1;
  memcpy(topic_buf, topic, tlen); topic_buf[tlen] = '\0';

  bool ok = g_mqttClient.subscribe(topic_buf);
  if (ok) {
    // Remember for re-subscribe after auto-reconnect
    strncpy(g_mqttLastSubTopic, topic_buf, sizeof(g_mqttLastSubTopic) - 1);
    g_mqttLastSubTopic[sizeof(g_mqttLastSubTopic) - 1] = '\0';
  }
  LOGF("[MQTT] Subscribed to '%s'? => %s\n", topic_buf, ok ? "OK" : "FAIL");
  return ok ? js_mktrue() : js_mkfalse();
}

// One reconnect attempt with the last successful credentials, then re-subscribe.
// Backoff is the caller's job (webscreen_runtime_wifi_mqtt_maintain_loop).
static bool elk_mqtt_try_reconnect(void) {
  if (!g_mqttHaveCreds) return false;
  if (WiFi.status() != WL_CONNECTED) return false;
  if (g_mqttClient.connected()) return true;

  bool ok;
  if (g_mqttLastUser[0] != '\0') {
    ok = g_mqttClient.connect(g_mqttLastClientId, g_mqttLastUser, g_mqttLastPass);
  } else {
    ok = g_mqttClient.connect(g_mqttLastClientId);
  }
  if (!ok) {
    LOGF("[MQTT] Reconnect failed, rc=%d\n", g_mqttClient.state());
    return false;
  }
  LOG("[MQTT] Reconnected to broker");
  if (g_mqttLastSubTopic[0] != '\0') {
    bool sub = g_mqttClient.subscribe(g_mqttLastSubTopic);
    LOGF("[MQTT] Re-subscribed to '%s'? => %s\n", g_mqttLastSubTopic, sub ? "OK" : "FAIL");
  }
  return true;
}

// mqtt_loop() — no-op from JS side. MQTT processing is handled by the
// C++ main loop to avoid re-entrant js_eval calls.
static jsval_t js_mqtt_loop(struct js *js, jsval_t *args, int nargs) {
  return js_mknull();
}

// mqtt_on_message("myCallback")
static jsval_t js_mqtt_on_message(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 1) return js_mkfalse();

  // Check if user passed a string naming the function
  size_t len = 0;
  char *str = js_getstr(js, args[0], &len);
  if (!str || len == 0 || len >= sizeof(g_mqttCallbackName)) {
    return js_mkfalse();
  }

  // Copy that name to our global
  memset(g_mqttCallbackName, 0, sizeof(g_mqttCallbackName));
  memcpy(g_mqttCallbackName, str, len);  // not zero-terminated by default
  g_mqttCallbackName[len] = '\0';

  Serial.print("[MQTT] JS callback name set to: ");
  LOG(g_mqttCallbackName);
  return js_mktrue();
}

// This function tries to connect to your MQTT broker

bool doMqttConnect() {  // extern const char* g_mqttBroker;
  // extern int         g_mqttPort;
  // g_mqttClient.setServer(g_mqttBroker, g_mqttPort);

  LOG("[MQTT] Checking broker connection...");

  // Attempt to connect with e.g. a default clientID (or user/pass if needed)
  bool ok = g_mqttClient.connect("WebScreenClient");
  if (!ok) {  // Print the reason code: g_mqttClient.state()
    LOGF("[MQTT] Connect fail, rc=%d\n", g_mqttClient.state());
    return false;
  }

  // If connected, subscribe to any default topic if you want:
  // g_mqttClient.subscribe("some/topic");

  LOG("[MQTT] Connected successfully");
  return true;
}

// This function tries to reconnect Wi-Fi if Wi-Fi is down

bool doWiFiReconnect() {
  LOG("[WiFi] Checking connection...");

  // If you have an SSID/pass stored
  // extern String g_ssid;
  // extern String g_pass;
  // WiFi.begin(g_ssid.c_str(), g_pass.c_str());

  // We'll do a quick wait for up to ~3 seconds, just for example:
  // (Tune to your needs)
  for (int i = 0; i < 15; i++) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.print("[WiFi] Reconnected. IP=");
      LOG(WiFi.localIP());
      return true;
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
  LOG("[WiFi] Still not connected");
  return false;
}

// Call this regularly to maintain Wi-Fi + MQTT

void wifiMqttMaintainLoop() {
  if (WiFi.status() != WL_CONNECTED) {
    unsigned long now = millis();
    // Try reconnect every 10 seconds
    if (now - lastWiFiReconnectAttempt > 10000) {
      lastWiFiReconnectAttempt = now;
      LOG("[WiFi] Connection lost, attempting recon...");
      doWiFiReconnect();
    }
    // If Wi-Fi is still down, we skip MQTT
    return;
  }

  if (!g_mqttClient.connected()) {
    unsigned long now = millis();
    if (now - lastMqttReconnectAttempt > 10000) {
      lastMqttReconnectAttempt = now;
      LOG("[MQTT] Lost MQTT, trying reconnect...");
      if (doMqttConnect()) {
        lastMqttReconnectAttempt = 0;
      }
    }
  }

  // If connected, let PubSubClient process inbound/outbound messages
  g_mqttClient.loop();
}

