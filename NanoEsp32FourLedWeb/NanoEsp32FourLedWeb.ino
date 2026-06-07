#include <WebServer.h>
#include <WiFi.h>

#if !defined(D2) || !defined(D3) || !defined(D4) || !defined(D5)
#error "Select Arduino Nano ESP32 in Arduino IDE before compiling this sketch."
#endif

constexpr uint16_t WEB_PORT = 80;

const char WIFI_SSID[] = "NanoESP32_FOUR_LED";
const char WIFI_PASSWORD[] = "fourled123";

struct LedOutput {
  const char *name;
  const char *label;
  uint8_t pin;
  bool is_on;
};

LedOutput leds[] = {
  { "red", "Red", D2, false },
  { "blue", "Blue", D3, false },
  { "yellow", "Yellow", D4, false },
  { "green", "Green", D5, false },
};

constexpr size_t LED_COUNT = sizeof(leds) / sizeof(leds[0]);

WebServer server(WEB_PORT);

void writeLed(size_t index)
{
  if (index >= LED_COUNT) {
    return;
  }

  digitalWrite(leds[index].pin, leds[index].is_on ? HIGH : LOW);
}

void setLedState(size_t index, bool on)
{
  if (index >= LED_COUNT) {
    return;
  }

  leds[index].is_on = on;
  writeLed(index);

  Serial.print(leds[index].label);
  Serial.println(on ? " LED on" : " LED off");
}

void allOff()
{
  for (size_t i = 0; i < LED_COUNT; i++) {
    setLedState(i, false);
  }
  Serial.println("All LEDs off");
}

int findLedIndex(const String &color)
{
  for (size_t i = 0; i < LED_COUNT; i++) {
    if (color == leds[i].name) {
      return (int)i;
    }
  }

  return -1;
}

void sendJsonState()
{
  String json;
  json.reserve(160);
  json += F("{\"leds\":{");

  for (size_t i = 0; i < LED_COUNT; i++) {
    if (i > 0) {
      json += ',';
    }
    json += '"';
    json += leds[i].name;
    json += F("\":");
    json += leds[i].is_on ? F("true") : F("false");
  }

  json += F("}}");

  server.send(200, "application/json", json);
}

void sendHomePage()
{
  String page;
  page.reserve(7800);
  page += F("<!DOCTYPE html><html lang=\"en\"><head>");
  page += F("<meta charset=\"utf-8\">");
  page += F("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1,viewport-fit=cover\">");
  page += F("<title>Nano ESP32 LED Control</title>");
  page += F("<style>");
  page += F(":root{font-family:-apple-system,BlinkMacSystemFont,\"Segoe UI\",sans-serif;color:#17202a;background:#f5f7fb;}");
  page += F("*{box-sizing:border-box;}body{margin:0;min-height:100vh;padding:18px;background:#f5f7fb;}");
  page += F("main{width:min(520px,100%);margin:0 auto;}h1{margin:6px 0 16px;text-align:center;font-size:1.65rem;}");
  page += F("section{margin:0 0 18px;}h2{margin:0 0 10px;font-size:.85rem;text-transform:uppercase;color:#5c6977;}");
  page += F(".grid{display:grid;grid-template-columns:1fr 1fr;gap:10px;}.panel{background:white;border:1px solid #d9e2ec;border-radius:8px;padding:12px;}");
  page += F(".color-name{display:flex;align-items:center;justify-content:space-between;margin-bottom:10px;font-weight:800;}.dot{width:14px;height:14px;border-radius:50%;display:inline-block;}");
  page += F(".red{background:#dc2626;}.blue{background:#2563eb;}.yellow{background:#facc15;}.green{background:#16a34a;}");
  page += F(".state{font-size:.8rem;color:#64748b;font-weight:800;}.active .state{color:#047857;}");
  page += F(".actions{display:grid;grid-template-columns:1fr 1fr;gap:8px;}button{border:0;border-radius:8px;min-height:48px;color:white;font-size:1rem;font-weight:800;}");
  page += F(".on-btn{background:#059669;}.off-btn{background:#dc2626;}.all-off{width:100%;min-height:58px;background:#111827;}");
  page += F("button:active{transform:translateY(1px);}@media(max-width:380px){.grid{grid-template-columns:1fr;}h1{font-size:1.45rem;}}");
  page += F("</style></head><body><main>");
  page += F("<h1>Nano ESP32 LED Control</h1>");
  page += F("<section><h2>LEDs</h2><div class=\"grid\">");

  for (size_t i = 0; i < LED_COUNT; i++) {
    page += F("<div class=\"panel led-panel\" data-color=\"");
    page += leds[i].name;
    page += F("\"><div class=\"color-name\"><span><span class=\"dot ");
    page += leds[i].name;
    page += F("\"></span> ");
    page += leds[i].label;
    page += F("</span><span class=\"state\">OFF</span></div><div class=\"actions\">");
    page += F("<button class=\"on-btn\" type=\"button\" data-on=\"1\">ON</button>");
    page += F("<button class=\"off-btn\" type=\"button\" data-on=\"0\">OFF</button>");
    page += F("</div></div>");
  }

  page += F("</div></section>");
  page += F("<button id=\"allOff\" class=\"all-off\" type=\"button\">ALL OFF</button>");
  page += F("<script>");
  page += F("const panels=[...document.querySelectorAll('.led-panel')];");
  page += F("function setPanel(color,on){const p=panels.find(x=>x.dataset.color===color);if(!p)return;p.classList.toggle('active',on);p.querySelector('.state').textContent=on?'ON':'OFF';}");
  page += F("async function loadState(){const r=await fetch('/api/state');const s=await r.json();Object.keys(s.leds).forEach(c=>setPanel(c,s.leds[c]));}");
  page += F("panels.forEach(p=>p.querySelectorAll('button').forEach(b=>b.addEventListener('click',async()=>{const color=p.dataset.color;const on=b.dataset.on;await fetch('/api/led?color='+encodeURIComponent(color)+'&on='+on);await loadState();})));");
  page += F("document.getElementById('allOff').addEventListener('click',async()=>{await fetch('/api/all-off');await loadState();});");
  page += F("loadState();");
  page += F("</script></main></body></html>");

  server.send(200, "text/html", page);
}

void handleLedApi()
{
  if (!server.hasArg("color") || !server.hasArg("on")) {
    server.send(400, "application/json", "{\"error\":\"Use color and on query parameters.\"}");
    return;
  }

  int led_index = findLedIndex(server.arg("color"));
  if (led_index < 0) {
    server.send(400, "application/json", "{\"error\":\"Unknown color.\"}");
    return;
  }

  String on_arg = server.arg("on");
  if (on_arg != "0" && on_arg != "1") {
    server.send(400, "application/json", "{\"error\":\"Use on=0 or on=1.\"}");
    return;
  }

  setLedState((size_t)led_index, on_arg == "1");
  sendJsonState();
}

void handleAllOffApi()
{
  allOff();
  sendJsonState();
}

void handleNotFound()
{
  server.send(404, "text/plain", "Not found. Open http://192.168.4.1/");
}

void setup()
{
  Serial.begin(115200);
  delay(300);

  for (size_t i = 0; i < LED_COUNT; i++) {
    pinMode(leds[i].pin, OUTPUT);
  }
  allOff();

  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);

  IPAddress local_ip(192, 168, 4, 1);
  IPAddress gateway(192, 168, 4, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.softAPConfig(local_ip, gateway, subnet);

  if (!WiFi.softAP(WIFI_SSID, WIFI_PASSWORD)) {
    Serial.println("Failed to start WiFi access point.");
    return;
  }

  server.on("/", HTTP_GET, sendHomePage);
  server.on("/api/led", HTTP_GET, handleLedApi);
  server.on("/api/all-off", HTTP_GET, handleAllOffApi);
  server.on("/api/state", HTTP_GET, sendJsonState);
  server.onNotFound(handleNotFound);
  server.begin();

  Serial.println();
  Serial.println("Nano ESP32 LED web app is ready.");
  Serial.print("WiFi network: ");
  Serial.println(WIFI_SSID);
  Serial.print("WiFi password: ");
  Serial.println(WIFI_PASSWORD);
  Serial.print("Open this address on your iPhone: http://");
  Serial.println(WiFi.softAPIP());
}

void loop()
{
  server.handleClient();
}
