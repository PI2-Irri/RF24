#include <SPI.h>
#include "Arduino.h"
#include "RF24.h"
#include "printf.h"

//DEFINES
#define FAILURE_HANDLING
#define LED1 2
#define LED2 4

//FUNCTIONS
void configureRadio();

//STRUCTS
struct ContextTag
{
  int moisture;
  float temperature;
  int battery;
};

//RF PARAMS
const uint64_t pipes[6] = 
                    { 
                    0xF0F0F0F0D2LL, 0xF0F0F0F0E1LL, 
                    0xF0F0F0F0E2LL, 0xF0F0F0F0E3LL, 
                    0xF0F0F0F0F1, 0xF0F0F0F0F2 
                    };

//GLOBAL VARIABLES
RF24 radio(13,5); // CE 13 & CS 5
unsigned long timer;
uint8_t pip = 2;

void setup()
{
    printf_begin();
    Serial.begin(115200);

    pinMode(LED1, OUTPUT);
    digitalWrite(LED1, HIGH);
    pinMode(LED2, OUTPUT);
    digitalWrite(LED2, HIGH);

    radio.begin();
    configureRadio();
    radio.openReadingPipe(1, pipes[5]); // InGround reading pipes is 5 (broadcast)
    radio.openWritingPipe(pipes[2]); // InGround writing pipes is 2, 3 and 4 
    printf("****************************\n");
    printf("RF24: InGround Simulator Status\n");
    radio.printDetails();
    printf("RF24: END OF STATUS\n");
    printf("****************************\n");
    radio.startListening();
}

void loop()
{
    bool sent;
    if((millis() - timer) > 8*1000)
    {
        // Simulate TAG Data
        struct ContextTag tag;
        tag.moisture = random(0, 100);
        tag.temperature = random(0, 100);
        tag.battery = random(0, 100);
        //Send to ControlHub
        radio.stopListening();
        radio.openWritingPipe(pipes[pip]);
        sent = radio.write(&tag, sizeof(tag));
        radio.startListening();
        if(sent)
        {
            digitalWrite(LED1, HIGH);
            digitalWrite(LED2, LOW);
        }
        else
        {
            digitalWrite(LED1, LOW);
            digitalWrite(LED2, HIGH);
        }

        // Next Pipe
        pip += 1;
        pip == 5 ? pip = 2: pip = pip;   
        timer = millis();
    }
}


void configureRadio()
{
    radio.setAutoAck(true);
    radio.setDataRate(RF24_250KBPS);
    radio.setPALevel(RF24_PA_HIGH);
    radio.setChannel(76);
    radio.setCRCLength(RF24_CRC_16);
    radio.setRetries(5,15); // 5*250us delay with 15 retries
}