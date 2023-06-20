#include <SPI.h>
#include <MFRC522.h>
#include <WiFiManager.h>
#include <BlynkSimpleEsp32.h>
#include <Adafruit_Fingerprint.h>
#include <Firebase_ESP_Client.h>
#include <addons/TokenHelper.h>
#include <addons/RTDBHelper.h> 
#include "MB_NTP.h"

#define SS_PIN       21   // ESP32 pin GPIO21
#define RST_PIN      22   // ESP32 pin GPIO22
#define FINGERPRINT_PASSWORD 0x0000 // Change to your desired password
#define FINGERPRINT_RX 16 // Change to the RX pin connected to the fingerprint module
#define FINGERPRINT_TX 17 // Change to the TX pin connected to the fingerprint module
#define SOLENOID_PIN 25   // ESP32 pin GPIO23
#define BUTTON_PIN   15    // Button pin (change it to the appropriate pin)
#define MAX_FINGERPRINTS 5 // Maximum number of fingerprints to be enrolled
#define API_KEY "AIzaSyARLQFuXpCWEydTItZLFEI6kpBnyuafKv0"
#define DATABASE_URL "https://locklogin-c6186-default-rtdb.asia-southeast1.firebasedatabase.app/"  // Database URL
#define USER_EMAIL "esp32test@gmail.com"
#define USER_PASSWORD "12345678"

char auth[] = "2_dwrI3ME-YuObdM3nLl0Gc8-WkpE0BV";  // Blynk authentication token

FirebaseData fbdoWrite; 
FirebaseData stream;
FirebaseData fbdoRead;
FirebaseData fbdoRead2;
FirebaseData fbdoRead3;
FirebaseAuth authh;
FirebaseConfig config;
FirebaseJson json;

unsigned long sendDataPrevMillis = 0; 
int count = 0;

String parentPath = "/lock";
String childPath[4] = {"/newfp", "/resetfp", "/value", "var"}; 

MFRC522 mfrc522(SS_PIN, RST_PIN);
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&Serial2);

int buttonState = HIGH;           // HIGH means the button is not pressed initially
int fingerprintCount = 0;         // Counter for enrolled fingerprints
bool enrollMode = false;          // Flag to indicate if enrollment mode is active
volatile bool dataChanged = false;

void unlock();
void getFingerprintID();
void enrollFingerprint();
void resetFingerprints(); // Function to reset fingerprint database

void streamCallback (MultiPathStream Stream) {
  size_t numChild = sizeof(childPath) / sizeof(childPath[0]);

  // for (size_t i = 0; i < numChild; i++) {
  //   if (Stream.get(childPath[i])) {
  //     Serial.printf("path: %s, event: %s, type: %s, value: %s%s", 
  //     Stream.dataPath.c_str(), Stream.eventType.c_str(), Stream.type.c_str(), Stream.value.c_str(), i < numChild - 1 ? "\n" : "");
  //   }
  // }
  if (Stream.get(childPath[2])) {
    if (strcmp(Stream.value.c_str(), "true") == 0) {
      Serial.println("Unlocking");
      digitalWrite(SOLENOID_PIN, HIGH);
    } else {
      digitalWrite(SOLENOID_PIN, LOW);
      Serial.println("Locked");
    }
  }

  if (Stream.get(childPath[0])) {
    if (strcmp(Stream.value.c_str(), "true") == 0) {
      Serial.println("Enrolling fingerprint");
      enrollMode = true;
      enrollFingerprint();
      enrollMode = false;
      Firebase.RTDB.setBool(&fbdoWrite, "/lock/newfp", false);
    }
  }

  if (Stream.get(childPath[1])) {
    if (strcmp(Stream.value.c_str(), "true") == 0) {
      Serial.println("Resetting fingerprints");
      // resetFingerprints();
      delay(2000);
      Firebase.RTDB.setBool(&fbdoWrite, "/lock/resetfp", false);
    }
  }

  Serial.println();
  dataChanged = true;

}

void streamTimeoutCallback(bool timeout)
{
  if (timeout)
    Serial.println("stream timed out, resuming...\n");

}

void setup() {
  Serial.begin(115200);
  Serial2.begin(57600, SERIAL_8N1, FINGERPRINT_RX, FINGERPRINT_TX);
  pinMode(SOLENOID_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  WiFiManager wifiManager;
  wifiManager.autoConnect("smartlock");
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  config.token_status_callback = tokenStatusCallback;
  authh.user.email = USER_EMAIL;
  authh.user.password = USER_PASSWORD;
  Firebase.reconnectWiFi(true);
  Firebase.setDoubleDigits(5);
  Firebase.begin(&config, &authh);

  // Blynk.begin(auth, WiFi.SSID().c_str(), WiFi.psk().c_str());

  // if (!Firebase.RTDB.beginStream(&fbdoRead, "/lock/value")) {
  //   Serial.printf("stream begin error, %s\n\n", fbdoRead.errorReason().c_str());
  // }

  // if(!Firebase.RTDB.beginStream(&fbdoRead2, "/lock/newfp")) {
  //   Serial.printf("stream 2 begin error, %s\n\n", fbdoRead2.errorReason().c_str());
  // }

  // if(!Firebase.RTDB.beginStream(&fbdoRead3, "/lock/resetfp")) {
  //   Serial.printf("stream 3 begin error, %s\n\n", fbdoRead3.errorReason().c_str());
  // }

  if (!Firebase.RTDB.beginMultiPathStream(&stream, parentPath)) {
    Serial.printf("multi stream begin error: %s\n\n", stream.errorReason().c_str());
  }

  Firebase.RTDB.setMultiPathStreamCallback(&stream, streamCallback, streamTimeoutCallback);

  SPI.begin();
  mfrc522.PCD_Init();

  if (finger.verifyPassword()) {
    Serial.println("Fingerprint sensor detected!");
    Serial.println("Ready to recognize fingerprints.");
  } else {
    Serial.println("Fingerprint sensor password incorrect!");
    while (1) { delay(1); }
  }

  // Blynk.virtualWrite(V1, LOW);  // Set Button V1 state to LOW in the Blynk app
  // Blynk.virtualWrite(V2, LOW);  // Set Button V2 state to LOW in the Blynk app
  Serial.println();
}

void loop() {
  // Blynk.run();  // Run Blynk connection

  if (Firebase.ready()) {
    // if (!Firebase.RTDB.readStream(&fbdoRead)) {
    //   Serial.printf("stream read error, %s\n\n", fbdoRead.errorReason().c_str());
    // }
    // if (fbdoRead.streamAvailable()) {
    //   if (fbdoRead.dataType() == "boolean") {
    //     bool flutterLock = fbdoRead.boolData();
    //     if (flutterLock) {
    //       digitalWrite(SOLENOID_PIN, HIGH);
    //       Serial.println("Unlocking...");
    //     } else {
    //       digitalWrite(SOLENOID_PIN, LOW);
    //       Serial.println("Locked.");
    //     }
    //   }
    // }

    // if (!Firebase.RTDB.readStream(&fbdoRead2)) {
    //   Serial.printf("stream 2 read error, %s\n\n", fbdoRead2.errorReason().c_str());
    // }
    // if (fbdoRead2.streamAvailable()) {
    //   if (fbdoRead2.dataType() == "boolean") {
    //     bool newFnp = fbdoRead2.boolData();
    //     if (newFnp == true && fingerprintCount < MAX_FINGERPRINTS) { 
    //       enrollMode = true;
    //       enrollFingerprint();
    //     } else {
    //       enrollMode = false;
    //       // Firebase.RTDB.setBool(&fbdoWrite, "/lock/newfp", false);
    //     }
    //   }
    // }

    // if (!Firebase.RTDB.readStream(&fbdoRead3)) {
    //   Serial.printf("stream 3 read error, %s\n\n", fbdoRead3.errorReason().c_str());
    // }
    // if (fbdoRead3.streamAvailable()) {
    //   if (fbdoRead3.dataType() == "boolean") {
    //     bool resetFnp = fbdoRead3.boolData();
    //     if (resetFnp == true) {
    //       // resetFingerprints();
    //       // Firebase.RTDB.setBool(&fbdoWrite, "/lock/resetfp", false);
    //     }
    //   }
    // }
    
  }

  if (dataChanged) {
    

    dataChanged = false;
  }

  buttonState = digitalRead(BUTTON_PIN);
  if (buttonState == LOW) {
    Serial.println("Resetting WiFiManager...");
    WiFiManager wifiManager;
    wifiManager.resetSettings();
    WiFi.disconnect(true);
    delay(2000);
    ESP.restart();
  }
  if (!mfrc522.PICC_IsNewCardPresent()) {
    if (!enrollMode) {                          //if there isnt and RFID card present get fingerprint
      getFingerprintID();
    }
    return;
  }

  if (!mfrc522.PICC_ReadCardSerial()) {     //Reads the RFID card UID into mfrc522.uid if its not there then return and do nothing
    return;
  }

  Serial.print("RFID Card UID: ");
  String uid = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {                       //Checks the UID of the RFID card and stores it in uid
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
    uid += String(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    uid += String(mfrc522.uid.uidByte[i], HEX);
  }
  Serial.println();


 uid.toUpperCase();
  if (uid == " 73 74 5E 17")            //checks if the UID is the same as the eligible UID, if it is the same door is unlocked.
  {
    // Serial.println("Akses diizinkan");
    unlock();
  } else {
    // Serial.println("Akses ditolak");
    getFingerprintID();
  }
  Serial.println();
  delay(1000); 
}

void getFingerprintID() {
  uint8_t p = finger.getImage();      // puts the image the fp sensor gets into 8 bit unsigned integer p as a buffer
  if (p != FINGERPRINT_OK) {
    // Serial.println("Failed to get fingerprint image!");       //when p isnt the signed up fingerprints keep printing failed to get fingerprint image
    return;
  }

  p = finger.image2Tz();        //converts the image to a template
  if (p != FINGERPRINT_OK) {
    // Serial.println("Failed to convert fingerprint image!");
    delay(1000);
    return;
  }

  p = finger.fingerFastSearch();     //searches the template for a match in the database
  if (p == FINGERPRINT_OK) {
    // Serial.println("Fingerprint found!");
    unlock();                               //Unlock door
  } else if (p == FINGERPRINT_NOTFOUND) {
    // Serial.println("Fingerprint not found.");
  } else {
    // Serial.println("Error searching for fingerprint!");
  }
}

void unlock() {
  Serial.println("Unlocking...");
  digitalWrite(SOLENOID_PIN, HIGH); 
  Firebase.RTDB.setBool(&fbdoWrite, F("/lock/var"), true);
  if(!Firebase.RTDB.setBool(&fbdoWrite, F("/lock/var"), true)) {
    Serial.print("unlock error: ");
    Serial.println(fbdoWrite.errorReason());
  }  
  delay(3000); 
  digitalWrite(SOLENOID_PIN, LOW); 
  Firebase.RTDB.setBool(&fbdoWrite, F("/lock/var"), false);
  Serial.println("Lock closed!");
  getFingerprintID(); 
}    

void enrollFingerprint() {
  Serial.println("Place your finger on the sensor...");
  delay(4000);

  uint8_t p = finger.getImage();
  if (p != FINGERPRINT_OK) {
    Serial.println("Failed to get fingerprint image!");
    delay(1000);
    return;
  }

  p = finger.image2Tz(1);
  if (p != FINGERPRINT_OK) {
    Serial.println("Failed to convert fingerprint image!");
    delay(1000);
    return;
  }

  Serial.println("Remove your finger");
  delay(3000);

  Serial.println("Place the same finger again...");
  delay(3000);
  p = finger.getImage();
  if (p != FINGERPRINT_OK) {
    Serial.println("Failed to get fingerprint image!");
    delay(1000);
    return;
  }

  p = finger.image2Tz(2);
  if (p != FINGERPRINT_OK) {
    Serial.println("Failed to convert fingerprint image!");
    delay(1000);
    return;
  }

  p = finger.createModel();
  if (p != FINGERPRINT_OK) {
    Serial.println("Failed to create fingerprint model!");
    delay(1000);
    return;
  }

  p = finger.storeModel(fingerprintCount + 1);
  if (p != FINGERPRINT_OK) {
    Serial.println("Failed to store fingerprint model!");
    delay(1000);
    return;
  }

  fingerprintCount++;
  Serial.println("Fingerprint enrolled!");
  delay(2000);

  if (fingerprintCount >= MAX_FINGERPRINTS) {
    Serial.println("Maximum number of fingerprints reached.");
    Firebase.RTDB.setBool(&fbdoWrite, "/lock/newfp", false);
    enrollMode = false;
    // Blynk.virtualWrite(V2, LOW); // Set Button V2 state to LOW in the Blynk app
  }

  // Read the fingerprint again automatically
  getFingerprintID();
}

void resetFingerprints() {
  Serial.println("Resetting fingerprint data...");
  
  finger.emptyDatabase();
  fingerprintCount = 0;
  
  Serial.println("Fingerprint data reset.");

  // Blynk.virtualWrite(V2, LOW); // Set Button V2 state to LOW in the Blynk app
}



// BLYNK_WRITE(V2) {
//   int buttonState = param.asInt();
//   if (buttonState == HIGH && fingerprintCount < MAX_FINGERPRINTS) {
//     enrollMode = true;
//     Blynk.virtualWrite(V2, HIGH); // Set Button V2 state to HIGH in the Blynk app
//     enrollFingerprint();
//   } else {
//     enrollMode = false;
//     Blynk.virtualWrite(V2, LOW); // Set Button V2 state to LOW in the Blynk app
//   }
// }

// BLYNK_WRITE(V3) {
//   int buttonState = param.asInt();
//   if (buttonState == HIGH) {
//     resetFingerprints();
//   }
// }
