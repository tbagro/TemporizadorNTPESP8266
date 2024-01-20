#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <NTPClient.h>
#include <FS.h>

#define DEBUG_MODE  // Comente ou descomente esta linha para ativar ou desativar o modo de depuração


const int relayPin = D5;  // Pino ao qual o relé está conectado
bool relayState = false;  // Estado inicial do relé: desligado

WiFiManager wifiManager;
WiFiUDP udp;
NTPClient timeClient(udp, "a.st1.ntp.br", -4 * 3600, 60000);  // Fuso horário: GMT-4

unsigned long pulseDuration = 35000;      // 40 segundos em milissegundos
unsigned long intervalBetweenPulses = 40;  // 40 minutos
unsigned long lastActivationTime;

const char *filePath = "/config.txt";

void hold(const unsigned int &ms) {
  unsigned long startTime = millis();
  while (millis() - startTime < ms) {
    yield();
  }
}

#ifdef DEBUG_MODE
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINT(x) Serial.print(x)
#else
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINT(x)
#endif

// void atualizarIntervaloEntrePulsos() {
//   DEBUG_PRINTLN("Digite o novo valor para o intervalo entre pulsos (em minutos): ");

//   while (!Serial.available()) {
//     delay(100);  // Aguarda a entrada do usuário
//   }

//   // Lê o valor inserido pelo usuário
//   int novoIntervalo = Serial.parseInt();

//   // Atualiza o valor da variável global
//   intervalBetweenPulses = novoIntervalo;

//   // Salva o novo valor no arquivo
//   File configFile = SPIFFS.open(filePath, "w");
//   if (!configFile) {
//     DEBUG_PRINTLN("Falha ao abrir o arquivo para escrita");
//     return;
//   }

//   configFile.println(intervalBetweenPulses);
//   configFile.close();

//   Serial.print("Intervalo entre pulsos atualizado para: ");
//   DEBUG_PRINTLN(intervalBetweenPulses);
// }

void saveLastPulseTime(unsigned long eventTime) {
  // Abre o arquivo para escrita
  File configFile = SPIFFS.open(filePath, "w");
  if (!configFile) {
    DEBUG_PRINTLN("Falha ao abrir o arquivo para escrita");
    return;
  }

  // Escreve o horário do último acionamento no arquivo
  configFile.println(eventTime);
  configFile.close();
  DEBUG_PRINTLN("Horário do último acionamento salvo com sucesso!");
}

unsigned long readLastPulseTime() {
  if (!SPIFFS.begin()) {
    DEBUG_PRINTLN("Falha ao montar o sistema de arquivos SPIFFS");
    return 0;
  }

  if (!SPIFFS.exists(filePath)) {
    DEBUG_PRINTLN("Arquivo de configuração não encontrado");
    return 0;
  }

  File configFile = SPIFFS.open(filePath, "r");
  if (!configFile) {
    DEBUG_PRINTLN("Falha ao abrir o arquivo para leitura");
    return 0;
  }

  String content = configFile.readStringUntil('\n');
  DEBUG_PRINT("valor salvo: ");
  DEBUG_PRINTLN(content);
  configFile.close();
  return content.toInt();
}

void printLastPulseTime() {
  // Obter o tempo do último acionamento do arquivo
  unsigned long lastPulseTime = readLastPulseTime();

  // Converter para o formato de tempo local
  time_t lastPulseTime_t = static_cast<time_t>(lastPulseTime);

  // Obter a estrutura de informações do tempo local
  struct tm *timeinfo;
  timeinfo = localtime(&lastPulseTime_t);

  // Criar uma string formatada com a data e hora
  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);

  // Imprimir o valor do último acionamento
  DEBUG_PRINT("Último acionamento registrado em: ");
  DEBUG_PRINTLN(lastPulseTime);
  DEBUG_PRINT("Último acionamento registrado em: ");
  DEBUG_PRINTLN(buffer);
}

void acionarReleComTempo(unsigned int ms) {
  digitalWrite(relayPin, HIGH);  // Ligar o relé
  saveLastPulseTime(timeClient.getEpochTime());
  printLastPulseTime();  // Imprimir o valor salvo do último acionamento
  DEBUG_PRINT("Rele on: ");
  DEBUG_PRINTLN(timeClient.getEpochTime());

  // Aguardar até que tenha passado o tempo desejado
  hold(ms);

  digitalWrite(relayPin, LOW);  // Desligar o relé
}

void desligarReleComTempo(unsigned int min) {
  unsigned long epoch = min * 60;
  DEBUG_PRINT("Rele off: ");
  DEBUG_PRINTLN(timeClient.getEpochTime());
  DEBUG_PRINT("epoch: ");
  DEBUG_PRINTLN(epoch);

  // Aguardar até que tenha passado o intervalo desejado desde a última ativação
  while (timeClient.getEpochTime() - readLastPulseTime() < epoch) {
    hold(1000);  // Atraso de 1 segundo
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(relayPin, OUTPUT);
  digitalWrite(relayPin, LOW);  // Garante que o relé está inicialmente desligado

  wifiManager.setConfigPortalTimeout(180);
  wifiManager.autoConnect("PoçoServer");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  timeClient.begin();

  // Inicia o sistema de arquivos SPIFFS
  if (!SPIFFS.begin()) {
    DEBUG_PRINTLN("Falha ao montar o sistema de arquivos SPIFFS");
    return;
  }
}

bool timeSynchronized = false;
void loop() {
  timeClient.update();

  if (WiFi.status() == WL_CONNECTED && WiFi.isConnected()) {
    // Verificar se o servidor NTP está atualizado
    if (!timeSynchronized || timeClient.update()) {
      // Set the timeSynchronized flag to true when time is successfully synchronized
      timeSynchronized = true;
      DEBUG_PRINTLN("Servidor NTP atualizado. Funções acionadas.");

      // Acionar o relé
      acionarReleComTempo(pulseDuration);
      // Aguarde um intervalo antes de verificar o desligamento do relé
      hold(5000);  // Atraso de 5 segundos para permitir que o relé seja acionado
      // Desligar o relé após o intervalo desejado
      desligarReleComTempo(intervalBetweenPulses);
    } else {
      DEBUG_PRINTLN("WiFi sem internet. Funções não acionadas.");
      timeSynchronized = false;
    }

    // Aguarde um intervalo antes da próxima iteração
    hold(1000);  // Atraso de 1 segundo
    DEBUG_PRINTLN(timeClient.getFormattedTime());
  }
}
