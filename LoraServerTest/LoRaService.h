/*
 * LoRaService.h
 *
 * Created: 10/12/2019 1:40:09 AM
 * Author: Vaughn Nugent
 * Description: Program to configure and communicate with a LoRa radio, using the Sandeepmistry library for Arduino.
 * this package relies on, and is compiled with the standard Arduino 1.8 Libraries for the ATMEGA2560 (Ardunio Mega)
 * this package expects you to modify your hardware serial TX and RX buffers to match the size of the Serial packet
 * size for best stability, performance and efficiency.
 *
 * Packet sizes rely on the consts defined below
 * Implementing sketch must initiate serial 
 *
 */


#ifndef LORASERVICE_H_
#define LORASERVICE_H_

#include "Arduino.h"
#include "LoRa.h"

const int RADIOPACKETSIZE = 252;
const int SERIALPACKETSIZE = 260;

typedef	enum unsigned char
{  
	INITRD = 2, IDLERD = 3, SLEEPRD = 4, SETRD = 5, SEND = 10, PACKETINFO = 6
	} command;

typedef enum unsigned char
{
	INIT = 0, OKAY = 1, WARNING = 3, ERR = 4 	
	} status;	

typedef enum unsigned char 
{
	no_err = 0, lora_init_err = 1, lora_send_err = 2			
	}errors;

// Initialize the radio with the default values 
int initRadio();

//Configure the hardware pins that are reqd for your radio (aside from spi pins)
void setHwPins(int enable_pin, int reset_pin, int interrupt_pin );

// Set values other than defaults for the radio 
void setRadioConfig(long freq, int power, int sf, long bw, int crate, long preamble, int sword, bool async );

// Takes a char array for data (api will handle this but you will need to know the array struct to use this)
/*
  long freq   - first 4 bytes 
  int tx_power - next 2 bytes
  int spreading factor - next 2 bytes
  long bandwidth - next 4 bytes
  int coding rate - next 2 bytes
  long preamble  - next 4 bytes
  int sync word  - next 2 bytes
  char async mode - last byte
  
  Your array must have 21 bytes for this data minimum to configure the radio from a command. If the array is smaller than 21 bytes, -1 is returned. 
*/

int setRadioConfig(char* data, int size);

// A single byte to set the dest id to check receive 
void setDestId(char id);

//Send a lora packet! 
int sendLoraPacket(char* data, int data_size);

// Handles callback from radio interrupt 
//Forwards a message to the serial output from the radio buffer immediately 
void onReceive(int packetSize);

//Send serial packet with 2 chars as headers and a larger data array to the serial output (usb)
int sendSerial(char header1, char header2, char* data, int data_size);

//Send serial packet with only 2 chars as headers and the rest of the serial packet as zeros
int sendSerial(char header1, char header2);

//Callback for serial event handling, this will handle commands and serial activity 
void serialEventHandler();

//Is called from serialEventHandler and will handle a given command and various other functions necessary to complete the command
int commandHandler(char instruction, char* data, int data_size);

#endif /* LORASERVICE_H_ */