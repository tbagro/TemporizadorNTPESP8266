#include <NTPClient.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <ESP8266WebServer.h>  // Biblioteca para o servidor web
#include <FS.h>                // Inclua a biblioteca para o sistema de arquivos SPIFFS

WiFiManager wifiManager;
ESP8266WebServer webServer(80);  // Cria o objeto do servidor web

WiFiUDP udp;
int Tz = -4;
NTPClient timeClient(udp, "a.st1.ntp.br", Tz * 3600, 60000);

int ON = 1;
int OFF = 40;
bool manualMode = false;
String relayPin = "D8";

void hold(const unsigned int &ms) {
  unsigned long m = millis();
  while (millis() - m < ms) {
    yield();
  }
}

unsigned long getTime() {
  timeClient.update();
  unsigned long now = timeClient.getEpochTime();
  return now;
}

void interval(int PIN, const unsigned int &min, bool Status) {
  digitalWrite(PIN, Status);
  unsigned long intervalo = min * 60;

  unsigned long sec = getTime();
  while ((getTime()) - sec < intervalo) {
    Status = !Status;
    Serial.print("diff   :");
    Serial.println(getTime() - sec);
    hold(1000);
  }
  Serial.print("status :");
  Serial.println(Status);
  hold(1000);
}

void saveSettingsToTxt() {
  File configFile = SPIFFS.open("/config.txt", "r");
  if (!configFile) {
    Serial.println(F("Failed to open config file for reading"));
    return;
  }

  ON = configFile.parseInt();
  OFF = configFile.parseInt();
  manualMode = configFile.parseInt();
  relayPin = configFile.readStringUntil('\r');  // Use '\r' para ler a variável do pino
  relayPin.trim();

  configFile.close();
}

void readSettingsFromTxt() {
  File configFile = SPIFFS.open("/config.txt", "r");
  if (!configFile) {
    Serial.println(F("Failed to open config file for reading"));
    return;
  }

  ON = configFile.parseInt();
  OFF = configFile.parseInt();
  manualMode = configFile.parseInt();
  relayPin = configFile.readStringUntil('\r');  // Use '\r' para ler a variável do pino
  relayPin.trim();

  configFile.close();
}

void handleRoot() {
  String html = "<html>";
  html += "<head><meta charset='UTF-8'>";
  html += "<style>";
  html += "body { font-family: Arial, sans-serif; background-color: white; text-align: center; color: blue; }";
  html += "h1 { margin-top: 20px; }";
  html += "form { margin-top: 20px; }";
  html += "input[type='text'], input[type='checkbox'], input[type='submit'] { margin: 5px; }";
  html += ".status { margin-top: 20px; }";
  html += "</style></head>";
  html += "<body>";
  html += "<h1>Temporizador</h1>";
  html += "<form action='/settings' method='POST'>";
  html += "<label for='on'>Tempo ON (min): </label><input type='text' name='on' value='" + String(ON) + "'><br>";
  html += "<label for='off'>Tempo OFF (min): </label><input type='text' name='off' value='" + String(OFF) + "'><br>";
  html += "<label for='pin'>Pino do Relé: </label><input type='text' name='pin' value='" + relayPin + "'><br>";
  html += "<label for='manual'>Modo Manual: </label><input type='checkbox' name='manual' " + String(manualMode ? "checked" : "") + "><br>";
  html += "<input type='submit' value='Salvar'>";
  html += "</form>";
  html += "<div class='status'>";
  html += "<p>Variáveis Salvas:</p>";
  html += "<p>Tempo ON (min): " + String(ON) + "</p>";
  html += "<p>Tempo OFF (min): " + String(OFF) + "</p>";
  html += "<p>Modo Manual: " + String(manualMode ? "Ativado" : "Desativado") + "</p>";
  html += "<p>Pino do Relé: " + String(relayPin) + "</p>";
  html += "</div>";
  html += "</body></html>";
  webServer.send(200, "text/html", html);
}

void handleSettings() {
  ON = webServer.arg("on").toInt();
  OFF = webServer.arg("off").toInt();
  manualMode = webServer.arg("manual") == "on";
  relayPin = webServer.arg("pin");  // Obter o valor do pino da solicitação HTTP
  saveSettingsToTxt();
  webServer.sendHeader("Location", "/");
  webServer.send(302, "text/plain", "Settings updated");
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_BUILTIN, OUTPUT);
  wifiManager.setConfigPortalTimeout(180);
  wifiManager.autoConnect("ESPWebServer");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  if (SPIFFS.begin()) {
    Serial.println(F("SPIFFS initialized"));
  } else {
    Serial.println(F("SPIFFS initialization failed"));
  }
  Serial.println(F("Conectado"));

  webServer.on("/", HTTP_GET, handleRoot);
  webServer.on("/settings", HTTP_POST, handleSettings);

  readSettingsFromTxt();

  timeClient.begin();
  webServer.begin();
}

unsigned long lastPumpTime = 0;  // Armazena o momento do último acionamento da bomba
unsigned int onInterval = ON;    // Intervalo em minutos que a bomba ficará acionada
bool pumpState = false;          // Estado da bomba (ligada/desligada)

void pumpControl() {
  unsigned long currentTime = millis() / 60000;  // Converte o tempo para minutos
  int relayPinValue = relayPin.toInt();          // Converter a string para um valor inteiro

  // Verifica se já passou o intervalo em que a bomba ficará acionada
  if (pumpState && currentTime - lastPumpTime >= onInterval) {
    pumpState = false;               // Desliga a bomba após o intervalo definido
    interval(relayPinValue, OFF, false);  // Desliga a bomba
    lastPumpTime = currentTime;      // Atualiza o último acionamento
  }

  // Verifica se é hora de ligar a bomba novamente
  if (!pumpState && currentTime - lastPumpTime >= OFF) {
    pumpState = true;              // Liga a bomba
    interval(relayPinValue, ON, true);  // Liga a bomba
    lastPumpTime = currentTime;    // Atualiza o último acionamento
  }
}

void loop() {
  webServer.handleClient();  // Lida com as requisições da interface web

  timeClient.update();
  hold(1000);

  if (!manualMode) {
    pumpControl();
  }

  hold(1000);
  Serial.println(timeClient.getFormattedTime());
  yield();
}
