#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>

LiquidCrystal_I2C lcd(0x27, 16, 2);  // Address 0x27 for a 16 chars and 2 line display

const int trigPin = D3;    // Trigger pin of ultrasonic sensor
const int echoPin = D4;    // Echo pin of ultrasonic sensor
const int buzzerPin = D5;  // Buzzer pin
const int buttonPin = D7;  // Button pin

unsigned long previousMillis = 0;
const long interval = 800;  // Interval for updating pushup count (milliseconds)

int pushupCount = 0;
bool countingPushups = false;

unsigned long lastPushupTime = 0;
unsigned long pushupTimeout = 6000;

const char* serverName = "192.168.74.26";
const String endpoint = "/register-flexao";

ESP8266WebServer server(80);

String nomeUsuario = "";

struct WiFiCredentials {
  char network[32];
  char password[64];
  char uid[64];
};

WiFiCredentials credentials;

void readCredentialsFromEEPROM() {
  EEPROM.begin(sizeof(credentials));
  EEPROM.get(0, credentials);
  EEPROM.end();
}

void saveCredentialsToEEPROM() {
  EEPROM.begin(sizeof(credentials));
  EEPROM.put(0, credentials);
  EEPROM.end();
}


void handleRoot() {
  String page = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'><style>";
  page += "body { font-family: Arial, sans-serif; margin: 0; padding: 0;}";
  page += "h1 { text-align: center;}";
  page += "form { max-width: 300px; margin: 20px auto; padding: 20px; border: 1px solid #ccc; border-radius: 5px; text-align: center; }";
  page += "input[type='text'], input[type='password'] { width: 100%; padding: 10px; margin: 5px 0; box-sizing: border-box; }";
  page += "input[type='submit'] { width: 100%; background-color: #4CAF50; color: white; padding: 10px; border: none; border-radius: 5px; cursor: pointer;}";
  page += "</style></head><body>";
  page += "<h1>Configuração da Rede WiFi</h1>";
  page += "<form action='/save' method='post'>";
  page += "Nome da Rede WiFi: <input type='text' name='network' value='" + String(credentials.network) + "'><br>";
  page += "Senha da Rede WiFi: <input type='password' name='password' value='" + String(credentials.password) + "'><br>";
  page += "Email: <input type='text' name='uid' value='" + String(credentials.uid) + "'><br>";
  page += "<input type='submit' value='Salvar'>";
  page += "</form></body></html>";
  server.send(200, "text/html", page);
}

void handleSave() {
  if (server.hasArg("network") && server.hasArg("password")) {
    String network = server.arg("network");
    String password = server.arg("password");
    String uid = server.arg("uid");
    network.toCharArray(credentials.network, sizeof(credentials.network));
    password.toCharArray(credentials.password, sizeof(credentials.password));
    uid.toCharArray(credentials.uid, sizeof(credentials.uid));
    saveCredentialsToEEPROM();
    server.send(200, "text/plain", "Configurações salvas com sucesso!");
    readCredentialsFromEEPROM();
    Serial.println(credentials.network);
    Serial.println(credentials.password);
    WiFi.disconnect();
    connect_wifi();
  } else {
    server.send(400, "text/plain", "Parâmetros ausentes");
  }
}

void connect_wifi() {
  WiFi.begin(credentials.network, credentials.password);
  for (int i = 0; i < 5; i++) {
    delay(1000);
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Wifi Connected");
      return;
    } else {
      Serial.println("Trying to connect");
    }
  }
  WiFi.softAP("esp8266", NULL);
  Serial.print("Endereço IP: ");
  Serial.println(WiFi.softAPIP());
}

void setup() {
  readCredentialsFromEEPROM();
  Serial.begin(115200);
  Serial.println(" ");
  Serial.println(credentials.network);
  Serial.println(credentials.password);

  Serial.println("");

  connect_wifi();

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP);

  lcd.init();
  lcd.backlight();

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Aperte o botao ");
  lcd.setCursor(0, 1);
  lcd.print("para iniciar");

  Serial.println("");
  Serial.print("Conectado à rede WiFi ");
  Serial.print("Endereço IP: ");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.on("/save", HTTP_POST, handleSave);

  server.begin();
  Serial.println("Servidor iniciado");
}

void enviar_requisicao(int quant_flexao, String email, String userId) {
  WiFiClient client;

  if (!client.connect(serverName, 4000)) {
    Serial.println("Connection failed.");
    return;
  }
  String postBody = "userId=" + userId + "&quantFlexoes=" + String(quant_flexao) + "&email=" + email;
  client.print(String("POST ") + endpoint + " HTTP/1.1\r\n" + "Host: " + serverName + "\r\n" + "Content-Type: application/x-www-form-urlencoded\r\n" + "Content-Length: " + String(postBody.length()) + "\r\n" + "Connection: close\r\n\r\n" + postBody);
  Serial.println("Request sent.");

  while (client.connected()) {
    if (client.available()) {
      String line = client.readStringUntil('\r');
      Serial.print(line);
    }
  }

  Serial.println("Closing connection.");
  client.stop();
}


int last_distance = 0;
void loop() {

  unsigned long currentMillis = millis();

  if (digitalRead(buttonPin) == LOW) {
    countingPushups = true;
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Flexoes: 0");
  }

  if (countingPushups && currentMillis - previousMillis >= interval) {
    int distance = getDistance();


    if (distance < 30) {
      pushupCount++;
      lcd.clear();

      lcd.setCursor(0, 0);
      lcd.print("Flexoes: ");
      lcd.print(pushupCount);
      lastPushupTime = currentMillis;

      tone(buzzerPin, 1000, 50);
    }
    previousMillis = currentMillis;
    last_distance = distance;
  }
  if (currentMillis - lastPushupTime >= pushupTimeout) {
    String email = credentials.uid;
    String userId = "1";
    if (countingPushups && pushupCount > 1) {
      enviar_requisicao(pushupCount, email, userId);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Aperte o botao ");
      lcd.setCursor(0, 1);
      lcd.print("para reiniciar");
      countingPushups = false;
    }
    pushupCount = 0;
    lastPushupTime = 0;
  }
  server.handleClient();
}

int getDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  return pulseIn(echoPin, HIGH) / 58;
}
