
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h>
#include <DHT.h>

// ===== WIFI =====
#define REDE_WIFI "SEU_WIFI"
#define SENHA_WIFI "SUA_SENHA"

// ===== FIREBASE =====
#define CHAVE_API "SUA_CHAVE_API"
#define URL_FIREBASE "https://metaclean-site-default-rtdb.firebaseio.com"
#define UID_UTILIZADOR "2fBIk7nUvieumX5Kd3Mj5qGE8le2"

// ===== SENSOR DHT11 =====
#define PIN_DHT 4
#define TIPO_DHT DHT11

DHT dht(PIN_DHT, TIPO_DHT);

// ===== SENSOR ULTRASSOM =====
#define TRIG 5
#define ECHO 18

// ===== SENSOR DE GAS MQ-5 =====
#define SENSOR_GAS 34

// ===== LEDS E BUZZER =====
#define LED_VERMELHO 26
#define LED_VERDE 27
#define BUZZER 25

// ===== LIMITES =====
#define LIMITE_TEMPERATURA 50
#define LIMITE_DISTANCIA 20
#define LIMITE_GAS 1800

// ===== FIREBASE =====
FirebaseData dadosFirebase;
FirebaseAuth autenticacao;
FirebaseConfig configuracao;

// ===== TEMPO DE ENVIO =====
unsigned long tempoAnterior = 0;
const unsigned long intervaloEnvio = 30000;

// ===== DISTANCIA =====
float medirDistancia() {
  digitalWrite(TRIG, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG, LOW);

  long duracao = pulseIn(ECHO, HIGH, 30000);

  if (duracao == 0) {
    return -1;
  }

  return duracao * 0.0343 / 2;
}

void setup() {

  Serial.begin(115200);

  // Inicialização dos sensores
  dht.begin();

  pinMode(TRIG, OUTPUT);
  pinMode(ECHO, INPUT);

  pinMode(LED_VERMELHO, OUTPUT);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(BUZZER, OUTPUT);

  pinMode(SENSOR_GAS, INPUT);

  analogReadResolution(12);

  // Conexão Wi-Fi
  WiFi.begin(REDE_WIFI, SENHA_WIFI);

  Serial.print("Conectando ao Wi-Fi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWi-Fi conectado!");

  // Configuração Firebase
  configuracao.api_key = CHAVE_API;
  configuracao.database_url = URL_FIREBASE;

  if (Firebase.signUp(&configuracao, &autenticacao, "", "")) {
    Serial.println("Firebase autenticado!");
  } else {
    Serial.printf("Erro Firebase: %s\n",
                  configuracao.signer.signupError.message.c_str());
  }

  Firebase.begin(&configuracao, &autenticacao);
  Firebase.reconnectWiFi(true);

  Serial.println("Sistema iniciado!");
}

void loop() {

  // ===== LEITURA DOS SENSORES =====

  float temperatura = dht.readTemperature();
  float umidade = dht.readHumidity();

  float distancia = medirDistancia();

  int gas = analogRead(SENSOR_GAS);

  // ===== VERIFICAÇÃO DOS ALERTAS =====

  bool alertaTemperatura =
    !isnan(temperatura) && temperatura > LIMITE_TEMPERATURA;

  bool alertaDistancia =
    distancia > LIMITE_DISTANCIA;

  bool alertaGas =
    gas > LIMITE_GAS;

  bool alertaGeral =
    alertaTemperatura || alertaDistancia || alertaGas;

  // ===== LEDS E BUZZER =====

  if (alertaGeral) {

    digitalWrite(LED_VERMELHO, HIGH);
    digitalWrite(LED_VERDE, LOW);
    digitalWrite(BUZZER, HIGH);

  } else {

    digitalWrite(LED_VERMELHO, LOW);
    digitalWrite(LED_VERDE, HIGH);
    digitalWrite(BUZZER, LOW);

  }

  // ===== MOSTRAR INFORMACOES =====

  Serial.println("\n========== SENSORES ==========");

  if (!isnan(temperatura)) {
    Serial.print("Temperatura: ");
    Serial.print(temperatura);
    Serial.println(" °C");
  } else {
    Serial.println("Temperatura: Erro de leitura");
  }

  if (!isnan(umidade)) {
    Serial.print("Umidade: ");
    Serial.print(umidade);
    Serial.println(" %");
  } else {
    Serial.println("Umidade: Erro de leitura");
  }

  if (distancia >= 0) {
    Serial.print("Distancia: ");
    Serial.print(distancia);
    Serial.println(" cm");
  } else {
    Serial.println("Distancia: Sem leitura");
  }

  Serial.print("Nivel do gas (MQ-5): ");
  Serial.println(gas);

  Serial.println("\n========== ALERTAS ==========");

  Serial.print("Temperatura: ");
  Serial.println(alertaTemperatura ? "ALERTA!" : "NORMAL");

  Serial.print("Distancia: ");
  Serial.println(alertaDistancia ? "ALERTA!" : "NORMAL");

  Serial.print("Gas: ");
  Serial.println(alertaGas ? "ALERTA!" : "NORMAL");

  Serial.print("Sistema geral: ");
  Serial.println(alertaGeral ? "PERIGO!" : "NORMAL");

  Serial.println("=============================");

  // ===== ENVIAR PARA O FIREBASE A CADA 30 SEGUNDOS =====

  if (millis() - tempoAnterior >= intervaloEnvio ||
      tempoAnterior == 0) {

    tempoAnterior = millis();

    if (Firebase.ready()) {

      String caminho = "dispositivos/" + String(UID_UTILIZADOR);

      bool sucesso = true;

      if (!isnan(temperatura)) {
        sucesso &= Firebase.RTDB.setFloat(
          &dadosFirebase, caminho + "/temperatura", temperatura);
      }

      if (!isnan(umidade)) {
        sucesso &= Firebase.RTDB.setFloat(
          &dadosFirebase, caminho + "/umidade", umidade);
      }

      if (distancia >= 0) {
        sucesso &= Firebase.RTDB.setFloat(
          &dadosFirebase, caminho + "/distancia", distancia);
      }

      sucesso &= Firebase.RTDB.setInt(
        &dadosFirebase, caminho + "/gas", gas);

      sucesso &= Firebase.RTDB.setBool(
        &dadosFirebase, caminho + "/alertaTemperatura", alertaTemperatura);

      sucesso &= Firebase.RTDB.setBool(
        &dadosFirebase, caminho + "/alertaDistancia", alertaDistancia);

      sucesso &= Firebase.RTDB.setBool(
        &dadosFirebase, caminho + "/alertaGas", alertaGas);

      sucesso &= Firebase.RTDB.setBool(
        &dadosFirebase, caminho + "/alertaGeral", alertaGeral);

      if (sucesso) {
        Serial.println("Dados enviados ao Firebase com sucesso!");
      } else {
        Serial.print("Erro ao enviar: ");
        Serial.println(dadosFirebase.errorReason());
      }

    } else {
      Serial.println("Firebase nao esta pronto!");
    }
  }

  delay(1000);
}
