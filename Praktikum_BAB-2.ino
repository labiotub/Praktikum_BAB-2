#include <Arduino.h>
#include <WiFi.h>
#include <Firebase_ESP_Client.h>
#include <OneWire.h>

//Provide the token generation process info.
#include "addons/TokenHelper.h"
//Provide the RTDB payload printing info and other helper functions.
#include "addons/RTDBHelper.h"

// Definisikan pin yang terhubung ke sensor DS18B20
const int sensorPin = 15; // Hubungkan sensor ke pin 4 pada ESP32

// Definisikan pin trigger dan echo sensor
const int trigPin = 27;
const int echoPin = 26;

// Define LED pin as built-in LED
const int ledPin = LED_BUILTIN; // Built-in LED on ESP32 (typically GPIO 2)

//Input nilai kalibrasi jarak
float md = 1;
float Vod = 0;

//Input nilai kalibrasi suhu
float mt = 1;
float Vot = 0;

OneWire oneWire(sensorPin);
float temperature;

// Insert your network credentials
#define WIFI_SSID "Lab_IoT"
#define WIFI_PASSWORD "L@bi0t63"

// Insert Firebase project API Key
#define API_KEY "AIzaSyAViPIS3Fu7MQMaULbV00_SFf0iMNfpXTE"

// Insert RTDB URLefine the RTDB URL */
#define DATABASE_URL "https://prakiot-b9a26-default-rtdb.firebaseio.com/" 

//Define Firebase Data object
FirebaseData fbdo;

FirebaseAuth auth;
FirebaseConfig config;

unsigned long sendDataPrevMillis = 0;
bool signupOK = false;

void setup(){
  Serial.begin(9600);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(300);
  }
  Serial.println();
  Serial.print("Connected with IP: ");
  Serial.println(WiFi.localIP());
  Serial.println();

  /* Assign the api key (required) */
  config.api_key = API_KEY;

  /* Assign the RTDB URL (required) */
  config.database_url = DATABASE_URL;

  /* Sign up */
  if (Firebase.signUp(&config, &auth, "", "")){
    Serial.println("ok");
    signupOK = true;
  }
  else{
    Serial.printf("%s\n", config.signer.signupError.message.c_str());
  }

  /* Assign the callback function for the long running token generation task */
  config.token_status_callback = tokenStatusCallback; //see addons/TokenHelper.h
  
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
  pinMode(trigPin, OUTPUT); // Atur pin trigger sebagai OUTPUT
  pinMode(echoPin, INPUT); // Atur pin echo sebagai INPUT
  
  pinMode(ledPin, OUTPUT); // Atur pin LED sebagai OUTPUT
}

void loop(){

  long duration;
  float d; // Variabel untuk menyimpan durasi dan jarak
  
  // Kirim sinyal ultrasonik
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  
  // Baca waktu yang diperlukan untuk sinyal kembali
  duration = pulseIn(echoPin, HIGH);
  
  // Hitung jarak berdasarkan waktu yang diperlukan
  d = (duration * 0.034 / 2) * md + Vod;
  
  // Mencetak hasil ke Serial Monitor
  Serial.print("Jarak: ");
  Serial.print(d);
  Serial.println(" cm");
  
  byte data[12];
  byte addr[8];

  // Cari alamat sensor DS18B20
  if (!oneWire.search(addr)) {
    oneWire.reset_search();
    delay(250);
    return;
  }

  // Periksa apakah alamat sensor valid
  if (OneWire::crc8(addr, 7) != addr[7]) {
    Serial.println("CRC salah!");
    return;
  }

  // Periksa apakah sensor adalah DS18B20
  if (addr[0] != 0x28) {
    Serial.println("Alamat yang tidak valid");
    return;
  }

  // Mulai konversi suhu
  oneWire.reset();
  oneWire.select(addr);
  oneWire.write(0x44, 1);

  // Tunggu sampai konversi selesai
  delay(1000);

  // Mulai komunikasi dengan sensor
  oneWire.reset();
  oneWire.select(addr);
  oneWire.write(0xBE);

  // Baca data dari sensor
  for (int i = 0; i < 9; i++) {
    data[i] = oneWire.read();
  }

  // Konversi data suhu ke dalam bentuk float
  int16_t raw = (data[1] << 8) | data[0];
  float t = ((float)raw / 16.0)* mt + Vot;

  // Tampilkan data suhu ke Serial Monitor
  Serial.print("Suhu: ");
  Serial.print(t);
  Serial.println(" derajat Celsius");

  delay(1000); // Tunggu 1 detik sebelum membaca kembali
  
  if (Firebase.ready() && signupOK && (millis() - sendDataPrevMillis > 15000 || sendDataPrevMillis == 0)){
    sendDataPrevMillis = millis();
    // Write an Int number on the database path test/int (suhu)
    if (Firebase.RTDB.setInt(&fbdo, "Data/Suhu", t)){
       Serial.println("PASSED");
       Serial.println("PATH: " + fbdo.dataPath());
       Serial.println("TYPE: " + fbdo.dataType());
     }
     else {
       Serial.println("FAILED");
       Serial.println("REASON: " + fbdo.errorReason());
     }

     // Write an Int number on the database path test/int (jarak)
     if (Firebase.RTDB.setInt(&fbdo, "Data/Jarak", d)){
       Serial.println("PASSED");
       Serial.println("PATH: " + fbdo.dataPath());
       Serial.println("TYPE: " + fbdo.dataType());
     }
     else {
       Serial.println("FAILED");
       Serial.println("REASON: " + fbdo.errorReason());
     }

     // Read LED state from Firebase (0 for OFF, 1 for ON)
     if (Firebase.RTDB.getInt(&fbdo, "Data/LED")){
       int ledState = fbdo.intData();
       if (ledState == 1){
         digitalWrite(ledPin, HIGH); // Turn LED ON
         Serial.println("LED ON");
       }
       else {
         digitalWrite(ledPin, LOW); // Turn LED OFF
         Serial.println("LED OFF");
       }
     }
     else {
       Serial.println("FAILED to get LED state");
       Serial.println("REASON: " + fbdo.errorReason());
     }
  }
}
