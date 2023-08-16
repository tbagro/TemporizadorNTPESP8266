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
String relayPin;
String relayPinSaved = relayPin;
int relayPinValue = relayPin.toInt();


unsigned long lastPumpTime = 0;
unsigned int onInterval = ON;
bool pumpState = false;
unsigned long lastRebootTime = 0;

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

void checkPumpStateAfterReboot() {
  if (millis() - lastRebootTime < 60000) {  // Check if less than 1 minute has passed since reboot
    if (pumpState) {                        // If pump was on before reboot
      pumpState = false;                    // Turn off the pump
      interval(relayPin.toInt(), OFF, false);
    }
  }
}



void pumpControl() {

  unsigned long currentTime = millis() / 60000;  // Converte o tempo para minutos
                                                 // Converter a string para um valor inteiro

  // Verifica se já passou o intervalo em que a bomba ficará acionada
  if (pumpState && currentTime - lastPumpTime >= onInterval) {

    pumpState = false;                    // Desliga a bomba após o intervalo definido
    interval(relayPinValue, OFF, false);  // Desliga a bomba
    digitalWrite(LED_BUILTIN, HIGH);
    lastPumpTime = currentTime;  // Atualiza o último acionamento
  }

  // Verifica se é hora de ligar a bomba novamente
  if (!pumpState && currentTime - lastPumpTime >= OFF) {
    pumpState = true;
    Serial.print("Pump :");
    Serial.println("acionado");         // Liga a bomba
    interval(relayPinValue, ON, true);  // Liga a bomba
    digitalWrite(LED_BUILTIN, LOW);

    lastPumpTime = currentTime;  // Atualiza o último acionamento
  }
}


const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta http-equiv="refresh" content="5"> <!-- Recarrega a página a cada 5 segundos -->
<style>
  body {
    font-family: Arial, sans-serif;
    background-color: white;
    text-align: center;
    color: blue;
  }
  h1 {
    margin-top: 20px;
  }
  form {
    margin-top: 20px;
  }
  input[type='text'], input[type='checkbox'], input[type='submit'] {
    margin: 5px;
  }
  .status {
    margin-top: 20px;
  }
</style>
</head>
<body>
  <h1>Temporizador On line</h1>
  <form action='/settings' method='POST'>
    <label for='on'>ON Time (min): </label>
    <input type='text' name='on' value='{{ON}}'><br>
    <label for='off'>OFF Time (min): </label>
    <input type='text' name='off' value='{{OFF}}'><br>
    <label for='pin'>Relay Pin: </label>
    <input type='text' name='pin' value='{{relayPin}}'><br>
    <label for='manual'>Modo Manual: </label>
    <button name='manual' value='on'>{{manualModeSaved}}</button>
    <input type='submit' value='Save'>
  </form>
  <div class='status'>
    <p>Saved Variables:</p>
    <p>Tempo ON (min): {{ON}}</p>
    <p>Tempo OFF (min): {{OFF}}</p>
    <p>Pino do Relé: {{relayPin}}</p>
    
  </div>
</body>
</html>
)=====";


void handleSettings() {
  ON = webServer.arg("on").toInt();
  OFF = webServer.arg("off").toInt();
  // Verifica se o botão de modo manual foi ativado
  if (webServer.hasArg("manual")) {
    manualMode = true;
  } else {
    manualMode = false;
  }
  relayPin = webServer.arg("pin");  // Obter o valor do pino da solicitação HTTP
  saveSettingsToTxt();
  webServer.sendHeader("Location", "/");
  webServer.send(302, "text/plain", "Settings updated");
}


void handleRoot() {
  String htmlContent = MAIN_page;  // Use the MAIN_page string
  // Add code to replace placeholders and handle variables
  htmlContent.replace("{{ON}}", String(ON));
  htmlContent.replace("{{OFF}}", String(OFF));
  // Replace {{manualMode}} with the appropriate text based on manualMode value
  // Substitui {{manualModeSaved}} com "Ativado" ou "Desativado" de acordo com o valor de manualMode
  if (manualMode) {
    htmlContent.replace("{{manualModeSaved}}", "Ativado");
  } else {
    htmlContent.replace("{{manualModeSaved}}", "Desativado");
  }

  htmlContent.replace("{{relayPin}}", String(relayPin));

  // Add code to send the modified HTML content to the client's browser
  webServer.send(200, "text/html", htmlContent);
}


void saveSettingsToTxt() {
  File configFile = SPIFFS.open("/config.txt", "w");
  if (!configFile) {
    Serial.println(F("Failed to open config file for writing"));
    return;
  }
  configFile.println(ON);
  configFile.println(OFF);
  configFile.println(manualMode);
  configFile.println(relayPin.toInt());  // Escreve o número do pino do relé como um inteiro


  configFile.close();
}

void readSettingsFromTxt() {
  File configFile = SPIFFS.open("/config.txt", "r");
  if (!configFile) {
    Serial.println(F("Failed to open config file for reading"));
    return;
  }

  Serial.println("Reading settings from config file:");

  ON = configFile.parseInt();
  Serial.print("ON: ");
  Serial.println(ON);

  OFF = configFile.parseInt();
  Serial.print("OFF: ");
  Serial.println(OFF);

  manualMode = configFile.parseInt();
  Serial.print("Manual Mode: ");
  Serial.println(manualMode);

  relayPin = configFile.parseInt();  // Lê o número do pino do relé como um inteiro
  Serial.print("relayPin : ");
  Serial.println(relayPin);


  configFile.close();
}



void setup() {
  Serial.begin(115200);

  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(relayPinValue, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  wifiManager.setConfigPortalTimeout(180);
  wifiManager.autoConnect("ESPWebServer");

  lastRebootTime = millis();  // Store the current time at startup
  Serial.println("Relay Pin: " + relayPin);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  if (SPIFFS.begin()) {
    Serial.println(F("SPIFFS initialized"));

    // Verifica se o arquivo config.txt já existe
    if (!SPIFFS.exists("/config.txt")) {
      Serial.println(F("Creating and saving default config.txt"));

      File configFile = SPIFFS.open("/config.txt", "w");
      if (configFile) {
        // Escreve os valores padrão no arquivo
        // Escreve os valores padrão no arquivo
        configFile.println("1");   // tempo ON  em minutos
        configFile.println("35");  // tempo OFF em minutos
        configFile.println("0");   // ManualMode (0 para falso)
        configFile.println("D5");  // relayPin
        configFile.close();

        Serial.println(F("Default values saved to config.txt:"));
        Serial.print("ON: ");
        Serial.println(ON);
        Serial.print("OFF: ");
        Serial.println(OFF);
        Serial.print("Manual Mode: ");
        Serial.println(manualMode);
        Serial.print("Relay Pin: ");
        Serial.println(relayPin);
      } else {
        Serial.println(F("Failed to create config.txt"));
      }
    }
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



void loop() {
  webServer.handleClient();  // Lida com as requisições da interface web
  timeClient.update();
  checkPumpStateAfterReboot();  // Check pump state after reboot
  hold(1000);

  if (!manualMode) {
    pumpControl();
  }

  hold(1000);
  //Serial.println(timeClient.getFormattedTime());
  yield();
}
