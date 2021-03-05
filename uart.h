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

#ifndef UART0_H_
#define UART0_H_

//Debug line:
//#define MAX_CHARS 5

#define MAX_CHARS 80
#define MAX_FIELDS 5
//Define the struct:
typedef struct _USER_DATA
{
    char buffer[MAX_CHARS+1];
    uint8_t fieldCount;
    uint8_t fieldPosition[MAX_FIELDS];
    char fieldType[MAX_FIELDS];
} USER_DATA;

//-----------------------------------------------------------------------------
// Subroutines
//-----------------------------------------------------------------------------

void initUart0();
void setUart0BaudRate(uint32_t baudRate, uint32_t fcyc);
void putcUart0(char c);
void putsUart0(char* str);
void putint8Uart0(uint8_t in); //Puts integer on Uart (upto 8 bit ints)
char getcUart0();
void getsUart0(USER_DATA* data); //Gets user data and puts it on terminal
void parseFields(USER_DATA* data); //Parses the user input
char* getFieldString(USER_DATA* data, uint8_t fieldNum); //Func to get string from fields
int32_t getFieldInteger(USER_DATA* data, uint8_t fieldNum); //Func to get integer from fields
bool isCommand(USER_DATA* data, const char strCommand[],uint8_t minArguments); //Check validity of command
bool kbhitUart0();

#endif
