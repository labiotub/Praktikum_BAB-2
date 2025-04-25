#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <ESP32Servo.h>
#include <OneWire.h>
#include <DallasTemperature.h>

// Token & Firebase Helper
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

// **Definisi Pin Sensor**
const int oneWireBus = 5; 
const int trigPin = 12;   // HC-SR04 Trigger
const int echoPin = 13;   // HC-SR04 Echo
const int ledPin = 2;     

// **Konstanta Kalibrasi**
float md = 1;  // Faktor kalibrasi jarak
float Vod = 0; // Offset jarak

OneWire oneWire(oneWireBus);
DallasTemperature sensors(&oneWire);

// **WiFi & Firebase**
#define WIFI_SSID "Lab_IoT"
#define WIFI_PASSWORD "L@bi0t63"
#define API_KEY "AIzaSyDcDUMEbFOHU04Q9oYcmR-JzCNNQAQYnRo"
#define DATABASE_URL "https://praktiot-default-rtdb.firebaseio.com/"

// **Deklarasi Firebase & Servo**
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;
Servo myServo;
int servoPin = 18;

unsigned long lastMillis = 0;
bool signupOK = false;

void setup() {
    Serial.begin(115200);

    // **Menghubungkan ke Wi-Fi**
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.print("Menghubungkan ke Wi-Fi");
    while (WiFi.status() != WL_CONNECTED) {
        Serial.print(".");
        delay(500);
    }
    Serial.println("\nWi-Fi Terhubung!");

    // **Konfigurasi Firebase**
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

    // **Inisialisasi Pin & Servo**
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

    // **Mengukur Jarak dengan HC-SR04**
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

    // **Membaca Suhu dari DS18B20**
    sensors.requestTemperatures(); 
    float suhu = sensors.getTempCByIndex(0);

    Serial.print("Suhu: ");
    Serial.print(suhu);
    Serial.println(" °C");

    delay(1000);

    // **Kirim Data ke Firebase Setiap 10 Detik**
    if (Firebase.ready() && signupOK && (millis() - lastMillis > 1000 || lastMillis == 0)) {
        lastMillis = millis();

        if (Firebase.RTDB.setFloat(&fbdo, "Data/Suhu", suhu)) {
            Serial.println("Suhu terkirim!");
        } else {
            Serial.println("Gagal mengirim suhu: " + fbdo.errorReason());
        }

        if (Firebase.RTDB.setFloat(&fbdo, "Data/Jarak", jarak)) {
            Serial.println("Jarak terkirim!");
        } else {
            Serial.println("Gagal mengirim jarak: " + fbdo.errorReason());
        }

        // **Ambil Data LED dari Firebase**
        if (Firebase.RTDB.getString(&fbdo, "Data/LED")) {
            String ledState = fbdo.stringData();
            if (ledState == "ON") {
                digitalWrite(ledPin, HIGH);
                Serial.println("LED ON");
            } else {
                digitalWrite(ledPin, LOW);
                Serial.println("LED OFF");
            }
        } else {
            Serial.println("Gagal membaca LED: " + fbdo.errorReason());
        }

        // **Ambil Data Servo dari Firebase**
        // Ambil Data Servo dari Firebase (dalam rentang 0-255)
        if (Firebase.RTDB.getInt(&fbdo, "Data/Servo")) {
            int servoValue255 = fbdo.to<int>();
            if (servoValue255 >= 0 && servoValue255 <= 255) {
                int servoPos = map(servoValue255, 0, 255, 0, 180); // Konversi ke 0–180
                myServo.write(servoPos);
                Serial.print("Servo dari Firebase (0–255): ");
                Serial.print(servoValue255);
                Serial.print(" | Servo (0–180): ");
                Serial.println(servoPos);
    }
} else {
    Serial.println("Gagal membaca Servo: " + fbdo.errorReason());
}

    }
}
