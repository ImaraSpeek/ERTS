//
//  main.cpp
//  ERTS
//
//  Created by Daniel Lemus Perez on 17/02/14.
//  Copyright (c) 2014 Daniel Lemus Perez. All rights reserved.
//

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "Package.h"

#include <unistd.h>  /* UNIX standard function definitions */
#include <fcntl.h>   /* File control definitions */
#include <errno.h>   /* Error number definitions */
#include <termios.h> /* POSIX terminal control definitions */

#include "read_js.h" //Diogo's Function
#include "read_kb.h" // Diogo's keyboard
#include "rs232.h" //Provides functions to open and close rs232 port


int main()
{
	term_nonblocking();
	keyboard_nonblocking();

	//Initializes the keymap from the keyboard
	int keymap[8] = {MODE_SAFE,0,0,0,0,0,0,0};
	//Initializes the keymap from the keyboard
	int jmap[4] = {0,0,0,0};
	//Opens the port
	int fd_rs232 = open_rs232_port();
	if (fd_rs232 == 0) {
		printf("Error opening the port \n");
		return 0;
	}
	//Clears the Joystick buffer ...Comment if joystick is not connected
	//clear_js_buffer();

	/*Initializes the Package Data (Lift,Roll,Pitch,Yaw for Control Modes)
											 (P,P1,P2,0 for Control Gains Mode)*/
	int data[PARAM_LENGTH] = {0,0,0,0};
	Package mPkg;
	InitPkg(&mPkg,MODE_SAFE); //Intializes Package
	int i;
	int result;
	int key = 0;
	while (key != 27) {
		
		//reads data from the joystick ...comment if joystick is not connected
		//read_js(jmap);
		//Gets the pressed key in the keyboard ... for termination (Press ESC)
		key = getchar();
		if (key != -1) read_kb(keymap,key);
		
		switch (keymap[0]) {
			case MODE_P: //CONTROL GAIN
				data[0] = keymap[5];
				data[1] = keymap[6];
				data[2] = keymap[7];
				data[3] = 0;
				break;
				
			default: //CONTROL MODES
				data[0] = jmap[0]+keymap[1];
				data[1] = jmap[1]+keymap[2];
				data[2] = jmap[2]+keymap[3];
				data[3] = jmap[3]+keymap[4];
				break;
		}

		//CREATES THE PACKAGE
		SetPkgMode(&mPkg, keymap[0]);
		SetPkgData(&mPkg, data);
		//Prints the package
		for (i = 0; i < PKGLEN; i++) {
			printf("[%x]",mPkg.Pkg[i]);
		}
		printf("\n");
		
		//writes in the port
		result = write(fd_rs232,mPkg.Pkg,7*sizeof(BYTE));
		//Asserts in case of sending wrong number of bytes
		assert(result == 7);

		
		// 20 msec pause
		usleep(20000);
	}
	close_rs232_port(fd_rs232);
	printf("\n Port is closed \n");
	return 0;
	
}

