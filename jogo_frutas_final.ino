/*
 * ============================================================
 *  POMAR DA MEMÓRIA
 *  Arduino Uno | 1x MFRC522 | YX5300 | 1 Botão
 * ============================================================
 *
 *  PINAGEM:
 *    MFRC522:
 *      SDA  → D10
 *      SCK  → D13
 *      MOSI → D11
 *      MISO → D12
 *      RST  → D5
 *      VCC  → 3.3V
 *      GND  → GND
 *
 *    YX5300:
 *      TX   → D4  (conecta no RX do módulo)
 *      RX   → D3  (conecta no TX do módulo)
 *      VCC  → 5V
 *      GND  → GND
 *
 *    Botão:
 *      Um lado → D2
 *      Outro   → GND
 *
 * ============================================================
 *  ARQUIVOS DE ÁUDIO NO CARTÃO SD (pasta /01/):
 *    001.mp3 → Boas-vindas
 *    002.mp3 → Tutorial
 *    003.mp3 → "Não foi dessa vez, tente novamente!"
 *    004.mp3 → "Maçã"
 *    005.mp3 → "Abacaxi"
 *    006.mp3 → "Banana"
 *    007.mp3 → "Morango"
 *    008.mp3 → "Pêra"
 *    009.mp3 → "Parabéns! Você completou o desafio!"
 * ============================================================
 */

#include <SPI.h>
#include <MFRC522.h>
#include <SoftwareSerial.h>

#define SS_PIN   10
#define RST_PIN   5
#define BTN_PIN   2
#define MP3_RX    4
#define MP3_TX    3

#define TRACK_BOASVINDAS  9
#define TRACK_TUTORIAL    5
#define TRACK_ERROU       8
#define TRACK_MACA        4
#define TRACK_ABACAXI     6
#define TRACK_BANANA      7
#define TRACK_MORANGO     3
#define TRACK_PERA        2
#define TRACK_ACERTOU     1
#define TRACK_ENCERRADO   10

#define MAX_TENTATIVAS    3

MFRC522 rfid(SS_PIN, RST_PIN);
SoftwareSerial mp3Serial(MP3_RX, MP3_TX);

// ─── Frutas cadastradas ───────────────────────────────────
struct Fruta {
  byte uid[4];
  const char* nome;
  uint8_t track;
};

const uint8_t NUM_FRUTAS = 5;
Fruta frutas[NUM_FRUTAS] = {
  {{0xFF, 0x0F, 0xBF, 0x27}, "Maca",    TRACK_MACA},     // substitua os UIDs!
  {{0xFF, 0x0F, 0x0E, 0xED}, "Abacaxi", TRACK_ABACAXI},
  {{0xFF, 0x0F, 0xE6, 0x26}, "Banana",  TRACK_BANANA},
  {{0xFF, 0x0F, 0x9E, 0x26}, "Morango", TRACK_MORANGO},
  {{0xFF, 0x0F, 0x77, 0x27}, "Pera",    TRACK_PERA},
};

// ─── Estado do jogo ───────────────────────────────────────
enum Estado { AGUARDANDO_INICIO, AGUARDANDO_COMECAR, JOGANDO };
Estado estado = AGUARDANDO_INICIO;

uint8_t sequencia[3];
uint8_t respostas[3];
uint8_t contRespostas  = 0;
uint8_t tentativas     = 0;

bool     btnAnterior    = HIGH;
uint32_t btnUltimoTempo = 0;

// ─── MP3 ──────────────────────────────────────────────────
void mp3Cmd(byte cmd, byte p1 = 0, byte p2 = 0) {
  byte buf[8] = {0x7E, 0xFF, 0x06, cmd, 0x00, p1, p2, 0xEF};
  for (uint8_t i = 0; i < 8; i++) mp3Serial.write(buf[i]);
  delay(30);
}

void mp3Play(uint8_t track)   { mp3Cmd(0x03, 0x00, track); }
void mp3Volume(uint8_t vol)   { mp3Cmd(0x06, 0x00, vol);   }  // max = 30

// Toca uma faixa e espera ms milissegundos antes de continuar
void mp3PlayEsperar(uint8_t track, uint16_t ms) {
  mp3Play(track);
  delay(ms);
}

// ─── Utilitários ──────────────────────────────────────────
bool uidIgual(byte* lido, byte* cadastrado) {
  for (byte i = 0; i < 4; i++)
    if (lido[i] != cadastrado[i]) return false;
  return true;
}

int8_t identificarFruta(byte* uid) {
  for (uint8_t i = 0; i < NUM_FRUTAS; i++)
    if (uidIgual(uid, frutas[i].uid)) return (int8_t)i;
  return -1;
}

void sortearSequencia() {
  bool usado[NUM_FRUTAS] = {false};
  for (uint8_t i = 0; i < 3; i++) {
    uint8_t idx;
    do { idx = random(NUM_FRUTAS); } while (usado[idx]);
    sequencia[i] = idx;
    usado[idx] = true;
  }
}

bool botaoPressionado() {
  bool btnAtual = digitalRead(BTN_PIN);
  if (btnAtual == LOW && btnAnterior == HIGH &&
      (millis() - btnUltimoTempo) > 50) {
    btnUltimoTempo = millis();
    btnAnterior    = btnAtual;
    return true;
  }
  btnAnterior = btnAtual;
  return false;
}

// ─── Setup ────────────────────────────────────────────────
void setup() {
  Serial.begin(9600);
  mp3Serial.begin(9600);
  SPI.begin();
  rfid.PCD_Init();
  rfid.PCD_SetAntennaGain(rfid.RxGain_max);
  pinMode(BTN_PIN, INPUT_PULLUP);
  randomSeed(analogRead(A0));
  delay(1500);
  mp3Volume(30);
  delay(200);
  Serial.println("Pronto. Aperte o botao para ouvir as instrucoes.");
}

// ─── Loop ─────────────────────────────────────────────────
void loop() {
  switch (estado) {

    case AGUARDANDO_INICIO:
      if (botaoPressionado()) {
        Serial.println("Tocando boas-vindas e tutorial...");
        mp3PlayEsperar(TRACK_BOASVINDAS, 7000);  // ajuste o tempo conforme o audio
        mp3PlayEsperar(TRACK_TUTORIAL,   17000);  // ajuste o tempo conforme o audio
        Serial.println("Aperte o botao para comecar.");
        estado = AGUARDANDO_COMECAR;
      }
      break;

    case AGUARDANDO_COMECAR:
      if (botaoPressionado()) iniciarRodada();
      break;

    case JOGANDO:
      verificarRFID();
      break;
  }
}

// ─── Iniciar rodada ───────────────────────────────────────
void iniciarRodada() {
  contRespostas = 0;
  tentativas    = 0;
  sortearSequencia();

  Serial.print("Sequencia sorteada: ");
  for (uint8_t i = 0; i < 3; i++) {
    Serial.print(frutas[sequencia[i]].nome);
    Serial.print(" ");
  }
  Serial.println();

  // Fala APENAS os nomes das 3 frutas sorteadas, em ordem
  for (uint8_t i = 0; i < 3; i++) {
    mp3PlayEsperar(frutas[sequencia[i]].track, 1800);
  }

  estado = JOGANDO;
  Serial.println("Aguardando frutas...");
}

// ─── Leitura RFID ────────────────────────────────────────
void verificarRFID() {
  rfid.PCD_Init();

  if (!rfid.PICC_IsNewCardPresent()) { delay(100); return; }
  if (!rfid.PICC_ReadCardSerial())   { delay(100); return; }

  byte* uid = rfid.uid.uidByte;

  Serial.print("UID lido: ");
  for (byte i = 0; i < 4; i++) {
    if (uid[i] < 0x10) Serial.print("0");
    Serial.print(uid[i], HEX);
    if (i < 3) Serial.print(", ");
  }
  Serial.println();

  int8_t idx = identificarFruta(uid);

  if (idx < 0) {
    Serial.println("Tag nao cadastrada, ignorando.");
    rfid.PICC_HaltA();
    rfid.PCD_StopCrypto1();
    return;
  }

  Serial.print("Fruta identificada: ");
  Serial.println(frutas[idx].nome);

  // Fala o nome da fruta aproximada
  mp3PlayEsperar(frutas[idx].track, 1800);

  respostas[contRespostas++] = (uint8_t)idx;

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();

  if (contRespostas == 3) {
    delay(500);
    avaliarResposta();
  }
}

// ─── Avaliação ────────────────────────────────────────────
void avaliarResposta() {
  bool correto = true;
  for (uint8_t i = 0; i < 3; i++) {
    if (respostas[i] != sequencia[i]) { correto = false; break; }
  }

  if (correto) {
    Serial.println("CORRETO! Parabens!");
    mp3PlayEsperar(TRACK_ACERTOU, 6000);
    delay(1000);
    Serial.println("Aperte o botao para jogar novamente.");
    estado = AGUARDANDO_COMECAR;

  } else {
    tentativas++;
    Serial.print("Errado. Tentativa ");
    Serial.print(tentativas);
    Serial.print("/");
    Serial.println(MAX_TENTATIVAS);

    mp3PlayEsperar(TRACK_ERROU, 4000);

    if (tentativas >= MAX_TENTATIVAS) {
      Serial.println("Tentativas esgotadas. Fim de jogo!");
      delay(500);
      mp3PlayEsperar(TRACK_ENCERRADO, 5000);
      Serial.println("Repetindo sequencia correta...");
      for (uint8_t i = 0; i < 3; i++) {
        mp3PlayEsperar(frutas[sequencia[i]].track, 1800);
      }
      delay(1000);
      Serial.println("Aperte o botao para comecar uma nova rodada.");
      estado = AGUARDANDO_COMECAR;

    } else {
      delay(500);
      Serial.println("Repetindo sequencia...");
      for (uint8_t i = 0; i < 3; i++) {
        mp3PlayEsperar(frutas[sequencia[i]].track, 1800);
      }
      contRespostas = 0;
      Serial.println("Tente novamente!");
    }
  }
}