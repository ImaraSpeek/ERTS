/*--------------
Manual Mode

Last update: 17-03-2014(Created by Diogo Monteiro)

*/

#define LIFT_ENGINE_LIMIT 800
#define LOW_LIFT_CONVERSION 450/75
#define HIGH_LIFT_CONVERSION (LIFT_ENGINE_LIMIT-450)/(255-75)


#define ROLLPITCHYAW_ENGINE_LIMIT 50
#define ROLLPITCHYAW_CONVERSION ROLLPITCHYAW_ENGINE_LIMIT/127
#define NEG_ROLLPITCHYAW_CONVERSION ROLLPITCHYAW_ENGINE_LIMIT/126
void manual_mode(int LIFT, int ROLL, int PITCH, int YAW,int *ae) {
int i;
	//LIFT
	for(i = 0; i < 4; i++) {

		if(LIFT<=75) {
			
			ae[i] = LIFT*LOW_LIFT_CONVERSION;
		}
		else {

			ae[i] = (LIFT-255)*HIGH_LIFT_CONVERSION+LIFT_ENGINE_LIMIT;
		}
	}

	//ROLL
		if(ROLL<=127) {
			ae[3] += ROLL*ROLLPITCHYAW_CONVERSION;
		}
		else {
			ae[1] += -(ROLL-255)*NEG_ROLLPITCHYAW_CONVERSION;
		}

	//PITCH
	if(PITCH<=127) {
			ae[0] += PITCH*ROLLPITCHYAW_CONVERSION;
		}
		else {
			ae[2] += -(PITCH-255)*NEG_ROLLPITCHYAW_CONVERSION;
		}

	//YAW
	if(YAW<=127) {
		ae[0] += YAW*ROLLPITCHYAW_CONVERSION;
		ae[2] += YAW*ROLLPITCHYAW_CONVERSION;
		
		ae[1] -= YAW*ROLLPITCHYAW_CONVERSION;
		ae[3]	-= YAW*ROLLPITCHYAW_CONVERSION;	
	}
	else {
		ae[1] += -(YAW-255)*NEG_ROLLPITCHYAW_CONVERSION;
		ae[3] += -(YAW-255)*NEG_ROLLPITCHYAW_CONVERSION;
		
		ae[0] -= -(YAW-255)*NEG_ROLLPITCHYAW_CONVERSION;
		ae[2] -= -(YAW-255)*NEG_ROLLPITCHYAW_CONVERSION;	
	}

}
