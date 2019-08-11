#include "ficses.h"
#define TBL_FILE0 "/home/asap/fic/ras_hunga/tbl/loop0_1.dat"
#define TBL_FILE1 "/home/asap/fic/ras_hunga/tbl/pass0_1.dat"
#define FIC00_ID 	0x0
#define FIC01_ID 	0x1
#define BRODCAST 	0xBB
#define SES_PKT0_3  0xfff7
#define DATA_SIZE	32

int main(int argc, char *argv[]) {
	
	float *wb;
	
	ficses_ap_rst(BRODCAST, FICSESL0_CH1);
	ficses_ap_start(FIC00_ID, FICSESL0_CH1);

	ficses_table_setting(FIC00_ID, TBL_FILE0, FICSESL0_CH1);	
	ficses_table_setting(FIC01_ID, TBL_FILE1, FICSESL0_CH1);	

	ficses_ap_start(FIC00_ID, FICSESL0_CH1);

	user_write_data (wb, FIC00_ID, DATA_SIZE, FICSESL0_CH2);

	ficses_register_read(FIC00_ID, SES_PKT0_3, FICSESL0_CH1);	
	
	return 0;
}

