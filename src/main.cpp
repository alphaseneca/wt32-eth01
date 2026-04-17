// ============================================================
// WT32-ETH01 — Complete Ethernet + MQTT Example
// ============================================================
// Board:  Wireless-Tag WT32-ETH01
// MCU:    ESP32 (Dual-core Xtensa LX6, 240 MHz)
// PHY:    LAN8720A (10/100 Mbps, on-board, via RMII)
//
// Internal Pin Mapping (FIXED on the PCB — do NOT change):
//   GPIO 23  → MDC  (Management Data Clock)
//   GPIO 18  → MDIO (Management Data I/O)
//   GPIO  0  → REF_CLK (50 MHz RMII clock input)
//   GPIO 16  → Oscillator enable (active HIGH)
//   PHY Address = 1
//
// Available User GPIOs:
//   GPIO  2, 4, 5, 12, 14, 15, 17, 32, 33, 35*, 36*, 39*
//   (* = input-only)
// ============================================================

#include <Arduino.h>

// ─── Method 1: Use the built-in ESP32 ETH library (simplest) ───
#include <ETH.h>
#include <WiFi.h>

// ─── (Optional) MQTT ───
#include <PubSubClient.h>

// ─── (Optional) Web Server ───
#include <WebServer.h>

// esp32-arduino v3 renamed WiFiClient → NetworkClient
#if ESP_ARDUINO_VERSION_MAJOR >= 3
using EthClient = NetworkClient;
#else
using EthClient = WiFiClient;
#endif

// ============================================================
// Configuration — change these to match your network
// ============================================================

// Set to true for static IP, false for DHCP
#define USE_STATIC_IP false

#if USE_STATIC_IP
IPAddress staticIP(192, 168, 1, 100);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress dns1(8, 8, 8, 8);
IPAddress dns2(8, 8, 4, 4);
#endif

// MQTT Broker
const char *mqtt_server = "broker.hivemq.com"; // ← Domain or IP both work
const int mqtt_port = 1883;                    // ← Use 8883 for secure MQTT, 1883 for unencrypted
const char *mqtt_user = "";                    // leave empty if no auth
const char *mqtt_password = "";
const char *mqtt_clientID = "wt32-eth01";

// ============================================================
// Global objects
// ============================================================
EthClient ethClient; // NetworkClient on v3, WiFiClient on v2
PubSubClient mqtt(ethClient);
WebServer server(80); // HTTP on port 80

// ============================================================
// Ethernet event flags
// ============================================================
static bool eth_connected = false;
static bool eth_got_ip = false;

// ============================================================
// Ethernet event handler
// ============================================================
#if ESP_ARDUINO_VERSION_MAJOR >= 3
// esp32-arduino v3+ uses Arduino event system
void onEthEvent(arduino_event_id_t event)
{
  switch (event)
  {
  case ARDUINO_EVENT_ETH_START:
    Serial.println("[ETH] Started");
    ETH.setHostname("wt32-eth01");
    break;
  case ARDUINO_EVENT_ETH_CONNECTED:
    Serial.println("[ETH] Link UP");
    eth_connected = true;
    break;
  case ARDUINO_EVENT_ETH_GOT_IP:
    Serial.printf("[ETH] Got IP: %s\n", ETH.localIP().toString().c_str());
    Serial.printf("[ETH] MAC:    %s\n", ETH.macAddress().c_str());
    Serial.printf("[ETH] Speed:  %d Mbps, %s\n",
                  ETH.linkSpeed(),
                  ETH.fullDuplex() ? "Full Duplex" : "Half Duplex");
    eth_got_ip = true;
    break;
  case ARDUINO_EVENT_ETH_DISCONNECTED:
    Serial.println("[ETH] Link DOWN");
    eth_connected = false;
    eth_got_ip = false;
    break;
  case ARDUINO_EVENT_ETH_STOP:
    Serial.println("[ETH] Stopped");
    eth_connected = false;
    eth_got_ip = false;
    break;
  default:
    break;
  }
}
#else
// esp32-arduino v2.x uses WiFiEvent
void onEthEvent(WiFiEvent_t event)
{
  switch (event)
  {
  case ARDUINO_EVENT_ETH_START:
    Serial.println("[ETH] Started");
    ETH.setHostname("wt32-eth01");
    break;
  case ARDUINO_EVENT_ETH_CONNECTED:
    Serial.println("[ETH] Link UP");
    eth_connected = true;
    break;
  case ARDUINO_EVENT_ETH_GOT_IP:
    Serial.printf("[ETH] Got IP: %s\n", ETH.localIP().toString().c_str());
    Serial.printf("[ETH] MAC:    %s\n", ETH.macAddress().c_str());
    Serial.printf("[ETH] Speed:  %d Mbps, %s\n",
                  ETH.linkSpeed(),
                  ETH.fullDuplex() ? "Full Duplex" : "Half Duplex");
    eth_got_ip = true;
    break;
  case ARDUINO_EVENT_ETH_DISCONNECTED:
    Serial.println("[ETH] Link DOWN");
    eth_connected = false;
    eth_got_ip = false;
    break;
  case ARDUINO_EVENT_ETH_STOP:
    Serial.println("[ETH] Stopped");
    eth_connected = false;
    eth_got_ip = false;
    break;
  default:
    break;
  }
}
#endif

// ============================================================
// MQTT callback — called when a message arrives on a subscribed topic
// ============================================================
void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  Serial.printf("[MQTT] Message on [%s]: ", topic);
  for (unsigned int i = 0; i < length; i++)
  {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Example: toggle GPIO 2 on "cmd/led" topic
  if (String(topic) == "cmd/led")
  {
    String msg;
    for (unsigned int i = 0; i < length; i++)
      msg += (char)payload[i];
    if (msg == "ON")
      digitalWrite(2, HIGH);
    if (msg == "OFF")
      digitalWrite(2, LOW);
  }
}

// ============================================================
// Connect / reconnect to MQTT broker
// ============================================================
void connectMQTT()
{
  if (mqtt.connected())
    return;

  Serial.print("[MQTT] Connecting to ");
  Serial.print(mqtt_server);
  Serial.print("...");

  bool ok;
  if (strlen(mqtt_user) > 0)
  {
    ok = mqtt.connect(mqtt_clientID, mqtt_user, mqtt_password);
  }
  else
  {
    ok = mqtt.connect(mqtt_clientID);
  }

  if (ok)
  {
    Serial.println(" connected!");
    // Subscribe to topics
    mqtt.subscribe("cmd/#");
    // Publish a hello
    mqtt.publish("status/wt32", "WT32-ETH01 online");
  }
  else
  {
    Serial.printf(" failed (rc=%d). Retrying in 5s...\n", mqtt.state());
  }
}

// ============================================================
// Web server handlers
// ============================================================
void handleRoot()
{
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>WT32-ETH01 Dashboard</title>
  <style>
    * { margin:0; padding:0; box-sizing:border-box; }
    body {
      font-family: 'Segoe UI', system-ui, sans-serif;
      background: #0f172a; color: #e2e8f0;
      display:flex; justify-content:center; align-items:center;
      min-height:100vh;
    }
    .card {
      background: #1e293b; border-radius: 16px;
      padding: 2rem; max-width: 480px; width: 90%;
      box-shadow: 0 20px 60px rgba(0,0,0,0.5);
    }
    h1 { color:#38bdf8; margin-bottom:1rem; font-size:1.5rem; }
    .row { display:flex; justify-content:space-between;
           padding:0.5rem 0; border-bottom:1px solid #334155; }
    .label { color:#94a3b8; }
    .value { color:#f1f5f9; font-weight:600; }
    .btn {
      display:inline-block; margin-top:1.5rem; padding:0.75rem 1.5rem;
      background:#2563eb; color:white; border:none; border-radius:8px;
      font-size:1rem; cursor:pointer; text-decoration:none;
    }
    .btn:hover { background:#1d4ed8; }
  </style>
</head>
<body>
  <div class="card">
    <h1>⚡ WT32-ETH01 Dashboard</h1>
    <div class="row"><span class="label">IP Address</span>
      <span class="value">)rawliteral" +
                ETH.localIP().toString() + R"rawliteral(</span></div>
    <div class="row"><span class="label">MAC Address</span>
      <span class="value">)rawliteral" +
                ETH.macAddress() + R"rawliteral(</span></div>
    <div class="row"><span class="label">Link Speed</span>
      <span class="value">)rawliteral" +
                String(ETH.linkSpeed()) + " Mbps" + R"rawliteral(</span></div>
    <div class="row"><span class="label">Uptime</span>
      <span class="value">)rawliteral" +
                String(millis() / 1000) + " seconds" + R"rawliteral(</span></div>
    <div class="row"><span class="label">Free Heap</span>
      <span class="value">)rawliteral" +
                String(ESP.getFreeHeap()) + " bytes" + R"rawliteral(</span></div>
    <div class="row"><span class="label">MQTT</span>
      <span class="value">)rawliteral" +
                String(mqtt.connected() ? "Connected ✅" : "Disconnected ❌") + R"rawliteral(</span></div>
    <a class="btn" href="/">🔄 Refresh</a>
  </div>
</body>
</html>
)rawliteral";
  server.send(200, "text/html", html);
}

void handleNotFound()
{
  server.send(404, "text/plain", "404 — Not Found");
}

// ============================================================
// SETUP
// ============================================================
void setup()
{
  Serial.begin(115200);
  delay(100);
  Serial.println("\n\n========================================");
  Serial.println("  WT32-ETH01 — Ethernet + MQTT + Web");
  Serial.println("========================================\n");

  // LED on GPIO 2 (optional — some boards have it)
  pinMode(2, OUTPUT);
  digitalWrite(2, LOW);

// ─── Register Ethernet event handler ───
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  Network.onEvent(onEthEvent);
#else
  WiFi.onEvent(onEthEvent);
#endif

// ─── Start Ethernet ───
// The argument order changed between esp32-arduino v2 and v3.
//
//  ETH.begin(phy_type, phy_addr, mdc, mdio, power_pin, clk_mode)
//
//  For WT32-ETH01:
//    PHY Type  = LAN8720
//    PHY Addr  = 1
//    MDC       = GPIO 23
//    MDIO      = GPIO 18
//    Power/OSC = GPIO 16  (enables the 50 MHz oscillator)
//    Clock     = GPIO0_IN (external clock on GPIO 0)
//
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ETH.begin(ETH_PHY_LAN8720, 1, 23, 18, 16, ETH_CLOCK_GPIO0_IN);
#else
  ETH.begin(1, 16, 23, 18, ETH_PHY_LAN8720, ETH_CLOCK_GPIO0_IN);
#endif

// ─── Static IP (optional) ───
#if USE_STATIC_IP
  ETH.config(staticIP, gateway, subnet, dns1, dns2);
#endif

  // Wait for IP (with timeout)
  Serial.print("[ETH] Waiting for IP");
  unsigned long start = millis();
  while (!eth_got_ip && millis() - start < 15000)
  {
    Serial.print(".");
    delay(500);
  }
  Serial.println();

  if (!eth_got_ip)
  {
    Serial.println("[ETH] ⚠ Timeout — no IP obtained. Check cable!");
  }

  // ─── Start Web Server ───
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("[HTTP] Web server started on port 80");

  // ─── Start MQTT ───
  mqtt.setServer(mqtt_server, mqtt_port);
  mqtt.setCallback(mqttCallback);
}

// ============================================================
// LOOP
// ============================================================
void loop()
{
  // Handle HTTP clients
  server.handleClient();

  // Handle MQTT
  if (eth_got_ip)
  {
    if (!mqtt.connected())
    {
      static unsigned long lastReconnect = 0;
      if (millis() - lastReconnect > 5000)
      {
        lastReconnect = millis();
        connectMQTT();
      }
    }
    mqtt.loop();

    // Publish sensor data every 10 seconds
    static unsigned long lastPublish = 0;
    if (mqtt.connected() && millis() - lastPublish > 10000)
    {
      lastPublish = millis();

      // Example: publish uptime and free heap
      String payload = "{\"uptime\":" + String(millis() / 1000) +
                       ",\"heap\":" + String(ESP.getFreeHeap()) +
                       ",\"ip\":\"" + ETH.localIP().toString() + "\"}";
      mqtt.publish("sensor/wt32", payload.c_str());
      Serial.println("[MQTT] Published: " + payload);
    }
  }
}