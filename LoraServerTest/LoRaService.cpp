/*
 * LoRaService.cpp
 *
 * Created: 10/11/2019 10:54:14 PM
 * Author : Vaughn Nugent
 * Description: Program to configure and communicate with a LoRa radio, using the Sandeepmistry library for Arduino.
 * this package relies on, and is compiled with the standard Arduino 1.8 Libraries for the ATMEGA2560 (Ardunio Mega)
 * this package expects you to modify your hardware serial TX and RX buffers to match the size of the Serial packet 
 * size for best stability, performance and efficiency.
 */ 

#include "LoRaService.h"
#include <stdlib.h>

const int RADIOPACKETSIZE = 251;
const int SERIALPACKETSIZE = 260;

//Hardware const start
int _enable = 10;              // LoRa radio chip select
int _resetPin = 9;            // LoRa radio reset
int _interruptPin = 2;        // change for your board; must be a hardware interrupt pin

//Radio Vars
char DESTID = 0xFF;            // destination to send to
char HWID   = 0xFF;

long _frequency = 915E6;       // Set Operating frequency
int _tx_power = 23;            // Transmit Power level
int _spreading_factor = 12;    // Spreading Factor
long _sig_bandwidth = 15.6E3;  // Signal Bandwidth
int _coding_rate = 8;          // Coding Rate denominator
long _lora_preamble = 10;      // Set lora radio preamble length
int _sync_word = 0;            // Lora Sync Word
bool _async_mode = false;      // Packet send in async mode

//Captured Packet info
int _last_packet_rssi;         // Save the last packets rssi
float _last_packet_snr;        // Save last packets s/n ratio
long _last_packet_freq_err;    // Records the last packets frequency error in hz (look at documentation)


LoRaServiceClass::LoRaServiceClass() {}

void LoRaServiceClass::setHwPins(int enable_pin, int reset_pin, int interrupt_pin )
{
  _enable = enable_pin;
  _resetPin =  reset_pin;
  _interruptPin = interrupt_pin;
}

void LoRaServiceClass::setRadioConfigArr(long freq, int power, int sf, long bw, int crate, long preamble, int sword, bool async )
{
  // Should do checks here 
  _frequency = freq;
  _tx_power = power;
  _spreading_factor = sf;
  _sig_bandwidth = bw;
  _coding_rate = crate;
  _lora_preamble = preamble;
  _sync_word = sword;
  _async_mode = async;
  
}
bool LoRaServiceClass::setRadioConfigArr(char* data, int size)
{
  if( size< 21)
    return false;
  
  int i =0; 
  
  _frequency= data[i++] | (data[i++] << 8) | (data[i++] << 16) | (data[i++] << 24)  ;
  
  _tx_power = data[i++] | (data[i++] << 8) ;
  
  _spreading_factor =  data[i++] | (data[i++] << 8);
  
  _sig_bandwidth = data[i++] | (data[i++] << 8) | (data[i++] << 16) | (data[i++] << 24);
  
  _coding_rate =  data[i++] | (data[i++] << 8) ;
  
  _lora_preamble = data[i++] | (data[i++] << 8) | (data[i++] << 16) | (data[i++] << 24);
  
  _sync_word =  data[i++] | (data[i++] << 8) ;
  
  _async_mode =  data[i++];
  
  return true; 
}

//Change the destination id 
void LoRaServiceClass::setDestId(char id)
{
  DESTID = id;  
}

void LoRaServiceClass::setHwId(char id)
{
  HWID = id;  
}


/*************************************************************/
/*                    RADIO SECTION                          */
/*************************************************************/


bool LoRaServiceClass::initRadio()
{
  LoRa.setPins(_enable, _resetPin, _interruptPin); // configure pinout 
 
 // Set LoRa values based on the globals configured above
  LoRa.setTxPower(_tx_power);
  LoRa.setFrequency(_frequency);
  LoRa.setSpreadingFactor(_spreading_factor);
  LoRa.setSignalBandwidth(_sig_bandwidth);
  LoRa.setCodingRate4(_coding_rate);
  LoRa.setPreambleLength(_lora_preamble);
  LoRa.setSyncWord(_sync_word);
  
  if (!LoRa.begin(_frequency))
  {           
    return false;
  }
  else
  {   //Enter continuous receive mode
    LoRa.receive();
    return true;
  }
  
}

/********* TRANSMIT **************/
//Creates and sends a lora packet, errors are 'handled' and status is updated and send to the serial uplink 
bool LoRaServiceClass::sendLoraPacket(char* data, int data_size)
{
  char lora_packet[RADIOPACKETSIZE] =  {0};  
  int start = 0;
  
  if(LoRa.beginPacket()==0)
  {  
      sendSerial(WARNING, lora_send_err);
      return false;
  }
  
   // Make sure the data buffer isn't too large
   if(data_size> RADIOPACKETSIZE)
   {
     data_size = RADIOPACKETSIZE; //Set the data size to the packet size so memcpy will truncate the rest of the data buffer
   }  
  
  LoRa.write(DESTID); // Target address 
  LoRa.write(HWID);   // Sender Address
  LoRa.write(0);      // Message ID
  LoRa.write(data_size); // Add payload length  

  // Copy contents from data buffer to the lora packet buffer to ensure full packet with all zeros
  for(int i = 0; i< data_size; i++)
  {
    lora_packet[start++] = data[i];
  }  
  
  int sent_bytes=0;
  // write lora_packet buffer to the radio buffer
  while(sent_bytes < data_size)
  {
   LoRa.write(lora_packet[sent_bytes++]);    
  }
  
  if(LoRa.endPacket()==0)
  {   
    return false; 
  } 
  
  //Clear the buffers by idle and re-init 
  LoRa.idle();
  initRadio();

  return true; 
  
}


/***************** Recieve ***************/
// Forward a message to the serial output from the radio buffer immediately 
void LoRaServiceClass::callReceive(int packetSize) {
  if(packetSize<4)          // if there's no packet, return
  {   
    sendSerial(WARNING, lora_receive_err);
    LoRa.flush();
  } 
  
  char serial_message[RADIOPACKETSIZE] ={0};  //create the message portion of the serial packet
    
  // Get stats from packet
  _last_packet_rssi = LoRa.packetRssi(); //rssi
  _last_packet_snr = LoRa.packetSnr();   //snr
  _last_packet_freq_err = LoRa.packetFrequencyError();
    
  //First 2 bytes are header info
  char recipient = LoRa.read();          // recipient address
  char sender = LoRa.read();            // sender address    
     
  //make sure the device ids are correct 
  if(sender != DESTID || recipient !=HWID)
  {
    char ids[2] = {sender,recipient};
    //Message is not for this device exit
   sendSerialData(WARNING, lora_receive_err, ids, 2);
    return;
  }
  
  /* This packet includes the last 2 bytes of the header, 
   * the first 2 bytes of the header will include the message ID and 
   * message length, the rest is data. 
   * the buffer is NOT null terminated.
   */
  //Capture payload
  while(LoRa.available())
  { int sm=0;
    serial_message[sm++] = LoRa.read();
  }

  //Write our message to serial immediately 
  sendSerialData(OKAY, 0, serial_message, RADIOPACKETSIZE);

  //Clear the buffers by idle and re-init 
  LoRa.idle();
  initRadio(); 
  
}


/*************************************************************/
/*                    SERIAL SECTION                         */
/*************************************************************/

int LoRaServiceClass::sendSerialData(char header1, char header2, char* data, int data_size)
{
  
   if(data_size>= (SERIALPACKETSIZE - 2) || data_size < 1)
   {
     //message is too large to send in a serial packet
     return 0;
   }
  
  char serial_packet[SERIALPACKETSIZE]={0}; 
  int msg_start = 0;

  //Insert headers
  serial_packet[msg_start++] = header1;
  serial_packet[msg_start++] = header2; 
  
 for(int i = 0; i<data_size; i++)
 {
  serial_packet[msg_start++] = data[i];  
 }
 
  int sent = Serial.write(serial_packet, SERIALPACKETSIZE);//send serial packet

  return sent;
}

int LoRaServiceClass::sendSerial(char header1, char header2)
{
  char serial_packet[SERIALPACKETSIZE-2] = {0};  
  int msg_start = 0;

  //Insert headers
  serial_packet[msg_start++] = header1;
  serial_packet[msg_start++] = header2;

 int sent = Serial.write(serial_packet, SERIALPACKETSIZE );//send serial packet
 
  // Returns the number of bytes sent
  return sent;
  
}

/*************** Recived Serial **************************/
void LoRaServiceClass::serialEventHandler()
{ 
  if(Serial.available()<2) // Can't use so flush the serial buffer
  {
    char left[Serial.available()];
    Serial.readBytes(left, Serial.available());    
  }
  
  char instruction = Serial.read();

 
  // Make sure the buffer wont be too large to stuff it into a radio packet if needed 
  int buff_size;
  if(Serial.available()> RADIOPACKETSIZE || Serial.available()==0)
  {
    buff_size = RADIOPACKETSIZE;
  }
  else
  {
   buff_size = Serial.available();
  }
  
  char data[buff_size]={0};
  int counter=0;
  
  
 while(Serial.available())
 {
   data[counter++] = Serial.read();  
 }

  commandHandler(instruction, data, (buff_size-2));
  
  // Flush the serial buffer when we are done because were not sure if we can use it later 
 if(Serial.available()>0)
 {
   char left[Serial.available()];
   Serial.readBytes(left, Serial.available());
   
 }
}



/*************************************************************/
/*                   COMMAND SECTION                         */
/*************************************************************/

bool LoRaServiceClass::commandHandler(char instruction, char* data, int data_size)
{
    switch(instruction)
  {   
    case IDLERD :               // Instruction to idle radio
    {
      LoRa.idle();
      return true;   
    }
    
    case SLEEPRD :              // Instruction to sleep
    {
      LoRa.sleep();
      return true;   
    }
  
     case SETRD :                // Instruction to modify radio
     { 
      // Initialize the radio with new settings     
      if(!setRadioConfigArr(data,data_size) && !initRadio())            
      {
        sendSerial(ERR, lora_init_err); // There was an init error, return error code  
        return false; 
      } 
      return true;                     
    }
    
    case SEND :                   // Instructions to send message
    {
      if(!sendLoraPacket(data, data_size));
      {
        sendSerial(ERR, lora_send_err);  // There was an error, return error code   
        return false; 
      }   
      return true;      
    }
    
    // Send the data back since we couldn't do anything with it 
    default :
    {   
       sendSerialData(OKAY, no_err, data, data_size); 
       return true;       
    }  
  
  }
  return true;   
}

LoRaServiceClass LoRaService;

