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

HX711 scale;

void setup() {
    Serial.begin(9600);

    //initialize scale
    scale.begin(LOADCELL1_DOUT_PIN, LOADCELL1_SCK_PIN);

    scale.set_scale(1265);  // Adjust this value based on calibration.
    scale.tare();  // Reset the scale to 0

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
    float weight = scale.get_units();

    //Serial output of load cells for manual calibration. Calibration could be improved so you aren't just guessing and checking the scale factor and then reflashing but that seems hard
    Serial.print("weight: ");
    Serial.println(weight);

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
            scale.tare();
            uint8_t zeroCommand[COMMAND_DATA_LENGTH] = {0};
            pCChr->setValue(zeroCommand, COMMAND_DATA_LENGTH);
            Serial.println("TARED");
            }
        
        //Updates WEIGHT_DATA
          if (pWChr) {
            int32_t weightInt = static_cast<int32_t>((weight) * 100); // Multiply by 100 to preserve 2 decimal places
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
    delay(500); //TODO tuning of delay and implementation of powersaving via sleep() or some other function.
}
