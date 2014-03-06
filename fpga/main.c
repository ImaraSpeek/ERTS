/*------------------------------------------------------------------
 *  based on the qtest - fixing all the todos
 *
 *  reads ae[0-3] from stdin
 *  (q,w,e,r increment ae[0-3], a,s,d,f decrement)
 *
 *  prints ae[0-3],sax,say,saz,sp,sq,sr,delta_t on stdout
 *  where delta_t is the qr-isr exec time
 *
 *  Imara Speek
 *  Embedded Real Time Systems
 *
 *  Version Feb 26, 2014

TODO determine priorities
TODO control values have to be send
TODO ask for log that is saved during running
TODO also want telemetry set and concurring protocol
TODO change modes to enum
TODO Send values to engines
TODO determine the period for the timer interrupt
TODO manual mode
TODO panic mode
TODO safe mode
TODO check what the & bytes are for the led function in main

 *------------------------------------------------------------------
 */

#include <stdio.h>
#include <string.h>
#include <x32.h>
#include "assert.h"

//For the buffer
#include <stdlib.h>

/*********************************************************************/

/* define some peripheral short hands
 */
#define X32_instruction_counter           peripherals[0x03]

#define X32_timer_per           peripherals[PERIPHERAL_TIMER1_PERIOD]
#define X32_leds		peripherals[PERIPHERAL_LEDS]
#define X32_ms_clock		peripherals[PERIPHERAL_MS_CLOCK]
#define X32_us_clock		peripherals[PERIPHERAL_US_CLOCK]
#define X32_QR_a0 		peripherals[PERIPHERAL_XUFO_A0]
#define X32_QR_a1 		peripherals[PERIPHERAL_XUFO_A1]
#define X32_QR_a2 		peripherals[PERIPHERAL_XUFO_A2]
#define X32_QR_a3 		peripherals[PERIPHERAL_XUFO_A3]
#define X32_QR_s0 		peripherals[PERIPHERAL_XUFO_S0]
#define X32_QR_s1 		peripherals[PERIPHERAL_XUFO_S1]
#define X32_QR_s2 		peripherals[PERIPHERAL_XUFO_S2]
#define X32_QR_s3 		peripherals[PERIPHERAL_XUFO_S3]
#define X32_QR_s4 		peripherals[PERIPHERAL_XUFO_S4]
#define X32_QR_s5 		peripherals[PERIPHERAL_XUFO_S5]
#define X32_QR_timestamp 	peripherals[PERIPHERAL_XUFO_TIMESTAMP]

#define X32_rs232_data		peripherals[PERIPHERAL_PRIMARY_DATA]
#define X32_rs232_stat		peripherals[PERIPHERAL_PRIMARY_STATUS]
#define X32_rs232_char		(X32_rs232_stat & 0x02)

#define X32_wireless_data	peripherals[PERIPHERAL_WIRELESS_DATA]
#define X32_wireless_stat	peripherals[PERIPHERAL_WIRELESS_STATUS]
#define X32_wireless_char	(X32_wireless_stat & 0x02)

#define X32_button		peripherals[PERIPHERAL_BUTTONS]
#define X32_switches		peripherals[PERIPHERAL_SWITCHES]

/*********************************************************************/

// BYTE and WORD sizes predefined
#define BYTE unsigned char
#define WORD unsigned short

// RX FIFO
#define FIFOSIZE 16
char	fifo[FIFOSIZE]; 
int	iptr, optr;

// mode, lift ,roll, pitch, yaw, checksum 
#define nParams		0x06

// mode, P, P1, P2, checksum
#define nParams2	0x05

// MODES LIST
#define SAFE_MODE			0x00
#define PANIC_MODE			0x01
#define MANUAL_MODE			0x02
#define CALIBRATION_MODE		0x03
#define YAW_CONTROL_MODE		0x04
#define FULL_CONTROL_MODE		0x05

#define STARTING_BYTE			0x80

// Parameter list numbering
#define MODE		0x00
#define LIFT		0x01
#define ROLL		0x02
#define PITCH		0x03
#define YAW		0x04
//#define PCONTROL	0x05

#define CHECKSUM	0x05

//RAMP-UP CHECK PARAMETERS
#define SAFE_INCREMENT 50

//BUTTERWORTH LOW PASS FILTER CONSTANTS
//for 10Hz cut-off frequency and 1266.5 Hz sampling freq.

#define A0		0x3A1BCD40
#define A1		0x3A9BCD40
#define A2		0x3A1BCD40
#define B0		1
#define B1		0xBFF705E5
#define B2		0x3F6EA798
//data logging variables
int   dl_time[DLOGSIZE];
int	dl_s1[DLOGSIZE];
int	dl_s2[DLOGSIZE];
int	dl_s3[DLOGSIZE];
int	dl_s4[DLOGSIZE];
int	dl_s5[DLOGSIZE];
int   dl_s6[DLOGSIZE];
int   dlcount = 0;

//DEFINE SIZE OF DATA LOGGING VARIABLES
#define DLOGSIZE	5000 //around 5 seconds
//filter parameters
int   y0[6] = {0,0,0,0,0,0};
int   y1[6] = {0,0,0,0,0,0};
int   y2[6] = {0,0,0,0,0,0};
int   x0[6] = {0,0,0,0,0,0};
int   x1[6] = {0,0,0,0,0,0};
int   x2[6] = {0,0,0,0,0,0};

/*********************************************************************/

// For defining the circular buffer
// Opaque buffer element type.  This would be defined by the application
typedef struct { BYTE value; } ElemType;

// fixed size for the buffer, no dyanmic allocation is needed
// actual size is minus one element because of the one slot open protocol 
#define CB_SIZE (62 + 1)
 
// Circular buffer object 
typedef struct {
	int         	start;  	/* index of oldest element              */
	int	       	end;    	/* index at which to write new element  */
	ElemType 	elems[CB_SIZE]; /* vector of elements                   */
	// including extra element for one slot open protocol
} CircularBuffer;

CircularBuffer cb;

/*********************************************************************/

// Globals
char	c;
int	demo_done;
int   prev_ae[4] = {0, 0, 0, 0};
int	ae[4];
int	s0, s1, s2, s3, s4, s5, timestamp;

int	isr_qr_counter;
int	isr_qr_time;
int	button;
int	inst;

void	toggle_led(int);
void	delay_ms(int);
void	delay_us(int);

// Own functions
void	decode();
void 	print_comm();
void 	check_start();
int 	check_sum();

int 	startflag = 0;
BYTE 	mode, roll, pitch, yaw, lift, pcontrol, p1control, p2control, checksum;

/*------------------------------------------------------------------
 * Circular buffer initialization - By Imara Speek
 * Point start and end to the adress of the allocated vector of elements
 *------------------------------------------------------------------
 */
void cbInit(CircularBuffer *cb) {
    cb->start = 0;
    cb->end   = 0;
}


/*------------------------------------------------------------------
 * Check if circular buffer is full - By Imara Speek
 *------------------------------------------------------------------
 */ 
int cbIsFull(CircularBuffer *cb) {
    return (cb->end + 1) % CB_SIZE == cb->start;
}


/*------------------------------------------------------------------
 * Check if circular buffer is empty - By Imara Speek
 *------------------------------------------------------------------
 */ 
int cbIsEmpty(CircularBuffer *cb) {
    return cb->end == cb->start;
}

/*------------------------------------------------------------------
 * Clean the buffer - By Imara Speek
 *------------------------------------------------------------------
 */ 
void cbClean(CircularBuffer *cb) {
	memset(cb->elems, 0, sizeof(ElemType) * CB_SIZE); 	
}


/*------------------------------------------------------------------
 * Write to buffer - By Imara Speek
 * Write an element, overwriting oldest element if buffer is full. App can
 * choose to avoid the overwrite by checking cbIsFull().
 *------------------------------------------------------------------
 */ 
void cbWrite(CircularBuffer *cb, ElemType *elem) {
    cb->elems[cb->end] = *elem;
    cb->end = (cb->end + 1) % CB_SIZE;
    if (cb->end == cb->start)
	{        
		cb->start = (cb->start + 1) % CB_SIZE; /* full, overwrite */
	}
// TODO determine if we want it to overwrite
}

/*------------------------------------------------------------------
 * Read from buffer - By Imara Speek
 * Read oldest element. App must ensure !cbIsEmpty() first. 
 *------------------------------------------------------------------  
 */
void cbRead(CircularBuffer *cb, ElemType *elem) {
    *elem = cb->elems[cb->start];
    cb->start = (cb->start + 1) % CB_SIZE;
}

/*------------------------------------------------------------------
 * get from buffer - By Imara Speek
 * Read oldest element. App must ensure !cbIsEmpty() first. 
 *------------------------------------------------------------------  
 */
BYTE cbGet(CircularBuffer *cb) {
	BYTE c;	

	c = cb->elems[cb->start].value;
	cb->start = (cb->start + 1) % CB_SIZE;

	return c;
}

/*------------------------------------------------------------------
 * Fixed Point Multiplication
 * Multiplies the values and then shift them right by 14 bits
 * By Daniel Lemus
 *------------------------------------------------------------------
 */
int mult(int &a,int &b)
{
	int result;
	result = a * b;
	return (result >>14);
}

/*------------------------------------------------------------------
 * 2nd Order Butterworth filter
 * By Daniel Lemus
 *------------------------------------------------------------------
 */
void Butt2Filter()
{
	int i;
	x0[0] = s0;
	x0[1] = s1;
	x0[2] = s2;
	x0[3] = s3;
	x0[4] = s4;
	x0[5] = s5;
	for (i=0; i<6; i++) {
		y0[i] = mult(A0,x0[i]) + mult(A1,x1[i]) + mult(A2,x2[i]) - mult(B1,y1[i]) - mult(B2,y2[i])
		x2[i] = x1[i];
		x1[i] = x0[i];
		y2[i] = y1[i];
		y1[i] = y0[i];
	}
}

/*------------------------------------------------------------------
 * Data Logging Storage
 * Store each parameter individually in arrays
 * By Daniel Lemus
 *------------------------------------------------------------------
 */
void DataStorage(void)
{
	if (dl_count < DLOGSIZE) {
		//stores time stamp
		dl_time[dl_count] = timestamp;
		// Stores desired variables (Change if needed)
		// e.g filtered values
		dl_s1[dl_count] = y0[0];
		dl_s2[dl_count] = y0[1];
		dl_s3[dl_count] = y0[2];
		dl_s4[dl_count] = y0[3];
		dl_s5[dl_count] = y0[4];
		dl_s6[dl_count] = y0[5];
		dl_count ++;
	}
	//to send back it is necessary to typecast to BYTE
}

/*------------------------------------------------------------------
 * Ramp-Up prevention function
 * Compares current - previous commanded speed and clip the current
 * value if necessary (To avoid sudden changes -> motor ramp-up)
 * By Daniel Lemus
 *------------------------------------------------------------------
 */
void CheckMotorRamp(void)
{
	int delta;
	for (i = 0; i < 4; i++) {
		delta = ae[i]-prev_ae[i];
		if (abs(delta) > SAFE_INCREMENT)) {
			if	(delta < 0)// Negative Increment
			{
				ae[i] = prev_ae - SAFE_INCREMENT;
			}
			else //POSITIVE INCREMENT
			{
				ae[i] = prev_ae + SAFE_INCREMENT;
			}
		}
		prev_ae[i] = ae[i];
	}
}

/*------------------------------------------------------------------
 * isr_rs232_tx -- QR link tx interrupt handler
 *------------------------------------------------------------------
 */
void isr_rs232_tx(void)
{
	
}

/*------------------------------------------------------------------
 * isr_qr_link -- QR link rx interrupt handler
 *------------------------------------------------------------------
 */
void isr_button(void)
{
	button = 1;
}

/*------------------------------------------------------------------
 * isr_qr_link -- QR link rx interrupt handler
 *------------------------------------------------------------------
 */
void isr_qr_link(void)
{
	int	ae_index;
	
	// record time
	isr_qr_time = X32_us_clock;
        inst = X32_instruction_counter;

	// get sensor and timestamp values
	s0 = X32_QR_s0; s1 = X32_QR_s1; s2 = X32_QR_s2; 
	s3 = X32_QR_s3; s4 = X32_QR_s4; s5 = X32_QR_s5;
	timestamp = X32_QR_timestamp;
	
	//Prints sensor and timestamp values
	printf("s0 = %i s1 = %i s2 = %i s3 = %i s4 = %i s5 = %i \n",s0,s1,s2,s3,s4,s5);

	// monitor presence of interrupts 
	isr_qr_counter++;
	if (isr_qr_counter % 500 == 0) 
	{
		toggle_led(2);
	}	

	// Clip engine values to be positive and 10 bits.
	for (ae_index = 0; ae_index < 4; ae_index++) 
	{
		if (ae[ae_index] < 0)
		{ 
			ae[ae_index] = 0;
		}
		ae[ae_index] &= 0x3ff;
	}
	
	//CHECK FOR POSSIBLE RAMP-UP VALUES BEFORE SENDING TO THE MOTORS
	CheckMotorRamp();
	
	// Send actuator values
	// (Need to supply a continous stream, otherwise
	// QR will go to safe mode, so just send every ms)
	X32_QR_a0 = ae[0];
	X32_QR_a1 = ae[1];
	X32_QR_a2 = ae[2];
	X32_QR_a3 = ae[3];

	// record isr execution time (ignore overflow)
        inst = X32_instruction_counter - inst;
	isr_qr_time = X32_us_clock - isr_qr_time;
}

/*------------------------------------------------------------------
 * isr_rs232_rx -- rs232 rx interrupt handler - By Imara Speek
 *------------------------------------------------------------------
 */
void isr_rs232_rx(void)
{
	// signal interrupt
	toggle_led(3);

	// may have received > 1 char before IRQ is serviced so loop
	while (X32_rs232_char) 
	{
		cb.elems[cb.end].value = (BYTE)X32_rs232_data;
		cb.end = (cb.end + 1) % CB_SIZE;
		if (cb.end == cb.start)
		{
			cb.start = (cb.start + 1) % CB_SIZE; /* full, overwrite */
		}
// TODO determine if we want it to overwrite		

/*
		fifo[iptr++] = X32_rs232_data;
// DEBUG DEBUG
//		printf("[%x]", X32_rs232_data);
		if (iptr >= FIFOSIZE)
			iptr = 0;
*/
	}

}

/*------------------------------------------------------------------
 * getchar -- read char from rx fifo, return -1 if no char available
 *------------------------------------------------------------------
 */
int 	getchar(void)
{
	BYTE	c;

	if (optr == iptr)
	{
		return -1;
	}	
	c = fifo[optr++];
	printf("[%x]", c);
	if (optr >= FIFOSIZE)
	{
		optr = 0;
	}	
	return c;
}


/*------------------------------------------------------------------
 * isr_wireless_rx -- wireless rx interrupt handler
 *------------------------------------------------------------------
 */
void isr_wireless_rx(void)
{
	BYTE c;

	/* signal interrupt
	 */
	toggle_led(4);


	/* may have received > 1 char before IRQ is serviced so loop
	 */
	while (X32_wireless_char) {
		fifo[iptr++] = X32_wireless_data;
		if (iptr > FIFOSIZE)
			iptr = 0;
	}

}

/*------------------------------------------------------------------
 * delay_ms -- busy-wait for ms milliseconds
 *------------------------------------------------------------------
 */
void delay_ms(int ms) 
{
	int time = X32_ms_clock;
	while(X32_ms_clock - time < ms)
		;
}

/*------------------------------------------------------------------
 * delay_us -- busy-wait for us microseconds
 *------------------------------------------------------------------
 */
void delay_us(int us) 
{
	int time = X32_us_clock;
	while(X32_us_clock - time < us)
		;
}

/*------------------------------------------------------------------
 * toggle_led -- toggle led # i
 *------------------------------------------------------------------
 */
void toggle_led(int i) 
{
	X32_leds = (X32_leds ^ (1 << i));
}

/*------------------------------------------------------------------
 * process_key -- process command keys
 *------------------------------------------------------------------
 */
void process_key(char c) 
{
	switch (c) {
		case 'q':
			ae[0] += 10;
			break;
		case 'a':
			ae[0] -= 10;
			if (ae[0] < 0) ae[0] = 0;
			break;
		case 'w':
			ae[1] += 10;
			break;
		case 's':
			ae[1] -= 10;
			if (ae[1] < 0) ae[1] = 0;
			break;
		case 'e':
			ae[2] += 10;
			break;
		case 'd':
			ae[2] -= 10;
			if (ae[2] < 0) ae[2] = 0;
			break;
		case 'r':
			ae[3] += 10;
			break;
		case 'f':
			ae[3] -= 10;
			if (ae[3] < 0) ae[3] = 0;
			break;
		default:
			demo_done = 1;
	}
}

/*------------------------------------------------------------------
 * print_state -- print all sensors and actuators
 *------------------------------------------------------------------
 */

void print_state(void) 
{
	int i;
	char text[100] , a;
	printf("%3d %3d %3d %3d | ",ae[0],ae[1],ae[2],ae[3]);
	printf("%3d %3d %3d %3d %3d %3d (%3d, %d)\r\n",
		s0,s1,s2,s3,s4,s5,isr_qr_time, inst);
        
	sprintf(text, "%d %d %d %d \r\n",ae[0],ae[1],ae[2],ae[3]);
    	i = 0;
    	while( text[i] != 0) {
       		delay_ms(1);
		// if (X32_switches == 0x03)
		if (X32_wireless_stat & 0x01 == 0x01)
			X32_wireless_data = text[i];

		i++;
    	}
}

/*------------------------------------------------------------------
 * Decoding function with a higher execution level
 * Function called in the timer ISR 
 * By Imara Speek 1506374
 *------------------------------------------------------------------
 */

void decode(void)
{	
	/* Get the next character in the buffer after the starting byte
	 * Whilst disabling all the interrupts CRITICAL SECTION
	 */

	//DISABLE_INTERRUPT(INTERRUPT_GLOBAL); 

	mode = getchar();
	lift = getchar();
	roll = getchar();
	pitch = getchar();
	yaw = getchar();
/*
	pcontrol = getchar();
	p1control = getchar();
	p2control = getchar();
 */
	checksum = getchar();

//	printf("\n we are decoding");

	//ENABLE_INTERRUPT(INTERRUPT_GLOBAL); 
}

/*------------------------------------------------------------------
 * Print commands
 * for DEBUGGING
 * By Imara Speek 1506374
 *------------------------------------------------------------------
 */

void print_comm(void)
{		
	printf("\n[%x][%x][%x][%x][%x][%x]\n", mode, lift, roll, pitch, yaw, checksum);
}

/*------------------------------------------------------------------
 * Check the checksum and return error message is package is corrupted
 * By Imara Speek 1506374
 *------------------------------------------------------------------
 */
int check_sum(void)
{
	BYTE sum;
	/* Checksum before decoding the package
	 */
	sum = 0;
	// DEBUG DEBUG DEBUG DEBUG
//	printf("\nChecksum = %x", checksum);
	
	sum = mode + lift + roll + pitch + yaw; // + pcontrol + p1control + p2control;
	sum = ~sum;
//	printf("\nSum = %x", sum);

//	printf("\n%x %x", checksum, sum);	
/*
	for (i = 0; i < CHECKSUM; i++) {
		sum += fifo[i];
	}
*/
	if (checksum != sum) {
		printf("\nInvalid Pkg");
		return 0;
	}
	else
		return 1;
}

/*------------------------------------------------------------------
 * main -- do the test
 *------------------------------------------------------------------
 */
int main() 
{
	// Character to store bytes from buffer	
	BYTE c;
	// Initialize the Circular buffer and elem to write from
	ElemType elem;

	/* prepare QR rx interrupt handler
	 */
        SET_INTERRUPT_VECTOR(INTERRUPT_XUFO, &isr_qr_link);
        SET_INTERRUPT_PRIORITY(INTERRUPT_XUFO, 21);
	isr_qr_counter = isr_qr_time = 0;
	ae[0] = ae[1] = ae[2] = ae[3] = 0;
        ENABLE_INTERRUPT(INTERRUPT_XUFO);
 	
	/* prepare timer interrupt to make sure the x32 can read fast enough
	 * as the main calls are not fast enough
	 */
	// TODO find most optimal timing interval for this
        //X32_timer_per = 5 * CLOCKS_PER_MS;
        //SET_INTERRUPT_VECTOR(INTERRUPT_TIMER1, &isr_qr_timer);
        //SET_INTERRUPT_PRIORITY(INTERRUPT_TIMER1, 21);
        //ENABLE_INTERRUPT(INTERRUPT_TIMER1);

	/* prepare rs232 tx interrupt
	 */
	SET_INTERRUPT_VECTOR(INTERRUPT_PRIMARY_TX, &isr_rs232_tx);
	SET_INTERRUPT_PRIORITY(INTERRUPT_PRIMARY_TX, 15);
	ENABLE_INTERRUPT(INTERRUPT_PRIMARY_TX);
	
	/* prepare button interrupt handler
	 */
        SET_INTERRUPT_VECTOR(INTERRUPT_BUTTONS, &isr_button);
        SET_INTERRUPT_PRIORITY(INTERRUPT_BUTTONS, 8);
	button = 0;
        ENABLE_INTERRUPT(INTERRUPT_BUTTONS);	

	/* prepare rs232 rx interrupt and getchar handler
	 */
        SET_INTERRUPT_VECTOR(INTERRUPT_PRIMARY_RX, &isr_rs232_rx);
        SET_INTERRUPT_PRIORITY(INTERRUPT_PRIMARY_RX, 20);
	while (X32_rs232_char) c = X32_rs232_data; // empty buffer
        ENABLE_INTERRUPT(INTERRUPT_PRIMARY_RX);

        /* prepare wireless rx interrupt and getchar handler
	 */
        SET_INTERRUPT_VECTOR(INTERRUPT_WIRELESS_RX, &isr_wireless_rx);
        SET_INTERRUPT_PRIORITY(INTERRUPT_WIRELESS_RX, 19);
        while (X32_wireless_char) c = X32_wireless_data; // empty buffer
        ENABLE_INTERRUPT(INTERRUPT_WIRELESS_RX);

	/* initialize some other stuff
	 */
        iptr = optr = 0;
	X32_leds = 0;
	demo_done = 0;

	// clean the buffer
	cbClean(&cb);
	// initialize the buffer
	cbInit(&cb);
	// Initialize value to write
	elem.value = 0;

	// Print to indicate start
	printf("Hello! \nMode, Parameter 1, Parameter 2, Parameter 3, Parameter 4, Checksum");

	// Enable all interrupts, starting the system
        ENABLE_INTERRUPT(INTERRUPT_GLOBAL); 

	while (! demo_done) {
		// See if there is a character in the buffer
		// and check whether that is the starting byte
// TODO Do we need to check if it is empty, no right?
		
		c = cbGet(&cb);
		printf("[%x]", c);
		if (c == STARTING_BYTE)
		{
//			decode();
			printf("\n\n");

		} 
		
/*		c = getchar();
		if (c == STARTING_BYTE) 
		{
			decode();
			if (check_sum())
			{
				printf("\nYay! [%x][%x][%x][%x][%x][%x]\n", mode, lift, roll, pitch, yaw, checksum);
			}
		}	
		// print_state();
                X32_leds = (X32_leds & 0xFC) | (X32_switches & 0x03 );
		if (button == 1){
			printf("You have pushed the button!!!\r\n");
			button = 0;
		}

		delay_ms(200);
*/
	}

	printf("Exit\r\n");

        DISABLE_INTERRUPT(INTERRUPT_GLOBAL);

	return 0;
}

