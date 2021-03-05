//Name: Aseem Thapa
//ID: 1001543178

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------


// Target Platform: EK-TM4C123GXL Evaluation Board
// Target uC:       TM4C123GH6PM
// System Clock:    40 MHz


//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include "tm4c123gh6pm.h"
#include "uart.h"
#include "wait.h"
#include "eeprom.h"

// PortB mask for D0 (used for PWM output)
#define GPO_Mask 1

bool metro_ON = 0; //Global variable to control metronome

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Initialize Hardware
void initHw()
{
    // Configure HW to work with 16 MHz XTAL, PLL enabled, system clock of 40 MHz
    SYSCTL_RCC_R = SYSCTL_RCC_XTAL_16MHZ | SYSCTL_RCC_OSCSRC_MAIN | SYSCTL_RCC_USESYSDIV | (4 << SYSCTL_RCC_SYSDIV_S);

    // Set GPIO ports to use APB (not needed since default configuration -- for clarity)
    SYSCTL_GPIOHBCTL_R = 0;

    // Enable clocks
    SYSCTL_RCGCPWM_R |= SYSCTL_RCGCPWM_R1;  //PWM1 clock
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R3; //Port D clk
    _delay_cycles(3);

    // Configure pins

    //Port E:
    GPIO_PORTD_DIR_R |= GPO_Mask;                       // Port D0 is output
    GPIO_PORTD_DR2R_R |= GPO_Mask;                      // Drive strength 2mA
    GPIO_PORTD_DEN_R |= GPO_Mask;                       // Enable port D0 at start
    GPIO_PORTD_AFSEL_R |= GPO_Mask;                     // Select the auxillary function (PWM output in this case)
    GPIO_PORTD_PCTL_R &= GPIO_PCTL_PD0_M;               // disable GPIO
    GPIO_PORTD_PCTL_R |= GPIO_PCTL_PD0_M1PWM0;          // enable PWM


    //Configuring the PWM1 Module:
    SYSCTL_SRPWM_R = SYSCTL_SRPWM_R1;                // reset PWM1 module
    SYSCTL_SRPWM_R = 0;                              // leave reset state

    // Port D0 == M1PWM0 which is in PWM1_Gen0_a

    PWM1_1_CTL_R = 0;                                // turn-off PWM1 generator 1
    PWM1_0_GENA_R = PWM_0_GENA_ACTCMPAD_ZERO | PWM_0_GENA_ACTLOAD_ONE;
                                                     // output 3 on PWM1, gen 1b, CMPA
    PWM1_0_LOAD_R = 0;                               // no output first

    PWM1_INVERT_R = PWM_INVERT_PWM0INV;              // Invert the output

    //For Inverted PWM signal, Duty Cycle = Compare/Load:

    PWM1_0_CMPA_R = 0;                               // no output first
    PWM1_0_CTL_R = PWM_0_CTL_ENABLE;                 // turn-on PWM1 generator 1
    PWM1_ENABLE_R = PWM_ENABLE_PWM0EN;               // enable outputs
}


//Function to write frequencies for time
void playNote (uint16_t freq, uint32_t time)
{
    uint32_t Load_val = 20000000/freq;      //This will be load value
    PWM1_0_LOAD_R = Load_val;
    PWM1_0_CMPA_R = Load_val >> 1;           //50% duty cycle
    waitMicrosecond(time);                   //play note for period: time
}

//Function that checks for note in note array:
//returns: index = found, -1 = not found
int check_for_note_in_arr (char note, char* Notes_arr)
{
    int i = 0;
    for (i=0; i<13; i++)
    {
        //When found return the index
        if (Notes_arr[i] == note) return i;
    }
    return -1;
}

//Interrupt routine for Uart0:
void Uart0ISR()
{
    //Turn off metronome
    if (metro_ON) metro_ON = 0;
}

//-----------------------------------------------------------------------------
// Main
//-----------------------------------------------------------------------------

int main(void)
{
    //Initialize hardware
    initHw();
    initUart0();
    initEeprom();

    //Display welcome message/Startup Sequence:
    int x = 0;
    for (x=0;x<128;x++)
    {
        putcUart0('*');
        waitMicrosecond(20000 - (5000 * (x/32)));
        if ((x%32) == 31) putsUart0("\r\n");
    }
    putsUart0("StartUP----------> JingleMaker\r\n");

    //Define variable:
    USER_DATA data;

    //Define arrays containing notes:
    //Note names:
    char Notes_arr[13] = {'A','b','B','C','d','D','e','E','F','g','G','a','O'};
    //NOTE-> flat notes like Db are written as lower case (Db <=> d).

    //Note frequencies:
    int Notes_freq[13] = {440,466,494,523,554,587,622,659,698,740,784,830,0};
    //NOTE-> any more notes you want can be added here with its frequency


    //Reminders to User on startup ->
    putsUart0("NOTE-> Current version supports notes from A4 - G#5 (1 octave)\r\n");
    putsUart0("NOTE-> No use of sharp symbol(#) instead used flat(b)\r\n");

    //Infinite loop:
    while (true)
    {
        //Read the data:
        getsUart0(&data);

        //Output the read data on terminal:
        putsUart0(data.buffer);

        //start from new line:
        putsUart0("\r\n");

        //Parse this data:
        parseFields(&data);

        if (isCommand(&data, "demo", 2))
        {
            //Syntax for this: demo tempo(int) sequence(string)
            //tempo range: 60 to 180 bpm
            //max no. of notes per jingle: 32 notes.
            //max one note can be held for: half note (i.e., 8 (16th)-notes)

            //check for tempo range and syntax validity
            int tempo = getFieldInteger(&data,1);
            char* sequence = getFieldString(&data,2);

            if (tempo >= 60 && tempo <= 180 && sequence[0] != '\0')
            {
                int count = 0;
                int str_ct = 0;
                int dur_ct = 0;
                //Arrays to keep track of notes in sequence
                char notes_to_play[64];
                char note_duration[32];
                int i = 0;
                bool chk = 1; //flag raised if something wrong before going to next section

                //Initialize everything to null
                for (i=0 ;i<32 ;i++)
                {
                    notes_to_play[i] = '\0';
                    notes_to_play[32+i] = '\0';
                    note_duration[i] = '\0';
                }

                //put the note names and durations into arrays:
                while (sequence[count] != '\0')
                {
                    //Check for strings (note name)
                    if (sequence[count] >= 'A' && sequence[count] <= 'g')
                    {
                        notes_to_play[str_ct] = sequence[count];
                        count++;
                        str_ct++;
                        //Also check the next character to be flat symbol (b)
                        if (sequence[count] == 'b')
                        {
                            //for flat notes use lower case:
                            notes_to_play[str_ct - 1] += 32;
                            count++;
                        }
                        //after a note it must be followed by a number
                        if (!(sequence[count] >= '1' && sequence[count] <= '8'))
                        {
                            putsUart0("Invalid entry-> Note not followed by duration\r\n");
                            chk = 0;
                        }
                    }
                    //Valid note durations are : 1-8
                    else if (sequence[count] >= '1' && sequence[count] <= '8')
                    {
                        note_duration[dur_ct] = sequence[count];
                        count++;
                        dur_ct++;

                        //a duration can only be single digit
                        if (sequence[count] >= '1' && sequence[count] <= '8')
                        {
                            putsUart0("Invalid entry-> Note duration can only be (1-8)\r\n");
                            chk = 0;
                        }
                    }

                    else{
                        count++;
                    }
                }

                /*Debug lines:
                putsUart0(notes_to_play);
                putsUart0("\r\n");
                putsUart0(note_duration);
                putsUart0("\r\n");
                */

                //Check if flag has been raised->
                if (!chk)
                {
                    //Do nothing since error msg should already be displayed.
                }

                //Check if no. of notes and durations by user match up
                else if (str_ct != dur_ct)
                {
                    putsUart0("Invalid entry. Notes don't align.\r\n");
                    //putsUart0(notes_to_play);
                }

                //When the no. of notes match the no. of durations
                else
                {
                    //Check if all the notes given are valid:
                    count = 0;
                    i = 0;
                    //go through the loop to find indices for each note
                    int index_arr[32];
                    //Initialize index array to -1s ->
                    for (i=0; i<32; i++) index_arr[i] = -1;
                    while(count < str_ct)
                    {
                        index_arr[count] = check_for_note_in_arr(notes_to_play[count],Notes_arr);
                        if (index_arr[count] == -1)
                        {
                            //The case for some note not found:
                            i = 1;
                        }
                        count++;
                    }

                    if (i == 1)
                    {
                        putsUart0("Note/s name invalid.\r\n");
                    }

                    //If all conditions are satisfied play:
                    else
                    {
                        for (i = 0; i < str_ct; i++)
                        {
                            int duration = note_duration[i] - 48;
                            playNote(Notes_freq[index_arr[i]],(15000000/tempo)*duration);
                        }
                        playNote(0,50000); //Silence

                    }

                }

            }
            else
            {
                putsUart0("Invalid syntax. Syntax-> demo tempo(60-180) note_sequence(string)\r\n");
            }
        }

        //Function to test the speacker:
        else if (isCommand(&data, "Chromatic", 1) || isCommand(&data, "chromatic", 1))
        {
            //Syntax for this: Chromatic tempo(int)
            int tempo = getFieldInteger(&data,1);
            if (tempo >= 60 && tempo <= 180)
            {
                //x bpm <=> 1 fourth note = (60/x) seconds <=> 1 (16th) note = 60/(x*4) s = (15/x) seconds = 15000/x ms
                int i = 0;
                //play the entire array:
                for (i = 0; i < 13; i++)
                {
                    playNote(Notes_freq[i],15000000/tempo);
                }
            }
            else
            {
                putsUart0("Invalid Tempo Input (range: 60 to 180)\r\n");
            }
        }

        //Function for Metronome:
        else if (isCommand(&data, "Metronome", 2) || isCommand(&data, "metronome", 2))
        {
            //Syntax for this: Metronome tempo(int) note(char)
            int tempo = getFieldInteger(&data,1);
            char note1 = getFieldString(&data,2)[0];
            char note2 = getFieldString(&data,2)[1];
            char note;
            if (tempo >= 60 && tempo <= 180)
            {
                //x bpm <=> 1 fourth note = (60/x) seconds <=> 1 (16th) note = 60/(x*4) s = (15/x) seconds = 15000/x ms
                int idx = 0;
                if (note2 == 'b')
                {
                    note = note1 + 32; //lower case for flat
                }
                else
                {
                    note = note1;
                }
                idx = check_for_note_in_arr(note, Notes_arr);
                if(idx == -1)
                {
                    putsUart0("Invalid Note entry.\r\n");
                }
                else
                {
                    //putcUart0(idx + 48);
                    metro_ON = 1;
                    //Use Uart interrupt here:
                    while (metro_ON)
                    {
                        //play quarter notes in given tempo
                        playNote(Notes_freq[idx],(60000000/tempo));
                        playNote(0,(60000000/tempo));
                    }
                    playNote(0,50000);
                }

            }
            else
            {
                putsUart0("Invalid Tempo Input (range: 60 to 180)\r\n");
            }
        }

        //Help Section----------->
        else if (isCommand(&data, "help", 0) || isCommand(&data, "Help", 0))
        {
            char choice = '\0';
            while (choice != '0')
            {
                putsUart0("Help Topics: \r\n");
                putsUart0("1. Function: demo\r\n");
                putsUart0("2. Function: Chromatic\r\n");
                putsUart0("3. Function: Metronome\r\n");
                putsUart0("0. Exit help.\r\n");

                choice = getcUart0();
                if (choice == '1')
                {
                    putsUart0("\r\n\r\ndemo Function-> Syntax: demo tempo note_sequence\r\n");
                    putsUart0("tempo = tempo for the notes to be played in. (int)\r\n");
                    putsUart0("note_sequence = sequence of notes to be played(string).\r\n");
                    putsUart0("eg Note sequence: E1Eb1E1Eb1E1B1D1C1A2 plays Fur-elise\r\n");
                    putsUart0("As shown in example each note is followed by the duration to hold that note.\r\n");
                    putsUart0("The duration is in number of sixteenth notes.\r\n");
                    putsUart0("A maximum of 32 notes can be played per demo function.\r\n");
                    putsUart0("Notes are represented by their names (eg: A or Ab (no sharps)). Rest is O\r\n\r\n\r\n");
                }
                else if (choice == '2')
                {
                    putsUart0("\r\n\r\nChromatic Function-> Syntax: Chromatic tempo\r\n");
                    putsUart0("Plays all the notes in specified tempo.\r\n");
                    putsUart0("Can be used as a debug function\r\n\r\n\r\n");
                }
                else if (choice == '3')
                {
                    putsUart0("\r\n\r\nMetronome Function-> Syntax: Metronome tempo note\r\n");
                    putsUart0("Plays user specified notes at quarter notes in given tempo.\r\n\r\n");
                }
                else if (choice == '0')
                {
                    putsUart0("\r\nExit Help menu.\r\n\r\n");
                }
                else{
                    putsUart0("\r\nInvalid Option.\r\n\r\n");
                }
            }

        }

        else if (data.buffer[0] != '\0')
        {
            putsUart0("Invalid code/syntax.\r\n");
        }
    }

}
