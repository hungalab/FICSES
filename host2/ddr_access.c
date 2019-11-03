#include "ficses.h"
#include <time.h>

// Common
#define RING_SIZE 	4
#define BROADCAST 	0xBB
// Register address
#define DDR_ADDRESS 	0xDDDA
#define DDR_WR_START 	0xDDD1
#define DDR_RD_START 	0xDDD2

int main(int argc, char *argv[]) {
	
	int *write_data1;
	int *read_data1;

	write_data1	= (int *)malloc(sizeof(int)*PACKETMAX);
	read_data1	= (int *)malloc(sizeof(int)*PACKETMAX);

	srand(time(NULL));
	//Reset - Start
	ficses_ap_rst(BROADCAST, FICSESL0_CH1);
	ficses_ap_start(BROADCAST, FICSESL0_CH1);

	for (int j = 0; j < RING_SIZE; j++){
		for (int i = 0; i < PACKETMAX; i++){
			write_data1[i]=rand();
			read_data1[i]=0;
		}

		///////////////////////////////////
		///////////// DDR WRITE ///////////
		///////////////////////////////////
		printf("Board [%d]: 128KB DDR WRITE!\n", j);
		//Setup DDR write address to 0x00100000
		ficses_register_write(j, DDR_ADDRESS, 0x00100000, FICSESL0_CH1);
		//Start ddr_write HLS module
		ficses_register_write(j, DDR_WR_START, 0x1, FICSESL0_CH1);
		//Send data to be written to DDR
		ddr_write_data (write_data1, j, PACKETMAX, FICSESL0_CH2);

		///////////////////////////////////
		///////////// DDR READ ////////////
		///////////////////////////////////
		printf("Board [%d]: 128KB DDR READ!\n", j);
		//Setup DDR write address to 0x00100000
		ficses_register_write(j, DDR_ADDRESS, 0x00100000, FICSESL0_CH1);
		//Enable FICSES PCIexpress read buffer
		ficses_start(RD_START_FICSESL0_CH2);
		//Start ddr_read HLS module
		ficses_register_write(j, DDR_RD_START, 0x1, FICSESL0_CH1);
		//Read data at DDR address 0x00100000
		ddr_read_data(read_data1, j, PACKETMAX, FICSESL0_CH2);

		//Verification
		for (int i=0; i < PACKETMAX; i++)
		{
			if (write_data1[i] != read_data1[i]){
				printf("Board [%d]: DDR Transaction error at data [%d] !\n", j, i);
			return 1;
			}
		}

		printf("Board [%d]: DDR verification successful !\n",j);

	}

	system("rm -f rd_binary wr_binary");
	return 0;
}

