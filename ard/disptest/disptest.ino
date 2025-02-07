#include <SPI.h>
#include "HX711.h"

// HX711 circuit wiring
const int LOADCELL1_DOUT_PIN = 4;
const int LOADCELL1_SCK_PIN = 5;

const int LOADCELL2_DOUT_PIN = 16;
const int LOADCELL2_SCK_PIN = 5;
// Pins für MAX7219-Verbindung
#define DATA_PIN  22   // DIN-Pin des MAX7219
#define CLK_PIN   23   // CLK-Pin des MAX7219
#define CS_PIN     19   // CS-Pin des MAX7219

// MAX7219-Befehle
#define MAX7219_REG_NOOP        0x00
#define MAX7219_REG_DIGIT0      0x01
#define MAX7219_REG_DIGIT1      0x02
#define MAX7219_REG_DIGIT2      0x03
#define MAX7219_REG_DIGIT3      0x04
#define MAX7219_REG_DIGIT4      0x05
#define MAX7219_REG_DIGIT5      0x06
#define MAX7219_REG_DIGIT6      0x07
#define MAX7219_REG_DIGIT7      0x08
#define MAX7219_REG_DECODEMODE  0x09
#define MAX7219_REG_INTENSITY   0x0A
#define MAX7219_REG_SCANLIMIT   0x0B
#define MAX7219_REG_SHUTDOWN    0x0C
#define MAX7219_REG_DISPLAYTEST 0x0F


HX711 scale1, scale2;

void setup() {
    Serial.begin(9600);
    //scale.begin(LOADCELL_DOUT_PIN, LOADCELL_SCK_PIN);
    // SPI-Initialisierung
    pinMode(CS_PIN, OUTPUT);
    SPI.begin(CLK_PIN, -1, DATA_PIN, CS_PIN);

    //digitalWrite(CS_PIN, HIGH);
    scale1.begin(LOADCELL1_DOUT_PIN, LOADCELL1_SCK_PIN);
    scale2.begin(LOADCELL2_DOUT_PIN, LOADCELL2_SCK_PIN);
    // MAX7219 initialisieren
    sendCommand(MAX7219_REG_SCANLIMIT, 0x07);   // Alle 8 Stellen aktivieren
    sendCommand(MAX7219_REG_DECODEMODE, 0xFF);  // 7-Segment-Decodierung aktivieren
    sendCommand(MAX7219_REG_INTENSITY, 0x08);   // Helligkeit einstellen
    sendCommand(MAX7219_REG_SHUTDOWN, 0x01);    // Normaler Betrieb

    // Linke und rechte Seite unabhängig steuern
    //displayLeftWithComma(1234, 3); // Zeigt "87,56" an (linke Seite)
    //displayRightWithComma(5678, 3); // Zeigt "43,21" an (rechte Seite)
}
void loop() {
    //if (scale1.is_ready() & scale2.is_ready()) {
    if(true) {  
      long reading1 = scale1.read();
      long reading2 = scale2.read();
      double formatted1 = mapf(reading1, 187452, 195090, 0, 7.8);
      double formatted2 = mapf(reading2, 367196, 375820, 0, 7.8);
      Serial.print("1: ");
      Serial.println(reading1);
      Serial.print("2: ");
      Serial.println(reading2);
      
      //sleep(100);
      //print_format_scale(formatted2);
      print_format_scale(formatted1 + formatted2);
  } else {
      Serial.println("HX711 not found.");
  }
    sleep(.1);
}

void print_format_scale(double num) {
 //displayLeftWithComma(formatted, -1);
      if(num < 0) num = 0;
      if(num < 10) {
        displayLeftWithComma(num*10, 1);
      }
      else if(num  < 100) {
        displayLeftWithComma(num*10, 1);
      }
      else if(num  < 1000) {
        displayLeftWithComma(num*10, 1);
      }
}

// Funktion zur Steuerung der linken vier Stellen mit Komma
void displayRightWithComma(uint16_t number, uint8_t commaPosition) {
    for (int i = 0; i < 4; i++) {
        uint8_t digit = number % 10;           // Extrahiere die letzte Ziffer
        if (i == commaPosition) {
            digit |= 0b10000000;               // Dezimalpunkt aktivieren
        }
        sendCommand(MAX7219_REG_DIGIT0 + i, digit); // Digit3 bis Digit0 (linke Seite)
        number = (number-digit)/10;
    }
}

// Funktion zur Steuerung der rechten vier Stellen mit Komma
void displayLeftWithComma(uint16_t number, uint8_t commaPosition) {
    for (int i = 0; i < 4; i++) {
        uint8_t digit = number % 10;           // Extrahiere die letzte Ziffer
        if (i == commaPosition) {
            digit |= 0b10000000;               // Dezimalpunkt aktivieren
        }
        sendCommand(MAX7219_REG_DIGIT4 + i, digit); // Digit7 bis Digit4 (rechte Seite)
        number /= 10;
    }
}

// Funktion zum Senden von Befehlen an den MAX7219
void sendCommand(uint8_t reg, uint8_t data) {
    digitalWrite(CS_PIN, LOW);               // Chip-Select aktivieren
    SPI.transfer(reg);                       // Register senden
    SPI.transfer(data);                      // Daten senden
    digitalWrite(CS_PIN, HIGH);              // Chip-Select deaktivieren
}

float mapf(float x, float in_min, float in_max, float out_min, float out_max) {
     float result;
     result = (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
     return result;
}