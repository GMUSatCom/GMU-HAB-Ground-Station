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

#pragma once 

#ifndef LORASERVICE_H_
#define LORASERVICE_H_

#include <Arduino.h>
#include <LoRa.h>
#include <stdlib.h>

class LoRaServiceClass {
public:    
    
     LoRaServiceClass();
    
  typedef enum Command
  {
    INITRD = 2, IDLERD = 3, SLEEPRD = 4, SETRD = 5, SEND = 8, PACKETINFO = 6
  } Command;

  typedef enum Status
  {
    INIT = 0, OKAY = 1, WARNING = 3, ERR = 4
  } Status;

  typedef enum Errors
  {
    no_err = 0, lora_init_err = 1, lora_send_err = 2, lora_receive_err = 3
  }Errors;
    
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


private:

  //Is called from serialEventHandler and will handle a given command and various other functions necessary to complete the command
  bool commandHandler(char instruction, char* data, int data_size);
    
  };  
  
  extern LoRaServiceClass LoRaService;
  
#endif /* LORASERVICE_H_ */
