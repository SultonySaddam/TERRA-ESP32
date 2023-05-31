#include <Arduino.h>
#include <ArduinoJson.h>
#include <EEPROM.h>
#include <HTTPClient.h>
#include <LiquidCrystal_I2C.h>
#include <PubSubClient.h>
#include <RTClib.h>
#include <SPI.h>
#include <Servo.h>
#include <WiFiManager.h>

#define START_LAMPU 8
#define FINISH_LAMPU 19
#define START_MAKAN_JAM 12
#define START_MAKAN_JAM2 16
#define START_MAKAN_MENIT 3
#define START_MAKAN_DETIK_AWAL 1
#define START_MAKAN_DETIK_AKHIR 3
#define LED 2
#define SERVO_PIN 18
const int relay4 = 19;

// MQTT
const int mqttPort = 1883;
const char* mqttUser = "";
const char* mqttPassword = "";
const char* TopicSchedule = "TERRARIUM/SCHEDULE";
const char* TopicStatusPair = "TERRARIUM/STATUS";
const char* TopicPublishToServer = "TERRARIUM/PUBLISH";
const char* mqttServer = "test.mosquitto.org";

String BASE_URL = "http://192.168.226.128:8000";

// LCD DISPLAY
int displayCondition = 1;
// 1 data makan
// 2 data air
// 3 suhu
// 4 kelembaban
// 5 data Lampu

char daysOfTheWeek[7][12] = {"Minggu", "Senin", "Selasa", "Rabu",
                             "Kamis",  "Jumat", "Sabtu"};

int tanggal, bulan, tahun, jam, menit, detik;

// variable millis
unsigned long makanTimeStamp = 0;
unsigned long displaytimestamp = 0;
int displayinterval = 5000;
int lamaBuka = 5000;

// Save Jam to EEPROM
unsigned long timeInSeconds = jam * 3600 + menit * 60 + detik;

String waktu = String(timeInSeconds);
String dataIN;
String dataId;
String dataPin;
String isLampAvailable;
String hari;
String StatusAlat;
boolean isMakan = false;
RTC_DS3231 rtc;
Servo servoMotor;
LiquidCrystal_I2C lcd(0x27, 20, 4);

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// MQTT
void connectToWiFi();
void connectToMqtt();
void callback(char* topic, byte* payload, unsigned int length);

void serial() {
  if (Serial2.available() > 0) {
    dataIN = Serial2.readStringUntil('!');
    Serial.println(dataIN);
  }
}

String stringSpliter(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length() - 1;

  for (int i = 0; i <= maxIndex & found <= index; i++) {
    if (data.charAt(i) == separator || i == maxIndex) {
      found++;
      strIndex[0] = strIndex[1] + 1;
      strIndex[1] = (i == maxIndex) ? i + 1 : i;
    }
  }

  return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

String readFromEEPROM(int addrOffset) {
  int newStrLen = EEPROM.read(addrOffset);
  char data[newStrLen + 1];
  for (int i = 0; i < newStrLen; i++) {
    data[i] = EEPROM.read(addrOffset + 1 + i);
  }
  data[newStrLen] = '\0';
  return String(data);
}

void writeToEEPROM(int addrOffset, const String& strToWrite) {
  int len = strToWrite.length();
  Serial.println("Store new data to EEPROM");
  EEPROM.write(addrOffset, len);
  for (int i = 0; i < len; i++) {
    EEPROM.write(addrOffset + 1 + i, strToWrite[i]);
    EEPROM.commit();
  }
}

int DeviceId(String endpoint) {
  String url = BASE_URL + endpoint;
  Serial.print("URL: ");
  Serial.println(url);
  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  int httpCode = http.GET();
  Serial.print("HTTP STATUS: ");
  Serial.println(httpCode);
  Serial.print("Data: ");

  String message = http.getString();
  Serial.println(message);
  StaticJsonDocument<500> doc;  // Memory pool
  DeserializationError error = deserializeJson(doc, message);
  String idDevice = doc["data"]["deviceID"];
  String idPin = doc["data"]["devicePIN"];
  Serial.println(idDevice);
  Serial.println(idPin);
  Serial.print("err: ");
  Serial.println(error.f_str());
  writeToEEPROM(50, idDevice);
  writeToEEPROM(70, idPin);
  dataId = readFromEEPROM(50);
  dataPin = readFromEEPROM(70);
  Serial.print("data yang diinput = ");
  Serial.println(dataId);
  Serial.println(dataPin);

  http.end();
  return httpCode;
}

void lampu() {  // fix
  DateTime now = rtc.now();
  jam = now.hour(), DEC;
  menit = now.minute(), DEC;
  detik = now.second(), DEC;

  if ((jam >= START_LAMPU) && (jam < FINISH_LAMPU))  // nyala  jam>=8 jam < 19
  {
    digitalWrite(relay4, HIGH);
    Serial.println("Lampu nyala, ");
    isLampAvailable = "true";
  } else {
    digitalWrite(relay4, LOW);
    Serial.println("Lampu mati, ");
    isLampAvailable = "false";
  }
}
void pakan() {
  String FORMAT_JAM = String(jam) + ":" + String(menit);
  Serial.println(FORMAT_JAM);
  String mode = readFromEEPROM(1);
  if (mode == "AUTO" && (jam == START_MAKAN_JAM) &&
      (menit == START_MAKAN_MENIT) && (detik >= START_MAKAN_DETIK_AWAL) &&
      (detik < START_MAKAN_DETIK_AKHIR)) {
    isMakan = true;
    makanTimeStamp = millis();
    Serial.println("Buka");
    servoMotor.write(0);
  }
  if (mode == "AUTO" && (jam == START_MAKAN_JAM2) &&
      (menit == START_MAKAN_MENIT) && (detik >= START_MAKAN_DETIK_AWAL) &&
      (detik < START_MAKAN_DETIK_AKHIR)) {
    isMakan = true;
    makanTimeStamp = millis();
    Serial.println("Buka");
    servoMotor.write(0);
  }

  if (mode == "MANUAL") {
    String jam1 = readFromEEPROM(10);
    if (jam1 == FORMAT_JAM && (detik >= START_MAKAN_DETIK_AWAL) &&
        (detik < START_MAKAN_DETIK_AKHIR)) {
      isMakan = true;
      makanTimeStamp = millis();
      Serial.println("Buka");
      servoMotor.write(0);
    }
    String jam2 = readFromEEPROM(20);
    if (jam2 == FORMAT_JAM && (detik >= START_MAKAN_DETIK_AWAL) &&
        (detik < START_MAKAN_DETIK_AKHIR)) {
      isMakan = true;
      makanTimeStamp = millis();
      Serial.println("Buka");
      servoMotor.write(0);
    }

    if (isMakan && millis() > makanTimeStamp + lamaBuka) {
      Serial.println("Tutup");
      servoMotor.write(90);
      isMakan = false;
    }
  }
}

void LCD() {
  // Variable PAYLOAD LCD
  // Format = Temp#hum#isfoodavailable#iswateravailable
  // String displayjam = String(jam) + ":" + String(menit);
  String payload = dataIN;  // diganti dataIN;
  String foodavailable = stringSpliter(payload, '#', 2);
  String wateravailable = stringSpliter(payload, '#', 3);
  String temperature = stringSpliter(payload, '#', 0);
  String humidity = stringSpliter(payload, '#', 1);
  String StatusAlat = readFromEEPROM(80);
  if (StatusAlat == "true") {
    switch (displayCondition) {
      case 1:
        lcd.setCursor(0, 0);
        lcd.print("ID ALAT: " + dataId);
        lcd.setCursor(0, 1);
        if (foodavailable == "true") {
          lcd.print("MAKAN TERSEDIA");
        }
        if (foodavailable == "false") {
          lcd.print("MAKAN HABIS");
        }
        break;

      case 2:
        lcd.setCursor(0, 0);
        lcd.print("ID ALAT: " + dataId);
        lcd.setCursor(0, 1);
        if (wateravailable == "true") {
          lcd.print("AIR TERSEDIA");
        }
        if (wateravailable == "false") {
          lcd.print("AIR HABIS");
        }
        break;

      case 3:
        lcd.setCursor(0, 0);
        lcd.print("ID ALAT: " + dataId);
        lcd.setCursor(0, 1);
        lcd.print("TEMP:" + temperature + "C");
        break;

      case 4:
        lcd.setCursor(0, 0);
        lcd.print("ID ALAT" + dataId);
        lcd.setCursor(0, 1);
        lcd.print("HUM:" + humidity + "%");
        break;
      default:
        break;
    }
    if (millis() - displaytimestamp > displayinterval) {
      lcd.clear();
      displaytimestamp = millis();
      displayCondition++;

      if (displayCondition > 4) {
        displayCondition = 1;
      }
    }
  }
  if (StatusAlat == "false") {
    lcd.setCursor(0, 0);
    lcd.print("ID ALAT" + dataId);
    lcd.setCursor(0, 1);
    lcd.print("PIN ALAT: " + dataPin);
  }
}

void setup() {
  Serial.begin(9600);
  Serial2.begin(9600, SERIAL_8N1, 16, 17);
  EEPROM.begin(512);

  // MQTT
  connectToMqtt();

  lcd.init();
  lcd.backlight();

  servoMotor.attach(SERVO_PIN);  // attaches the servo on ESP32 pin
  servoMotor.write(90);
  WiFiManager wm;
  bool res;

  pinMode(relay4, OUTPUT);
  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);

  // Alamat 1: MODE
  // Alamat 2: JAM-1
  // Alamat 3: JAM-2
  writeToEEPROM(1, "MANUAL");
  writeToEEPROM(10, "12:2");
  writeToEEPROM(20, "15:00");
  writeToEEPROM(80, "true");

  Serial.print("MODE: ");
  Serial.println(readFromEEPROM(1));
  Serial.print("JAM -1: ");
  Serial.println(readFromEEPROM(10));
  Serial.print("JAM -2: ");
  Serial.println(readFromEEPROM(20));
  Serial.print("Status Alat: ");
  Serial.println(readFromEEPROM(80));

  res = wm.autoConnect("AutoConnectAP", "password");
  if (!res) {
    Serial.println("Failed to connect");
  } else {
    Serial.println("connected...yeey :)");
  }

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1)
      ;
  }

  if (rtc.lostPower()) {
    Serial.println("RTC lost power, lets set the time!");
    // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // following line sets the RTC to the date & time this sketch was compiled

    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
    //  This line sets the RTC with an explicit date & time, for example to
    //  set
  }
  Serial.println("Waiting Data");
  // if (!client.connected()) {
  //   reconnect();
  // }

  Serial.print("Status EEPROM: ");
  // // float TestAddress;
  // // EEPROM.get(0, TestAddress);
  dataId = readFromEEPROM(50);
  dataPin = readFromEEPROM(70);

  if (dataId == "" || dataPin == "") {
    Serial.print("EEPROM Kosong");
    DeviceId("/device/initialization");
  }

  else {
    Serial.println("EEPROM isi");
    dataId = readFromEEPROM(50);
    dataPin = readFromEEPROM(70);
    Serial.print("Isi EEPROM Adalah = ");
    Serial.print(dataId);
    Serial.println(dataPin);
  }
}

void loop() {
  // String data = "xKY3p7#32#70#false#false";
  // String data = ("xKY3p7#", dataIN, "#", isLampAvailable);
  // client.publish("PNJ/xKY3p7/TERRA", data.c_str());
  // Serial.print("Data: ");
  // Serial.println(data);
  serial();
  lampu();
  pakan();
  LCD();
  if (!mqttClient.connected()) {
    connectToMqtt();
  }

  mqttClient.loop();
  delay(1000);
}

// function connect to MQTT with PubSubClient Library
void connectToMqtt() {
  mqttClient.setServer(mqttServer, mqttPort);
  mqttClient.setCallback(callback);
  while (!mqttClient.connected()) {
    Serial.println("Connecting to PubSubClient MQTT...");

    if (mqttClient.connect("PNJ/ESP32/test", mqttUser, mqttPassword)) {
      Serial.println("Connected to PubSubClient MQTT broker");

      // Subscribe to the desired topic
      mqttClient.subscribe(TopicSchedule);
    } else {
      Serial.print("Failed to connect to MQTT broker, state: ");
      Serial.println(mqttClient.state());
      delay(2000);
    }
  }
}

// callback function to retrive payload from MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("Message arrived in topic: " + String(topic));

  Serial.print("Message: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  Serial.println("-----------------------");
}
//}