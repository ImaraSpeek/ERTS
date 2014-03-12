/*--------------
Moderate RPM's value to lift threshold and after few seconds enter safe mode

05-03-2014(Created by Diogo Monteiro)

*/

#define LIFT_THRESHOLD 500 //TODO Define this values experimentally
#define ENGINE_STEP 30
int i;

void panic_mode(void)
{
	for (i = 0; i < 4; i++) 
	{	
		if(ae[i]-LIFT_THRESHOLD <= ENGINE_STEP && ae[i]-LIFT_THRESHOLD >= -ENGINE_STEP) 
		{
			ae[i] = LIFT_THRESHOLD;
		}
		else if (ae[i]-LIFT_THRESHOLD <= -ENGINE_STEP) 
		{
			ae[i]+=	30;
		}
		else 
		{
			ae[i]-=	30;
		}
	}
	
	if (ae[0] == LIFT_THRESHOLD && ae[1] == LIFT_THRESHOLD && ae[2] == LIFT_THRESHOLD && ae[3] == LIFT_THRESHOLD) 
	{
		delay_ms(2000);//wait 2 seconds
//		prev_mode=0;//enter safe_mode
		mode = SAFE_MODE;
	}
}

