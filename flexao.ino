#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
#include <ESP8266WiFi.h>


LiquidCrystal_I2C lcd(0x27, 16, 2);  // Address 0x27 for a 16 chars and 2 line display

const int trigPin = D3;  // Trigger pin of ultrasonic sensor
const int echoPin = D4;  // Echo pin of ultrasonic sensor
const int buzzerPin = D5; // Buzzer pin
const int buttonPin = D7; // Button pin

unsigned long previousMillis = 0;
const long interval = 800;  // Interval for updating pushup count (milliseconds)

int pushupCount = 0;
bool countingPushups = false;

unsigned long lastPushupTime = 0;
unsigned long pushupTimeout = 6000;

const char* serverName = "192.168.1.64"; 
const String endpoint = "/register-flexao";

void setup() {
  Serial.begin(115200);
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);
  pinMode(buzzerPin, OUTPUT);
  pinMode(buttonPin, INPUT_PULLUP); 
  
  lcd.init();                    
  lcd.backlight();                
  
  lcd.setCursor(0,0);
  lcd.print("Pushup Counter");
}

void enviar_requisicao(int quant_flexao, String email, String userId){
  WiFiClient client;

  if (!client.connect(serverName, 4000)) { 
    Serial.println("Connection failed.");
    return;
  }
  String postBody = "userId=" + userId + "&quantFlexoes=" + String(quant_flexao) + "&email=" + email;
  client.print(String("POST ") + endpoint + " HTTP/1.1\r\n" +
               "Host: " + serverName + "\r\n" +
               "Content-Type: application/x-www-form-urlencoded\r\n" +
               "Content-Length: " + String(postBody.length()) + "\r\n" + 
               "Connection: close\r\n\r\n" +
               postBody);
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
  }

  if (countingPushups && currentMillis - previousMillis >= interval) {
    int distance = getDistance();
    

    if (distance < 30 ) {
      pushupCount++;
      lcd.clear();

      lcd.setCursor(0,0);
      lcd.print("Flexoes: ");
      lcd.print(pushupCount);
      lastPushupTime = currentMillis;
    
      tone(buzzerPin, 1000, 50); 
    }
    previousMillis = currentMillis;
    last_distance = distance;
  }
  if (currentMillis - lastPushupTime >= pushupTimeout) {
    String email = "patrickteixeira2004@gmail.com";
    String userId = "1";
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Aperte o botao ");
    lcd.setCursor(0,1);
    lcd.print("para reiniciar");
    if (countingPushups && pushupCount > 1){
      enviar_requisicao(pushupCount, email, userId);
      countingPushups = false;
    }
    Serial.println(countingPushups);
    pushupCount = 0;
    lastPushupTime = 0; 
  }
}

int getDistance() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  return pulseIn(echoPin, HIGH) / 58;  
}