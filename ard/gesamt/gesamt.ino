#include <Arduino.h>
#include <NimBLEDevice.h>
#include <SPI.h>
#include "HX711.h"

//BLE variables
static NimBLEServer* pServer;

//setting 20 byte weight value package based on https://github.com/BooKooCode/OpenSource/blob/main/bookoo_mini_scale%2Fprotocols.md
//functionality only to interface with: https://github.com/kstam/esp-arduino-ble-scales/tree/v3 
//values that never change are set below
uint8_t WEIGHT_DATA[20] = {0}; //initalizes an array with all values set to 0x00
uint8_t COMMAND_DATA[6] = {0};
const std::string TARE_COMMAND_STRING("\x03\x0A\x01\x00\x00\x08", 6);
//these could probably be handleded differently
const size_t WEIGHT_DATA_LENGTH = 20;
const size_t COMMAND_DATA_LENGTH = 6;

// HX711 pin assignment
const int LOADCELL1_DOUT_PIN = 4;
const int LOADCELL1_SCK_PIN = 5;

const int LOADCELL2_DOUT_PIN = 16;
const int LOADCELL2_SCK_PIN = 5;

HX711 scale1, scale2;

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

void setup() {
    Serial.begin(9600);

    //Init 7 Seg
    pinMode(CS_PIN, OUTPUT);
    SPI.begin(CLK_PIN, -1, DATA_PIN, CS_PIN);
    // MAX7219 initialisieren
    sendCommand(MAX7219_REG_SCANLIMIT, 0x07);   // Alle 8 Stellen aktivieren
    sendCommand(MAX7219_REG_DECODEMODE, 0xFF);  // 7-Segment-Decodierung aktivieren
    sendCommand(MAX7219_REG_INTENSITY, 0x08);   // Helligkeit einstellen
    sendCommand(MAX7219_REG_SHUTDOWN, 0x01);    // Normaler Betrieb


    //initialize scale
    scale1.begin(LOADCELL1_DOUT_PIN, LOADCELL1_SCK_PIN);
    scale2.begin(LOADCELL2_DOUT_PIN, LOADCELL2_SCK_PIN);

    scale1.set_scale(1265);  // Adjust this value based on calibration.
    scale2.set_scale(1265);
    scale1.tare();  // Reset the scale to 0
    scale2.tare();

    //setting values in weight data array 
    WEIGHT_DATA[0] = 0x03; //PRODUCT NUMBER
    WEIGHT_DATA[1] = 0x0B; //TYPE
    WEIGHT_DATA[5] = 'g'; //unit of weight *note that I don't know if this is correct to set grams

    //BLE server setup//
    //Setup to emulate the BooKoo Mini BLE Transmission Protocol here: https://github.com/BooKooCode/OpenSource/blob/main/bookoo_mini_scale%2Fprotocols.md
    //Initalize the library
    NimBLEDevice::init("BOOKO");

    pServer = NimBLEDevice::createServer();
    
    NimBLEService* pWeightService = pServer->createService("0FFE");
    

    NimBLECharacteristic* pWeightCharacteristic = pWeightService->createCharacteristic("FF11", NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY, 20); //weight command characteristic
    pWeightCharacteristic->setValue(WEIGHT_DATA);

    NimBLECharacteristic* pCommandCharacteristic = pWeightService->createCharacteristic("FF12", NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::NOTIFY, 6);
    pCommandCharacteristic->setValue(COMMAND_DATA);

    // Start the services when finished creating all Characteristics and Descriptors //
    pWeightService->start();

    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID("0FFE"); // advertise the UUID of our service
    pAdvertising->setName("BOOKO"); // advertise the device name
    pAdvertising->start(); 
}


void loop() { //would be nice to move some of this code to functions for readability
    //reads scale in grams based on scale factor
    float weight1 = scale1.get_units();
    float weight2 = scale2.get_units();
    //Serial output of load cells for manual calibration. Calibration could be improved so you aren't just guessing and checking the scale factor and then reflashing but that seems hard
    Serial.print("weight: ");
    Serial.println(weight1 + weight2);

    //Updates weight data if BLE is connected and service is running
    if (pServer->getConnectedCount()) {
      Serial.println("Client connected");
      
      NimBLEService* pSvc = pServer->getServiceByUUID("0FFE");
      
      if (pSvc) {
        NimBLECharacteristic* pCChr = pSvc->getCharacteristic("FF12");
        NimBLECharacteristic* pWChr = pSvc->getCharacteristic("FF11");

        //checks for tare command,adjusts tare value if tare command is present, resets command value after tare is complete
        if (pCChr) {
          std::string COMMAND = pCChr->getValue();
          Serial.print("COMMAND: ");
          for (size_t i = 0; i < COMMAND.length(); i++) {
              Serial.print("0x");
              Serial.print((uint8_t)COMMAND[i], HEX);
              Serial.print(" ");
          }
            if (COMMAND == TARE_COMMAND_STRING) {
            scale1.tare();
            scale2.tare();
            uint8_t zeroCommand[COMMAND_DATA_LENGTH] = {0};
            pCChr->setValue(zeroCommand, COMMAND_DATA_LENGTH);
            Serial.println("TARED");
            }
        
        //Updates WEIGHT_DATA
          if (pWChr) {
            int32_t weightInt = static_cast<int32_t>((weight1+weight2) * 100); // Multiply by 100 to preserve 2 decimal places
            WEIGHT_DATA[6] = (weightInt < 0) ? 45 : 43; // '-' for negative, '+' for positive
            uint32_t absWeight = static_cast<uint32_t>(abs(weightInt));
            WEIGHT_DATA[7] = (absWeight >> 16) & 0xFF;
            WEIGHT_DATA[8] = (absWeight >> 8) & 0xFF;
            WEIGHT_DATA[9] = absWeight & 0xFF;

            //update checksum
            uint8_t checksum = 0;
            for (size_t i = 0; i < WEIGHT_DATA_LENGTH - 1; ++i) {
                checksum ^= WEIGHT_DATA[i];
            }
            WEIGHT_DATA[19] = checksum;
            pWChr->setValue(WEIGHT_DATA, sizeof(WEIGHT_DATA));
            pWChr->notify();
            Serial.println("Weight data sent via BLE.");
          }
        }
      }
    }
    //Write data to 7 Segment
    print_format_scale(weight1 + weight2);
    delay(100); //TODO tuning of delay and implementation of powersaving via sleep() or some other function.
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