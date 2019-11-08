/*
* Version 1.0 HAB LoRa radio managment Service AKA "LoRaAirService"
* Vaughn Nugent 2019
* Responsible for controlling the onboard LoRa radio module using the RadioHead C++ libraries 
* There is no header at this time but will come soon.
* This file is currently configured to send dummy packets every 100ms for testing purposes 
* Threads are spawned to handle such tasks as sending and receiving data from the radio 
* aswell as handling the data source control (csv file checking and parsing) 
*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string>
#include <sstream> //istringstream
#include <iostream> // cout
#include <fstream> // ifstream
#include <thread>  // std::thread
#include <atomic> //Atomic fields

using namespace std;

#include <signal.h>
#include <bcm2835.h>
#include <RH_RF69.h>
#include <RH_RF95.h>

#define BOARD_DRAGINO_PIHAT
#define BOARD_LORASPI

#include "../RasPiBoards.h"
#include <sys/unistd.h>

#define GLOBAL_THREAD_DELAY 100


// Our RadioIDS Configuration
#define RF_FREQUENCY  915.00
#define RF_NODE_ID    5
#define RF_DEST_ID    10

#define PACKET_SIZE 252

// Create an instance of a driver
RH_RF95 rf95(RF_CS_PIN, RF_IRQ_PIN);

// File locations 
//static char[] _gps_csv_path= "/home/vaughn/lora/gps.csv";

//Track last rx rssi 
volatile std::atomic<uint8_t> _last_rssi(0);

/* Atomic for use across threads*/

volatile std::atomic<uint8_t> _current_flag;

//Data/runtime fields 
volatile std::atomic<uint8_t> RX_BUFF[PACKET_SIZE] = {};
volatile std::atomic<uint8_t> TX_BUFF[PACKET_SIZE] = {};

typedef enum FLAGS : uint8_t
{
	INIT_ERR = 0x05, RX_ERR = 0x10, TX_ERR = 0x11 
		
	} FLAGS;


//Flag for Ctrl-C
//Used to sync thread pool
volatile sig_atomic_t force_exit = false;

void sig_handler(int sig);
void rx_thread_handler(int thread_id);
bool ReceiveData();
void temp_send_thread_handler(int id);
bool Send(uint8_t * data, int len);
void csv_handler(int id);
void set_rx_buff(int offset, uint8_t* data, uint8_t length);
void set_tx_buff(int offset, uint8_t* data, uint8_t length);
uint8_t* get_rx_buff(int length);
uint8_t* get_tx_buff(int length);



void sig_handler(int sig)
{
	force_exit=true;
}

void rx_thread_handler(int thread_id)
{
	//Run thread loop to handle RX	
	while(!force_exit)
	{				
		#ifdef RF_IRQ_PIN
		// We have a IRQ pin ,poll it instead reading
		// Modules IRQ registers from SPI in each loop
		
		// Rising edge fired ?
		if (bcm2835_gpio_eds(RF_IRQ_PIN)) {
			// Now clear the eds flag by setting it to 1
			bcm2835_gpio_set_eds(RF_IRQ_PIN);
			//printf("Packet Received, Rising event detect for pin GPIO%d\n", RF_IRQ_PIN);
		#endif
			
		if (rf95.available()) {
	
		 //Call rx data handler to retrieve data	
		 ReceiveData();	
				
		#ifdef RF_IRQ_PIN
		}
		#endif		
		}
		//Delay this thread by global thread timing delay
		bcm2835_delay(GLOBAL_THREAD_DELAY);			
	}
	//Loop ended join will complete in main thread
}

//Method that handles the received data 
bool ReceiveData()
{
	//Data is available 
	
	uint8_t len  = PACKET_SIZE;
	uint8_t from = rf95.headerFrom();
	uint8_t to   = rf95.headerTo();
	uint8_t id   = rf95.headerId();
	uint8_t flags= rf95.headerFlags();
	_last_rssi  = rf95.lastRssi();
	
	if(to != RF_NODE_ID || from != RF_DEST_ID )
		{
			_current_flag = RX_ERR;
			
			//Throw into idle mode to clear buffers
			rf95.setModeIdle();
			// Re-init to rx mode anytime we arent sending
			rf95.setModeRx();
			
			return false;
		}
	
	uint8_t temp_buff[len] = {0};
	
	//Get data back and store it in the rx buffer
	if (rf95.recv(temp_buff, &len)) {
		
		//Copy over to the internal array
		set_tx_buff(0,temp_buff,len);
		
		std::cout << "Packet Received \n";
		
		
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
		uint8_t dummy_data[50] = {0};
		dummy_data[0] = 0x05;
		dummy_data[1] = 0x06;
		dummy_data[2] = 0x07;
		dummy_data[3] = 0x08;
		
		if(Send(dummy_data, 50))
			std::cout << "Sent dummy packet successfully \n";
		
		//Delay sending every 1 second
		bcm2835_delay(100);	
		
	}
	
	//Loop ended join will complete in main thread
}

//Data sending handler method
bool Send(uint8_t* data, int len)
{	
	//Enable tx mode for good practice
	rf95.setModeTx();
	
	//Match buffer lengths
	uint8_t data_size = sizeof(data);
	if(data_size >= PACKET_SIZE)
	{
		//Only use up to packet size-1 to account for the header flags
		data_size = PACKET_SIZE -1 ;
	}
	
	//Set the tx buffer and flag
	set_tx_buff(1, data, len);
	TX_BUFF[0] = _current_flag;
	
	//Send our TX_BUFF	
	if(rf95.send(get_tx_buff(PACKET_SIZE), PACKET_SIZE))
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
{	
	//truncate if the buffer is larger
	if(length+offset >PACKET_SIZE)
	{
		length = PACKET_SIZE-offset ;
	}
	
	int i=0; 
	while(i<length)
	{
		RX_BUFF[offset++] = data[i++];
	}
}
void set_tx_buff(int offset, uint8_t* data, uint8_t length)
{
	//truncate if the buffer is larger
	if(length+offset >PACKET_SIZE)
	{
		length = PACKET_SIZE-offset ;
	}
		
	int i=0;
	while(i<length)
	{
		RX_BUFF[offset++] = data[i++];
	}
}

uint8_t* get_rx_buff(int length)
{
	if(length>PACKET_SIZE)
		length=PACKET_SIZE;
	uint8_t* data = new uint8_t[length];
		
	for(int i=0; i<length; i++)
	{
		data[i] = RX_BUFF[i];
	}
	return data;	
} 

uint8_t* get_tx_buff(int length)
{
	if(length>PACKET_SIZE)
	length=PACKET_SIZE;
	uint8_t* data = new uint8_t[length];
	
	for(int i=0; i<length; i++)
	{
		data[i] = TX_BUFF[i];
	}
	return data;
}




//Main thread
int main (int argc, const char* argv[] )
{
	//attach signal interrupt to our signal handler
	signal(SIGINT, sig_handler);
	
	//init BCM
	if (!bcm2835_init()) {
		//If it fails exit because the rest of our program depends on the bcm system
		return 0;
	}
	
	
	#ifdef RF_IRQ_PIN
	// Configure the Interrupt pin
	pinMode(RF_IRQ_PIN, INPUT);
	bcm2835_gpio_set_pud(RF_IRQ_PIN, BCM2835_GPIO_PUD_DOWN);
	// Now we can enable Rising edge detection
	bcm2835_gpio_ren(RF_IRQ_PIN);
	#endif
	
	
	#ifdef RF_RST_PIN
	// Configure the gpio reset pin
	pinMode(RF_RST_PIN, OUTPUT);
	digitalWrite(RF_RST_PIN, LOW );
	bcm2835_delay(150);
	digitalWrite(RF_RST_PIN, HIGH );
	bcm2835_delay(100);
	#endif
	
	//Init RadioHead radio object
	if (!rf95.init())
	{
		fprintf( stderr, "\nRF95 module init failed, Please verify wiring/module *DEBUG* \n" );
		return 0;
	}
	
	#ifdef RF_IRQ_PIN
	// Since we may check IRQ line with bcm_2835 Rising edge detection
	// In case radio already have a packet, IRQ is high and will never
	// go to low so never fire again
	// Except if we clear IRQ flags and discard one if any by checking
	rf95.available();

	// Now we can enable Rising edge detection
	bcm2835_gpio_ren(RF_IRQ_PIN);
	#endif
	
	//Set transmit power
	rf95.setTxPower(14, false);
	
	// Adjust Frequency
	rf95.setFrequency(RF_FREQUENCY);
	
	// Configure Radio Headers
	rf95.setThisAddress(RF_NODE_ID);
	rf95.setHeaderFrom(RF_NODE_ID);
	//Destination ID
	rf95.setHeaderTo(RF_DEST_ID);
	
	// Pre-init rx mode as good practice
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
		
	//Wait for threads to exit. Threads must manage exit themselves!
	temp_send_thread.join();
	std::cout << "Temp Sender thread has joined\n" ;
	rx_thread.join();
	std::cout << "RX thread has joined\n" ;
	csv_thread.join();
	std::cout << "CSV Handler thread has joined\n" ;
	return 1;
	
}