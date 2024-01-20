#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <NTPClient.h>
#include <FS.h>


const int relayPin = D5;  // Pino ao qual o relé está conectado
bool relayState = false;  // Estado inicial do relé: desligado

WiFiManager wifiManager;
WiFiUDP udp;
NTPClient timeClient(udp, "a.st1.ntp.br", -4 * 3600, 60000);  // Fuso horário: GMT-4

void hold(const unsigned int &ms) {
  // Atraso não bloqueante
  unsigned long m = millis();
  while (millis() - m < ms) {
    yield();
  }
}

unsigned long pulseDuration = 40000;      // 40 segundos em milissegundos
unsigned long intervalBetweenPulses = 1;  // 40 minutos
unsigned long lastActivationTime;

const char *filePath = "/config.txt";

void listFiles() {
  Dir dir = SPIFFS.openDir("/");
  while (dir.next()) {
    Serial.print("Nome do arquivo: ");
    Serial.println(dir.fileName());
  }
}


void saveLastPulseTime(unsigned long eventTime) {
  // Abre o arquivo para escrita
  File configFile = SPIFFS.open(filePath, "w");
  if (!configFile) {
    Serial.println("Falha ao abrir o arquivo para escrita");
    return;
  }

  // Escreve o horário do último acionamento no arquivo
  configFile.println(eventTime);
  configFile.close();
  Serial.println("Horário do último acionamento salvo com sucesso!");
}

unsigned long readLastPulseTime() {
  if (!SPIFFS.begin()) {
    Serial.println("Falha ao montar o sistema de arquivos SPIFFS");
    return 0;
  }

  if (!SPIFFS.exists(filePath)) {
    Serial.println("Arquivo de configuração não encontrado");
    return 0;
  }

  File configFile = SPIFFS.open(filePath, "r");
  if (!configFile) {
    Serial.println("Falha ao abrir o arquivo para leitura");
    return 0;
  }

  String content = configFile.readStringUntil('\n');
  Serial.print("Conteúdo lido do arquivo: ");
  Serial.println(content);
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
  Serial.print("Último acionamento registrado em: ");
  Serial.println(buffer);
}



void acionarReleComTempo(unsigned int ms) {  // atraso em millis
  // Ligar o relé (assumindo HIGH para ligar

  digitalWrite(relayPin, HIGH);
  // Imprimir o valor salvo do último acionamento
  printLastPulseTime();
  // Obter o tempo atual em millis
  unsigned long tempoInicio = millis();
  Serial.println("ligarReleComTempo");
  // Aguardar até que tenha passado o tempo desejado
  while (millis() - tempoInicio < ms) {
    // Atraso não bloqueante
    yield();
  }
  // Desligar o relé (assumindo LOW para desligar, ajuste conforme necessário)
  saveLastPulseTime(timeClient.getEpochTime());
  // desligarReleComTempo(intervalBetweenPulses);  //Desliga o rele
}

void desligarReleComTempo(unsigned int min) {
  // Obter o tempo atual epoch em minutos
  unsigned long agoraMinutos = timeClient.getEpochTime() / 60;
  Serial.println("desligarReleComTempo");
  digitalWrite(relayPin, LOW);
  // Verificar se passou o intervalo desejado desde a última ativação
  if (agoraMinutos - readLastPulseTime() >= min) {
    // Chamar a função para desligar o relé (ajuste conforme necessário)
    // Atualizar o tempo da última desativação em minutos
    lastActivationTime = agoraMinutos;
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
    Serial.println("Falha ao montar o sistema de arquivos SPIFFS");
    return;
  }
  listFiles();
  hold(5000);  // Atraso de 5 segundo para evitar verificações muito rápidas
}

void loop() {
  timeClient.update();
  // Acionar o relé
  acionarReleComTempo(pulseDuration);
  // Desligar o relé após o intervalo desejado
  desligarReleComTempo(intervalBetweenPulses);

  hold(1000);  // Atraso de 1 segundo para evitar verificações muito rápidas
}
