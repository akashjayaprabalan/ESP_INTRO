#include <WebServer.h>
#include <WiFi.h>

constexpr uint8_t LED_PIN = 2;
constexpr uint16_t WEB_PORT = 80;


const char WIFI_SSID[] = "NanoESP32_LED";
const char WIFI_PASSWORD[] = "ledcontrol";

WebServer server(WEB_PORT);
bool led_is_on = false;

void setLed(bool on)
{
  led_is_on = on;
  digitalWrite(LED_PIN, on ? HIGH : LOW);
  Serial.println(on ? "LED is ON" : "LED is OFF");
}

void sendHomePage()
{
  const char *state_text = led_is_on ? "ON" : "OFF";
  const char *state_class = led_is_on ? "on" : "off";

  String page;
  page.reserve(3600);
  page += F("<!DOCTYPE html><html lang=\"en\"><head>");
  page += F("<meta charset=\"utf-8\">");
  page += F("<meta name=\"viewport\" content=\"width=device-width,initial-scale=1,viewport-fit=cover\">");
  page += F("<title>Nano ESP32 LED</title>");
  page += F("<style>");
  page += F(":root{font-family:-apple-system,BlinkMacSystemFont,\"Segoe UI\",sans-serif;color:#16202a;background:#f5f7fb;}");
  page += F("*{box-sizing:border-box;}");
  page += F("body{margin:0;min-height:100vh;display:flex;align-items:center;justify-content:center;padding:24px;}");
  page += F("main{width:min(430px,100%);}");
  page += F("h1{margin:0 0 16px;font-size:1.8rem;text-align:center;}");
  page += F(".status{margin:0 0 18px;padding:16px;border:1px solid #d7dee8;border-radius:8px;background:white;text-align:center;}");
  page += F(".label{display:block;margin-bottom:6px;color:#5c6977;font-size:.88rem;font-weight:700;text-transform:uppercase;}");
  page += F(".state{font-size:2.2rem;font-weight:800;}");
  page += F(".state.on{color:#047857;}.state.off{color:#b42318;}");
  page += F(".controls{display:grid;grid-template-columns:1fr 1fr;gap:12px;}");
  page += F("button{width:100%;min-height:74px;border:0;border-radius:8px;color:white;font-size:1.25rem;font-weight:800;box-shadow:0 8px 18px rgba(17,24,39,.18);}");
  page += F("button:active{transform:translateY(1px);box-shadow:0 4px 10px rgba(17,24,39,.22);}");
  page += F(".on-button{background:#059669;}.off-button{background:#dc2626;}");
  page += F("@media(max-width:340px){.controls{grid-template-columns:1fr;}h1{font-size:1.55rem;}button{font-size:1.15rem;}}");
  page += F("</style></head><body><main>");
  page += F("<h1>Nano ESP32 LED</h1>");
  page += F("<section class=\"status\"><span class=\"label\">Current state</span><span class=\"state ");
  page += state_class;
  page += F("\">");
  page += state_text;
  page += F("</span></section>");
  page += F("<section class=\"controls\">");
  page += F("<form action=\"/on\" method=\"get\"><button class=\"on-button\" type=\"submit\">ON</button></form>");
  page += F("<form action=\"/off\" method=\"get\"><button class=\"off-button\" type=\"submit\">OFF</button></form>");
  page += F("</section>");
  page += F("</main></body></html>");

  server.send(200, "text/html", page);
}

void redirectHome()
{
  server.sendHeader("Location", "/", true);
  server.send(303, "text/plain", "");
}

void handleLedOn()
{
  setLed(true);
  redirectHome();
}

void handleLedOff()
{
  setLed(false);
  redirectHome();
}

void handleNotFound()
{
  server.send(404, "text/plain", "Not found. Open http://192.168.4.1/");
}

void setup()
{
  Serial.begin(115200);
  delay(300);

  pinMode(LED_PIN, OUTPUT);
  setLed(false);

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
  server.on("/on", HTTP_GET, handleLedOn);
  server.on("/off", HTTP_GET, handleLedOff);
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
