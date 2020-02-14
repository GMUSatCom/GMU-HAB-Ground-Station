/*
 * LoRaService.h
 *
 * Created: 10/12/2019 1:40:09 AM
 * Author: Vaughn Nugent
 * Description: Program to configure and communicate with a LoRa radio, using the Sandeepmistry library for Arduino.
 * This package relies on, and is compiled with the standard Arduino 1.8 Libraries for the ATMEGA2560 (Ardunio Mega)
 * Ideally you should increase the size of your serial buffers to match the serial packet size, but the latest version
 * of the RadioManager software will work with stock hardware serial buffers size for best stability, performance and efficiency.
 */
#include "LoRaService.h"

//Captured Packet info
int _last_packet_rssi;         // Save the last packets rssi
float _last_packet_snr;        // Save last packets s/n ratio
long _last_packet_freq_err;    // Records the last packets frequency error in hz (look at documentation)

LoRaServiceClass::LoRaServiceClass() 
{  
}

void LoRaServiceClass::setHwPins(int enable_pin, int reset_pin, int interrupt_pin )
{
  _enable = enable_pin;
  _resetPin =  reset_pin;
  _interruptPin = interrupt_pin;
}

void LoRaServiceClass::setRadioConfigArr(long freq, int power, int sf, long bw, int crate, long preamble, int sword, bool async)
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
  if( size< 20)
    return false;
  
  int i =0; 
  
  _frequency= data[i] | (data[i+1] << 8) | (data[i+2] << 16) | (data[i+3] << 24)  ;
  
  _tx_power = data[i+4] | (data[i+5] << 8) ;
  
  _spreading_factor =  data[i+6] | (data[i+7] << 8);
  
  _sig_bandwidth = data[i+8] | (data[i+9] << 8) | (data[i+10] << 16) | (data[i+11] << 24);
  
  _coding_rate =  data[i+12] | (data[i+13] << 8) ;
  
  _lora_preamble = data[i+14] | (data[i+15] << 8) | (data[i+16] << 16) | (data[i+17] << 24);
  
  _sync_word =  data[i+18] | (data[i+19] << 8) ;
  
  _async_mode =  data[i+20];
  
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
 
  if (!LoRa.begin(_frequency))
  {           
    LoRaService.sendSerial(LoRaService.ERR, LoRaService.lora_init_err);
    return false;
  }
  else
  {   	
    LoRa.idle();
    delayMicroseconds(10);
	  // Set LoRa values based on the globals configured above
	  LoRa.setTxPower(_tx_power);
    LoRa.setSpreadingFactor(_spreading_factor);
    LoRa.setSignalBandwidth(_sig_bandwidth);
    SUBMIT_CALLBACK;
    delayMicroseconds(10);    
    LoRa.receive();
    //notify our serial uplink that we initiated successfully  
    LoRaService.sendSerial(LoRaService.OKAY, LoRaService.no_err); 
    return true;
  }  
}

int LoRaServiceClass::getRssi()
{
	return _last_packet_rssi;
}

float LoRaServiceClass::getSnr()
{
	return _last_packet_snr;
}
long LoRaServiceClass::getFreqErr()
{
	return _last_packet_freq_err;
}

/********* TRANSMIT **************/
//Creates and sends a lora packet, errors are 'handled' and status is updated and send to the serial uplink 
bool LoRaServiceClass::sendLoraPacket(char* data, int data_size)
{
    char lora_packet[RADIOPAYLOADSIZE] =  {0};  
    int start = 0;
    int tm_count = 0;
    
   // wait until the radio is ready to send a packet 
   //Begin packet puts the rf95 into standby mode
   while (LoRa.beginPacket(_implicit_header) == 0)
   {
     if(tm_count >= _send_timeout)
     {
  	   sendSerial(ERR, send_timeout_err);
       LoRa.receive();
  	   return false;
     }		   
     delay(_async_delay); tm_count++;
   }	   
  
  
   // Make sure the data buffer isn't too large
   if(data_size> RADIOPAYLOADSIZE)
   {
     data_size = RADIOPAYLOADSIZE; //Set the data size to the packet size so copy will truncate the rest of the data buffer
   }  
  
  // If were not using implicit headers we need to make the header here
  if(!_implicit_header)
  {
	  LoRa.write(DESTID);    // Target address
	  LoRa.write(HWID);      // Sender Address
	  LoRa.write(0);         // Message ID
	  LoRa.write(data_size); // Add payload length
  }  

  // Copy contents from data buffer to the lora packet buffer to ensure full packet with all zeros
  for(int i = 0; i< data_size; i++)
  {
    lora_packet[start++] = data[i];
  }  
  
  int sent_bytes=LoRa.write(lora_packet,data_size);   
  
  if(LoRa.endPacket(_async_mode) == 0)
  {       
    sendSerial(ERR, lora_send_err);
    LoRa.receive();
    return false; 
  }  
  if(sent_bytes != data_size)
  {
    sendSerial(WARNING,lora_send_err);
    LoRa.receive();
    return false; 
  }
    
  sendSerial(OKAY, no_err);
  LoRa.receive();
  return true;   
}

/***************** Receive ***************/
// Forward a message to the serial output from the radio buffer immediately 
void LoRaServiceClass::callReceive(int packetSize) 
{
  // if there's no packet, return (change this to header length if using explicit headers)
  if(packetSize <=4)   
  {   
    sendSerial(WARNING, lora_receive_err);
    return;
  } 

  LoRa.idle();              //Idle mode to read fifo
  delayMicroseconds(10);    //Wait for mfg reccomended time
  
  char serial_message[packetSize-4] ={0};  //create the message portion of the serial packet
  int sz=0;
	
  // Get stats from packet
  _last_packet_rssi = LoRa.packetRssi(); //rssi
  _last_packet_snr = LoRa.packetSnr();   //snr
  _last_packet_freq_err = LoRa.packetFrequencyError();
    	
  //First 4 bytes are header info
  char recipient = LoRa.read();          // recipient address
  char sender = LoRa.read();             // sender address   
  char uk1 = LoRa.read();
  char uk2 = LoRa.read();
  //Just passing the message id and size to serial   
     
  //make sure the device ids are correct 
  if(sender != DESTID || recipient !=HWID)
  {
  	//Message is not for this device exit
	  char ids[2] = {sender,recipient};    
	  sendSerialData(WARNING, lora_receive_err, ids, 2);
    LoRa.receive(0);
	  return;
  }  
   
  // Capture payload. byte/byte is recommended  
  while(sz<packetSize && LoRa.available())
  { 	  
    serial_message[sz++] = LoRa.read();
  }  
  //Write our message to serial immediately 
  sendSerialData(OKAY, sz, serial_message, (packetSize-4)); 
  //Enable Recieve mode again
  LoRa.receive(0); 
}

/*************************************************************/
/*                    SERIAL SECTION                         */
/*************************************************************/
int LoRaServiceClass::sendSerialData(char header1, char header2, char* data, int data_size)
{
  Serial.flush();
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
  //send serial packet   

  return Serial.write(serial_packet, SERIALPACKETSIZE);
}

//Overload to send just 2 header values, used when data isn't available or needed to send 
int LoRaServiceClass::sendSerial(char header1, char header2)
{
  Serial.flush();
  
  char serial_packet[SERIALPACKETSIZE] = {0};  
  int msg_start = 0;

  //Insert headers
  serial_packet[msg_start++] = header1;
  serial_packet[msg_start++] = header2;  
 
  return  Serial.write(serial_packet, SERIALPACKETSIZE); 
}

/*************** Received Serial **************************/
void LoRaServiceClass::serialEventHandler()
{ 
  if(Serial.available()<2) // Can't use so flush the serial buffer
  {
    char left[Serial.available()];
    Serial.readBytes(left, Serial.available());   
    Serial.flush(); 
    return;
  }
  
  // Retrieve the instruction 
  char instruction = Serial.read();

   // Make sure the buffer wont be too large to stuff it into a radio packet if needed 
  int buff_size;
  if(Serial.available()> RADIOPAYLOADSIZE)
  {
    buff_size = RADIOPAYLOADSIZE;
  }
  else
  {
   buff_size = Serial.available();
  }
  
  char data[buff_size]={0};
  Serial.readBytes(data,buff_size);  
  Serial.flush();

  // Call the handler to run commands 
  commandHandler(instruction, data, buff_size); 
  
}


/*************************************************************/
/*                   COMMAND SECTION                         */
/*************************************************************/

bool LoRaServiceClass::commandHandler(char instruction, char* data, int data_size)
{
    switch(instruction)
  {   
    case INITRD :
    {
       if(initRadio())
       {
          sendSerial(OKAY, no_err);
          return true;
       }
       else
       {
          sendSerial(ERR, lora_init_err);
          return false;
       }
    }
    
    case IDLERD :               // Instruction to idle radio
    {
      LoRa.idle();
	    sendSerial(OKAY, no_err); // To acknowledge the command was run successfully
      return true;   
    }
    
    case SLEEPRD :              // Instruction to sleep
    {
      LoRa.sleep();
	    sendSerial(OKAY, no_err); // To acknowledge the command was run successfully
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
	    sendSerial(OKAY, no_err); // To acknowledge the command was run successfully
      return true;                     
    }
    
    case SEND :                   // Instructions to send message
    {        
      return sendLoraPacket(data, data_size);      
    }
    
    // Send the data back since we couldn't do anything with it 
    default :
    { //Unfound bug this is not a fix
      if(instruction != 0x00) 
          sendSerial(WARNING, invalid_command_err); 
       return true;       
    }   
  }
  return true;   
}

// Create object
LoRaServiceClass LoRaService;

//Function out of class to allow for static callback
void Receive(int packetSize)
{
	LoRaService.callReceive(packetSize);
}
