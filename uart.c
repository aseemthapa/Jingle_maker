//Name: Aseem Thapa
//ID: 1001543178

//-----------------------------------------------------------------------------
// Hardware Target
//-----------------------------------------------------------------------------

// Target Platform: EK-TM4C123GXL
// Target uC:       TM4C123GH6PM
// System Clock:    -

// Hardware configuration:
// UART Interface:
//   U0TX (PA1) and U0RX (PA0) are connected to the 2nd controller
//   The USB on the 2nd controller enumerates to an ICDI interface and a virtual COM port

//-----------------------------------------------------------------------------
// Device includes, defines, and assembler directives
//-----------------------------------------------------------------------------

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "tm4c123gh6pm.h"
#include "uart.h"

// PortA masks
#define UART_TX_MASK 2
#define UART_RX_MASK 1

//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

// Initialize UART0
void initUart0()
{
    // Enable clocks
    SYSCTL_RCGCUART_R |= SYSCTL_RCGCUART_R0;
    SYSCTL_RCGCGPIO_R |= SYSCTL_RCGCGPIO_R0;
    _delay_cycles(3);

    // Configure UART0 pins
    GPIO_PORTA_DIR_R |= UART_TX_MASK;                   // enable output on UART0 TX pin
    GPIO_PORTA_DIR_R &= ~UART_RX_MASK;                   // enable input on UART0 RX pin
    GPIO_PORTA_DR2R_R |= UART_TX_MASK;                  // set drive strength to 2mA (not needed since default configuration -- for clarity)
    GPIO_PORTA_DEN_R |= UART_TX_MASK | UART_RX_MASK;    // enable digital on UART0 pins
    GPIO_PORTA_AFSEL_R |= UART_TX_MASK | UART_RX_MASK;  // use peripheral to drive PA0, PA1
    GPIO_PORTA_PCTL_R &= ~(GPIO_PCTL_PA1_M | GPIO_PCTL_PA0_M); // clear bits 0-7
    GPIO_PORTA_PCTL_R |= GPIO_PCTL_PA1_U0TX | GPIO_PCTL_PA0_U0RX;
                                                        // select UART0 to drive pins PA0 and PA1: default, added for clarity

    // Configure UART0 to 115200 baud, 8N1 format
    UART0_CTL_R = 0;                                    // turn-off UART0 to allow safe programming
    UART0_CC_R = UART_CC_CS_SYSCLK;                     // use system clock (40 MHz)
    UART0_IBRD_R = 21;                                  // r = 40 MHz / (Nx115.2kHz), set floor(r)=21, where N=16
    UART0_FBRD_R = 45;                                  // round(fract(r)*64)=45
    UART0_LCRH_R = UART_LCRH_WLEN_8 | UART_LCRH_FEN;    // configure for 8N1 w/ 16-level FIFO
    UART0_CTL_R = UART_CTL_TXE | UART_CTL_RXE | UART_CTL_UARTEN;
                                                        // enable TX, RX, and module
}

// Set baud rate as function of instruction cycle frequency
void setUart0BaudRate(uint32_t baudRate, uint32_t fcyc)
{
    uint32_t divisorTimes128 = (fcyc * 8) / baudRate;   // calculate divisor (r) in units of 1/128,
                                                        // where r = fcyc / 16 * baudRate
    UART0_IBRD_R = divisorTimes128 >> 7;                // set integer value to floor(r)
    UART0_FBRD_R = (((divisorTimes128 + 1)) >> 1) & 63; // set fractional value to round(fract(r)*64)
}

// Blocking function that writes a serial character when the UART buffer is not full
void putcUart0(char c)
{
    while (UART0_FR_R & UART_FR_TXFF);               // wait if uart0 tx fifo full
    UART0_DR_R = c;                                  // write character to fifo
}

// Blocking function that writes a string when the UART buffer is not full
void putsUart0(char* str)
{
    uint8_t i = 0;
    while (str[i] != '\0')
        putcUart0(str[i++]);
}

// Put integer on the Uart (int32_t):
void putint8Uart0 (uint8_t in)
{
    if (in == 0)
    {
        putcUart0(48);
    }
    else
    {
        //The numbers have to be between 0 - 255 i.e., three digits.
        char outstr[4];
        int x = 0;
        for (x = 0; x < 3; x++)
        {
            outstr[2-x] = (in % 10) + 48;
            in = in/10;
        }
        outstr[3] = '\0';
        putsUart0(outstr);
    }
}

// Blocking function that returns with serial data once the buffer is not empty
char getcUart0()
{
    while (UART0_FR_R & UART_FR_RXFE);               // wait if uart0 rx fifo empty
    return UART0_DR_R & 0xFF;                        // get character from fifo
}

// Get User data:
void getsUart0(USER_DATA* data){
    int count = 0;
    while (true)
    {
        //Get char from fifo:
        char c = getcUart0();

        //Case for backspace:
        if ((c == 8 || c == 127) && count > 0){
            count--;
        }

        //Case for carriage return
        else if (c==13){
            data->buffer[count] = '\0';
            return;
        }

        //Case for full string buffer:
        else if (count==MAX_CHARS){
            data->buffer[count] = '\0';
            return;
        }

        //Case for other characters:
        else if (c>=32){
            data->buffer[count] = c;
            count++;
        }

        //Debug case:
        else{
            continue;
        }
    }
}

void parseFields(USER_DATA* data){
    data->fieldCount = 0; //Initialize field counter to 0
    int i;
    //Initialize the data:
    for (i=0; i<MAX_FIELDS; i++)
    {
        data->fieldType[i] = 'x'; //x for no data
        data->fieldPosition[i] = 0; //Initial value
    }
    int pos = 0; //counter for array traversal
    //Run a loop until all of buffer is read or max no of fields is read.
    //Using a = alpha, n = numeric, d = delimiter.
    while(data->fieldCount < MAX_FIELDS)
    {
        char here = data->buffer[pos]; //The character being looked at
        char next = data->buffer[pos+1]; //Next character to look at
        if ((here < '0' || here > '9') && (here < 'a' || here > 'z') && (here < 'A' || here > 'Z'))
        {
            //i.e., if the current char is a delimiter
            //For case of numeric value in the next char:
            if (next >= '0' && next <= '9') {
                data->fieldPosition[data->fieldCount] = pos+1; //Since next is the actual value.
                data->fieldType[data->fieldCount] = 'n'; //n for numeric value
                data->fieldCount = data->fieldCount + 1; //Increase the field counter
                //putsUart0("Num\r\n");
            }
            //For case of alphabetical value next char:
            if ((next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z')) {
                data->fieldPosition[data->fieldCount] = pos+1; //Since next is the actual value.
                data->fieldType[data->fieldCount] = 'a'; //a for alpha value
                data->fieldCount = data->fieldCount + 1; //Increase the field counter
                //putsUart0("Alp\r\n");
            }
            data->buffer[pos] = '\0'; //Change the delimiter to a null pointer
            //If the next char is delimiter do nothing and continue loop
        }

        //when the position is 0 but char is not delimiter:
        else
        {
            if (pos == 0)
            {
                if (here >= '0' && here <= '9') {
                    data->fieldPosition[data->fieldCount] = pos; //Since here is the actual value.
                    data->fieldType[data->fieldCount] = 'n'; //n for numeric value
                    data->fieldCount = data->fieldCount + 1; //Increase the field counter
                    //putsUart0("Num\r\n");
                }
                //For case of alphabetical value next char:
                if ((here >= 'a' && here <= 'z') || (here >= 'A' && here <= 'Z')) {
                    data->fieldPosition[data->fieldCount] = pos; //Since here is the actual value.
                    data->fieldType[data->fieldCount] = 'a'; //a for alpha value
                    data->fieldCount = data->fieldCount + 1; //Increase the field counter
                    //putsUart0("Alp\r\n");
                }
            }
        }
        pos = pos + 1; //Increase position
        if(next == '\0') break; //break from loop when next char is null char.
    }
}

char* getFieldString(USER_DATA* data, uint8_t fieldNum)
{
    //Check if data in fieldNum is indeed alpha
    //Also check for validity of fieldNum
    if(data->fieldType[fieldNum] == 'a' && fieldNum >= 0 && fieldNum < MAX_FIELDS)
    {
        char* x = data->buffer + data->fieldPosition[fieldNum];
        return x;
    }
    else return '\0';
}


int32_t getFieldInteger(USER_DATA* data, uint8_t fieldNum)
{
    //Check if data in fieldNum is indeed numeric
    //Also check for validity of fieldNum
    if(data->fieldType[fieldNum] == 'n' && fieldNum >= 0 && fieldNum < MAX_FIELDS)
    {
        //Now convert to integer:
        int pos = data->fieldPosition[fieldNum]; //This is position of field
        int out = 0;
        //Loop until a string terminator is found:
        while (data->buffer[pos] != '\0')
        {
            out = (out * 10) + (data->buffer[pos] - '0'); //subtracting '0' will give the actual value of int
            pos++; //Increment the position
        }
        return out;
    }
    else return 0;
}

bool isCommand(USER_DATA* data, const char strCommand[],uint8_t minArguments)
{
    char* comm_name = getFieldString(data,0); //For an input to be command first field must be string
    //Now check if comm_name == strCommand[]------>
    if (strcmp(comm_name, strCommand) != 0)
    {
        return false; //The names did not match
    }
    //Check for the no of arguments
    if (minArguments > ((data->fieldCount)-1))
    {
        //-1 because one of the fields is the command
        return false;
    }
    return true;
}

// Returns the status of the receive buffer
bool kbhitUart0()
{
    return !(UART0_FR_R & UART_FR_RXFE);
}
