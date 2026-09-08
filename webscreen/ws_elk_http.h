// ws_elk_http.h — fragment of the WebScreen Elk/LVGL bridge.
//
// NOT a standalone header: it is included exactly once, in order, by
// lvgl_elk.h (which is itself included only by webscreen_runtime.cpp).
// Symbols here may depend on every fragment included before it.
// Split from the former 3,700-line lvgl_elk.h monolith; see lvgl_elk.h
// for the include order.

//~~~~~~~~~~~~~~~~~~~~~~~~~~~ 1) HTTP ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Hard cap on the HTTP response body kept in RAM — an unbounded body is a
// direct heap-exhaustion/reset path on a ~320KB internal heap.
#ifndef WEBSCREEN_HTTP_MAX_RESPONSE
#define WEBSCREEN_HTTP_MAX_RESPONSE (64 * 1024)
#endif

// Helper function to read the HTTP response body

String readHttpResponseBody(WiFiClient &client) {
  String headers;
  String body;
  bool chunked = false;
  int contentLength = -1;
  String statusLine;

  // Wait for data to be available (server might take time to respond)
  unsigned long waitStart = millis();
  while (!client.available() && client.connected() && (millis() - waitStart) < 5000) {
    vTaskDelay(pdMS_TO_TICKS(50));
  }

  if (!client.available()) {
    LOG("No response received from server");
    return "";
  }

  // Read headers - check both connected and available for robustness
  unsigned long timeout = millis() + 15000;  // 15 second timeout
  bool firstLine = true;
  while ((client.connected() || client.available()) && millis() < timeout) {
    if (!client.available()) {
      // Wait a bit for more data
      vTaskDelay(pdMS_TO_TICKS(50));
      // Check again and break if still no data after a short wait
      if (!client.available() && !client.connected()) {
        break;
      }
      continue;
    }
    String line = client.readStringUntil('\n');
    line.trim();  // Remove \r and whitespace
    if (line.length() == 0) {  // Empty line = end of headers
      break;
    }
    if (firstLine) {
      statusLine = line;
      LOG("HTTP Response: " + statusLine);
      firstLine = false;
    }
    headers += line + "\n";
    // Check for chunked encoding (case-insensitive)
    String lineLower = line;
    lineLower.toLowerCase();
    if (lineLower.indexOf("transfer-encoding: chunked") >= 0) {
      chunked = true;
    }
    // Parse Content-Length (case-insensitive)
    if (lineLower.startsWith("content-length:")) {
      int colonPos = line.indexOf(':');
      if (colonPos > 0) {
        String lenStr = line.substring(colonPos + 1);
        lenStr.trim();
        contentLength = lenStr.toInt();
      }
    }
  }

  LOGF("Headers received. Chunked: %s, Content-Length: %d\n", chunked ? "yes" : "no", contentLength);

  if (chunked) {
    timeout = millis() + 15000;
    bool truncated = false;
    while (millis() < timeout && !truncated) {
      // Read chunk size
      String sizeLine = client.readStringUntil('\n');
      sizeLine.trim();
      int chunkSize = strtol(sizeLine.c_str(), NULL, 16);
      if (chunkSize <= 0) {            // No more chunks
        client.readStringUntil('\n');  // Read trailing \r\n
        break;
      }

      // Read the chunk data
      char buf[512];
      int bytesRead = 0;
      while (bytesRead < chunkSize) {
        int toRead = chunkSize - bytesRead;
        if (toRead > (int)sizeof(buf)) toRead = sizeof(buf);
        int n = client.readBytes(buf, toRead);
        if (n <= 0) break;  // Timeout or error
        int room = WEBSCREEN_HTTP_MAX_RESPONSE - (int)body.length();
        int keep = n < room ? n : room;
        if (keep > 0) body.concat(buf, keep);  // Length-aware append, binary-safe
        if (keep < n) {
          truncated = true;
          break;
        }
        bytesRead += n;
      }

      if (truncated) break;
      client.readStringUntil('\n');  // Read trailing \r\n
    }
    if (truncated) {
      LOGF("HTTP body truncated at %d bytes (WEBSCREEN_HTTP_MAX_RESPONSE)\n", (int)body.length());
    }
  } else if (contentLength > 0) {
    // Use Content-Length to read exact number of bytes (capped)
    LOG("Reading body with Content-Length");
    int target = contentLength;
    if (target > WEBSCREEN_HTTP_MAX_RESPONSE) {
      LOGF("Content-Length %d exceeds cap, truncating to %d bytes\n", contentLength, WEBSCREEN_HTTP_MAX_RESPONSE);
      target = WEBSCREEN_HTTP_MAX_RESPONSE;
    }
    body.reserve(target);
    int bytesRead = 0;
    timeout = millis() + 15000;
    char buf[512];
    while (bytesRead < target && millis() < timeout) {
      int avail = client.available();
      if (avail > 0) {
        int toRead = target - bytesRead;
        if (toRead > (int)sizeof(buf)) toRead = sizeof(buf);
        if (toRead > avail) toRead = avail;  // Don't block inside readBytes
        int n = client.readBytes(buf, toRead);
        if (n <= 0) break;
        body.concat(buf, n);
        bytesRead += n;
      } else if (!client.connected()) {
        break;
      } else {
        vTaskDelay(pdMS_TO_TICKS(10));
      }
    }
    LOGF("Body read: %d bytes\n", bytesRead);
  } else {
    // No Content-Length, read until connection closes (capped)
    LOG("Reading body until connection closes");
    timeout = millis() + 15000;
    int bytesRead = 0;
    char buf[512];
    while ((client.connected() || client.available()) && millis() < timeout) {
      int avail = client.available();
      if (avail > 0) {
        int toRead = avail < (int)sizeof(buf) ? avail : (int)sizeof(buf);
        int room = WEBSCREEN_HTTP_MAX_RESPONSE - bytesRead;
        if (toRead > room) toRead = room;
        if (toRead <= 0) {
          LOGF("HTTP body truncated at %d bytes (WEBSCREEN_HTTP_MAX_RESPONSE)\n", bytesRead);
          break;
        }
        int n = client.readBytes(buf, toRead);
        if (n <= 0) break;
        body.concat(buf, n);
        bytesRead += n;
      } else {
        // Wait a bit for more data before checking again
        vTaskDelay(pdMS_TO_TICKS(100));
      }
    }
    LOGF("Body read: %d bytes (no Content-Length)\n", bytesRead);
  }

  return body;
}

// Bridging function to parse JSON and extract a value for a given key
static jsval_t js_parse_json_value(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) {
    LOG("js_parse_json_value: Not enough arguments");
    return js_mkstr(js, "", 0);
  }

  // Retrieve JSON string
  size_t json_len;
  char *jsonStr_cstr = js_getstr(js, args[0], &json_len);
  if (!jsonStr_cstr) {
    LOG("js_parse_json_value: Argument 1 is not a string");
    return js_mkstr(js, "", 0);
  }
  String jsonStr(jsonStr_cstr, json_len);

  // Retrieve key string
  size_t key_len;
  char *key_cstr = js_getstr(js, args[1], &key_len);
  if (!key_cstr) {
    LOG("js_parse_json_value: Argument 2 is not a string");
    return js_mkstr(js, "", 0);
  }
  String keyStr(key_cstr, key_len);

  // Strip surrounding quotes if present
  if (keyStr.startsWith("\"") && keyStr.endsWith("\"") && keyStr.length() >= 2) {
    keyStr = keyStr.substring(1, keyStr.length() - 1);
  }

  // Parse JSON using ArduinoJson
  StaticJsonDocument<1024> doc;  // Adjust size as needed
  DeserializationError error = deserializeJson(doc, jsonStr);
  if (error) {
    LOGF("parse_json_value: JSON parse failed: %s\n", error.c_str());
    return js_mkstr(js, "", 0);
  }

  // Check if JSON is an object
  if (!doc.is<JsonObject>()) {
    LOG("parse_json_value: JSON is not an object");
    return js_mkstr(js, "", 0);
  }

  JsonObject obj = doc.as<JsonObject>();

  // Extract the value
  JsonVariant value = obj[keyStr.c_str()];

  // Check if the key exists
  if (value.isNull()) {
    return js_mkstr(js, "", 0);
  }

  // Convert the value to string, regardless of its type
  String resultStr;
  if (value.is<const char *>()) {
    resultStr = String(value.as<const char *>());
  } else if (value.is<double>()) {
    resultStr = String(value.as<double>());
  } else if (value.is<bool>()) {
    resultStr = value.as<bool>() ? "true" : "false";
  } else {  // For other types, attempt to stringify
    resultStr = String(value.as<String>());
  }

  // Return the extracted value to JavaScript
  return js_mkstr(js, resultStr.c_str(), resultStr.length());
}

// Bridging function to perform string index search
static jsval_t js_str_index_of(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) {
    LOG("str_index_of: Not enough arguments");
    return js_mknum(-1);
  }

  // Retrieve haystack string
  size_t haystack_len;
  char *haystack_cstr = js_getstr(js, args[0], &haystack_len);
  if (!haystack_cstr) {
    LOG("str_index_of: Argument 1 is not a string");
    return js_mknum(-1);
  }
  String haystackStr(haystack_cstr, haystack_len);

  // Retrieve needle string
  size_t needle_len;
  char *needle_cstr = js_getstr(js, args[1], &needle_len);
  if (!needle_cstr) {
    LOG("str_index_of: Argument 2 is not a string");
    return js_mknum(-1);
  }
  String needleStr(needle_cstr, needle_len);

  // Strip surrounding quotes if present
  if (haystackStr.startsWith("\"") && haystackStr.endsWith("\"") && haystackStr.length() >= 2) {
    haystackStr = haystackStr.substring(1, haystackStr.length() - 1);
  }
  if (needleStr.startsWith("\"") && needleStr.endsWith("\"") && needleStr.length() >= 2) {
    needleStr = needleStr.substring(1, needleStr.length() - 1);
  }

  return js_mknum(haystackStr.indexOf(needleStr));
}

// Bridging function to perform string substring extraction
static jsval_t js_str_substring(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 3) {
    LOG("str_substring: Not enough arguments");
    return js_mkstr(js, "", 0);
  }

  // Retrieve string
  size_t str_len;
  char *str_cstr = js_getstr(js, args[0], &str_len);
  if (!str_cstr) {
    LOG("str_substring: Argument 1 is not a string");
    return js_mkstr(js, "", 0);
  }
  String strStr(str_cstr, str_len);

  // Check if arguments 2 and 3 are numbers
  if (js_type(args[1]) != JS_NUM || js_type(args[2]) != JS_NUM) {
    LOG("str_substring: Arguments 2 and 3 must be numbers");
    return js_mkstr(js, "", 0);
  }

  // Extract numerical values
  int start = (int)js_getnum(args[1]);
  int length = (int)js_getnum(args[2]);

  // Strip surrounding quotes if present
  if (strStr.startsWith("\"") && strStr.endsWith("\"") && strStr.length() >= 2) {
    strStr = strStr.substring(1, strStr.length() - 1);
  }

  // Handle negative length (extract until end)
  if (length < 0) {
    strStr = strStr.substring(start);
  } else {  // Ensure that start + length does not exceed string length
    int end = start + length;
    if (end > (int)strStr.length()) {
      end = strStr.length();
    }
    strStr = strStr.substring(start, end);
  }

  return js_mkstr(js, strStr.c_str(), strStr.length());
}
// TLS handshake transiently needs ~45KB of contiguous internal heap;
// attempting it any lower fails deep inside mbedTLS and can reset the device
// instead of failing the request.
static bool elk_http_tls_heap_ok(void) {
  size_t largest = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
  if (largest < 50 * 1024) {
    LOGF("HTTP: TLS aborted, largest free internal block %u < 50KB\n", (unsigned)largest);
    return false;
  }
  return true;
}

// Shared tail of one HTTP attempt: write the request, read the body, close.
// Takes the base WiFiClient so the TLS client passes through unchanged
// (readHttpResponseBody relies on the same inheritance).
static String elk_http_exchange(WiFiClient &client, const char *method, const String &host,
                                const String &path, const String *body, const char *contentType) {
  client.print(String(method) + " " + path + " HTTP/1.1\r\n");
  client.print(String("Host: ") + host + "\r\n");
  for (auto &hdr : g_http_headers) {
    client.print(hdr.first + ": " + hdr.second + "\r\n");
  }
  if (body != NULL) {
    client.print(String("Content-Type: ") + contentType + "\r\n");
    client.printf("Content-Length: %d\r\n", body->length());
  }
  client.print("Connection: close\r\n\r\n");
  if (body != NULL) {
    client.print(*body);
  }
  String response = readHttpResponseBody(client);
  client.stop();
  return response;
}

// Shared scaffold for the http_get / http_post / http_delete bindings: URL
// parse (scheme, host[:port] with https->443 / http->80 defaults, path),
// TLS-vs-plain client, heap gate, request, response. Returns the exact string
// the binding hands back to JS — including the literal "ERROR: low memory"
// from the per-attempt TLS heap gate.
//
// maxRetries > 0 selects the historical GET behavior wholesale: retry with
// 1s/2s/4s backoff, explicit 15s socket / 10s connect timeouts, and verbose
// per-step logging. Single-shot verbs (maxRetries == 0) keep library-default
// timeouts, the quiet "manual approach" logging, and bail out on connect
// failure without the trailing "Done ..." log. These quirks are preserved
// observable behavior, not style — do not unify them here.
static String elk_http_request(const char *method, const String &url, const String *body,
                               const char *contentType, int maxRetries,
                               const char *logName, const char *connectFailMsg) {
  // Determine if HTTPS or HTTP; no scheme prefix => assume HTTPS
  bool useSSL = true;
  const String HTTPS_PREFIX = "https://";
  const String HTTP_PREFIX = "http://";
  String rest = url;
  if (rest.startsWith(HTTPS_PREFIX)) {
    rest.remove(0, HTTPS_PREFIX.length());
    useSSL = true;
  } else if (rest.startsWith(HTTP_PREFIX)) {
    rest.remove(0, HTTP_PREFIX.length());
    useSSL = false;
  }

  // Find first slash => host + path
  int slashPos = rest.indexOf('/');
  String hostWithPort, path;
  if (slashPos < 0) {
    hostWithPort = rest;
    path = "/";
  } else {
    hostWithPort = rest.substring(0, slashPos);
    path = rest.substring(slashPos);
  }

  // Parse port from host (e.g., "192.168.1.20:2000" or "example.com:8080")
  String host;
  int port = useSSL ? 443 : 80;  // Default ports
  int colonPos = hostWithPort.indexOf(':');
  if (colonPos > 0) {
    host = hostWithPort.substring(0, colonPos);
    port = hostWithPort.substring(colonPos + 1).toInt();
    if (port <= 0 || port > 65535) {
      port = useSSL ? 443 : 80;  // Invalid port, use default
    }
  } else {
    host = hostWithPort;
  }

  const bool verbose = (maxRetries > 0);
  if (verbose) {
    LOG(String("\n") + logName + " => " + (useSSL ? "HTTPS" : "HTTP"));
  } else {
    LOG(String("\n") + logName + " => manual approach");
  }
  LOG("Host: " + host);
  LOGF("Port: %d\n", port);
  LOG("Path: " + path);
  if (body != NULL) {
    LOGF("Body length=%d\n", body->length());
  }

  String response;

  for (int attempt = 0; attempt <= maxRetries; attempt++) {
    if (attempt > 0) {
      LOGF("Retry attempt %d...\n", attempt);
      // Exponential backoff: 1s, 2s, 4s between retries
      int delayMs = 1000 * (1 << (attempt - 1));
      vTaskDelay(pdMS_TO_TICKS(delayMs));
    }

    if (useSSL) {
      if (!elk_http_tls_heap_ok()) {
        return String("ERROR: low memory");  // Gated per attempt; returned to JS verbatim
      }
      WiFiClientSecure client;
      if (verbose) client.setTimeout(15000);  // 15 second timeout for read/write operations
      if (g_httpCAcert) {
        client.setCACert(g_httpCAcert);
        if (verbose) LOG("Using CA cert for HTTPS");
      } else {
        client.setInsecure();
        if (verbose) LOG("Using insecure mode for HTTPS");
      }

      if (verbose) LOGF("Connecting to %s:%d (HTTPS)...\n", host.c_str(), port);
      // Verbose (GET) uses a 10 second connection timeout; single-shot verbs
      // keep the library default
      bool connected = verbose ? client.connect(host.c_str(), port, 10000)
                               : client.connect(host.c_str(), port);
      if (!connected) {
        LOG(connectFailMsg);
        if (maxRetries == 0) return String("");  // Single-shot: no "Done ..." log
        continue;  // Retry
      }
      if (verbose) LOG("Connected!");

      response = elk_http_exchange(client, method, host, path, body, contentType);
    } else {
      WiFiClient client;
      if (verbose) client.setTimeout(15000);  // 15 second timeout for read/write operations

      if (verbose) LOGF("Connecting to %s:%d (HTTP)...\n", host.c_str(), port);
      bool connected = verbose ? client.connect(host.c_str(), port, 10000)
                               : client.connect(host.c_str(), port);
      if (!connected) {
        LOG(connectFailMsg);
        if (maxRetries == 0) return String("");  // Single-shot: no "Done ..." log
        continue;  // Retry
      }
      if (verbose) LOG("Connected!");

      response = elk_http_exchange(client, method, host, path, body, contentType);
    }

    if (verbose) LOGF("Response length: %d bytes\n", response.length());

    // If we got a response, break out of retry loop
    if (response.length() > 0) {
      break;
    }
  }

  if (verbose) {
    if (response.length() == 0) {
      LOG("HTTP GET failed after all retries!");  // Only the retrying verb (GET) gets here
    }
  } else {
    LOGF("Done %s. response size=%d\n", method, response.length());
  }
  return response;
}

static jsval_t js_http_get(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 1) return js_mkstr(js, "", 0);
  String url = js_arg_str(js, args[0]);
  // 3 retries => up to 4 attempts total
  String response = elk_http_request("GET", url, NULL, NULL, 3,
                                     "js_http_get", "Connection failed!");
  return js_mkstr(js, response.c_str(), response.length());
}

static jsval_t js_http_post(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mkstr(js, "", 0);
  String url = js_arg_str(js, args[0]);
  String jsonBody = js_arg_str(js, args[1]);
  String response = elk_http_request("POST", url, &jsonBody, "application/json", 0,
                                     "js_http_post", "Connection failed (POST)!");
  return js_mkstr(js, response.c_str(), response.length());
}

static jsval_t js_http_delete(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 1) return js_mkstr(js, "", 0);
  String url = js_arg_str(js, args[0]);
  String response = elk_http_request("DELETE", url, NULL, NULL, 0,
                                     "js_http_delete", "Connection failed (DELETE)!");
  return js_mkstr(js, response.c_str(), response.length());
}

static jsval_t js_http_set_header(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 2) return js_mkfalse();

  String k = js_arg_str(js, args[0]);
  String v = js_arg_str(js, args[1]);

  // Append to our global vector
  g_http_headers.push_back(std::make_pair(k, v));
  LOGF("Added header: %s: %s\n", k.c_str(), v.c_str());
  return js_mktrue();
}

static jsval_t js_http_clear_headers(struct js *js, jsval_t *args, int nargs) {
  g_http_headers.clear();
  return js_mktrue();
}

static jsval_t js_http_set_ca_cert_from_sd(struct js *js, jsval_t *args, int nargs) {
  if (nargs < 1) return js_mkfalse();
  String path = js_arg_str(js, args[0]);

  // Open file from SD
  File f = SD_MMC.open(path, FILE_READ);
  if (!f) {
    LOGF("Failed to open CA cert file: %s\n", path.c_str());
    return js_mkfalse();
  }

  size_t size = f.size();
  if (size == 0) {
    LOGF("CA file is empty: %s\n", path.c_str());
    f.close();
    return js_mkfalse();
  }

  // Reallocate or free old buffer
  if (g_httpCAcert) {
    free(g_httpCAcert);
    g_httpCAcert = nullptr;
  }

  // Allocate enough bytes (include space for trailing '\0')
  g_httpCAcert = (char *)malloc(size + 1);
  if (!g_httpCAcert) {
    LOG("Not enough RAM to store CA cert!");
    f.close();
    return js_mkfalse();
  }

  // Read the file
  size_t bytesRead = f.readBytes(g_httpCAcert, size);
  f.close();
  g_httpCAcert[bytesRead] = '\0';  // Null-terminate

  LOGF("Loaded CA cert (%u bytes) from SD file: %s\n", (unsigned)bytesRead, path.c_str());
  return js_mktrue();
}
