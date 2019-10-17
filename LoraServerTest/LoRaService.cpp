/*
 * LoRaService.cpp
 *
 * Created: 10/11/2019 10:54:14 PM
 * Author : Vaughn Nugent
 */ 


#include "LoRaService.h"

const byte ERRCODES[6] = {
  0x00,
  0x02, // Lora init error
  0x03, // Lora send error
  0x04, 
  0x00,
  0x00};

const byte STATUSCODES[6] = 
{  0x01, // Init status
  0x02, // OK! status
  0x03, // ERR Status (pair with ERRCODE)
  0x04, // Warning
  0x00,
  0x00};

//LORA const start
int enable = 7;             // LoRa radio chip select
int resetPin = 6;           // LoRa radio reset
int interruptPin = 1;       // change for your board; must be a hardware interrupt pin

//Radio Vars
long frequency = 915E6;       // Set Operating frequency
int tx_power = 23;            // Transmit Power level
int spreading_factor = 12;    // Spreading Factor
long sig_bandwidth = 15.6E3;  // Signal Bandwidth
int coding_rate = 8;          // Coding Rate denominator
long lora_preamble = 10;      // Set lora radio preamble length
int sync_word = 0;            // Lora Sync Word
bool async_mode = false;      // Packet send in async mode

//Lora Packet Vars
byte DESTID = 0xFF;           // destination to send to
int send_delay = 200;         // interval between sends (not used)

int last_packet_rssi;         // Save the last packets rssi
float last_packet_snr;        // Save last packets sn ratio
long last_packet_freq_err;    // Records the last packets frequency error in hz (look at documentation


void setHwPins(int enable_pin, int reset_pin, int interrupt_pin )
{
  enable = enable_pin;
  resetPin =  reset_pin;
  interruptPin = interrupt_pin;
}

void setDestId(byte id)
{
	DESTID = id;	
}


int initRadio()
{
  LoRa.setPins(enable, resetPin, interruptPin); // configure pinout 
  LoRa.setTxPower(tx_power);
  LoRa.setFrequency(frequency);
  LoRa.setSpreadingFactor(spreading_factor);
  LoRa.setSignalBandwidth(sig_bandwidth);
  LoRa.setCodingRate4(coding_rate);
  LoRa.setPreambleLength(lora_preamble);
  LoRa.setSyncWord(sync_word);
  LoRa.onReceive(onReceive);

  if (!LoRa.begin(frequency))
  {             // initialize ratio at 915 MHz        
    return -1;
  }
  else
  return 1;
}

/********* TRANSMIT **************/
//Creates and sends a lora packet, errors are handled and status is updated and send to the serial uplink 
int sendLoraPacket(char* data, int data_size)
{
  char lora_packet[RADIOPACKETSIZE] =  {0}; 
  int msg_start =0;

  lora_packet[msg_start++] = HARDWAREID;
  lora_packet[msg_start++] = DESTID;

                                                                                 
  if(data_size>= (RADIOPACKETSIZE - (msg_start+2)) || data_size < 1) // Make sure the message is small enough to send as a single packet
  {
    //message is too large to send in a lora packet
    return -1;
  }
  for(int loc=0; loc<data_size; loc++)
  {
    lora_packet[msg_start++] = data[loc]; //write message collected to the packet
  }

  if(LoRa.beginPacket() == 0)
  {
   return -1;    
  }
    
  int sent_bytes = LoRa.write((byte *)lora_packet, RADIOPACKETSIZE); // write lora_packet buffer to the radio buffer

  if(LoRa.endPacket(async_mode) == 0)
  {    
    return -1; 
  } 
  
  LoRa.idle();
  initRadio();

  return sent_bytes; 
  
}

/***************** Recieve ***************/
/*
 * Forward a message to the serial output from the radio buffer immediately 
 */
void onReceive(int packetSize) {
  if(LoRa.available()<2)          // if there's no packet, return
	{
		LoRa.flush();
	}
 
	
  char serial_message[RADIOPACKETSIZE] ={0};  //create the message portion of the serial packet
  
  // Get stats from packet
  last_packet_rssi = LoRa.packetRssi(); //rssi
  last_packet_snr = LoRa.packetSnr();   //snr
  last_packet_freq_err = LoRa.packetFrequencyError();
  
  //make sure the device ids are correct 
  if(LoRa.read() != DESTID || LoRa.read() != HARDWAREID)
  {
    //Message is not for this device exit and free mem
    return;
  }
 
	LoRa.readBytes(serial_message, RADIOPACKETSIZE);

  //Write our message to serial 
  sendSerial(STATUSCODES[1], 0, serial_message, RADIOPACKETSIZE);

 LoRa.idle();
 initRadio(); 
  
}


/*************************************************************/
/*                    SERIAL SECTION                         */
/*************************************************************/

int sendSerial(char header1, char header2, char* data, int data_size)
{
  
  char serial_packet[SERIALPACKETSIZE]={0}; 
  int msg_start = 0;

  //Insert headers
  serial_packet[msg_start++] = header1;
  serial_packet[msg_start++] = header2;

  if(data_size>= (SERIALPACKETSIZE - msg_start) || data_size < 1)
  {
    //message is too large to send in a serial packet   
    return -1;
  }
  
 for(int i = 0; i<data_size; i++)
 {
	serial_packet[msg_start++] = data[i];	 
 }
 
  int sent = Serial.write(serial_packet, SERIALPACKETSIZE);//send serial packet

  return sent;
}

int sendSerial(char header1, char header2)
{
  char serial_packet[2] = {0};  
  int msg_start = 0;

  //Insert messages
  serial_packet[msg_start++] = header1;
  serial_packet[msg_start++] = header2;

  int sent = Serial.write(serial_packet, sizeof(serial_packet));//send serial packet
 
  return sent;
  
}

/*************** Recived Serial **************************/
void serialEventHandler()
{       
	if(Serial.available()<2)
	{
		Serial.flush();  // Cant use it, flush serial buffer
		return;
	}
 
  //make a details message from the remaining serial contents  
 
  int buff_size = Serial.available();
  char buff[buff_size]={0};
  
  Serial.readBytes(buff, buff_size);
  
  byte device_id = buff[0];
  byte instruction = buff[1];
  char data[buff_size-2]={0};
	for (int i =0; i< buff_size-2; i++)
	{
		data[i] = buff[i+2];				
	}

  commandHandler(instruction, data, (buff_size-2));
  
  Serial.flush();
}



/*************************************************************/
/*                   COMMAND SECTION                         */
/*************************************************************/

void commandHandler(byte instruction, char* data, int data_size)
{
    
	if(instruction == 49)                      // Instruction to idle
    {
		LoRa.idle();		
	}
    
    if(instruction == 50)                // Instruction to sleep
    {
		LoRa.sleep();		
	}
	
   if(instruction == 51)                // Instruction to modify radio
   { 
		if(data[0] != 0x00 )
		frequency = data[0] ;          // Set Operating frequency
		if(data[1] != 0x00 )
		tx_power = data[1];            // Transmit Power level
		if(data[2] != 0x00 )
		spreading_factor = data[2];    // Spreading Factor
		if(data[3] != 0x00 )
		sig_bandwidth = data[3];       // Signal Bandwidth
		if(data[4] != 0x00 )
		coding_rate = data[4];         // Coding Rate denominator
		if(data[5] != 0x00 )
		lora_preamble = data[5];       // Set lora radio preamble length
		if(data[6] != 0x00 )
		sync_word = data[6];           // Lora Sync Word
		if(data[7] != 0x00)
		async_mode = false;            // Packet send in async mode
		if(data[7] != 0x01)
		async_mode = true;             // Packet send in async mode
		if(initRadio()==-1)                // Initialize the radio with new setting
		{
		  sendSerial(STATUSCODES[2], ERRCODES[1]); // There was an init error, return error code   
		}		                    
	}

    if(instruction == 52)                    // Instructions to send message
    {
		if(sendLoraPacket(data, data_size)==-1);
		{
		  sendSerial(STATUSCODES[2], ERRCODES[2]);  // There was an error, return error code   
		}		
	}
    
    else
    {		
		 sendSerial(STATUSCODES[2], ERRCODES[1], data, data_size);	     
    }  
	
	return;   
}