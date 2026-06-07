#include <WebServer.h>
#include <WiFi.h>

#if !defined(D2) || !defined(D3) || !defined(D4) || !defined(D5) || !defined(D6)
#error "Select Arduino Nano ESP32 in Arduino IDE before compiling this sketch."
#endif

constexpr uint16_t WEB_PORT = 80;
constexpr unsigned long BLINK_INTERVAL_MS = 500;

const char WIFI_SSID[] = "NanoESP32_LED_MOTOR";
const char WIFI_PASSWORD[] = "ledmotor123";

struct BlinkLed {
  const char *name;
  const char *label;
  uint8_t pin;
  bool enabled;
  bool output_on;
};

BlinkLed leds[] = {
  { "red", "Red", D2, false, false },
  { "blue", "Blue", D3, false, false },
  { "yellow", "Yellow", D4, false, false },
  { "green", "Green", D5, false, false },
};

constexpr size_t LED_COUNT = sizeof(leds) / sizeof(leds[0]);
constexpr uint8_t MOTOR_PIN = D6;
constexpr uint8_t MOTOR_MIN_PERCENT = 0;
constexpr uint8_t MOTOR_MAX_PERCENT = 100;

WebServer server(WEB_PORT);
unsigned long previous_blink_ms = 0;
uint8_t motor_percent = 0;

void writeLedOutputsLow() {
  for (size_t i = 0; i < LED_COUNT; i++) {
    leds[i].output_on = false;
    digitalWrite(leds[i].pin, LOW);
  }
}

void setLedEnabled(size_t index, bool enabled) {
  if (index >= LED_COUNT) {
    return;
  }

  leds[index].enabled = enabled;
  if (!enabled) {
    leds[index].output_on = false;
    digitalWrite(leds[index].pin, LOW);
  }

  Serial.print(leds[index].label);
  Serial.println(enabled ? " LED blinking" : " LED off");
}

void setMotorPercent(uint8_t percent) {
  if (percent > MOTOR_MAX_PERCENT) {
    percent = MOTOR_MAX_PERCENT;
  }

  motor_percent = percent;
  int pwm_value = map(motor_percent, MOTOR_MIN_PERCENT, MOTOR_MAX_PERCENT, 0, 255);
  analogWrite(MOTOR_PIN, pwm_value);

  Serial.print("Motor slider: ");
  Serial.print(motor_percent);
  Serial.println("%");
}

void allOff() {
  for (size_t i = 0; i < LED_COUNT; i++) {
    leds[i].enabled = false;
  }
  writeLedOutputsLow();
  setMotorPercent(0);
  Serial.println("All outputs off");
}

int findLedIndex(const String &color) {
  for (size_t i = 0; i < LED_COUNT; i++) {
    if (color == leds[i].name) {
      return (int)i;
    }
  }

  return -1;
}

bool parsePercent(const String &value, uint8_t *percent) {
  if (value.length() == 0) {
    return false;
  }

  for (size_t i = 0; i < value.length(); i++) {
    if (!isDigit(value.charAt(i))) {
      return false;
    }
  }

  int parsed = value.toInt();
  if (parsed < MOTOR_MIN_PERCENT || parsed > MOTOR_MAX_PERCENT) {
    return false;
  }

  *percent = (uint8_t)parsed;
  return true;
}

void sendJsonState() {
  String json;
  json.reserve(192);
  json += F("{\"leds\":{");

  for (size_t i = 0; i < LED_COUNT; i++) {
    if (i > 0) {
      json += ',';
    }
    json += '"';
    json += leds[i].name;
    json += F("\":");
    json += leds[i].enabled ? F("true") : F("false");
  }

  json += F("},\"motor\":");
  json += motor_percent;
  json += '}';

  server.send(200, "application/json", json);
}

void sendHomePage() {
  String page;
  page.reserve(9200);
  page += F("<!DOCTYPE html><html lang=\"en\"><head>");
  page += F("<meta charset=\"utf-8\">");
  page += F("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1,viewport-fit=cover\">");
  page += F("<title>Nano ESP32 LED Motor</title>");
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
  page += F(".motor-row{display:flex;align-items:center;gap:12px;}.motor-value{min-width:54px;text-align:right;font-size:1.25rem;font-weight:900;color:#334155;}");
  page += F("input[type=range]{width:100%;accent-color:#7c3aed;}button:active{transform:translateY(1px);}");
  page += F("@media(max-width:380px){.grid{grid-template-columns:1fr;}h1{font-size:1.45rem;}}");
  page += F("</style></head><body><main>");
  page += F("<h1>Nano ESP32 Control</h1>");
  page += F("<section><h2>Blink LEDs</h2><div class=\"grid\">");

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
  page += F("<section><h2>Vibration Motor Signal</h2><div class=\"panel\"><div class=\"motor-row\">");
  page += F("<input id=\"motorSlider\" type=\"range\" min=\"0\" max=\"100\" value=\"0\">");
  page += F("<span id=\"motorValue\" class=\"motor-value\">0%</span>");
  page += F("</div></div></section>");
  page += F("<button id=\"allOff\" class=\"all-off\" type=\"button\">ALL OFF</button>");
  page += F("<script>");
  page += F("const panels=[...document.querySelectorAll('.led-panel')];");
  page += F("const slider=document.getElementById('motorSlider');const motorValue=document.getElementById('motorValue');");
  page += F("function setPanel(color,on){const p=panels.find(x=>x.dataset.color===color);if(!p)return;p.classList.toggle('active',on);p.querySelector('.state').textContent=on?'BLINKING':'OFF';}");
  page += F("async function loadState(){const r=await fetch('/api/state');const s=await r.json();Object.keys(s.leds).forEach(c=>setPanel(c,s.leds[c]));slider.value=s.motor;motorValue.textContent=s.motor+'%';}");
  page += F("panels.forEach(p=>p.querySelectorAll('button').forEach(b=>b.addEventListener('click',async()=>{const color=p.dataset.color;const on=b.dataset.on;await fetch('/api/led?color='+encodeURIComponent(color)+'&on='+on);await loadState();})));");
  page += F("let motorTimer;slider.addEventListener('input',()=>{motorValue.textContent=slider.value+'%';clearTimeout(motorTimer);motorTimer=setTimeout(async()=>{await fetch('/api/motor?value='+slider.value);await loadState();},80);});");
  page += F("document.getElementById('allOff').addEventListener('click',async()=>{await fetch('/api/all-off');await loadState();});");
  page += F("loadState();");
  page += F("</script></main></body></html>");

  server.send(200, "text/html", page);
}

void handleLedApi() {
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

  setLedEnabled((size_t)led_index, on_arg == "1");
  sendJsonState();
}

void handleMotorApi() {
  if (!server.hasArg("value")) {
    server.send(400, "application/json", "{\"error\":\"Use value query parameter.\"}");
    return;
  }

  uint8_t value = 0;
  if (!parsePercent(server.arg("value"), &value)) {
    server.send(400, "application/json", "{\"error\":\"Motor value must be 0 through 100.\"}");
    return;
  }

  setMotorPercent(value);
  sendJsonState();
}

void handleAllOffApi() {
  allOff();
  sendJsonState();
}

void handleNotFound() {
  server.send(404, "text/plain", "Not found. Open http://192.168.4.1/");
}

void updateBlinkingLeds() {
  unsigned long now = millis();
  if (now - previous_blink_ms < BLINK_INTERVAL_MS) {
    return;
  }

  previous_blink_ms = now;
  for (size_t i = 0; i < LED_COUNT; i++) {
    if (leds[i].enabled) {
      leds[i].output_on = !leds[i].output_on;
      digitalWrite(leds[i].pin, leds[i].output_on ? HIGH : LOW);
    } else if (leds[i].output_on) {
      leds[i].output_on = false;
      digitalWrite(leds[i].pin, LOW);
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);

  for (size_t i = 0; i < LED_COUNT; i++) {
    pinMode(leds[i].pin, OUTPUT);
  }
  pinMode(MOTOR_PIN, OUTPUT);
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
  server.on("/api/motor", HTTP_GET, handleMotorApi);
  server.on("/api/all-off", HTTP_GET, handleAllOffApi);
  server.on("/api/state", HTTP_GET, sendJsonState);
  server.onNotFound(handleNotFound);
  server.begin();

  Serial.println();
  Serial.println("Nano ESP32 LED + motor web app is ready.");
  Serial.print("WiFi network: ");
  Serial.println(WIFI_SSID);
  Serial.print("WiFi password: ");
  Serial.println(WIFI_PASSWORD);
  Serial.print("Open this address on your iPhone: http://");
  Serial.println(WiFi.softAPIP());
  Serial.println("Do not connect a bare DC 3V 12000rpm motor directly to D6.");
}

void loop() {
  server.handleClient();
  updateBlinkingLeds();
}
