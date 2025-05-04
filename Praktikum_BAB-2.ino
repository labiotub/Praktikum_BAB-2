#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <ESP32Servo.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

const int oneWireBus = 5; 
const int trigPin = 12;   
const int echoPin = 13;   
const int ledPin = 2;     

float md = 1;
float Vod = 0;

OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

#define WIFI_SSID "SSID"
#define WIFI_PASSWORD "PASSWORD"
#define API_KEY "APIKEY"
#define DATABASE_URL "URL"

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
Servo myServo;
int servoPin = 18;

unsigned long lastMillis = 0;
bool signupOK = false;

void setup() {
    Serial.begin(115200);

    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Menghubungkan ke Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }
    Serial.println("\nWi-Fi Terhubung!");

    config.api_key = API_KEY;
    config.database_url = DATABASE_URL;
    config.token_status_callback = tokenStatusCallback;

    if (Firebase.signUp(&config, &auth, "", "")) {
        Serial.println("Firebase SignUp berhasil!");
        signupOK = true;
    } else {
        Serial.println("Firebase SignUp gagal: " + String(config.signer.signupError.message.c_str()));
    }

    Firebase.begin(&config, &auth);
    Firebase.reconnectWiFi(true);

    pinMode(trigPin, OUTPUT);
    pinMode(echoPin, INPUT);
    pinMode(ledPin, OUTPUT);
    myServo.attach(servoPin);
    myServo.write(0);

    sensors.begin();
}

void loop() {
    long duration;
    float jarak;

    digitalWrite(trigPin, LOW);
    delayMicroseconds(2);
    digitalWrite(trigPin, HIGH);
    delayMicroseconds(10);
    digitalWrite(trigPin, LOW);
    duration = pulseIn(echoPin, HIGH);
    jarak = (duration * 0.034 / 2) * md + Vod;

    Serial.print("Jarak: ");
    Serial.print(jarak);
    Serial.println(" cm");

    sensors.requestTemperatures(); 
    float suhu = sensors.getTempCByIndex(0);

    Serial.print("Suhu: ");
    Serial.print(suhu);
    Serial.println(" °C");

    delay(1000);

    if (Firebase.ready() && signupOK && (millis() - lastMillis > 1000 || lastMillis == 0)) {
        lastMillis = millis();

        Firebase.RTDB.setFloat(&fbdo, "Data/Suhu", suhu);
        Firebase.RTDB.setFloat(&fbdo, "Data/Jarak", jarak);

        if (Firebase.RTDB.getString(&fbdo, "Data/LED")) {
            String ledStr = fbdo.stringData();
            ledStr.trim();                   
            ledStr.replace("\"", "");        
            ledStr.replace("\\", "");        

            Serial.println("Nilai LED setelah dibersihkan: " + ledStr);

            if (ledStr == "1") {
                digitalWrite(ledPin, HIGH);
                Serial.println("LED ON");
            } else if (ledStr == "0") {
                digitalWrite(ledPin, LOW);
                Serial.println("LED OFF");
            } else {
                Serial.println("Nilai LED tidak valid setelah pembersihan: " + ledStr);
            }
        } else {
           Serial.println("Gagal membaca LED: " + fbdo.errorReason());
        }

        if (Firebase.RTDB.getString(&fbdo, "Data/Servo")) {
            String servoStr = fbdo.stringData();
            servoStr.trim();
            servoStr.replace("\"", "");
            servoStr.replace("\\", "");
            int servoVal = servoStr.toInt();
            if (servoVal >= 0 && servoVal <= 100) {
                int servoPos = map(servoVal, 0, 100, 0, 180);
                myServo.write(servoPos);
                Serial.print("Servo dari Firebase (0–100): ");
                Serial.print(servoVal);
                Serial.print(" | Servo (0–180): ");
                Serial.println(servoPos);
            } else {
                Serial.println("Nilai Servo tidak valid: " + servoStr);
            }
        } else {
            Serial.println("Gagal membaca Servo: " + fbdo.errorReason());
        }
    }
}
