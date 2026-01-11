#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(0x27,  20, 4);

#define pumpePin  4    // Pumpe an Pin 4
#define tasterPin 7    // Taster an Pin 7
#define fuellstand 5

#define DHTPIN 6   
#define DHTTYPE DHT22

float temp;
float humidity;
unsigned long letzteSerialZeit = 0;
enum PumpenModus { KEINER, AUTO, MANUELL };
PumpenModus pumpenModus = KEINER;

DHT dht(DHTPIN, DHTTYPE);

unsigned long pumpeStartZeit = 0;
bool pumpeAn = false;

void setup() {
  Serial.begin(9600); // Serieller Monitor zur Kontrolle
  if (!rtc.begin()) {
    Serial.println("RTC nicht gefunden!"); 
    while (1);
  }

  // Uhrzeit setzen (einmalig)
  // Format: Jahr, Monat, Tag, Stunde, Minute, Sekunde
  //rtc.adjust(DateTime(2025, 12, 25, 11, 28, 50)); 


  pinMode(pumpePin, OUTPUT);
  digitalWrite(pumpePin, LOW);
  
  pinMode(tasterPin, INPUT); // (externer) Pullup, weil Taster gegen GND
  pinMode(fuellstand, INPUT);

  // LCD
  //initialize lcd screen
  lcd.init();
  // turn on the backlight
  lcd.backlight();

  dht.begin();
}

void loop() {
  DateTime jetzt = rtc.now();
  temp = dht.readTemperature();
  humidity = dht.readHumidity();
  

  // --- Serielle Ausgabe nur jede Sekunde ---
  if (millis() - letzteSerialZeit >= 1000) {
    letzteSerialZeit = millis();
    Serial.print("Uhrzeit: ");
    if (jetzt.hour() < 10) Serial.print("0");
    Serial.print(jetzt.hour());
    Serial.print(":");
    if (jetzt.minute() < 10) Serial.print("0");
    Serial.print(jetzt.minute());
    Serial.print(":");
    if (jetzt.second() < 10) Serial.print("0");
    Serial.println(jetzt.second());

      // Temperatur + Luftfeuchtigkeit
    Serial.print("  |  Temp: ");
    if (!isnan(temp)) {
      Serial.print(temp);
      Serial.print(" °C");
    } else {
      Serial.print("Fehler");
    }

    Serial.print("  |  Feuchte: ");
    if (!isnan(humidity)) {
      Serial.print(humidity);
      Serial.println(" %");
    } else {
      Serial.println("Fehler");
    }
  }

  // LCD-Ausgabe
  // Pumpe aus
  lcd.setCursor(0, 0); // Spalte 0, Zeile 0
  lcd.print("Uhrzeit: ");
  lcd.setCursor(12, 0);
  if (jetzt.hour() < 10) lcd.print("0");
  lcd.print(jetzt.hour());
  lcd.print(":");
  if (jetzt.minute() < 10) lcd.print("0");
  lcd.print(jetzt.minute());
  lcd.print(":");
  if (jetzt.second() < 10) lcd.print("0");
  lcd.print(jetzt.second());

  lcd.setCursor(0, 1); // Spalte 0, Zeile 1
  lcd.print("Temperatur:"); 
  lcd.setCursor(15, 1); //
  lcd.print(temp, 1); // eine Nachkommastelle
  lcd.print("C");
  lcd.setCursor(0, 2); //
  lcd.print("Luftfeuchte:"); 
  lcd.setCursor(15, 2);
  lcd.print(humidity, 1); // eine Nachkommastelle
  lcd.print("%");



// Füllstandkontrolle
  if (digitalRead(fuellstand) == LOW){
    pumpeAn = false;
    pumpenModus = KEINER;
    digitalWrite(pumpePin, LOW);
    Serial.println("Pumpe ausgeschaltet!");
    lcd.setCursor(0, 3);
    lcd.print("Fuellstand zu gering");
  }
// Automatischer Pumpenstart
  else if (jetzt.hour() == 22 && jetzt.minute() == 0 && jetzt.second() == 0 && !pumpeAn && digitalRead(fuellstand) == HIGH) {// else if (jetzt.minute() % 1 == 0 && jetzt.second() == 0 && !pumpeAn && digitalRead(fuellstand) == HIGH) {  // --- Automatische Pumpe ---
    pumpeAn = true;
    pumpenModus = AUTO;
    pumpeStartZeit = millis();
    digitalWrite(pumpePin, HIGH);
    Serial.println("Pumpe automatisch eingeschaltet!");
    if (pumpeAn){
      // lcd.clear(); // Löscht alles auf LCD
      lcd.setCursor(0, 3);
      lcd.print("Pumpe auto ein!");
    }
  }
// Manueller Pumpenstart
  else if (digitalRead(tasterPin) == HIGH && !pumpeAn && digitalRead(fuellstand) == HIGH) { // --- Manueller Taster ---
    pumpeAn = true;
    pumpenModus = MANUELL;
    pumpeStartZeit = millis();
    digitalWrite(pumpePin, HIGH);
    Serial.println("Pumpe manuell eingeschaltet!");
    if (pumpeAn){
      // lcd.clear(); // Löscht alles auf LCD
      lcd.setCursor(0, 3);
      lcd.print("Pumpe manuell ein!");
    }
  }
// Ausschalten der Pumpe
  else if (pumpeAn) {

    unsigned long laufzeit;

    // Laufzeit je nach Modus bestimmen
    if (pumpenModus == AUTO) {
        laufzeit = 20000; // 20 Sekunden
    }
    else if (pumpenModus == MANUELL) {
        laufzeit = 10000; // 10 Sekunden
    }
    else {
        // Falls aus irgendeinem Grund KEINER, einfach Pumpe aus
        pumpeAn = false;
        digitalWrite(pumpePin, LOW);
        pumpenModus = KEINER;
        lcd.setCursor(0, 3);
        lcd.print("                    ");
        return;
    }

    // Verbleibende Zeit berechnen
    long verbleibend_ms = laufzeit - (millis() - pumpeStartZeit);

    if (verbleibend_ms > 0) {
        int verbleibendSek = verbleibend_ms / 1000;

        // LCD-Ausgabe vorbereiten
        lcd.setCursor(0, 3); // Zeile 4 (0-index)

        if (pumpenModus == AUTO) {
            lcd.print("Pumpe auto ein:  ");
        } else if (pumpenModus == MANUELL) {
            lcd.print("Pumpe manu ein:  ");
        }

        // Sekunden ausgeben
        if (verbleibendSek < 10) {
          lcd.print("0");   // führende Null
        }
        lcd.print(verbleibendSek);
        lcd.print("s"); // Leerzeichen zum Überschreiben alter Zahlen
    }
    else {
        // Pumpe ausschalten, Modus zurücksetzen
        pumpeAn = false;
        pumpenModus = KEINER;
        digitalWrite(pumpePin, LOW);

        // LCD-Text löschen
        lcd.setCursor(0, 3);
        lcd.print("                    ");

        Serial.println("Pumpe ausgeschaltet!");
    }
  }
// Nichts tun
  else if (!pumpeAn && digitalRead(tasterPin) == LOW &&  digitalRead(fuellstand) == HIGH){
    lcd.setCursor(0, 3);         
    lcd.print("                    ");
  }

  

  delay(100);
}