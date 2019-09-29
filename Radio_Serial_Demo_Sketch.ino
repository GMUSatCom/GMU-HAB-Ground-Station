/*
Vaughn Nugent 
Sample Serial coms code for GMU HAB Radio Team

*/


int analog_out_pins[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13}; //Just the numbers of the analog pins (dont worry about the details)
int pin_status[14] =    {0,0,0,0,0,0,0,0,0,0,0,0,0,0}; //13 initialize low cuz c programming


void setup() {
      for(int i=0; i<sizeof(analog_out_pins); i++)
      {
        pinMode(analog_out_pins[i], OUTPUT); //typicall (sets the output regsters can look up online if you want)
      }
      
      Serial.begin(9600); //sets the baud rate, pretty typical rate for stuff like this
      /*
       * "The baud rate is the rate at which information is transferred in a communication channel. 
       * In the serial port context, "9600 baud" means that the serial port is capable of transferring a maximum of 9600 bits per second."
       * equals 1200 bytes per second
       */
}

void loop() {

   //An ISR is called when serial data is incoming to load the incoming data to a buffer
   // just sit here and do nothing for now, the serialEvent() handler written below is invoked when a serial event is raised ie: when the ISR is done writing to the buffer  
    
}



/* This method is called when an interrupt service routine handles a serial input 
 *  and raises the serialEvent flag
 * This means whenever the arduino handles a serial receive (so when you send data to it) 
 * this even gets raised so everything else (below interrupts) will happen after this function exits
 */
void serialEvent()
{
//statements
  byte command[2];
 if(Serial.available()>0)
 {
  Serial.readBytes(command, sizeof(command));
  Serial.flush(); // since we read as many bytes as we can accept (2) flush the buffer because we cant handle any more commands. 
  // If we did not flush, we could handle the 2 byte command, then handle the next 2 byte command. This would by ideal assuming the commands were correct and there isnt any corrupt data in the buffer for whatever reason
  // The reason I am flushing the buffer it to make sure there are only 2 bytes at any given time in the buffer assuming the commands will be handled before the master program has time to send another, so no commands will un handled
 }
 commandHandler(command);
 
 
}

void commandHandler(byte invokedCommand[])
{
    // We know the invokedCommand byte array will be 2 bytes in length from above, which means on commands that dont use the second byte can just be ingnored since we are only using the second byte to specify the pin value



    if(invokedCommand[0]== 1) //It is a byte that has an integer value equivalent to 1. Remember an integer is 2 bytes on these boards, so the rest is just padded. Basically we can just compare the byte to an decimal value because the operations work identical
    {
       //Flash Led Program defined below
       flashLed();
     
       String s = "Done!";
       byte output_buff[sizeof(s)]; 
       s.getBytes(output_buff, sizeof(s));// Taking an ascii string and breaking its bytes into a buffer (byte array) 
       
       Serial.write(output_buff,sizeof(output_buff)); // Serial write sends a single byte one after the other 
       return; //to end the function incase we want to do other stuff below
             
    }
  
   
    if(invokedCommand[0]== 2) 
    {
      /* We know this must be a command to PinHi
       * Right now we just hope that the program wont send an invalid pin code byte (the second byte in the command array)
       * and that it won't send the command without the pin code. There are NO CHECKS in this code, all the checks are done
       * in the software application to keep things a little simpler. 
       */

      analogWrite(invokedCommand[1], 255); // Set the commanded pin 
      pin_status[invokedCommand[1]] = 255; // set the position for the pins (0-13) in the pin_status array so we know it was set high when we check later 
      
     
       String s = "Done!";
       byte output_buff[sizeof(s)]; 
       s.getBytes(output_buff, sizeof(s));// Taking an ascii string and breaking its bytes into a buffer (byte array) 
       
       Serial.write(output_buff,sizeof(output_buff)); // Serial write sends a single byte one after the other 
       return; //to end the function incase we want to do other stuff below     
      
    }
    
    
    if(invokedCommand[0]== 3)
    {
      /* We know this must be a command to PinLo
       * Right now we just hope that the program wont send an invalid pin code byte (the second byte in the command array)
       * and that it won't send the command without the pin code. There are NO CHECKS in this code, all the checks are done
       * in the software application to keep things a little simpler. 
       */

      analogWrite(invokedCommand[1], 0);
      pin_status[invokedCommand[1]] = 0; // set the position for the pins (0-13) in the high pins arry 


    //Since our program can interpret strings, we can just write whatever we want back to the serial console. In the future, we will need to structre this most likely a 'word' array so we can control it. We can do other managment or error
    //checking in software after the data has been pushed to the serial listener     
       
      String s = "Done!";
      byte output_buff[sizeof(s)];
      s.getBytes(output_buff, sizeof(s));
       
      Serial.write(output_buff,sizeof(output_buff));
      return;
    }
    
    
   if(invokedCommand[0]== 4)
     {

      /* Personally this command is my favorite. This status command is important to us because, when it is called, 
       * it must respond with lookup data from the stored pin_status state table
       */
      
       String output = "This is the current pin state\n";       
      
       for(int i = 0; i<14; i++)
       {
        output= output + "Pin: " + analog_out_pins[i] + " state= " + pin_status[i] + "\n";             
       }
       //Alternativley we can use the print command, since this is a large string, and the buffer generated is much larger than the serial buffer can handle and outputs errors. 
       Serial.print(output);              
     
       return;
     }   

       // sent back connected message
       String s = "Connected to Arduino device with Vaughn Nugent's, HAB Radio Demo firmware!";      
       Serial.print(s);   
       return; 
    
    
}


void flashLed()
{
  //flash the onboard led for 300ms with 500ms delay 5 times when called
  int c=0;
  int saved_state = pin_status[13];
  while(c<5)
  {
  analogWrite(13,255);
  pin_status[13]=255;
  delay(300);
  analogWrite(13,0);
  pin_status[13]=0;
  delay(500);
  c++; // ;)~
  }
  pin_status[13] = saved_state;
  analogWrite(13,saved_state);
}
