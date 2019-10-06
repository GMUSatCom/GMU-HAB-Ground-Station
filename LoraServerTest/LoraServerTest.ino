#include <SPI.h>              // include libraries spi and LoRa From Sandeepmisty
#include <LoRa.h>

/*
   Vaughn Nugent
   Gmu Sat Com Hab Ground Station working sketch for testing LoRa Radio 
   Using LoRa libraries from Sandeep Mistry for Arduino
*/

/*  This needs to go into the serial buffer files otherwise the buffer size wont be increased 
 
#define SERIAL_TX_BUFFER_SIZE 260 // Increase hardware serial buffers to 256 bytes.
#define SERIAL_RX_BUFFER_SIZE 250

#define _SS_MAX_RX_BUFF 256 // Software serial buffer size increase
#define _SS_MAX_TX_BUFF 260

*/

#define SERIALPACKETSIZE 260
#define RADIOPACKETSIZE 252
#define BAUDRATE 9600

const byte HARDWAREID = 0x00;
                         
const byte *ERRCODES[6] = {
0x00, 
0x02, // Lora init error
0x03, // Lora send error
0x04, // Lora preamble missmatch
0x00, 
0x00};

const byte *STATUSCODES[6] = {
  0x01, // Init status
  0x02, // OK! status
  0x03, // ERR Status (pair with ERRCODE) 
  0x04, // Warning
  0x00, 
  0x00};

const byte *PREAMBLE[10] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

//LORA const start
const int enable = 7;             // LoRa radio chip select
const int resetPin = 6;           // LoRa radio reset
const int interruptPin = 1;       // change for your board; must be a hardware interrupt pin

//Radio Vars 
long frequency = 915E6;       // Set Operating frequency 
int tx_power = 23;            // Transmit Power level
int spreading_factor = 12;    // Spreading Factor
long sig_bandwidth = 0;       // Signal Bandwidth
int coding_rate = 0;          // Coding Rate denominator
long lora_preamble = 10;      // Set lora radio preamble length
int sync_word = 0;            // Lora Sync Word
boolean async_mode = false;   // Packet send in async mode

//Lora Packet Vars 
byte DESTID = 0xFF;           // destination to send to
int send_delay = 200;         // interval between sends (not used)
byte HEADER1 = 0x00;          // Lora packet header byte 1
byte HEADER2 = 0x00;          // Lora packet header byte 2

int last_packet_rssi;         // Save the last packets rssi 
float last_packet_snr;        // Save last packets sn ratio
long last_packet_freq_err;    // Records the last packets frequency error in hz (look at documentation

int current_status_code;
int active_err_code;

void setup() { 
  pinMode(13,OUTPUT);
  current_status_code = 0; //init status
  Serial.begin(BAUDRATE);    // initialize serial and wait for serial connection to host
  while (!Serial);
 
  // Set lora control pins, only doing this once on setup, not every time we init the radio
  LoRa.setPins(enable, resetPin, interruptPin);// set CS, reset, IRQ pin

  if(initLora() == -1)
  {
    while(current_status_code == 2)
    {
      //Flash led with error code. Flash 3 times then go out for 3 seconds 
      analogWrite(13,255); 
      delay(200); 
      analogWrite(13,0);
      delay(200); 
      analogWrite(13,255); 
      delay(200); 
      analogWrite(13,0);
      delay(200);
      analogWrite(13,255); 
      delay(200); 
      analogWrite(13,0); 
      delay(3000);
    }
  }
  
  current_status_code = 1; //init complete and ready to handle lora coms
}

void loop() {
  
   onReceive(LoRa.parsePacket()); // submit event handler 
}


/*************************************************************/
/*                    LORA SECTION                           */
/*************************************************************/

//Init lora radio with the values currently configured in the lora constants section 
int initLora()
{  
  LoRa.setTxPower(tx_power);
  LoRa.setFrequency(frequency);
  LoRa.setSpreadingFactor(spreading_factor);
  LoRa.setSignalBandwidth(sig_bandwidth);
  LoRa.setCodingRate4(coding_rate);
  LoRa.setPreambleLength(lora_preamble);
  LoRa.setSyncWord(sync_word); 

  if (!LoRa.begin(frequency))
  {             // initialize ratio at 915 MHz
    current_status_code = 2;  // Set error status code
    active_err_code = 1;
    String message = "There was an issue starting the lora radio"; 
    byte m[sizeof(message)];
    message.getBytes(m, sizeof(message));
    sendSerial(current_status_code, active_err_code , m); //submit current status code and error code 1 (lora init error)  along with the error message 
    return -1;
  }
  else
  return 1;  
}

/********* TRANSMIT **************/
//Creates and sends a lora packet, errors are handled and status is updated and send to the serial uplink 
int send_lora_packet(byte * message)
{
  byte lora_packet[RADIOPACKETSIZE] =  {0x00}; 
  addPreamble(lora_packet);
  int msg_start = sizeof(PREAMBLE);

  lora_packet[msg_start++] = HARDWAREID;
  lora_packet[msg_start++] = DESTID;
  lora_packet[msg_start++] = HEADER1;
  lora_packet[msg_start++] = HEADER2;   

                                                                                 
  if(sizeof(message)>= (RADIOPACKETSIZE - (msg_start+10)) || sizeof(message) < 1) // Make sure the message is small enough to send as a single packet
  {
    //message is too large to send in a lora packet
    free(lora_packet);
    return -1;
  }
  for(int loc=0; loc<sizeof(message); loc++)
  {
    lora_packet[msg_start++] = message[loc]; //write message collected to the packet
  }

  if(LoRa.beginPacket() == 0)
  {
    current_status_code = 2;  // Set error status code
    active_err_code = 2;
    String message = "Radio is busy or failed"; 
    byte m[sizeof(message)];
    message.getBytes(m, sizeof(message));
    sendSerial(current_status_code, active_err_code, m); //submit current status code and error code 1 (lora send error) along with the error message 
    return -1;    
  }
    
  int sent_bytes = LoRa.write(lora_packet, RADIOPACKETSIZE); // write lora_packet buffer to the radio buffer

  if(LoRa.endPacket(async_mode) == 0)
  {
    current_status_code = 2;  // Set error status code
    active_err_code = 2;
    String message = "Failed sending LoRa Packet"; 
    byte m[sizeof(message)];
    message.getBytes(m, sizeof(message));
    sendSerial(current_status_code, active_err_code, m); //submit current status code and error code 1 (lora send error) along with the error message 
    return -1; 
  } 

  return sent_bytes; 
  
}

/***************** Recieve ***************/
/*
 * Forward a message to the serial output from the radio buffer imediatly 
 */
void onReceive(int packetSize) {
  if (packetSize == 0) return;          // if there's no packet, return
  
  int serial_msg_start = 0;  
  byte serial_message[RADIOPACKETSIZE] ={0x00};  //create the message portion of the serial packet
   
  // Get stats from packet
  last_packet_rssi = LoRa.packetRssi(); //rssi
  last_packet_snr = LoRa.packetSnr();   //snr
  last_packet_freq_err = LoRa.packetFrequencyError();

  //check preamble/sync 
  while(1)
  {
    if(LoRa.peek() == 0x00) // preabmle zeros 
      LoRa.read();
    else if( LoRa.peek() == -1)
    {
      current_status_code = 3;
      active_err_code = 3;
      return;
    }    
    else
      break;
  }
  
  //make sure the device ids are correct 
  if(LoRa.read() != DESTID || LoRa.read() != HARDWAREID)
  {
    //Message is not for this device exit and free mem
    free(serial_message);
    return;
  }
  
  while (LoRa.available()) {
    serial_message[serial_msg_start++] = LoRa.read(); // add all recived bytes to the message section 
  }

  //Write our message to serial 
  sendSerial(current_status_code, active_err_code, serial_message);

  return;   
  
}


void addPreamble(byte *buff)
{
  if(sizeof(buff)< 10) //make sure the buffer has enough room for preamble
  return;

  for(int i = 0; i<= sizeof(PREAMBLE); i++)
  {
    buff[i] = PREAMBLE[i];
  }
}


/*************************************************************/
/*                    SERIAL SECTION                         */
/*************************************************************/
int sendSerial(int status_code, int error_code, byte * message)
{
  byte *serial_packet = malloc(SERIALPACKETSIZE * sizeof(byte));
  int msg_start = 0;

  //Insert messages
  serial_packet[msg_start++] = HARDWAREID;
  serial_packet[msg_start++] = STATUSCODES[status_code];
  serial_packet[msg_start++] = ERRCODES[error_code];

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

   int sent = Serial.write((byte *) &serial_packet, sizeof(serial_packet));//send serial packet

  free(serial_packet); // free packet now its sent

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
  Serial.println("poopy");

  commandHandler(instruction, details);

}



/*************************************************************/
/*                   COMMAND SECTION                         */
/*************************************************************/


void commandHandler(word instruction, byte * details)
{
  switch(instruction){
    
     case 0x0002 :              // Instruction to get status
        getStatus();
        break;
        
    case 0x0003 :               // Instruction to reset status 
        resetStatus();
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
       initLora();                       // Initialize the radio with new settings       
        break;

      case 0x000a :           // Instructions to send message
        send_lora_packet(details); 
        break;

      default :
        String message = "Default Message"; 
        byte m[sizeof(message)];
        message.getBytes(m, sizeof(message));
        sendSerial(current_status_code, active_err_code, m); 
        

  }
}

void resetStatus()
{
      current_status_code = 0;    // Set OK status code
      active_err_code = 0;       // Set No error code 
      String message = "Status set to OK"; 
      byte m[sizeof(message)];
      message.getBytes(m, sizeof(message));
      sendSerial(current_status_code, active_err_code, m); 
}
void getStatus()
{
      String message = "Getting status"; 
      byte m[sizeof(message)];
      message.getBytes(m, sizeof(message));
      sendSerial(current_status_code, active_err_code, m); 
}
