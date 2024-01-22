/* 
   Código sem WiFi, para acionamento estilo minuteria, onde o sistema calcula intervalos de acionamento,
   armazena na memória EEPROM e compara com o último valor salvo.
*/

#include <Wire.h>
#include <RTClib.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>


RTC_DS3231 rtc;
const int pinoRele = D5;
unsigned long tempoAtivoPadrao = 35;    // Tempo ativo padrão: 10 segundos
unsigned long tempoInativoPadrao = 2100;  // Tempo inativo padrão em segundos: ex: 30 minutos*60 = (1800 segundos)

unsigned long horaUltimoAcionamento = 0;

void hold(unsigned long ms) {  // função de contagem de tempo não bloqueante
  unsigned long startTime = millis();
  while (millis() - startTime < ms) {
    // Aguarda sem bloquear
    yield();
    wdt_reset();
  }
}

bool debug = false;  // Altere para false para desativar os comentários de debug

// void sleepMode() {
//   set_sleep_mode(SLEEP_MODE_PWR_DOWN);
//   sleep_enable();
//   power_all_disable();
//   sleep_mode();
//   sleep_disable();
//   power_all_enable();
//   if (debug) Serial.println("domindo");
// }



void setup() {
  Serial.begin(115200);
  pinMode(pinoRele, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(pinoRele, LOW);
  digitalWrite(LED_BUILTIN, LOW);

  WiFi.mode(WIFI_OFF);
  EEPROM.begin(512);

  if (!rtc.begin()) {
    if (debug) Serial.println("Não foi possível encontrar o módulo RTC!");
    digitalWrite(LED_BUILTIN, HIGH);
    hold(200);
    digitalWrite(LED_BUILTIN, LOW);
    while (1)
      ;
  }

  if (rtc.lostPower()) {
    if (debug) Serial.println("O módulo RTC perdeu a energia e o tempo está incorreto!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
    horaUltimoAcionamento = lerEEPROM();
}

void loop() {

  DateTime now = rtc.now();
  unsigned long unixTime = now.unixtime();
  // Restaura a última hora de acionamento salva na EEPROM

  hold(1000);

  if ((unixTime - horaUltimoAcionamento) >= tempoInativoPadrao) {
    hold(2000);
    // Tempo inativo atingido, aciona o relé pulsado
    digitalWrite(pinoRele, HIGH);
    digitalWrite(LED_BUILTIN, LOW);
    hold(tempoAtivoPadrao * 1000);  // Mantém o relé acionado pelo tempo ativo
                                    // Salva a hora do último acionamento na EEPROM

    if (debug) Serial.println("rele ligado ");
    digitalWrite(pinoRele, LOW);
    digitalWrite(LED_BUILTIN, HIGH);

    // Entra no modo de sleep
    //ESP.deepSleep(10e6); /* Sleep for 5 seconds */
    horaUltimoAcionamento = unixTime;
    salvarEEPROM(horaUltimoAcionamento);

    if (rtc.lostPower()) {
      Serial.println("Erro: O RTC não está ativo!");
      // Adicione aqui qualquer ação que você deseja realizar em caso de erro no RTC
      digitalWrite(LED_BUILTIN, HIGH);
      hold(200);
      digitalWrite(LED_BUILTIN, LOW);
      digitalWrite(pinoRele, LOW);
      while (1)
        ;
    }
  }

  // Aguarda um curto período antes de verificar novamente
  hold(1000);
  yield();
}

// Função para salvar a hora do último acionamento na EEPROM
void salvarEEPROM(uint32_t hora) {
  uint32_t horaAtual = lerEEPROM();

  if (horaAtual != hora) {
    if (debug) Serial.print("valor salvo ");
    Serial.println(hora);
    EEPROM.write(0, (hora >> 24) & 0xFF);
    EEPROM.write(1, (hora >> 16) & 0xFF);
    EEPROM.write(2, (hora >> 8) & 0xFF);
    EEPROM.write(3, hora & 0xFF);
    delay(10);
  }
}

uint32_t lerEEPROM() {
  uint32_t hora = 0;
  hora |= (EEPROM.read(0) << 24);
  hora |= (EEPROM.read(1) << 16);
  hora |= (EEPROM.read(2) << 8);
  hora |= EEPROM.read(3);

  // Aciona o LED embutido por 300 milissegundos
  digitalWrite(LED_BUILTIN, HIGH);
  hold(400);
  digitalWrite(LED_BUILTIN, LOW);

  if (debug) Serial.print("Valor lido: ");
  Serial.println(hora);
  return hora;
}
