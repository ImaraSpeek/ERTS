/*--------------
Define DC offset of the sensors while QR is not moving

05-03-2014(Created by Diogo Monteiro)

*/
//GLOBALS
int sax_offset = 0;
int say_offset = 0;
int saz_offset = 0;
int sp_offset = 0;
int sq_offset = 0;
int sr_offset = 0;

void calibration_mode() {

	sax_offset = sax;
	say_offset = say;
	saz_offset = saz;
	sp_offset = sp;
	sq_offset = sq;
	sr_offset = sr;

}