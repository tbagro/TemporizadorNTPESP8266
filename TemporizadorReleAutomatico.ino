#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <NTPClient.h>
#include <FS.h>

const int relayPin = D5;  // Pino ao qual o relé está conectado
bool relayState = false;  // Estado inicial do relé: desligado

WiFiManager wifiManager;
WiFiUDP udp;
NTPClient timeClient(udp, "a.st1.ntp.br", -4 * 3600, 60000);  // Fuso horário: GMT-4

unsigned long pulseDuration = 40000;      // 40 segundos em milissegundos
unsigned long intervalBetweenPulses = 2;  // 40 minutos
unsigned long lastActivationTime;

const char *filePath = "/config.txt";


void atualizarIntervaloEntrePulsos() {
  Serial.println("Digite o novo valor para o intervalo entre pulsos (em minutos): ");

  while (!Serial.available()) {
    delay(100);  // Aguarda a entrada do usuário
  }

  // Lê o valor inserido pelo usuário
  int novoIntervalo = Serial.parseInt();

  // Atualiza o valor da variável global
  intervalBetweenPulses = novoIntervalo;

  // Salva o novo valor no arquivo
  File configFile = SPIFFS.open(filePath, "w");
  if (!configFile) {
    Serial.println("Falha ao abrir o arquivo para escrita");
    return;
  }

  configFile.println(intervalBetweenPulses);
  configFile.close();

  Serial.print("Intervalo entre pulsos atualizado para: ");
  Serial.println(intervalBetweenPulses);
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
  Serial.print("valor salvo: ");
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
  Serial.println(lastPulseTime);
  Serial.print("Último acionamento registrado em: ");
  Serial.println(buffer);
}

void acionarReleComTempo(unsigned int ms) {
  digitalWrite(relayPin, HIGH);  // Ligar o relé
  saveLastPulseTime(timeClient.getEpochTime());
  printLastPulseTime();  // Imprimir o valor salvo do último acionamento
  Serial.print("Rele on: ");
  Serial.println(timeClient.getEpochTime());

  // Aguardar até que tenha passado o tempo desejado
  delay(ms);

  digitalWrite(relayPin, LOW);  // Desligar o relé
}

void desligarReleComTempo(unsigned int min) {
  unsigned long epoch = min * 60;
  Serial.print("Rele off: ");
  Serial.println(timeClient.getEpochTime());
  Serial.print("epoch: ");
  Serial.println(epoch);

  // Aguardar até que tenha passado o intervalo desejado desde a última ativação
  while (timeClient.getEpochTime() - readLastPulseTime() < epoch) {
    delay(1000);  // Atraso de 1 segundo
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
}

void loop() {
  timeClient.update();
  
  // Acionar o relé
  acionarReleComTempo(pulseDuration);
  
  // Desligar o relé após o intervalo desejado
  desligarReleComTempo(intervalBetweenPulses);
  //atualiza o valor  entre os pulsos
  atualizarIntervaloEntrePulsos();
}
