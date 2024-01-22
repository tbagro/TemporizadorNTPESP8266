/* 
   Código sem WiFi, para acionamento estilo minuteria, onde o sistema calcula intervalos de acionamento,
   armazena na memória EEPROM e compara com o último valor salvo.
*/

#include <Wire.h>
#include <RTClib.h>
#include <EEPROM.h>

RTC_DS3231 rtc;
const int pinoRele = D5;
unsigned long tempoAtivoPadrao = 35;    // Tempo ativo padrão: 10 segundos
unsigned long tempoInativoPadrao = 1800 ;  // Tempo inativo padrão: ex: 30 minutos*60 = (1800 segundos)

unsigned long horaUltimoAcionamento = 0;

void hold(unsigned long ms) {  // função de contagem de tempo não bloqueante
  unsigned long startTime = millis();
  while (millis() - startTime < ms) {
    // Aguarda sem bloquear
    yield();
    wdt_reset();
  }
}

bool debug = true;  // Altere para false para desativar os comentários de debug

void setup() {
  Serial.begin(115200);
  pinMode(pinoRele, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(pinoRele, LOW);
  digitalWrite(LED_BUILTIN, LOW);

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

  // Restaura a última hora de acionamento salva na EEPROM
  horaUltimoAcionamento = lerEEPROM();
  hold(1000);
}

void loop() {
// Verifica se o RTC esta funcionando  ainda
  if ((rtc.lostPower())) { 
    Serial.println("Erro: O RTC não está ativo!");
    // Adicione aqui qualquer ação que você deseja realizar em caso de erro no RTC
    digitalWrite(LED_BUILTIN, HIGH);
    hold(200);
    digitalWrite(LED_BUILTIN, LOW);
  }

  DateTime now = rtc.now();
  unsigned long unixTime = now.unixtime();
//verifica se o valor salvo, menos unixtime é maior ou igual tempo de espera proposto
  if ((unixTime - horaUltimoAcionamento) >= (tempoInativoPadrao) {
    hold(2000);
    // Tempo inativo atingido, aciona o relé pulsado
    digitalWrite(pinoRele, HIGH);
    hold(tempoAtivoPadrao * 1000);  // Mantém o relé acionado pelo tempo ativo
    if (debug) Serial.print("rele ligado ");
    digitalWrite(pinoRele, LOW);

    // Atualiza a hora do último acionamento
    horaUltimoAcionamento = unixTime;

    // Salva a hora do último acionamento na EEPROM
    salvarEEPROM(horaUltimoAcionamento);
  }

  // Aguarda um curto período antes de verificar novamente
  hold(1000);
  yield();
}

// Função para salvar a hora do último acionamento na EEPROM
void salvarEEPROM(unsigned long hora) {
  if (debug) Serial.print("valor salvo ");
  Serial.println(hora);
  EEPROM.put(0, hora);
}

// Função para ler a hora do último acionamento da EEPROM
unsigned long lerEEPROM() {
  unsigned long hora;
  EEPROM.get(0, hora);

  // Aciona o LED embutido por 300 milissegundos
  digitalWrite(LED_BUILTIN, HIGH);
  hold(400);
  digitalWrite(LED_BUILTIN, LOW);

  if (debug) Serial.print("Valor lido: ");
  Serial.println(hora);
  return hora;
}

