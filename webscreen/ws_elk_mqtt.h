// ws_elk_mqtt.h — fragment of the WebScreen Elk/LVGL bridge: MQTT support.
//
// NOT a standalone header: it is included exactly once, in order, by
// lvgl_elk.h (which is itself included only by webscreen_runtime.cpp).
// Symbols here may depend on every fragment included before it.
//
// Contents:
//  - onMqttMessage(): PubSubClient receive callback feeding a bounded
//    message queue (never calls js_eval — Elk is not re-entrant)
//  - processPendingMqttMessage(): preserves the legacy polling entry point
//  - elk_mqtt_try_reconnect(): one reconnect + re-subscribe attempt with the
//    working credentials (backoff lives in the runtime maintain loop)
//  - mqtt_* JS bindings: init, connect, publish, subscribe, loop, on_message,
//    plus the has_message/get_payload/msg_clear/dropped polling API

#include "webscreen_mqtt_queue.h"
static WebscreenMqttQueue g_mqttMessages;
static WebscreenMqttSubscriptions g_mqttSubscriptions;
static uint32_t g_mqttLastRestoreMs = 0;

// Credentials of the last successful connect + subscriptions, used by
// elk_mqtt_try_reconnect() after a broker drop. Empty user => anonymous.
static char g_mqttLastClientId[64] = "";
static char g_mqttLastUser[64] = "";
static char g_mqttLastPass[64] = "";
static bool g_mqttHaveCreds = false;

void onMqttMessage(char *topic, byte *payload, unsigned int length) {
  if (g_mqttCallbackName[0]) g_mqttMessages.push(topic, payload, length);
}

// Delivery remains polling-based; never re-enter Elk from PubSubClient.
void processPendingMqttMessage() {}

static jsval_t js_mqtt_has_message(struct js *js, jsval_t *args, int nargs) {
  return g_mqttMessages.front() ? js_mktrue() : js_mkfalse();
}

static jsval_t js_mqtt_get_payload(struct js *js, jsval_t *args, int nargs) {
  const auto *message = g_mqttMessages.front();
  return message ? js_mkstr(js, message->payload, message->length) : js_mkstr(js, "", 0);
}

static jsval_t js_mqtt_get_topic(struct js *js, jsval_t *args, int nargs) {
  const auto *message = g_mqttMessages.front();
  return message ? js_mkstr(js, message->topic, strlen(message->topic)) : js_mkstr(js, "", 0);
}

static jsval_t js_mqtt_msg_clear(struct js *js, jsval_t *args, int nargs) {
  g_mqttMessages.pop();
  return js_mknull();
}

static jsval_t js_mqtt_dropped(struct js *js, jsval_t *args, int nargs) {
  return js_mknum(g_mqttMessages.dropped());
}

static void elk_mqtt_restore_subscriptions() {
  if (!g_mqttSubscriptions.pending() || !g_mqttClient.connected()) return;
  uint32_t now = millis();
  if (now - g_mqttLastRestoreMs < 5000) return;
  g_mqttLastRestoreMs = now;
  g_mqttSubscriptions.restore([](const char *topic) { return g_mqttClient.subscribe(topic); });
}

// JavaScript-exposed bridging functions
// mqtt_init(broker, port)
static jsval_t js_mqtt_init(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mkfalse();

  size_t broker_len = 0;
  char *broker = js_getstr(js, args[0], &broker_len);
  double requested_port = js_getnum(args[1]);
  if (js_type(args[1]) != JS_NUM || !isfinite(requested_port) || requested_port < 1 || requested_port > 65535 ||
      !broker || !broker_len || broker_len >= sizeof(g_mqttBrokerCopy) || memchr(broker, 0, broker_len)) return js_mkfalse();
  int port = (int)requested_port;
  // Reinitializing a broker must not reuse the previous app's session state.
  if (g_mqttClient.connected()) g_mqttClient.disconnect();
  g_mqttHaveCreds = false;
  g_mqttMessages.clear();
  g_mqttSubscriptions.clear();

  // Copy broker string to persistent buffer — PubSubClient::setServer()
  // stores only the pointer, not a copy. The Elk JS heap pointer becomes
  // dangling after the function returns.
  size_t copy_len = broker_len < sizeof(g_mqttBrokerCopy) - 1 ? broker_len : sizeof(g_mqttBrokerCopy) - 1;
  memcpy(g_mqttBrokerCopy, broker, copy_len);
  g_mqttBrokerCopy[copy_len] = '\0';

  g_mqttClient.setServer(g_mqttBrokerCopy, port);
  g_mqttClient.setCallback(onMqttMessage);
  if (!g_mqttClient.setBufferSize(WEBSCREEN_MQTT_MAX_PACKET_SIZE)) return js_mkfalse();
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
  if (!clientID || clientID_len >= sizeof(g_mqttLastClientId) || memchr(clientID, 0, clientID_len) ||
      (nargs >= 2 && (!user || user_len >= sizeof(g_mqttLastUser) || memchr(user, 0, user_len))) ||
      (nargs >= 3 && (!pass || pass_len >= sizeof(g_mqttLastPass) || memchr(pass, 0, pass_len)))) {
    return js_mkfalse();
  }

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
  if (user && user_len > 0) {
    // Copy user/pass to local buffers too
    size_t ulen = user_len < sizeof(user_buf) - 1 ? user_len : sizeof(user_buf) - 1;
    memcpy(user_buf, user, ulen); user_buf[ulen] = '\0';
    size_t plen = pass_len < sizeof(pass_buf) - 1 ? pass_len : sizeof(pass_buf) - 1;
    if (plen) memcpy(pass_buf, pass, plen);
    pass_buf[plen] = '\0';
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
    g_mqttSubscriptions.request_restore();
    g_mqttLastRestoreMs = millis() - 5000;
    elk_mqtt_restore_subscriptions();
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

  if (topic_len >= 128 || memchr(topic, 0, topic_len) || msg_len > WEBSCREEN_MQTT_MAX_PACKET_SIZE) return js_mkfalse();
  char topic_buf[128];
  memcpy(topic_buf, topic, topic_len);
  topic_buf[topic_len] = '\0';
  // PubSubClient reads these bytes synchronously and never invokes Elk.
  bool ok = g_mqttClient.publish(topic_buf, (const uint8_t *)message, (unsigned int)msg_len, false);
  return ok ? js_mktrue() : js_mkfalse();
}

// mqtt_subscribe(topic)
static jsval_t js_mqtt_subscribe(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 1) return js_mkfalse();
  size_t topic_len = 0;
  char *topic = js_getstr(js, args[0], &topic_len);
  if (!topic || topic_len == 0) return js_mkfalse();

  if (topic_len >= 128 || memchr(topic, 0, topic_len)) return js_mkfalse();
  char topic_buf[128];
  memcpy(topic_buf, topic, topic_len); topic_buf[topic_len] = '\0';
  if (!g_mqttSubscriptions.can_add(topic_buf)) return js_mkfalse();
  bool ok = g_mqttClient.subscribe(topic_buf);
  if (ok) g_mqttSubscriptions.remember(topic_buf);
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
  g_mqttSubscriptions.request_restore();
  g_mqttLastRestoreMs = millis() - 5000;
  elk_mqtt_restore_subscriptions();
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
