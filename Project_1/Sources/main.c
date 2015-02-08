                /******************************************************************************
 * Timer Output Compare Demo
 *
 * Description:
 *
 * This demo configures the timer to a rate of 1 MHz, and the Output Compare
 * Channel 1 to toggle PORT T, Bit 1 at rate of 10 Hz. 
 *
 * The toggling of the PORT T, Bit 1 output is done via the Compare Result Output
 * Action bits.  
 * 
 * The Output Compare Channel 1 Interrupt is used to refresh the Timer Compare
 * value at each interrupt
 * 
 * Author:
 *  Jon Szymaniak (08/14/2009)
 *  Tom Bullinger (09/07/2011)	Added terminal framework
 *
 *****************************************************************************/


// system includes
#include <hidef.h>      /* common defines and macros */
#include <stdio.h>      /* Standard I/O Library */
#include <stdlib.h>

// project includes
#include "types.h"
#include "derivative.h" /* derivative-specific definitions */



// Definitions

// Change this value to change the frequency of the output compare signal.
// The value is in Hz.
#define OC_FREQ_HZ    ((UINT16)10)

// Macro definitions for determining the TC1 value for the desired frequency
// in Hz (OC_FREQ_HZ). The formula is:
//
// TC1_VAL = ((Bus Clock Frequency / Prescaler value) / 2) / Desired Freq in Hz
//
// Where:
//        Bus Clock Frequency     = 2 MHz
//        Prescaler Value         = 2 (Effectively giving us a 1 MHz timer)
//        2 --> Since we want to toggle the output at half of the period
//        Desired Frequency in Hz = The value you put in OC_FREQ_HZ
//
#define BUS_CLK_FREQ  ((UINT32) 2000000)   
#define PRESCALE      ((UINT16)  2)         
#define TC1_VAL       ((UINT16)  (((BUS_CLK_FREQ / PRESCALE) / 2) / OC_FREQ_HZ))


#define NUM_SAMPLES   ((UINT16) 1000)
#define RANGE_LOWER   ((UINT16) 950)
#define RANGE_UPPER   ((UINT16) 1050)
#define RANGE_SPREAD  ((UINT16) RANGE_UPPER-RANGE_LOWER)

//#define complete      ((UINT8) 1)  
UINT8 complete = FALSE;
UINT8 capture_enable = FALSE;
UINT8 should_run = TRUE;

UINT16 histogram_values[RANGE_SPREAD+1] = {0};
UINT16 capture_values[NUM_SAMPLES];

UINT16 currVal = 0;
UINT16 prevVal = 0;
UINT16 position = -1;


// Initializes SCI0 for 8N1, 9600 baud, polled I/O
// The value for the baud selection registers is determined
// using the formula:
//
// SCI0 Baud Rate = ( 2 MHz Bus Clock ) / ( 16 * SCI0BD[12:0] )
//--------------------------------------------------------------
void InitializeSerialPort(void)
{
    // Set baud rate to ~9600 (See above formula)
    SCI0BD = 13;          
    
    // 8N1 is default, so we don't have to touch SCI0CR1.
    // Enable the transmitter and receiver.
    SCI0CR2_TE = 1;
    SCI0CR2_RE = 1;
}


// Initializes I/O and timer settings for the demo.
//--------------------------------------------------------------       
void InitializeTimer(void)
{
  // Set the timer prescaler to %2, since the bus clock is at 2 MHz,
  // and we want the timer running at 1 MHz
  TSCR2_PR0 = 1;
  TSCR2_PR1 = 0;
  TSCR2_PR2 = 0;
    
  // Enable input capture on Channel 1
  TIOS_IOS1 = 0;
  
  // Set up output compare action to toggle Port T, bit 1
  //TCTL2_OM1 = 0;
  //TCTL2_OL1 = 1;
  
  //Set timer counter to 0
  TCNT  = 0;
  
  //Look for rising edge on timer channel 1 (pin 15)
  TCTL4_EDG1B  = 0;
  TCTL4_EDG1A  = 1;
  
  // Clear the Output Compare Interrupt Flag (Channel 1) 
  TFLG1 = TFLG1_C1F_MASK;
  
  // Enable the output compare interrupt on Channel 1;
  TIE_C1I = 1;  
  
  //
  // Enable the timer
  // 
  TSCR1_TEN = 1;
   
  //
  // Enable interrupts via macro provided by hidef.h
  //
  EnableInterrupts;
}


// Output Compare Channel 1 Interrupt Service Routine
// Refreshes TC1 and clears the interrupt flag.
//          
// The first CODE_SEG pragma is needed to ensure that the ISR
// is placed in non-banked memory. The following CODE_SEG
// pragma returns to the default scheme. This is neccessary
// when non-ISR code follows. 
//
// The TRAP_PROC tells the compiler to implement an
// interrupt funcion. Alternitively, one could use
// the __interrupt keyword instead.
// 
// The following line must be added to the Project.prm
// file in order for this ISR to be placed in the correct
// location:
//		VECTOR ADDRESS 0xFFEC OC1_isr 
#pragma push
#pragma CODE_SEG __SHORT_SEG NON_BANKED
//--------------------------------------------------------------       
void interrupt 9 OC1_isr( void )
{ 
  //if( !capture_enable ){
   // TFLG1   =   TFLG1_C1F_MASK;
  //  return;
 // }
  
  //if( tc1_count == -1 ){
    
  
  //}
  
  if(capture_enable != TRUE){
    TFLG1   =   TFLG1_C1F_MASK;
    return;
  }
  
  if( position == -1){
    prevVal = TC1;
    position +=1;
    TFLG1   =   TFLG1_C1F_MASK;
    return;
    
  }else if( position >= 1000 ){
    capture_enable = FALSE;
    complete = TRUE;
    TFLG1   =   TFLG1_C1F_MASK;
    return;
    
  }else{
    currVal = TC1;
    capture_values[position] = abs(currVal - prevVal);
    prevVal = currVal;
    position += 1;
    TFLG1   =   TFLG1_C1F_MASK;
    return;
  }
  
  return;
  
}
#pragma pop


// This function is called by printf in order to
// output data. Our implementation will use polled
// serial I/O on SCI0 to output the character.
//
// Remember to call InitializeSerialPort() before using printf!
//
// Parameters: character to output
//--------------------------------------------------------------       
void TERMIO_PutChar(INT8 ch)
{
    // Poll for the last transmit to be complete
    do
    {
      // Nothing  
    } while (SCI0SR1_TC == 0);
    
    // write the data to the output shift register
    SCI0DRL = ch;
}


// Polls for a character on the serial port.
//
// Returns: Received character
//--------------------------------------------------------------       
UINT8 GetChar(void)
{ 
  // Poll for data
  do
  {
    // Nothing
  } while(SCI0SR1_RDRF == 0);
   
  // Fetch and return data from SCI0
  return SCI0DRL;
}


// Entry point of our application code
//--------------------------------------------------------------       
void main(void)
{

  UINT8 userInput = 0;
  UINT16 num = 0;
  UINT16 iter= 0;
  UINT16 wait = 0;
  
  InitializeSerialPort();
  InitializeTimer();
 
  (void)printf("Press any key to begin data collection or q to quit...\r\n");
  userInput = GetChar();
  if( userInput == 'q' || userInput == 'Q' ){
    (void)printf("Exiting program now\r\n");
    return;
  }
 
  while( should_run ){
    
    // Show initial prompt

    capture_enable = TRUE;
    
    (void)printf("Collecting data");
    while( complete != TRUE ){
        wait += 1;
        if((wait % 10000)==0){
          wait = 0;
          (void)printf(".");
        }
        continue;
    }
    
    (void)printf("\r\nData collection complete.\r\n");
    
    (void)printf("Compiling histogram...\r\n");
    
    for( iter = 0; iter < NUM_SAMPLES; iter++){
      histogram_values[capture_values[iter]-RANGE_LOWER] +=1;  
    }
    
    (void)printf("Histogram compiled. Press any key to show results...\r\n");
    userInput = GetChar();
    
    (void)printf("Period in micro-seconds : occurances\r\n");
    (void)printf("------------------------------------\r\n");  
    for( iter = 0; iter < RANGE_SPREAD; iter++){
      if( histogram_values[iter] != 0 ){
        (void)printf("%d:%d\r\n",(iter + RANGE_LOWER), histogram_values[iter] );
        userInput = GetChar();   
      }
    }
    (void)printf("------------------------------------\r\n");
  
  
    //Set state variables back to 0
    //memset(histogram_values,0,sizeof(histogram_values));
    //memset(capture_values,0,sizeof(capture_values));
    for(iter = 0; iter < NUM_SAMPLES; iter++){
      capture_values[iter] = 0;
    }
    for(iter = 0; iter <= RANGE_SPREAD; iter++){
      histogram_values[iter] = 0;
    }
    capture_enable = FALSE;
    complete = FALSE;
    currVal = 0;
    prevVal = 0;
    position = -1;
    
    
    
    (void)printf("Press any key to repeat or q to quit...\r\n");
    userInput = GetChar();
    if( userInput == 'q' || userInput == 'Q' ){
      (void)printf("Exiting program now\r\n");
      should_run = FALSE;
      return;
    }
  }
  
  
}
