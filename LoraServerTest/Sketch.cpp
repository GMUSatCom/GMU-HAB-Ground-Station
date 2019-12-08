/*
 * Sketch.cpp
 *
 * Created: 10/20/2019 2:59:19 PM
 * Author: Vaughn Nugent
 * Version 1.0.2
 * Example sketch for using the LoRaServiceClass to manage the RFM95 radio
 * and serial communications. 
 */

#include "LoRaService.h"
#include "LiquidCrystal_I2C.h" //If you do not wish to use the LCD display, just comment out the header file

#ifdef FDB_LIQUID_CRYSTAL_I2C_H
LiquidCrystal_I2C lcd(0x27, 16, 2);//LCD Object Constructor
#endif

#define SERIALDELAY 5 // MS to delay the serial event check

//LCD Display vars
int cur_rssi = 0;
float cur_snr = 0;
long cur_frqerr = 0;

void setup() 
{  
//Macro to allow LoRaService to configure Serial
BEGINSERIAL;

#ifdef FDB_LIQUID_CRYSTAL_I2C_H
  //Start the LCD
  lcd.begin();
  //enable Backlight
  lcd.backlight();
#endif

  // Start the radio
  if(!LoRaService.initRadio())
  {
    pinMode(13,OUTPUT);
    //Notify serial uplink initiation failed and run a loop to flash the onboard led
    LoRaService.sendSerial(LoRaService.ERR, LoRaService.lora_init_err);

#ifdef FDB_LIQUID_CRYSTAL_I2C_H
    //Print to the LDC 
    lcd.print("Radio Init");
    lcd.setCursor(0, 1); // bottom left
    lcd.print("Failed, Check!");
#endif

    while(1) // Failed, do nothing
    {      
      analogWrite(13,255);
      delay(200);
      analogWrite(13,0);
      delay(200);      
    }
  }
  //notify our serial uplink that we initiated successfully  
  LoRaService.sendSerial(LoRaService.OKAY, LoRaService.no_err);  

#ifdef FDB_LIQUID_CRYSTAL_I2C_H
  //Sart the display 
  lcd.print(String(cur_rssi) + "dB SNR " + String(cur_snr));
  lcd.setCursor(0, 1); // bottom left
  lcd.print("Frq Err " + String(cur_frqerr));
#endif
}

void loop() {  
  setScreen();
  delay(SERIALDELAY); // How long to delay checking of the serial rx
}

/*
  SerialEvent occurs whenever a new data comes in the hardware serial RX. This
  routine is run between each time loop() runs, so using delay inside loop can
  delay response. Multiple bytes of data may be available.
*/
//Function macro inhereited from LoRaService
SERIALEVENT;

//Code to handle updating the LCD Display
void setScreen() 
{
  //If we have updated values for rssi/snr/freq_err then update the display
  //Otherwise leave the display alone
  if (cur_rssi != LoRaService.getRssi() || cur_snr != LoRaService.getSnr() || cur_frqerr != LoRaService.getFreqErr()){
    //Update values
    cur_rssi = LoRaService.getRssi(); cur_snr = LoRaService.getSnr(); cur_frqerr = LoRaService.getFreqErr();
    //Set display
#ifdef FDB_LIQUID_CRYSTAL_I2C_H
    lcd.setCursor(0,0);
    lcd.print(String(cur_rssi) + "dB SNR " + String(cur_snr));
    lcd.setCursor(0, 1); // bottom left
    lcd.print("Frq Err " + String(cur_frqerr));
#endif
    return;
  }
  return;  
}
