/*
 * LoRaService.cpp
 *
 * Created: 10/11/2019 10:54:14 PM
 * Author : Vaughn Nugent
 */ 


#include "LoRaService.h"

const byte *ERRCODES[6] = {
  0x00,
  0x02, // Lora init error
  0x03, // Lora send error
  0x04, 
  0x00,
  0x00};

const byte *STATUSCODES[6] = {
  0x01, // Init status
  0x02, // OK! status
  0x03, // ERR Status (pair with ERRCODE)
  0x04, // Warning
  0x00,
  0x00};

const byte *HEADERCODES[6]
{ 0x01, //acknowledge
  0x02,
  0x03,
  0x04,
  0x05,
  0x06  
  };



//LORA const start
int enable = 7;             // LoRa radio chip select
int resetPin = 6;           // LoRa radio reset
int interruptPin = 1;       // change for your board; must be a hardware interrupt pin

//Radio Vars
long frequency = 915E6;       // Set Operating frequency
int tx_power = 23;            // Transmit Power level
int spreading_factor = 12;    // Spreading Factor
long sig_bandwidth = 0;       // Signal Bandwidth
int coding_rate = 0;          // Coding Rate denominator
long lora_preamble = 10;      // Set lora radio preamble length
int sync_word = 0;            // Lora Sync Word
bool async_mode = false;      // Packet send in async mode

//Lora Packet Vars
byte DESTID = 0xFF;           // destination to send to
int send_delay = 200;         // interval between sends (not used)

int last_packet_rssi;         // Save the last packets rssi
float last_packet_snr;        // Save last packets sn ratio
long last_packet_freq_err;    // Records the last packets frequency error in hz (look at documentation


int main(void)
{
    /* Initialize the SAM system */
   SystemInit();
   Serial.begin(BAUDRATE);
   while(!Serial);
      
   if(initLora()==-1)
   {
    sendSerial('1','0');
   } 

    while(1)
    {
       onReceive(LoRa.parsePacket()); // submit callback function
    }
}


/*************************************************************/
/*                    RADIO SECTION                          */
/*************************************************************/


void setHwPins(int enable_pin, int reset_pin, int interrupt_pin )
{
  enable = enable_pin;
  resetPin =  reset_pin;
  interruptPin = interrupt_pin;
}


int initLora()
{
  LoRa.setPins(enable, resetPin, interruptPin); // configure pinout 
  LoRa.setTxPower(tx_power);
  LoRa.setFrequency(frequency);
  LoRa.setSpreadingFactor(spreading_factor);
  LoRa.setSignalBandwidth(sig_bandwidth);
  LoRa.setCodingRate4(coding_rate);
  LoRa.setPreambleLength(lora_preamble);
  LoRa.setSyncWord(sync_word);

  if (!LoRa.begin(frequency))
  {             // initialize ratio at 915 MHz        
    return -1;
  }
  else
  return 1;
}

/********* TRANSMIT **************/
//Creates and sends a lora packet, errors are handled and status is updated and send to the serial uplink 
int sendLoraPacket(byte * message)
{
  byte lora_packet[RADIOPACKETSIZE] =  {0x00}; 
  int msg_start =0;

  lora_packet[msg_start++] = HARDWAREID;
  lora_packet[msg_start++] = DESTID;

                                                                                 
  if(sizeof(message)>= (RADIOPACKETSIZE - (msg_start+2)) || sizeof(message) < 1) // Make sure the message is small enough to send as a single packet
  {
    //message is too large to send in a lora packet
    return -1;
  }
  for(int loc=0; loc<sizeof(message); loc++)
  {
    lora_packet[msg_start++] = message[loc]; //write message collected to the packet
  }

  if(LoRa.beginPacket() == 0)
  {
   return -1;    
  }
    
  int sent_bytes = LoRa.write(lora_packet, RADIOPACKETSIZE); // write lora_packet buffer to the radio buffer

  if(LoRa.endPacket(async_mode) == 0)
  {    
    return -1; 
  } 

  return sent_bytes; 
  
}

/***************** Recieve ***************/
/*
 * Forward a message to the serial output from the radio buffer immediately 
 */
void onReceive(int packetSize) {
  if (packetSize == 0) return;          // if there's no packet, return
  
  int serial_msg_start = 0;  
  byte serial_message[RADIOPACKETSIZE] ={0x00};  //create the message portion of the serial packet
  
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
    
  while (LoRa.available()) {
    serial_message[serial_msg_start++] = LoRa.read(); // add all received bytes to the message section 
  }

  //Write our message to serial 
  sendSerial(STATUSCODES[1], 0x00, serial_message);

  return;   
  
}


/*************************************************************/
/*                    SERIAL SECTION                         */
/*************************************************************/

int sendSerial(byte header1, byte header2, byte * message)
{
  byte *serial_packet = malloc(SERIALPACKETSIZE * sizeof(byte));
  int msg_start = 0;

  //Insert messages
  serial_packet[msg_start++] = header1;
  serial_packet[msg_start++] = header2;

  if(sizeof(message)>= (SERIALPACKETSIZE - msg_start) || sizeof(message) < 1)
  {
    //message is too large to send in a serial packet
    free(serial_packet);
    return -1;
  }
  for(int loc=0; loc<sizeof(message); loc++)
  {
    serial_packet[msg_start++] = message[loc]; //write message collected to the packet
  }

  int sent = Serial.write(serial_packet, sizeof(serial_packet));//send serial packet

  free(serial_packet); // free packet now its sent

  return sent;
  
}

int sendSerial(byte header1, byte header2)
{
  byte *serial_packet = malloc(2 * sizeof(byte));
  int msg_start = 0;

  //Insert messages
  serial_packet[msg_start++] = header1;
  serial_packet[msg_start++] = header2;

  int sent = Serial.write(serial_packet, sizeof(serial_packet));//send serial packet

  free(serial_packet);
  return sent;
  
}

/*************** Recived Serial **************************/
void serialEvent()
{       
  byte device_id = Serial.read();
  word instruction = {Serial.read() << 8 | Serial.read()};

  //make a details message from the remaining serial contents
  byte details[Serial.available()]={0x00};
  int i=0;
  
  while(Serial.available())
  {
    details[i] = Serial.read();
    i++;
  }
  commandHandler(instruction, details);

}



/*************************************************************/
/*                   COMMAND SECTION                         */
/*************************************************************/

void commandHandler(word instruction, byte * details)
{
  switch(instruction){
    
    case 0x0002 :              // Instruction to idle
    LoRa.idle();
    break;
    
    case 0x0003 :               // Instruction to sleep
    LoRa.sleep();
    break;

    case 0x0005 :               // Instruction to modify radio
    if(details[0] != 0x00 )
    frequency = details[0] ;        // Set Operating frequency
    if(details[1] != 0x00 )
    tx_power = details[1];         // Transmit Power level
    if(details[2] != 0x00 )
    spreading_factor = details[2]; // Spreading Factor
    if(details[3] != 0x00 )
    sig_bandwidth = details[3];    // Signal Bandwidth
    if(details[4] != 0x00 )
    coding_rate = details[4];      // Coding Rate denominator
    if(details[5] != 0x00 )
    lora_preamble = details[5];    // Set lora radio preamble length
    if(details[6] != 0x00 )
    sync_word = details[6];        // Lora Sync Word
    if(details[7] != 0x00)
    async_mode = false;            // Packet send in async mode
    if(details[7] != 0x01)
    async_mode = true;             // Packet send in async mode
    if(initLora())                 // Initialize the radio with new setting
    {
      sendSerial(STATUSCODES[2], ERRCODES[1]); // There was an init error, return error code   
    }                       
    break;

    case 0x000a :           // Instructions to send message
    if(sendLoraPacket(details)==-1);
    {
      sendSerial(STATUSCODES[2], ERRCODES[2]);  // There was an error, return error code   
    }
    break;

    default :
    String message = "Default Message";
    byte m[sizeof(message)];
    message.getBytes(m, sizeof(message));
    sendSerial(STATUSCODES[2], 0x00, m);  

  }
}
