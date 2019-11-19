/*
 Copyright (C) 2011 J. Coliz <maniacbug@ymail.com>

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 version 2 as published by the Free Software Foundation.

 03/17/2013 : Charles-Henri Hallard (http://hallard.me)
              Modified to use with Arduipi board http://hallard.me/arduipi
						  Changed to use modified bcm2835 and RF24 library
TMRh20 2014 - Updated to work with optimized RF24 Arduino library

 */

/**
 * Example RF Radio Ping Pair
 *
 * This is an example of how to use the RF24 class on RPi, communicating to an Arduino running
 * the GettingStarted sketch.
 */

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
#include <unistd.h>
#include <RF24/RF24.h>

using namespace std;

// RF24 Params
RF24 radio(26,22);
bool radioNumber = 1;
const uint8_t pipes[][6] = {"1Node","2Node"};

int main(int argc, char** argv){

	// Init RF24
	radio.begin();
	radio.setAutoAck(true);
	radio.setDataRate(RF24_250KBPS);
	radio.setPALevel(RF24_PA_LOW);
	printf("\n ************ RF24: Control Hub Status ***********\n");
	radio.printDetails();
	printf("\n ************ RF24: END OF STATUS ***********\n");
	// Open a writing and reading pipe on each radio, with opposite addresses
	if ( !radioNumber )    {
	  radio.openWritingPipe(pipes[0]);
	  radio.openReadingPipe(1,pipes[1]);
	} else {
	  radio.openWritingPipe(pipes[1]);
	  radio.openReadingPipe(1,pipes[0]);
	}
	// Start the radio listening for data
	radio.startListening();

	while (1)
	{
		//Handling Failures (Reset Radio & Open Pipe)
		if(radio.failureDetected)
		{
	   		radio.begin(); 
	   		radio.failureDetected = 0;
		    if ( !radioNumber ) {
		      radio.openWritingPipe(pipes[0]);
		      radio.openReadingPipe(1,pipes[1]);
		    } else {
		      radio.openWritingPipe(pipes[1]);
		      radio.openReadingPipe(1,pipes[0]);
		    }
	    }

		printf("\n ************ Send to Controller Hub ***********\n");
		cout << "Send ON Time: \n>";
		uint16_t time;
		cin >> time;
		printf("\nON TIME: %d \n", time);
		// Send to Controller Hub
		printf("Sending ...\n");
		radio.stopListening();
		bool sent = radio.write(&time, sizeof(uint16_t));
		if (!sent) {
			printf("failed. \n");
		} else {
			printf("Sent! \n");
		}
		radio.startListening();

		//Check for Controller Hub Response
		while(!radio.available())
			;

		if(radio.available())
		{
			uint32_t rmsg;
			while(radio.available())
				radio.read(&rmsg, sizeof(uint32_t));

			uint16_t elapsedTime = rmsg >> 16;
			uint16_t totalLitres = (uint16_t) rmsg;
			printf("\nElapsed Time %d s. & Total Flow %d l.\n", elapsedTime, totalLitres);
		}
	}
  return 0;
}

