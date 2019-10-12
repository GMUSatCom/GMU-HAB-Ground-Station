/*
 * LoRaService.h
 *
 * Created: 10/12/2019 1:40:09 AM
 *  Author: Vaughn Nugent
 */ 


#ifndef LORASERVICE_H_
#define LORASERVICE_H_

#include "sam.h"
#include "arduino.h"
#include "HardwareSerial.h"
#include "LoRa.h"

const unsigned int RADIOPACKETSIZE = 252;
const unsigned int  SERIALPACKETSIZE = 260;
const unsigned int  BAUDRATE = 9600;

const byte HARDWAREID = 0x00;

int initLora();

void setHwPins(int enable_pin, int reset_pin, int interrupt_pin );

void setHwPins(int enable_pin, int reset_pin, int interrupt_pin );

int sendLoraPacket(byte * message);

void onReceive(int packetSize);

int sendSerial(byte header1, byte header2, byte * message);

int sendSerial(byte header1, byte header2);

void serialEvent();

void commandHandler(word instruction, byte * details);



#endif /* LORASERVICE_H_ */
