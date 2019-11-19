#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>
#include "Arduino.h"
#include "RF24.h"
#include "printf.h"

// FUNCTIONS
void acquireFlowSensorData(bool printInSerial);
void IRAM_ATTR pulseCounter();
void displayIrri();
bool returnStatus();
bool isEmpty();
bool isFull();
bool checkLevel(bool mustBeFull);

// RF24 Params
#define FAILURE_HANDLING
bool radioNumber = 0;
bool exitRoutine = 0;
byte addresses[][6] = {"1Node","2Node"};
RF24 radio(13,5); // CE 13 & CS 5

// Sys Timer Params
unsigned long rtimeCounter = 0;
uint16_t rtime = 0;
uint16_t elapsedTime = 0;

// OLED Display Params
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1
Adafruit_SSD1306 disp(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
// Debug Options
bool debugFlowSensor = true; 

// Flow Sensor Params
byte flowSensorPin = 12; // Using GPIO 12
float calibrationFlowFactor = 4.25; // 4.25 pulses per litre/minute of flow
float flowRate = 0;
float flowLitres = 0;
float totalLitres = 0;
volatile byte pulseCount = 0;
unsigned long oldFlowTime = 0;

// Relay Module Params
byte relayPin1 = 25;
byte relayPin2 = 26;
bool relayStatus1 = 1; // HIGH == OFF

// Level Switches Params
#define MUST_BE_FULL_TO_START 1
byte emptyLevelPin = 2; // Using GPIO 2
byte fullLevelPin = 4; // Using GPIO 4

void setup()
{
    // Initialize printf library
    printf_begin();
    // Initialize serial com
    Serial.begin(9600);
    // Initialize OLED Display
    if(!disp.begin(SSD1306_SWITCHCAPVCC, 0x3C)) 
        Serial.println(F("SSD1306 allocation failed"));
    disp.clearDisplay();
    disp.setTextSize(2);
    disp.setTextColor(WHITE);
    
    // Set Flow Sensor
    pinMode(flowSensorPin, INPUT);
    digitalWrite(flowSensorPin, HIGH);

    // Set Relay Module
    pinMode(relayPin1, OUTPUT);
    digitalWrite(relayPin1, relayStatus1);

    // Set Level Switches
    pinMode(emptyLevelPin, OUTPUT);
    digitalWrite(emptyLevelPin, LOW); 
    pinMode(fullLevelPin, INPUT_PULLDOWN);
    digitalWrite(fullLevelPin, LOW);

    // Init RF24
    radio.begin();
    radio.setAutoAck(true);
    radio.setDataRate(RF24_250KBPS);
    radio.setPALevel(RF24_PA_LOW); 
    Serial.println(F("****************************"));
    Serial.println(F("RF24: Controller Hub Status"));
    radio.printDetails();
    Serial.println(F("RF24: END OF STATUS"));
    Serial.println(F("****************************"));
    // Open a writing and reading pipe on each radio, with opposite addresses
    if(radioNumber){
        radio.openWritingPipe(addresses[1]);
        radio.openReadingPipe(1,addresses[0]);
    }else{
        radio.openWritingPipe(addresses[0]);
        radio.openReadingPipe(1,addresses[1]);
    }
    // Start the radio listening for data
    radio.startListening();
}

void loop()
{
    // Received messages while relay is inactive
    if (radio.available() & (relayStatus1 == HIGH))
    {
        while(radio.available()) {
            radio.read(&rtime, sizeof(uint16_t));
        }
        printf("Got payload (%d bytes). ON TIME - %d seconds...\n", sizeof(uint16_t), rtime);
        // Reset values
        flowRate = 0; 
        flowLitres = 0;
        totalLitres = 0;
        pulseCount = 0;
        oldFlowTime = 0;
        rtimeCounter = 0;
        elapsedTime = 0;
        exitRoutine = 0;
        // Activate Relay & Attach Interrupt
        if (checkLevel(MUST_BE_FULL_TO_START) & (rtime > 0))
        {
            relayStatus1 = LOW; // LOW is ON
            digitalWrite(relayPin1, relayStatus1);
            attachInterrupt(digitalPinToInterrupt(flowSensorPin), pulseCounter, FALLING);
        }
        else
        {
            printf("\nNot enough water to start irrigating!\n");
            exitRoutine = returnStatus();
        }
    } 
    else if (radio.available() & (relayStatus1 == LOW)) // Received messages while relay is active
    {
        uint16_t rRequest;
        while(radio.available()) {
            radio.read(&rRequest, sizeof(uint16_t));
        }
        if (rRequest == 0) // OFF Request is 0, otherwise ignore
        {
            relayStatus1 = HIGH; // HIGH is OFF
            digitalWrite(relayPin1, relayStatus1);
            detachInterrupt(digitalPinToInterrupt(flowSensorPin));
            printf("\nShutdown by Request!\n");
            exitRoutine = returnStatus();
        }
    }

    // Process Sys Timer if relay is active
    if (((millis() - rtimeCounter) > 1000) & (relayStatus1 == LOW))
    {
        elapsedTime += 1;
        rtimeCounter = millis();
        if (rtime - elapsedTime <= 0) // End of Sys Timer
        {
            relayStatus1 = HIGH; // HIGH is OFF
            digitalWrite(relayPin1, relayStatus1);
            detachInterrupt(digitalPinToInterrupt(flowSensorPin));
            printf("\nTimer ended! Shutdown :)\n");
            exitRoutine = returnStatus();
        }
    }

    // Checking Water Level
    if (isEmpty() & (relayStatus1 == LOW))
    {
        //SHUTDOWN
        relayStatus1 = HIGH; // HIGH is OFF
        digitalWrite(relayPin1, relayStatus1);
        detachInterrupt(digitalPinToInterrupt(flowSensorPin));
        printf("\nOoops! Water ended too soon!\n");
        exitRoutine = returnStatus();
    }

    // Process Flow Sensor Readings each second if relay is active
    if (((millis() - oldFlowTime) > 1000) & (relayStatus1 == LOW))
    {
        acquireFlowSensorData(debugFlowSensor);
    }

    // OLED Display
    displayIrri();
}

void acquireFlowSensorData(bool printInSerial)
{  
    detachInterrupt(digitalPinToInterrupt(flowSensorPin));
    flowRate = ((1000.0 / (millis() - oldFlowTime)) * pulseCount) / calibrationFlowFactor;
    oldFlowTime = millis();
    flowLitres = (flowRate/60);
    totalLitres += flowLitres;
    if (printInSerial)
    {
        // Print the flow rate for this second in litres / minute
        Serial.print("Flow rate: ");
        Serial.print(int(flowRate));
        Serial.print("L/min");
        Serial.print("\t");
        // Print the cumulative total of litres flowed since starting
        Serial.print("Output Liquid Quantity: ");        
        Serial.print(totalLitres);
        Serial.println("L"); 
        Serial.print("\t");
    }
    pulseCount = 0;
    attachInterrupt(digitalPinToInterrupt(flowSensorPin), pulseCounter, FALLING);
}

void IRAM_ATTR pulseCounter()
{
  // Increment the pulse counter
  pulseCount++;
}

bool isEmpty()
{
    return !digitalRead(emptyLevelPin);
}

bool isFull()
{
    return digitalRead(fullLevelPin);
}

bool checkLevel(bool mustBeFull)
{
    if (mustBeFull)
        return isFull() & !isEmpty();
    else
        return !isEmpty();
}

bool returnStatus()
{
    // Format Message
    uint32_t msg;
    msg = (uint32_t) elapsedTime << 16;
    msg = msg + (uint32_t) round(totalLitres);
    // Send Message
    radio.stopListening();
    bool sent = radio.write(&msg, sizeof(uint32_t));
    if (sent)
        printf("\nreturnStatus: SUCESS! \n");
    else
        printf("\nreturnStatus: FAIL! \n");
    radio.startListening();
    return sent;
}

void displayIrri()
{
    unsigned char fillstate4_icon16x16[] =
    {
        0b00000000, 0b00000000, //                 
        0b01111111, 0b11111110, //  ############## 
        0b01111111, 0b11111110, //  ##############
        0b01111111, 0b11111110, //  ############## 
        0b00000000, 0b00000000, //                 
        0b01111111, 0b11111110, //  ############## 
        0b01111111, 0b11111110, //  ############## 
        0b01111111, 0b11111110, //  ############## 
        0b00000000, 0b00000000, //                 
        0b01111111, 0b11111110, //  ############## 
        0b01111111, 0b11111110, //  ############## 
        0b01111111, 0b11111110, //  ############## 
        0b00000000, 0b00000000, //                 
        0b01111111, 0b11111110, //  ############## 
        0b01111111, 0b11111110, //  ############## 
        0b01111111, 0b11111110, //  ############## 
    };

    unsigned char fillstate2_icon16x16[] =
    {
        0b00000000, 0b00000000, //                 
        0b00000000, 0b00000000, //                 
        0b00000000, 0b00000000, //                 
        0b00000000, 0b00000000, //                 
        0b00000000, 0b00000000, //                 
        0b00000000, 0b00000000, //                 
        0b00000000, 0b00000000, //                 
        0b00000000, 0b00000000, //                 
        0b00000000, 0b00000000, //                 
        0b01111111, 0b11111110, //  ############## 
        0b01111111, 0b11111110, //  ############## 
        0b01111111, 0b11111110, //  ############## 
        0b00000000, 0b00000000, //                 
        0b01111111, 0b11111110, //  ############## 
        0b01111111, 0b11111110, //  ############## 
        0b01111111, 0b11111110, //  ############## 
    };

    unsigned char fillstate1_icon16x16[] =
    {
        0b00000000, 0b00000000, //                 
        0b00000000, 0b00000000, //                 
        0b00000000, 0b00000000, //                 
        0b00000000, 0b00000000, //                 
        0b00000000, 0b00000000, //                 
        0b00000000, 0b00000000, //                 
        0b00000000, 0b00000000, //                 
        0b00000000, 0b00000000, //                 
        0b00000000, 0b00000000, //                 
        0b00000000, 0b00000000, //                 
        0b00000000, 0b00000000, //                 
        0b00000000, 0b00000000, //                 
        0b00000000, 0b00000000, //                 
        0b01111111, 0b11111110, //  ############## 
        0b01111111, 0b11111110, //  ############## 
        0b01111111, 0b11111110, //  ############## 
    };

    unsigned char error_icon16x16[] =
    {
        0b00000000, 0b00000000, //                 
        0b11111111, 0b11111111, // ################
        0b10000000, 0b00000001, // #              #
        0b10111111, 0b11111101, // # ############ #
        0b10100000, 0b00000101, // # #          # #
        0b10100000, 0b00000101, // # #          # #
        0b10100000, 0b00000101, // # #          # #
        0b10100000, 0b00000101, // # #          # #
        0b10100000, 0b00000101, // # #          # #
        0b10100000, 0b00000101, // # #          # #
        0b10100000, 0b00000101, // # #          # #
        0b10100000, 0b00000101, // # #          # #
        0b10100000, 0b00000101, // # #          # #
        0b10111111, 0b11111101, // # ############ #
        0b10000000, 0b00000001, // #              #
        0b11111111, 0b11111111, // ################
    };

    unsigned char clock_icon16x16[] =
    {
        0b00000000, 0b00000000, //                 
        0b00000000, 0b00000000, //                 
        0b00000011, 0b11100000, //       #####     
        0b00000111, 0b11110000, //      #######    
        0b00001100, 0b00011000, //     ##     ##   
        0b00011000, 0b00001100, //    ##       ##  
        0b00110000, 0b00000110, //   ##         ## 
        0b00110000, 0b00000110, //   ##         ## 
        0b00110000, 0b11111110, //   ##    ####### 
        0b00110000, 0b10000110, //   ##    #    ## 
        0b00110000, 0b10000110, //   ##    #    ## 
        0b00011000, 0b10001100, //    ##   #   ##  
        0b00001100, 0b00011000, //     ##     ##   
        0b00000111, 0b11110000, //      #######    
        0b00000011, 0b11100000, //       #####     
        0b00000000, 0b00000000, //                 
    };

    // Show info in display
    disp.clearDisplay();
    // Flow Rate
    disp.setCursor(0, 0);
    disp.print(flowRate);
    disp.setTextSize(1);
    disp.print(" l/min");
    disp.setTextSize(2);
    //Total Flow
    disp.setCursor(0, 16);
    disp.print(totalLitres);
    disp.setTextSize(1);
    disp.print(" l");
    disp.setTextSize(2);
    // Water Level
    if (isFull() & !isEmpty())
        disp.drawBitmap(100, 4, fillstate4_icon16x16, 16, 16, 1);
    else if (!isFull() & !isEmpty())
        disp.drawBitmap(100, 4, fillstate2_icon16x16, 16, 16, 1);
    else if (!isFull() & isEmpty())
        disp.drawBitmap(100, 4, fillstate1_icon16x16, 16, 16, 1);
    else
        disp.drawBitmap(100, 4, error_icon16x16, 16, 16, 1);
    // Relay Status
    if (relayStatus1 == HIGH)
    {
        disp.setCursor(0, 32);
        disp.print("IRRI.OFF:(");
    }
    else
    {
        disp.drawBitmap(0, 32, clock_icon16x16, 16, 16, 1);
        disp.setCursor(20, 32);
        disp.print(elapsedTime);
        disp.print("/");
        disp.print(rtime);
        disp.print(" s");
    }
    disp.setCursor(0, 50);
    disp.print("EXIT:");
    if (exitRoutine)
        disp.print("OK");
    else
        disp.print("FAIL!");
    disp.display();
}
