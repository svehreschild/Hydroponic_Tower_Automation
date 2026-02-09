#include <Wire.h>
#include <RTClib.h>
#include <LiquidCrystal_I2C.h>
#include <DHT.h>

RTC_DS3231 rtc;
LiquidCrystal_I2C lcd(0x27,  20, 4);

// PIN Definitionen
#define pumpePin  4    // Pumpe an Pin 4
#define tasterPin 7    // Taster an Pin 7
#define fuellstand 5
#define DHTPIN 6   
#define DHTTYPE DHT22

// Anpassbare Zeiten
const unsigned long DAUER_AUTO    = 30000;   // Bewässerung (Automatik)
const unsigned long DAUER_MANUELL = 10000;    // Bewässerung (Manueller Tastendruck)
const int INTERVALL_MINUTEN       = 30;       // Alle X Minuten gießen
const int START_STUNDE            = 9;        // Ab XX:00 Uhr
const int ENDE_STUNDE             = 20;       // Bis XX:00 Uhr
const unsigned long PAUSE_DAUER   = 3 * 3600000ULL; // X=3 Stunden Pause nach Langdruck (ULL = Unsigned Long Long)
const unsigned long LONG_PRESS_ZEIT = 3000;   // Langdruck

// Variablen
float temp;
float humidity;
unsigned long letzteSerialZeit = 0;
unsigned long pumpeStartZeit = 0;
unsigned long pauseStartZeit = 0;
unsigned long tasterGedruecktZeit = 0;

bool pumpeAn = false;
bool tasterWarGedrueckt = false;

enum PumpenModus { KEINER, AUTO, MANUELL, PAUSE };
PumpenModus pumpenModus = KEINER;

DHT dht(DHTPIN, DHTTYPE);


void setup() {
  Serial.begin(9600); // Serieller Monitor zur Kontrolle
  if (!rtc.begin()) {
    Serial.println("RTC nicht gefunden!"); 
    while (1);
  }

  // Uhrzeit setzen (einmalig)
  // Format: Jahr, Monat, Tag, Stunde, Minute, Sekunde
  // rtc.adjust(DateTime(2026, 2, 9, 18, 21, 40)); 


  pinMode(pumpePin, OUTPUT);
  digitalWrite(pumpePin, LOW);
  
  pinMode(tasterPin, INPUT); // (externer) Pullup, weil Taster gegen GND
  pinMode(fuellstand, INPUT);


  lcd.init();
  lcd.backlight();
  dht.begin();
}

void loop() {
  DateTime jetzt = rtc.now();
  temp = dht.readTemperature();
  humidity = dht.readHumidity();
  unsigned long aktuelleMillis = millis();

  /// TASTER-LOGIK (Kurz/Lang)
  bool tasterStatus = digitalRead(tasterPin);

  if (tasterStatus == HIGH) { // Taster wird gedrückt
    if (!tasterWarGedrueckt) {
      tasterGedruecktZeit = aktuelleMillis;
      tasterWarGedrueckt = true;
    }
    // Langdruck erkannt?
    if (aktuelleMillis - tasterGedruecktZeit >= LONG_PRESS_ZEIT && pumpenModus != PAUSE) {
      pumpeAn = false;
      digitalWrite(pumpePin, LOW);
      pumpenModus = PAUSE;
      pauseStartZeit = aktuelleMillis;
      Serial.println("PAUSE AKTIVIERT");
    }
  } 
  else { // Taster losgelassen
    if (tasterWarGedrueckt) {
      unsigned long drueckDauer = aktuelleMillis - tasterGedruecktZeit;
      
      if (drueckDauer < LONG_PRESS_ZEIT) {
        if (pumpenModus == PAUSE) {
          pumpenModus = KEINER; // Pause abbrechen
          Serial.println("PAUSE ABGEBROCHEN");
        } 
        else if (!pumpeAn && digitalRead(fuellstand) == HIGH) {
          pumpeAn = true;
          pumpenModus = MANUELL;
          pumpeStartZeit = aktuelleMillis;
          digitalWrite(pumpePin, HIGH);
        }
      }
      tasterWarGedrueckt = false;
    }
  }

  // PAUSEN-TIMER
  if (pumpenModus == PAUSE && (aktuelleMillis - pauseStartZeit >= PAUSE_DAUER)) {
    pumpenModus = KEINER;
    Serial.println("PAUSE BEENDET");
  }

  // LCD ANZEIGE (Display Zeilen 1-3)
  lcd.setCursor(0, 0); lcd.print("Uhrzeit:    ");
  if (jetzt.hour() < 10) lcd.print("0"); lcd.print(jetzt.hour()); lcd.print(":");
  if (jetzt.minute() < 10) lcd.print("0"); lcd.print(jetzt.minute()); lcd.print(":");
  if (jetzt.second() < 10) lcd.print("0"); lcd.print(jetzt.second());

  lcd.setCursor(0, 1); lcd.print("Temp:          "); lcd.print(temp, 1); lcd.print("C");
  lcd.setCursor(0, 2); lcd.print("Feuchte:       "); lcd.print(humidity, 1); lcd.print("%");

  // SYSTEM-STEUERUNG (Display Zeile 4)
  
  // Füllstand kritisch
  if (digitalRead(fuellstand) == LOW) {
    pumpeAn = false;
    pumpenModus = KEINER;
    digitalWrite(pumpePin, LOW);
    lcd.setCursor(0, 3);
    lcd.print("Fuellstand zu gering");
  } 
  // Pause aktiv
  else if (pumpenModus == PAUSE) {
    unsigned long rest = (PAUSE_DAUER - (aktuelleMillis - pauseStartZeit)) / 1000;
    lcd.setCursor(0, 3);
    lcd.print("Pause:        ");
    lcd.print(rest / 3600); lcd.print("h ");
    lcd.print((rest % 3600) / 60); lcd.print("m");
  }
  // Pumpe läuft (Automatik oder Manuell)
  else if (pumpeAn) {
    unsigned long laufzeit = (pumpenModus == AUTO) ? DAUER_AUTO : DAUER_MANUELL;
    long verbleibend = (laufzeit - (aktuelleMillis - pumpeStartZeit)) / 1000;

    if (verbleibend > 0) {
      lcd.setCursor(0, 3);
      lcd.print(pumpenModus == AUTO ? "Auto-Run:        " : "Manu-Run:         ");
      lcd.print(verbleibend); lcd.print("s");
    } else {
      pumpeAn = false;
      pumpenModus = KEINER;
      digitalWrite(pumpePin, LOW);
      lcd.setCursor(0, 3); lcd.print("                    ");
    }
  }
  // Automatik Start-Check
  else if (jetzt.hour() >= START_STUNDE && jetzt.hour() < ENDE_STUNDE && 
           jetzt.minute() % INTERVALL_MINUTEN == 0 && jetzt.second() == 0 && 
           pumpenModus == KEINER) {
    pumpeAn = true;
    pumpenModus = AUTO;
    pumpeStartZeit = aktuelleMillis;
    digitalWrite(pumpePin, HIGH);
  }
  // Standby
  else {
    lcd.setCursor(0, 3);
    lcd.print("Bereit              ");
  }

  delay(100); 
}