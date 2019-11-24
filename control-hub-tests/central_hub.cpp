#include <cstdlib>
#include <iostream>
#include <RF24/RF24.h>
#include <csignal>

using namespace std;

// Radio pipe addresses for the 2 nodes to communicate.
// First pipe is for writing, 2nd, 3rd, 4th, 5th & 6th is for reading...
const uint64_t pipes[6] = 
					{ 0xF0F0F0F0D2LL, 0xF0F0F0F0E1LL, 
						0xF0F0F0F0E2LL, 0xF0F0F0F0E3LL, 
						0xF0F0F0F0F1, 0xF0F0F0F0F2 
					};

RF24 radio(26,22);
bool sender = false;

void configurePipes(int wpipe)
{
	int n = 1;
	for(uint8_t i=0; i<6; i++){
	    if(i == wpipe)
	    {
	    	radio.openWritingPipe(pipes[i]);
	    }
    	else
    	{
    		radio.openReadingPipe(n, pipes[i]);
    		n +=1;
    	}
	}
}

void signalHandler(int signum)
{
	sender = true;
}


int main(int argc, char** argv) 
{
	signal(SIGINT, signalHandler);

	radio.begin();
	radio.setAutoAck(true);
	radio.setDataRate(RF24_250KBPS);
	radio.setPALevel(RF24_PA_MAX);
	radio.setChannel(76);
	radio.setCRCLength(RF24_CRC_16);
	// configurePipes(0);
	radio.openWritingPipe(pipes[0]);
	radio.openReadingPipe(1, pipes[1]);
	radio.openReadingPipe(2, pipes[2]);
	// radio.setRetries(10, 20);
	radio.printDetails();
	radio.startListening();

	uint8_t pip = 1;
	printf("Start...\n");
	while(1)
	{

		while(radio.available(&pip))
		{
			uint8_t msg;
			radio.read(&msg, sizeof(uint8_t));	
			printf("\nRecv: size=%i payload=%d pipe=%d\n",sizeof(uint8_t),msg,pip);
		}

		// if(radio.failureDetected)
		// {
			// printf("RF24 ERRRRR!");
			// exit(1);
		// }

		pip += 1;
		if(pip > 2)
			pip = 1;

		if(sender)
		{
			sender = false;
			printf("\n ************ Send to Controller Hub ***********\n");
			cout << "Send ON Time (0 to exit): \n>";
			uint8_t time;
			int t;
			cin >> t;
			time = (uint8_t) t;
			printf("\nON TIME: %d \n", time);
			if(time == 0)
				exit(0);
			// Send to Controller Hub
			radio.stopListening();	
			printf("Stopped ...\n");
			radio.flush_tx();
			printf("Flush TX ...\n");
			printf("Sending ...\n");
			radio.flush_rx();
			printf("Flush RX ...\n");
			bool sent = radio.write(&time, sizeof(uint8_t));
			radio.startListening();	
			if (!sent) {
				printf("failed. \n");
			} else {
				printf("Sent! \n");
			}
		}

		delayMicroseconds(2);
	}
	
	return 0;
}

