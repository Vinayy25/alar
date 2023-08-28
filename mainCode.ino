#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <unordered_map>
#include <string>
#include <SoftwareSerial.h>
#include <sstream>
#include <iostream>
#include <DHT.h>
 
#include <SPI.h>
#include <MFRC522.h>
TaskHandle_t task1Handle;
TaskHandle_t task2Handle;
TaskHandle_t task3Handle;
TaskHandle_t task4Handle;
const byte rxPin = 16; //rx2
const byte txPin = 17; //tx2
const byte rx2Pin = 33;
const byte tx2Pin = 32;
// const byte rx3Pin=21;
// const byte tx3Pin=22;
const int barcodeScannerPin = 21;
const int rfidScannerPin = 13;
HardwareSerial mySerial(1);
SoftwareSerial barcodeScannerSerial(rx2Pin, tx2Pin);
SoftwareSerial rfidScannerSerial(2, 3);
// MFRC522 mfrc522(rx3Pin, tx3Pin);  

unsigned char Buffer[9];
unsigned char spongeCountHEX[8] = {0x5A, 0xA5, 0x05, 0x82, 0x60, 0x00, 0x00, 0x00};
unsigned char maxCountHEX[8] = {0x5A, 0xA5, 0x05, 0x82, 0x90, 0x00, 0x00, 0x00};
unsigned char barcodeScannerState[8] = {0x5A, 0xA5, 0x05, 0x82, 0x55, 0x00, 0x00, 0x00};
unsigned char rfidScannerState[8] = {0x5A, 0xA5, 0x05, 0x82, 0x56, 0x00, 0x00, 0x00};
String scannedData="";
String userAction="";
int spongeCount=0;
int maxCount=0;
using namespace std;

//class implementation for handling all the backend tasks 

class Procedure{
 public:
 string procedureNumber;
 
 unordered_map < string, bool > cottonId ;

//search cotton id in the list 
 bool findCottonId( string cottonIdToAddOrRemove )
 {

  if(cottonId.count( cottonIdToAddOrRemove ) == true)
  {
    return true;
  }
   else 
  {
    return false;
  }

 }
//add new cotton id to the list
 void addCottonId ( string cottonIdToAddOrRemove )
 {
  cottonId[ cottonIdToAddOrRemove ] = true;
 }

//remove cotton id from the list 
 void removeCottonId ( string cottonIdToAddOrRemove ) 
 {

  cottonId[ cottonIdToAddOrRemove ] = false;
  
  cottonId.erase( cottonIdToAddOrRemove );

 }

//check if the procedure can be ended 
 bool endProcedure ()
 {

  if( cottonId.size() == false )
  {
    return false;
  } else 
  {
    return true;
  }

 }

 
};




// task of managing cotton ids 
void manageCottonId(void *parameter){
    Procedure patient;
    bool procedureStarted = false;

    while (true) {
        
        spongeCount=patient.cottonId.size();
        maxCount=max(spongeCount,maxCount);

        spongeCountHEX[6] = highByte(spongeCount);
        spongeCountHEX[7] = lowByte(spongeCount);
        maxCountHEX[6] = highByte(maxCount);
        maxCountHEX[7] = lowByte(maxCount);
        
        // Check if there's a new user action
        if (!userAction.isEmpty()) {
            if (userAction == "start") {
                // Initialize a new Procedure instance
                procedureStarted = true;
            }
            // ... handle other user actions ...
            else if(userAction=="end"){
              if(patient.endProcedure()){
              procedureStarted=false;
              spongeCount=0;
              maxCount=0;
              patient.cottonId.clear();
              }
            }else if(userAction=="forceStop"){
              procedureStarted=false;
              spongeCount=0;
              maxCount=0;
              patient.cottonId.clear();
            }
            // Reset the shared resource
            userAction = "";
        }

        // Check if the procedure is started
        if (procedureStarted) {
            // Read data from the barcode scanner
            
                // Process the scanned data and take appropriate actions
                if (!scannedData.isEmpty()) {
                    if (patient.findCottonId(scannedData.c_str())) {
                        patient.removeCottonId(scannedData.c_str());
                        
                    } else {
                        patient.addCottonId(scannedData.c_str());
                        
                    }
                    
                }
           scannedData="";
            
        }
        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }


}

//task to handle the procedure of taking input from display 
void manageUserInput(void *parameter) {

  while (true) {
     if (mySerial.available()) {
      // Acquire the mutex before accessing the Buffer array
      
        // Access the Buffer array safely
        for (int i = 0; i < 9; i++) {
          Buffer[i] = mySerial.read();
        }



      if (Buffer[0] == 0x5A) {
        switch (Buffer[4]) {
          case 0x55: // for barcode scanner
            if (Buffer[8] == 1) {  
              
              Serial.println("Barcode scanner ON");
              barcodeScannerState[7] = 1;
            } else if(Buffer[8]==0){
              Serial.println("Barcode scanner OFF");
              barcodeScannerState[7] = 0;
            }
            break;

          case 0x56: // for RFID scanner
            if (Buffer[8] == 1) {   
            
              Serial.println("RFID scanner ON");
              rfidScannerState[7] = 1;


            } else if(Buffer[8]==0){
              Serial.println("RFID scanner OFF");
              rfidScannerState[7] = 0;

            }
            break;

          case 0x70: // program control
            if (Buffer[8] == 1) {
              Serial.println("Start the program");
              userAction = "start";
            }
            break;

          case 0x71: // end program
            if (Buffer[8] == 1) {
              Serial.println("End the program");
              userAction = "end";
            }
            break;

          case 0x72: // force stop
            if (Buffer[8] == 1) {
              Serial.println("Force stop");
              userAction = "forceStop";
            }
            break;

          
        }

      }
    }

    if(barcodeScannerSerial.available()){
       scannedData = barcodeScannerSerial.readStringUntil('\n');
       scannedData.trim();
       Serial.println(scannedData);

    }
  //  if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) 
  //    {
  //   Serial.print("UID tag: ");
  //   for (byte i = 0; i < mfrc522.uid.size; i++) 
  //   {
  //       Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? "0" : "");
  //       Serial.print(mfrc522.uid.uidByte[i], HEX);
  //   }
  //   Serial.println();

    // You can add your custom logic here to process the UID or perform actions based on it
// }

    
     vTaskDelay(pdMS_TO_TICKS(200));
 }
}


void updateUI(void *parameter){
  while(true){
  // mySerial.write(barcodeScannerState,8);
  // vTaskDelay(pdMS_TO_TICKS(500));

  // mySerial.write(rfidScannerState,8);  
  // vTaskDelay(pdMS_TO_TICKS(500));

  mySerial.write(spongeCountHEX,8);
  vTaskDelay(pdMS_TO_TICKS(1000));

  mySerial.write(maxCountHEX,8);
  vTaskDelay(pdMS_TO_TICKS(100));

  }
}

void updateGPIO(void *parameter){
  while(true){
  if(barcodeScannerState[7]==0x01){
    digitalWrite(barcodeScannerPin,HIGH);
  }else {
    digitalWrite(barcodeScannerPin,LOW);
  }

  if(rfidScannerState[7]==0x01){
    digitalWrite(rfidScannerPin,LOW);
    
  }else {
    digitalWrite(rfidScannerPin,HIGH);
  }
  vTaskDelay(pdMS_TO_TICKS(100));
  }
}


void setup() {
 //initialize other shared resources here
    pinMode(barcodeScannerPin, OUTPUT);
    pinMode(rfidScannerPin, OUTPUT);
    Serial.begin(115200);
    rfidScannerSerial.begin(9600);
    mySerial.begin(115200, SERIAL_8N1, rxPin, txPin);
    barcodeScannerSerial.begin(9600);
      // mfrc522.PCD_Init();
  xTaskCreate(manageCottonId, "Task1", 1000, NULL, 0, &task1Handle);
  xTaskCreate(manageUserInput, "Task2", 1000, NULL, 0, &task2Handle);
  xTaskCreate(updateUI, "Task3",1000,NULL,0,&task3Handle);
  xTaskCreate(updateGPIO,"Task4",1000,NULL,0,&task4Handle);

}

void loop() {}
