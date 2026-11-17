#include <WiFi.h>
#include <HTTPClient.h>
#include <DHT.h>

// ==========================================================
// PREENCHA COM OS DADOS DA REDE WI-FI E IP DO SEU COMPUTADOR
const char* ssid     = "NOME_DO_SEU_WIFI";
const char* password = "SENHA_DO_SEU_WIFI";
// Descubra o IP digitando 'ipconfig' no terminal do seu PC
const char* serverUrl = "http://192.168.1.13:8000/api/telemetria"; 
// ==========================================================

// --- Configuração dos Pinos ---
#define DHTPIN       4
#define DHTTYPE      DHT22
#define LDR_PIN      1 
#define POT_PIN      2
#define BUTTON_PIN   5
#define BUZZER_PIN   18

#define RED_PIN      15
#define GREEN_PIN    16
#define BLUE_PIN     17

// --- Parâmetros e Limiares ---
#define LIMITE_ESCURO 2000 // Ajustar conforme calibração do LDR (0 a 4095)

// --- Objetos ---
DHT dht(DHTPIN, DHTTYPE);

// --- Variáveis Globais de Controle ---
volatile bool sistemaHabilitadoBotao = true; 
volatile unsigned long ultimoDebounce = 0;
const unsigned long tempoDebounce = 250; // ms

bool tempAtiva = true;
bool ldrAtivo = true;
bool buzzerAtivo = true;

unsigned long ultimoEnvioAPI = 0;
const unsigned long intervaloEnvioAPI = 3000; // Envia para o banco a cada 3 segundos

// --- Interrupção Externa do Botão ---
void IRAM_ATTR isrBotao() {
  unsigned long agora = millis();
  if (agora - ultimoDebounce > tempoDebounce) {
    sistemaHabilitadoBotao = !sistemaHabilitadoBotao;
    ultimoDebounce = agora;
  }
}

// --- Funções Auxiliares do LED RGB ---
void definirCorRGB(uint8_t r, uint8_t g, uint8_t b) {
  analogWrite(RED_PIN, r);
  analogWrite(GREEN_PIN, g);
  analogWrite(BLUE_PIN, b);
}

void apagarLED() {
  definirCorRGB(0, 0, 0);
}

// --- Função de Envio para a API (FastAPI -> InfluxDB) ---
void enviarDadosParaAPI(float temp, float umid, int luz, bool ledLigado, String nomeCor, bool botaoEstado) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "application/json");

    // Monta o JSON manualmente (não precisa instalar biblioteca extra)
    String payload = "{";
    payload += "\"temperatura\":" + String(isnan(temp) ? 0.0 : temp, 2) + ",";
    payload += "\"umidade\":" + String(isnan(umid) ? 0.0 : umid, 2) + ",";
    payload += "\"luminosidade\":" + String(luz) + ",";
    payload += "\"estado_led\":\"" + String(ledLigado ? "ON" : "OFF") + "\",";
    payload += "\"cor_led\":\"" + nomeCor + "\",";
    payload += "\"botao\":" + String(botaoEstado ? "true" : "false");
    payload += "}";

    int httpCode = http.POST(payload);
    if (httpCode > 0) {
      Serial.print(" | API HTTP: ");
      Serial.print(httpCode);
    } else {
      Serial.print(" | Erro API: ");
      Serial.print(http.errorToString(httpCode));
    }
    http.end();
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  // Inicialização de Pinos
  pinMode(RED_PIN, OUTPUT);
  pinMode(GREEN_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  // Configuração da Interrupção Externa
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), isrBotao, FALLING);

  // Inicialização dos Sensores
  dht.begin();

  // Conexão Wi-Fi
  Serial.print("Conectando ao Wi-Fi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWi-Fi conectado! IP do ESP32: " + WiFi.localIP().toString());
}

void loop() {
  // 1. Leitura dos Sensores
  float temp = dht.readTemperature(); 
  float umid = dht.readHumidity();
  int ldrVal = analogRead(LDR_PIN);    
  int potVal = analogRead(POT_PIN);    

  bool emPerigoTemp = false;
  if (!isnan(temp)) {
    if (temp < 0.0 || temp > 25.0) { 
      emPerigoTemp = true;
    }
  }

  // 2. Lógica do Buzzer
  if (emPerigoTemp && buzzerAtivo && sistemaHabilitadoBotao) {
    digitalWrite(BUZZER_PIN, HIGH); 
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }

  // 3. Determinação da Cor pelo Potenciômetro
  uint8_t potR = 0, potG = 0, potB = 0;
  String nomeCor = "Vermelho";
  if (potVal < 1365) {
    potR = 255; potG = 0; potB = 0;
    nomeCor = "Vermelho";
  } else if (potVal < 2730) {
    potR = 255; potG = 255; potB = 0;
    nomeCor = "Amarelo";
  } else {
    potR = 0; potG = 0; potB = 255;
    nomeCor = "Azul";
  }

  // 4. Lógica de Condição para Acionamento do LED RGB
  bool ambienteEscuro = (ldrVal < LIMITE_ESCURO);
  bool tempPermitida = (!emPerigoTemp);

  bool condicaoLdr = ldrAtivo ? ambienteEscuro : true;
  bool condicaoTemp = tempAtiva ? tempPermitida : true;

  bool ledDeveLigar = sistemaHabilitadoBotao && condicaoLdr && condicaoTemp;

  if (ledDeveLigar) {
    definirCorRGB(potR, potG, potB);
  } else {
    apagarLED();
    nomeCor = "Apagado";
  }

  // 5. Atualização do Monitor Serial
  Serial.print("Temperatura: ");
  if (isnan(temp)) {
    Serial.print("Erro de Leitura");
  } else {
    Serial.print(temp);
    Serial.print(" °C"); 
  }

  Serial.print(" | Estado LED: ");
  Serial.print(ledDeveLigar ? "ON" : "OFF"); 

  if (emPerigoTemp && tempAtiva) {
    Serial.print(" | Perigo! Desligar!"); 
  }

  // 6. Envio Periódico para a API / InfluxDB
  if (millis() - ultimoEnvioAPI >= intervaloEnvioAPI) {
    enviarDadosParaAPI(temp, umid, ldrVal, ledDeveLigar, nomeCor, sistemaHabilitadoBotao);
    ultimoEnvioAPI = millis();
  }

  Serial.println();
  delay(500);
}