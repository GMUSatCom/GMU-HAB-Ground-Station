using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading;


/*
 * Vaughn Nugent Sept 28, 2019
 * Demo program for semi-complicated serial transmission between arduino and the System Consol Host
 * 
 * This program is not meant to be complete and I am not that great of a programmer, this is meant to be 
 * a demo for the GMU High Altitude Balloon Radio team to understand low level serial comunication over UART between a 
 * System Console Host (commands) and a MCU, more specifically an arduino mega and uno. 
 * 
 */


namespace Arduino_USB_Console
{
    class Program
    {
        /*This will function as a lookup table for the numbers of the configured pins on the ardunio 
        *For the arduino, it would likely benefit by using a word array to be able to encode 2 byte values
        * 
        * The reson I am using lookup tables, is that within the program you can just call an index and it will respond
        * with a value from this global static table. This means the values in these tables can represent much more complex
        * codes and simply called by indexing the array
        */
        private static readonly byte[] ard_pins = { // decimal values here are just represented here, but are actually just a byte and stored in this array
                0,
                1,
                2,
                3,
                4,
                5,
                6,
                7,
                8,
                9,
                10,
                11,
                12,
                13,
            };
        //List of commands as a single byte that must be implemented by our arduino firmware. A value will be used from this byte array and sent to the ard over serial
        //For this demo, they are just integers here and in the firmware, and I am only using values 1-4 (index 0-3) for the demo options
        private static readonly byte[] commands = {
                1, 
                2,
                3,
                4,
                5,
                6,
                7,
                8,
                9,
                10,                
         };

       private static readonly Int32[] rates = { 4800, 9600, 19200, 115200 }; // Baud rates table in case you wanted to specify more rates

        static void Main(string[] args)
        {
            int port_num = -1;
            while (port_num < 0)
            {
                port_num = SelectPortNum(); //get the com number your arduino is on 
                if (port_num == -2)
                {
                    return;
                }
            }
            int baud_rate = -1;
            while (baud_rate < 0)
            {
                baud_rate = SelectBaud(); //allow the user to select the baud rate from the console, baud rate needs to be the same on both sides for this to work
                if (baud_rate == -2)
                {
                    return;
                }
            }

            // Yeeeeet

            SerialHandler sh = new SerialHandler(); //Make a new serial handler object from the SerialHandler Class I made
            if (!sh.Start(port_num, baud_rate))
            {
                Console.WriteLine("There was a problem opening the port");
                Main(null);
            }

            //Console.CancelKeyPress += new ConsoleCancelEventHandler(exitHandler); // If I want to handle the ctrl + c exit event ie close the serial port I can just attach the event handler

            Console.WriteLine("Listening for commands. Type 'c' for list of commands. Type 'exit' to exit");
            Console.WriteLine("Default commands are 'flash led', 'pinhi', 'pinlo', 'status' ");

            //************* Invoke connection message from machine by calling a command that isnt configured 
            byte[] finalcommand = { Program.commands[5] }; // 5 is not a setup command, so it will invoke the default response in firmware, with reponds with the conenction message.
            sh.WriteSerial(finalcommand);
            //**************

            while (CommandHandler(sh))
            {
                Thread.Sleep(500); //Delay a little bit, so that way the user can input another command before the board is ready to handle another interrupt without issue              
            }
        }


        private static int SelectPortNum()
        {
            Console.WriteLine("Available COM Ports. Type 'exit' to exit");
            int count = 1;
            foreach (string port in SerialHandler.GetPorts()) //Displays all available system COM Ports. goto getports method
            {
                Console.WriteLine(count + @") " + port); count++;
            }
            String captured_input = Console.ReadLine();//Get the string from the console when user presses enter
            if (captured_input == "exit") // Check string
            {
                return -2; // code for exit
            }
            try
            {
                Int32 selected_port_number = Int32.Parse(captured_input) - 1;
                if (selected_port_number >= SerialHandler.GetPorts().Length)
                {
                    Console.WriteLine("That is not a valid port number try again.");
                    return -1;
                }
                return selected_port_number;
            }
            catch
            {
                Console.WriteLine("Nah dawg I can't use this... " + captured_input + " Just type a number beside the port");
                return -1;
            }

        }

        private static int SelectBaud()
        {   
            //Displays the configured baud rates from the rates table and allows for the user to select one


            Console.WriteLine("Okay, now please select the baud rate (This demo requires 9600). Type 'exit' to exit");
            for (Int32 ind = 0; ind < Program.rates.Length; ind++)
            {
                Console.WriteLine((ind + 1) + ") " + Program.rates[ind]);
            }
            String captured_input = Console.ReadLine();
            if (captured_input == "exit") // Check for exit
            {
                return -2; // return exit code
            }
            try
            {
                Int32 selected = Int32.Parse(captured_input) - 1;
                if (selected >= 0 || selected < Program.rates.Length)
                {
                    return Program.rates[selected];
                }
                else
                    return -1; // try again 
            }
            catch
            {
                Console.WriteLine("Nah dawg I can't use this... " + captured_input + " Just type the number beside the rate");
                return -1;
            }

        }

        // Handler that would handle the crtl c or program exit events, if this wasnt a static class you would want to close and dispose of program resources here. Dont worry about it
        private static void ExitHandler(object sender, ConsoleCancelEventArgs e)
        {
            return; 
        }



        /*
         * Fun part, calling commands 
         * 
         * This method is the Command Handler
         * 
         * You will notice this section is specifc to the function of the firmware programmed to the arduino here.
         * This is the interface between the user understanding the command and making the machine understand the command 
         * in the most generic way possible. 
         * 
         * I mean this because we dont need to know the machine codes here, just their index in the commands lookup table
         */
        public static Boolean CommandHandler(SerialHandler sh)
        {           
            String captured_input = Console.ReadLine(); // first caputure console input 
            if (captured_input == "exit") // Check for exit
            {
                sh.ClosePort(); // close the port (good practice)
                return false; // will exit the loop
            }
            if (captured_input == "c") //Display list of commands again
            {
                Console.WriteLine("Default commands are 'flash led', 'pinhi', 'pinlo', 'status' ");
                return true;
            }

            if (captured_input == "flash led") // The flash led program in the firmware
            {
                byte[] finalcommand = { Program.commands[0] }; // Flash led programs command location in the lookup table
                sh.WriteSerial(finalcommand);
                return true;
            }
            
            if (captured_input == "pinhi") //set pin high program
            {
                byte[] finalcommand = { Program.commands[1], GetPinNum() };// Set a pin hi command location in the lookup table
                sh.WriteSerial(finalcommand);
                return true;
            }
           
            if (captured_input == "pinlo") // Set pin low program
            {
                byte[] finalcommand = { Program.commands[2], GetPinNum() };// Set a pin lo command location in the lookup table
                sh.WriteSerial(finalcommand);
                return true;
            }
            
            if (captured_input == "status") // Get status from board.
            {
                /*
                 * IMO most important method here. This method (according to the way its programmed in firmware,
                 * just checks the states table within the machine. That means if the pin was modified by code in the firmware
                 * it can just send an update, or we can ask it and we will see the update when we call again. So your states table 
                 * could have changed if your firmware allowed it to without this program knowing. It would just mean it would be
                 * responsible for sending state changes, or we would have to poll for it. 
                 * 
                 */

                byte[] finalcommand = { Program.commands[3] }; // Status command location
                sh.WriteSerial(finalcommand);
                return true;
            }            
            
            Console.WriteLine("Press c for list of commands "); // default if user doesnt match the names 
            return true;

        }

        private static byte GetPinNum()
        {
            //Gets the user specified pin number from the consol for some commands, *this is specifc to the demo program*

            Console.WriteLine("Type analog pin number. Valid pins are 0-13, pin 13 should be your onboard led.");
            String captured_input = Console.ReadLine();
            try
            {
                Int32 selected = Int32.Parse(captured_input) ; // makes sure the pin number is valid, and it's dependant on the values in the table 
                if (selected > 0 || selected < Program.ard_pins.Length)
                {
                    return Program.ard_pins[selected];
                }
                else
                    return GetPinNum();// outside of configured pins do it again 
            }
            catch
            {
                return GetPinNum();// just incase it couldnt parse the int, it will throw an exception just sends the user to type it in again
            }
        }
    }
}
