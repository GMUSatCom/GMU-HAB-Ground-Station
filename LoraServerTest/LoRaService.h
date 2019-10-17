/*
 * LoRaService.h
 *
 * Created: 10/12/2019 1:40:09 AM
 *  Author: Vaughn Nugent
 */ 


#ifndef LORASERVICE_H_
#define LORASERVICE_H_

//#include "sam.h"
#include "Arduino.h"
#include "LoRa.h"

const int RADIOPACKETSIZE = 252;
const int SERIALPACKETSIZE = 260;
const int BAUDRATE = 9600;

const char HARDWAREID = 0x00;

int initRadio();

void setDestId(byte id);

void setHwPins(int enable_pin, int reset_pin, int interrupt_pin );

void setHwPins(int enable_pin, int reset_pin, int interrupt_pin );

int sendLoraPacket(char* data, int data_size);

void onReceive(int packetSize);

int sendSerial(char header1, char header2, char* data, int data_size);

int sendSerial(char header1, char header2);

void serialEventHandler();

void commandHandler(byte instruction, char* data, int data_size);

#endif /* LORASERVICE_H_ */