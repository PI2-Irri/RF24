#include <iostream>
#include <sstream>
#include <cstdlib>
#include <csignal>
#include <vector>
#include <queue> 
#include <RF24/RF24.h>
#include <plog/Log.h>
#include <plog/Appenders/ColorConsoleAppender.h>
/*
* PLOG Installation notes
* $ git clone https://github.com/SergiusTheBest/plog
* mkdir build 
* cd build
* cmake ..
* make -j
* sudo make install
*/

using namespace std;

// RF24 Params //
/*
* Pipes Description:
* 0 - ControlHub to ControllerHub 
* 1 - ControllerHub to ControlHub
* 2 - InGround1 to ControlHub
* 3 - InGround2 to ControlHub
* 4 - InGround3 to ControlHub
* 5 - ControlHub broadcast to InGround Sensors
*/
const uint64_t pipes[6] = 
					{ 
					0xF0F0F0F0D2LL, 0xF0F0F0F0E1LL, 
					0xF0F0F0F0E2LL, 0xF0F0F0F0E3LL, 
					0xF0F0F0F0F1, 0xF0F0F0F0F2 
					};

// Structs //
struct ActuatorCommand
{
    bool status;
    uint16_t timer;
};

struct ActuatorData
{
	uint16_t water_comsumption;
	uint8_t reservoir_level;
};

struct ContextTag
{
  int moisture;
  float temperature;
  int battery;
};

struct inGroundTag
{
	ContextTag tag;
	uint64_t rfAdress;
};

// FUNCTIONS //
vector<string> splitDelimiter(const string &str, char delimiter);
ActuatorCommand actuatorCommandParser(const string &str);
void configureRadio();
void configurePipes();
void signalHandler(int signum);
void testControllerHub();

// GLOBAL VARIABLES //
bool testCHub = false;
RF24 radio(26,22); // BCM 26 as nRF CE & BCM22 (SPI1 CE2) as nRF CSN 
queue<ActuatorData> controllerHubData;
queue<inGroundTag> inGroundData;

// ****************************   MAIN   **************************** 
int main(int argc, char *argv[])
{
	bool begin;
	uint8_t pipe = 1;

    //Catch Signal
    signal(SIGTSTP, &signalHandler); // ^z to perform ControllerHub Test Routine

	// Initialize Logger
    static plog::ColorConsoleAppender<plog::TxtFormatter> consoleAppender;
    plog::init(plog::verbose, "Log.txt").addAppender(&consoleAppender); 
    PLOG_INFO << "IRRI says: Hello Log World!";

    // Radio Setup
	begin = radio.begin();
	PLOG_INFO_IF(begin) << "configureRadio: RF24 started!";
	PLOG_FATAL_IF(!begin) << "configureRadio: RF24 couldn't begin :(";
    configureRadio();
    configurePipes();
    radio.printDetails();
    radio.startListening();

    PLOG_INFO << "MAIN LOOP STARTED";
    while(1)
    {
        // Failure Detection Routine
        PLOG_FATAL_IF(radio.failureDetected) << "RF24 FAILED!!!!!";
        if(radio.failureDetected)
        {
            radio.failureDetected = false;
            // Perform Radio Setup
            begin = radio.begin();
            PLOG_INFO_IF(begin) << "RF24 started after failure!";
            PLOG_FATAL_IF(!begin) << "RF24 couldn't begin after failure :((";
            configureRadio();
            configurePipes();
            radio.printDetails();
            radio.startListening();
            PLOG_WARNING << "Radio reset successfuly after failureDetected.";  
        }

        // Iterate over pipes for incoming messages
        while(radio.available(&pipe))
    	{
    		if(pipe == 1) // Message from ControllerHub
    		{
    		    // Read Message
    		    struct ActuatorData rdata;
    		    radio.read(&rdata, sizeof(rdata));
    		    // Push message into Queue
    		    controllerHubData.push(rdata);
    		    PLOG_VERBOSE << "ControllerHub Pipe " << (int) pipe << ": Recv: " << rdata.water_comsumption << " litres and reservoir level is " << (int) rdata.reservoir_level;
    		}
    		else if(pipe > 1 && pipe < 5) // Message from InGround Sensors
    		{
    		    // Read Message
    		    struct ContextTag rtag;
    		    struct inGroundTag rdata;
    		    radio.read(&rtag, sizeof(rtag));
    		    // Push message into Queue
    		    rdata.tag = rtag;
    		    rdata.rfAdress = pipes[pipe];
    		    inGroundData.push(rdata);
    		    PLOG_VERBOSE << "InGround Pipe " << (int) pipe << ": Recv: " << (int) rtag.moisture << "% RH, " << (int) rtag.temperature << " Celsius and " << (int) rtag.battery << "% battery";
    		}
    	}
        delayMicroseconds(20);
    	
        // Next Pipe...
    	pipe += 1;
    	pipe > 5 ? pipe = 1 : pipe = pipe;

        // Check flag for ControllerHub Testing
        if(testCHub)
            testControllerHub();


    }
	return 0;	
}
// ******************************************************************

void testControllerHub()
{
    int status, timer;
    struct ActuatorCommand cmd;
    bool sent;

    testCHub = false;
    cout << "** SEND TO CONTROLLER HUB **\n";
    cout << "Status (0-OFF / 1-ON) > ";
    cin >> status;
    cout << "Timer (s) > ";
    cin >> timer;
    cmd.status = (bool) status;
    cmd.timer = (uint16_t) timer;
    radio.stopListening();
    sent = radio.write(&cmd, sizeof(cmd));
    radio.startListening();
    if(sent)
        cout << "SENT.\n";
    else
        cout << "FAILED :(\n";
}

void signalHandler(int signum)
{
    testCHub = true;
}

//configureRadio: Configure RF24 radio
void configureRadio()
{
	radio.setAutoAck(true);
	radio.setDataRate(RF24_250KBPS);
	radio.setPALevel(RF24_PA_HIGH);
	radio.setChannel(76);
	radio.setCRCLength(RF24_CRC_16);
	radio.setRetries(5,15); // 5*250us delay with 15 retries
}

//configurePipes: Configure RF24 Pipes
void configurePipes()
{
	for(uint8_t i=1; i<6; i++)
		radio.openReadingPipe(i, pipes[i]);
	radio.openWritingPipe(pipes[0]);
}


// actuatorCommandParser: String parser to populate a Actuador command
ActuatorCommand actuatorCommandParser(const string &str)
{
    struct ActuatorCommand command;
    vector<string> v;
    vector<string> s = splitDelimiter(str, ',');
    for(auto tokens : s)
    {
        v = splitDelimiter(tokens, ':');
        if(v[0] == "status")
        {
            if(v[1] == "on")
                command.status = true;
            else
                command.status = false;
        }
        if (v[0] == "timer")
            command.timer = (uint16_t) stoi(v[1]);
    }
    return command;
}

// splitDelimiter: Split string for single character delimiter
vector<string> splitDelimiter(const string &str, char delimiter)
{
    vector<string> result;
    stringstream ss (str);
    string item;

    while (getline(ss, item, delimiter))
        result.push_back(item);
    
    return result;
}