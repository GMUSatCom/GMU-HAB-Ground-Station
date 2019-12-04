/*
* Version 1.1 HAB LoRa radio management Service AKA "LoRaAirService"
* Vaughn Nugent 2019
* Responsible for controlling the onboard LoRa radio module using the RadioHead C++ libraries 
* There is no header at this time but will come soon.
* This program relies on the ARM BCM2835 hardware header file for Raspberry Pi Model A, B, B+, the Compute Module, and the Raspberry Pi Zero. 
*/


using namespace std;

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sstream> //istringstream
#include <iostream> // cout
#include <fstream> // ifstream
#include <thread>  // std::thread
#include <atomic> //Atomic fields

#include <bcm2835.h>
#include <RH_RF95.h>

#define BOARD_DRAGINO_PIHAT
#define BOARD_LORASPI

#include "../RasPiBoards.h"
#include <sys/unistd.h>

#define GLOBAL_THREAD_DELAY 10

// Our RadioIDS Configuration
#define RF_FREQUENCY  915.00
#define RF_NODE_ID    10
#define RF_DEST_ID    10

//Packet Size
#define PACKET_SIZE 200

//Redefine the gpio pins if needed

#ifdef RF_IRQ_PIN
#undef RF_IRQ_PIN
#endif // RF_IRQ_PIN
//Re-define as physical pin #22 and GPIO 15 as the IRQ pin
#define RF_IRQ_PIN RPI_BPLUS_GPIO_J8_15  

#ifdef RF_RST_PIN
#undef RF_RST_PIN
#endif // RF_RST_PIN
//Re-define spi reset as physical pin #15 or GPIO 10 
#define RF_RST_PIN RPI_BPLUS_GPIO_J8_10  


#ifdef RF_CS_PIN 
#undef RF_CS_PIN
#endif // RF_CS_PIN 
//Re-define the spi chip select pin as CE0 or pin #24 or GPIO 18
#define RF_CS_PIN  RPI_BPLUS_GPIO_J8_18

// Create an instance of the driver
RH_RF95 rf95(RF_CS_PIN, RF_IRQ_PIN);

// File locations 
const char* _gps_csv_path= "/home/vaughn/lora/gps.csv";

//Track last rx rssi 
volatile std::atomic<uint8_t> _last_rssi(0);

/* Atomic for use across threads*/
volatile std::atomic<uint8_t> _current_flag;

//Data/runtime fields 
volatile uint8_t _rx_buff[PACKET_SIZE] = { 0 };

typedef enum FLAGS : uint8_t
{
	INIT_ERR = 0x05, RX_ERR = 0x10, TX_ERR = 0x11 
		
	} FLAGS;


//Flag for Ctrl-C
//Used to sync thread pool
volatile sig_atomic_t force_exit = false;
volatile int new_data_rx = 0;

//Funtion prototypes
void sig_handler(int sig);
void rx_thread_handler(int thread_id);
bool ReceiveData();
void temp_send_thread_handler(int id);
bool Send(uint8_t* data, int len);
void csv_handler(int id);
void set_rx_buff(int offset, uint8_t* data, uint8_t length);
char* get_rx_buff(int length);
void empty_rx_buff();


void sig_handler(int sig)
{
	force_exit=true;
}

void rx_thread_handler(int thread_id)
{
	//Run thread loop to handle RX	
	while(!force_exit)
	{
		// Rising edge fired ?
		if (bcm2835_gpio_eds(RF_IRQ_PIN)) {
			// Now clear the eds flag by setting it to 1
			bcm2835_gpio_set_eds(RF_IRQ_PIN);

			if (rf95.available())
			{
				//Call rx data handler to retrieve data	
				ReceiveData();
			}
		}
		//Delay this thread by global thread timing delay
		bcm2835_delay(GLOBAL_THREAD_DELAY);		
	}
	//Loop ended join will complete in main thread
}

//Method that handles the received data 
bool ReceiveData()
{
	printf("Packet Received\n");
	//Data is available 
	
	uint8_t len   = PACKET_SIZE;
	uint8_t from  = rf95.headerFrom();
	uint8_t to    = rf95.headerTo();
	uint8_t id    = rf95.headerId();
	uint8_t flags = rf95.headerFlags();
	_last_rssi    = rf95.lastRssi();
			
	uint8_t temp_buff[len] = {0};
	
	//Get data back and store it in the rx buffer
	if (rf95.recv(temp_buff, &len)) 
	{		
		//Copy over to the internal array
		set_rx_buff(0,temp_buff,len);		
		
		//Throw into idle mode to clear buffers
		rf95.setModeIdle();	
		// Re-init to rx mode anytime we arent sending
		rf95.setModeRx();
						 
		//DONE
		return true;
	 }
	else
	{ 
		//Failed to receive, set status flag for next send cycle
		_current_flag = RX_ERR;
	
		//Throw into idle mode to clear buffers
		rf95.setModeIdle();
		// Re-init to rx mode anytime we arent sending
		rf95.setModeRx();
	
		return false;		
	}	
}

//Temporary thread sending data 
void temp_send_thread_handler(int id)
{	
	while(!force_exit)
	{
		uint8_t dummy_data[100] = { 0 };
		dummy_data[10] = 0x05;
		dummy_data[20] = 0x06;
		dummy_data[25] = 0x07;
		dummy_data[80] = 0x08;
		
		if(Send(dummy_data, sizeof(dummy_data)))
		{			
			printf("Dummy packet sent\n");
		}
		else
		{
			printf("Failed to send\n");
			break;
		}
		
		//Delay sending every 1 second
		bcm2835_delay(1000);			
	}
	
	//Loop ended join will complete in main thread
}


//Data sending handler method
bool Send(uint8_t* data, int len)
{	
	//Enable tx mode for good practice
	rf95.setModeTx();
		
	uint8_t packet[PACKET_SIZE] = { 0 };

	int i = 0;
	while((i+1) < PACKET_SIZE && i < len)		
	{
		packet[i+1] = data[i];
		i++;
	}
	packet[0] = _current_flag;	
	
	//Without this delay the packet will not send. UNKNOW REASON
	bcm2835_delay(1);

	//Send our TX_BUFF	
	if(rf95.send(packet, PACKET_SIZE))
	{
		//Blocks thread for TX complete
		if(rf95.waitPacketSent())
		{
			//Throw into idle mode to clear buffers
			rf95.setModeIdle();
			// Re-init to rx mode anytime we arent sending
			rf95.setModeRx();
			return true;
		}
		//Couldn't send data. FAILED
		else
		{
			//Throw into idle mode to clear buffers
			rf95.setModeIdle();
			// Re-init to rx mode anytime we arent sending
			rf95.setModeRx();
			return false;
		}
	}
	//Couldn't send data. FAILED
	else
	{
		//Throw into idle mode to clear buffers
		rf95.setModeIdle();
		// Re-init to rx mode anytime we arent sending
		rf95.setModeRx();
		return false;
	}
}


void csv_handler(int id)
{		
	while(!force_exit)
	{
		//run csv management here
		
		
		//Delay this thread by global thread timing delay
		bcm2835_delay(GLOBAL_THREAD_DELAY);
	}
	
	//Loop ended join will complete in main thread
}


void set_rx_buff(int offset, uint8_t* data, uint8_t length)
{	//Set all values in the buffer to zero
	std::fill_n(_rx_buff, PACKET_SIZE, 0);
	
	//truncate if the buffer is larger
	if(length+offset >PACKET_SIZE)
	{
		length = PACKET_SIZE-offset ;
	}
	
	int i=0; 
	while(i<length)
	{
		_rx_buff[offset] = data[i];
		i++; offset++;
	}
	
	new_data_rx = 1;
}
void empty_rx_buff()
{	//Set all values in the buffer to zero
	_rx_buff = new uint8_t[PACKET_SIZE];
	std::fill_n(_rx_buff, PACKET_SIZE, 0);	
}

char* get_rx_buff(int length)
{	
	if(length > PACKET_SIZE)
		length = PACKET_SIZE;
		
	char data[PACKET_SIZE]={0};
		
	for(int i=0; i<length; i++)
	{
		data[i] = (char) (_rx_buff[i]);
	}
	return data;	
} 

//Main thread
int main (int argc, const char* argv[] )
{
	//attach signal interrupt to our signal handler
	signal(SIGINT, sig_handler);
	
	//init BCM
	if (!bcm2835_init()) 
	{
		//If it fails exit because the rest of our program depends on the bcm system
		return EXIT_FAILURE;
	}	
	

	// Configure the Interrupt pin
	pinMode(RF_IRQ_PIN, INPUT);
	bcm2835_gpio_set_pud(RF_IRQ_PIN, BCM2835_GPIO_PUD_DOWN);
	// Now we can enable Rising edge detection
	bcm2835_gpio_ren(RF_IRQ_PIN);

	

	// Configure the gpio reset pin
	pinMode(RF_RST_PIN, OUTPUT);
	digitalWrite(RF_RST_PIN, LOW );
	bcm2835_delay(150);
	digitalWrite(RF_RST_PIN, HIGH );
	bcm2835_delay(100);


	//Init RadioHead radio object
	if (!rf95.init())
	{
		fprintf( stderr, "\nRF95 module init failed, Please verify wiring/module *DEBUG* \n" );
		
		//We can't do anything so exit and log the failure
		return EXIT_FAILURE;
	}
	

	// Since we may check IRQ line with bcm_2835 Rising edge detection
	// In case radio already has a packet, IRQ is high and will never
	// go to low, so never fire again
	// Except if we clear IRQ flags and discard one if any by checking
	rf95.available();
	// Now we can enable Rising edge detection
	bcm2835_gpio_ren(RF_IRQ_PIN);
	
	//Set transmit power
	rf95.setTxPower(14, false);
	
	// Adjust Frequency
	rf95.setFrequency(RF_FREQUENCY);
	
	// Configure Radio Headers
	rf95.setThisAddress(RF_NODE_ID);
	rf95.setHeaderFrom(RF_NODE_ID);
	rf95.setHeaderTo(RF_DEST_ID);
	
	// Pre-init rx mode
	rf95.setModeRx();	
	
	//Start thread pool
	std::thread rx_thread (rx_thread_handler, 0);
	std::cout << "Rx Thread handler initialized\n";
	std::thread csv_thread (csv_handler, 1);
	std::cout << "Temporary send thread handler initialized\n";
	std::thread temp_send_thread (temp_send_thread_handler, 2);
	std::cout << "CSV thread handler initialized\n";
	
	while(!force_exit)
	{	
		bcm2835_delay(GLOBAL_THREAD_DELAY);
	}
	
	std::cout << "\nExit signaled, waiting for threads to join\n" ;
		
	//Wait for threads to exit. Threads must manage exit themselves!
	temp_send_thread.join();
	std::cout << "Temp Sender thread has joined\n" ;
	rx_thread.join();
	std::cout << "RX thread has joined\n" ;
	csv_thread.join();
	std::cout << "CSV Handler thread has joined\n" ;
	
	//Process closed successfully
	return EXIT_SUCCESS;
	
}