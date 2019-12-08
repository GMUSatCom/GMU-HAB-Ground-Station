/*
 * LoRaService.h
 *
 * Created: 10/12/2019 1:40:09 AM
 * Author: Vaughn Nugent
 * Description: Program to configure and communicate with a LoRa radio, using the Sandeepmistry library for Arduino.
 * This package relies on, and is compiled with the standard Arduino 1.8 Libraries for the ATMEGA2560 (Ardunio Mega)
 * Ideally you should increase the size of your serial buffers to match the serial packet size, but the latest version
 * of the RadioManager software will work with stock hardware serial buffers size for best stability, performance and efficiency.
 *
 * Packet sizes rely on the consts defined below
 * Implementing sketch must initiate serial 
 *
 */

#pragma once 

#ifndef LORASERVICE_H_
#define LORASERVICE_H_

#include <Arduino.h>
#include <LoRa.h>

#define RADIOPAYLOADSIZE 252 //Set the Radio Payload Packet Size
#define SERIALPACKETSIZE 260 //Set the device serial uplink packet size (Must match RadioManager software at 260 bytes)
#define REC_BAUD 115200      //Define the reccomended baud rate
#define BEGINSERIAL Serial.begin(REC_BAUD); //Setup Serial Macro
#define SERIALEVENT void serialEvent(){LoRaService.serialEventHandler();} //Serial event handler Macro


class LoRaServiceClass {
public:    
    
  LoRaServiceClass();
    
  typedef enum Command
  {
    INITRD = 0x02, IDLERD = 0x03, SLEEPRD = 0x04, SETRD = 0x05, SEND = 0x08, PACKETINFO = 0x06
  } Command;

  typedef enum Status
  {
    INIT = 0x01, OKAY = 0x02, WARNING = 0x03, ERR = 0x04
  } Status;

  typedef enum Errors
  {
    no_err = 0x10, lora_init_err = 0x01, lora_send_err = 0x02, lora_receive_err = 0x03, invalid_command_err = 0x04, send_timeout_err = 0x05
  } Errors;
  
  //Hardware default pins (configurable)
  int _enable = 10;              // LoRa radio chip select
  int _resetPin = 9;             // LoRa radio reset
  int _interruptPin = 2;         // change for your board; must be a hardware interrupt pin
  
  //Radio Vars
  char DESTID = 10;            // destination to send to
  char HWID   = 10;            // This device ID 
      
  // Initialize the radio with the default values 
  bool initRadio();

  //Configure the hardware pins that are reqd for your radio (aside from spi pins)
  void setHwPins(int enable_pin, int reset_pin, int interrupt_pin );

  // Set values other than defaults for the radio 
  void setRadioConfigArr(long freq, int power, int sf, long bw, int crate, long preamble, int sword, bool async );

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
  bool setRadioConfigArr(char* data, int size);

  // A single byte to set the dest id to check receive 
  void setDestId(char id);
  
  // single byte to set HW id 
  void setHwId(char id);

  //Send a lora packet!
  bool sendLoraPacket(char* data, int data_size);

  //Send serial packet with 2 chars as headers and a larger data array to the serial output (usb)
  int sendSerialData(char header1, char header2, char* data, int data_size);

  //Send serial packet with only 2 chars as headers and the rest of the serial packet as zeros
  int sendSerial(char header1, char header2);

  //Callback for serial event handling, this will handle commands and serial activity 
  void serialEventHandler();
  
  // Handles callback from radio interrupt
  //Forwards a message to the serial output from the radio buffer immediately
  void callReceive(int packetSize);
  
  // Returns last packet rssi
  int getRssi();
  
  // Returns last packet snr
  float getSnr();
  
  //Returns last packet frequency error value
  long getFreqErr();
  
private:

  //Is called from serialEventHandler and will handle a given command and various other functions necessary to complete the command
  bool commandHandler(char instruction, char* data, int data_size);

  bool _implicit_header = false; // Set header mode
  int _send_timeout = 10;        // Timeout on async send
  int _async_delay = 20;         // Delay to retry sending
  long _frequency = 915E6;       // Set Operating frequency
  int _tx_power = 23;            // Transmit Power level
  int _spreading_factor = 12;    // Spreading Factor
  long _sig_bandwidth = 15.6E3;  // Signal Bandwidth
  int _coding_rate = 8;          // Coding Rate denominator
  long _lora_preamble = 10;      // Set lora radio preamble length
  int _sync_word = 0x12;         // Lora Sync Word
  bool _async_mode = false;      // Packet send in async mode
    
  };  
  
  extern LoRaServiceClass LoRaService;
  
/* Public callback function used to create the class callback function. Called during object construction */
void Receive(int packetsize);
  
#endif /* LORASERVICE_H_ */
