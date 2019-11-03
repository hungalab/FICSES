#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>

#define FILEMAX 	131072	// FICSES: Maximum number of files necessary for 16GB DDR access
							// 128KBx131072= 16GB

#define PACKETMAX 	8192 	// FICSES: Number of packet in a single FICSES transaction 
							// each is 128bit (128KB data)

// PCIexpress address: DO NOT CHANGE!!
#define PCIE_BAR_CTRL_ADDR 0xF7000008
#define PCIE_BAR_DATA_ADDR 0xF6C00000

// FICSES_Lane0 Channel IDs: DO NOT CHANGE!!
#define FICSESL0_CH0 3
#define FICSESL0_CH1 2
#define FICSESL0_CH2 1
#define FICSESL0_CH3 0

// FICSES_Lane0 Channel Start enable register values: DO NOT CHANGE!!
// Write: FICSES --> FIC
#define WR_START_FICSESL0_CH0 0x08000000
#define WR_START_FICSESL0_CH1 0x04000000
#define WR_START_FICSESL0_CH2 0x02000000
#define WR_START_FICSESL0_CH3 0x01000000
// Read: FIC --> FICSES
#define RD_START_FICSESL0_CH0 0x00080000
#define RD_START_FICSESL0_CH1 0x00040000
#define RD_START_FICSESL0_CH2 0x00020000
#define RD_START_FICSESL0_CH3 0x00010000

// Zeus to FICSES memcpy parameters: DO NOT CHANGE!!
#define MMAP_LENGTH 4
#define PAGE_SIZE 1024
#define DEVICE_FILE "/dev/mem"

// File settings
#define WRITE_BINARY_FILE "wr_binary"
#define READ_BINARY_FILE "rd_binary"

// FICSES CMDs
#define START 		0x1
#define REG_WR 		0x2
#define REG_RD 		0x3
#define RESET 		0x4
#define TBL_SET		0x5
#define UNDEFINED	0x0

// Routing table setting: Slot number register address
#define SNUM 		0xFFF8

// FICSES Start enable function
int ficses_start(int channel_enable)
{
  char buff[1024];
  char file[256];
  char *mmaped;
  int fd;
  unsigned int st,length,poff;
  int r;
  unsigned char tmp1;
  unsigned short tmp2;
  unsigned int tmp4;
  unsigned long tmp8; 

	st=PCIE_BAR_CTRL_ADDR;
	length=MMAP_LENGTH;
	poff=st%PAGE_SIZE;
	
	fd = open(DEVICE_FILE,O_RDWR);
	if (fd < 0){
		fprintf(stderr,"Cannot Open /dev/mem\n");
		return 1;
	}

	if ((mmaped = mmap(0, length+poff, PROT_WRITE|PROT_READ, MAP_SHARED, fd, st-poff))==MAP_FAILED)
	{
       	fprintf(stderr,"cannot mmap\n");
       	return 1;
    	}

	if (length == 1){
		*(unsigned char *)(mmaped+poff) = (unsigned char)channel_enable;
	}
	else if (length == 2){
		*(unsigned short *)(mmaped+poff) = (unsigned short)channel_enable;
	}
	else if (length == 4){
		*(unsigned int *)(mmaped+poff) = (unsigned int)channel_enable;
	}
	else{
		*(unsigned long *)(mmaped+poff) = (unsigned long)channel_enable;
	}
	msync(mmaped+poff,length, MS_ASYNC);

	munmap(mmaped,length);
	close(fd);
	return 0;
}

// Convert a read_file binary to a pointer 
int read_binary_file (char* binary_file, int * read_data) {
    unsigned int 	data1; 			// 32bit: [31:0]
    unsigned int 	data2; 			// 32bit: [127:32]
    unsigned int 	data3; 			// 32bit: [159:64]
    unsigned int 	data4; 			// 32bit: [127:96]
    unsigned int 	header; 		// 32bit: [159:128]
    unsigned int 	cmd=0x02AA; 	// 32bit: [191:160] : "02" MUST be provided; "AA" can be random 
    unsigned long	filler=0x0; 	// 64bit: [255:192]
  	char file[256];
	
	int i, j;
	FILE *fp;

	sprintf(file,"%s",binary_file);
	/* open binary file to read */
	if ((fp = fopen(file, "rb")) == NULL){
	       fprintf(stderr,"Cannot open %s\n", file);
             return 1;
	}
	
    /* Read and skip the first 256KB allocated to the writing buffer   */
    for (i=0; i < PACKETMAX; i++)
	   {
		   fread(&filler, sizeof(filler), 1, fp);
		   fread(&filler, sizeof(filler), 1, fp);
		   fread(&filler, sizeof(filler), 1, fp);
		   fread(&filler, sizeof(filler), 1, fp);
	   }
	
    /* Read and store the second 256KB allocated to the reading buffer   */
    for (i=0; i < PACKETMAX; i++)
	   {
		  fread(&data1, sizeof(data1), 1, fp);	
		  // read_data[0]=data1; 			// Bug! (wrong assignement to read_data) Added by Akram 08-29-2019
		  read_data[i*4]=data1;
		   
		  fread(&data2, sizeof(data2), 1, fp);  
		  read_data[(i*4)+1]=data2;
		   
		  fread(&data3, sizeof(data3), 1, fp);  
		  read_data[(i*4)+2]=data3;

		  fread(&data4, sizeof(data4), 1, fp);  
		  read_data[(i*4)+3]=data4;

		  fread(&header, sizeof(header), 1, fp);  

		  fread(&cmd, sizeof(cmd), 1, fp); 

		  fread(&filler, sizeof(filler), 1, fp);
	   }
	/* close file */
    fclose(fp);

    return 0;
}

// Write 128KB of data: FICSES --> FIC
int ficses_write_128KB(char* binary_file, int ficses_channel)
{
  char file[256];
  char *mmaped, *fmaped;
  char *mmaped_tmp, *fmaped_tmp;
  int fd, fh;
  unsigned int st;
  unsigned int length = 0x80000;
  long  page_size, map_size;
  int channel_enable;
	
	sprintf(file,"%s",binary_file);
	fh = open(file, O_RDONLY, 0666);
	if(fh < 0) {
	        printf("Error : can't open file\n");
	        return -1;
	}
	
	fd = open(DEVICE_FILE,O_RDWR);
	if (fd < 0){
		fprintf(stderr,"Cannot Open /dev/mem\n");
		return 1;
	}

    switch (ficses_channel) {
        case FICSESL0_CH0: 
			channel_enable= WR_START_FICSESL0_CH0;
		break;
			
        case FICSESL0_CH1: 
			channel_enable= WR_START_FICSESL0_CH1;
		break;
			
        case FICSESL0_CH2: 
			channel_enable= WR_START_FICSESL0_CH2;
		break;
			
        case FICSESL0_CH3: 
			channel_enable= WR_START_FICSESL0_CH3;
		break;

		default: 
			printf("Illegal FICSES channel ID (Must be 0~3) \n");
		return 1;
	}	
	
	st=PCIE_BAR_DATA_ADDR;	// base address
	st += ficses_channel*length;
	
	page_size = getpagesize();
	map_size = ((length-1) / page_size + 1) * page_size;

	if ((fmaped = (char *)mmap(0, map_size, PROT_READ, MAP_SHARED, fh, 0))==MAP_FAILED)
	{
		fprintf(stderr,"cannot fmap\n");
	       	return 1;
	}

	fmaped_tmp = fmaped;
	if ((mmaped = (char *)mmap(0, map_size, PROT_WRITE, MAP_SHARED, fd, st))==MAP_FAILED)
	{
		fprintf(stderr,"cannot mmap\n");
	       	return 1;
	}
	mmaped_tmp = mmaped;
	while(map_size--){
		*mmaped++ = *fmaped++;
	}
	
	msync(mmaped_tmp, map_size, MS_ASYNC);
	msync(fmaped_tmp, map_size, MS_ASYNC);
	munmap(fmaped_tmp,map_size);
	munmap(mmaped_tmp,map_size);
	close(fh);
	close(fd);
	
	ficses_start(channel_enable);
		
	return 0;
}	

// Read 128KB of data: FIC --> FICSES
int ficses_read_128KB(char* binary_file, int ficses_channel, int * read_data, int channel_enable_status)
{
	
	char buff[0x80000];
	char file[256];
	char *mmaped, *fmaped;
	char *mmaped_tmp, *fmaped_tmp;
	int fd, fh;
	unsigned int st=0, ch, poff, length = 0x80000, i;
	unsigned long  page_size, map_size, map_size1;
	char c = EOF;
	int channel_enable=0;
	
    switch (ficses_channel) {
        case FICSESL0_CH0: 
			channel_enable= RD_START_FICSESL0_CH0;
		break;
			
        case FICSESL0_CH1: 
			channel_enable= RD_START_FICSESL0_CH1;
		break;
			
        case FICSESL0_CH2: 
			channel_enable= RD_START_FICSESL0_CH2;
		break;
			
        case FICSESL0_CH3: 
			channel_enable= RD_START_FICSESL0_CH3;
		break;

		default: 
			printf("Illegal FICSES channel ID (Must be 0~3) \n");
		return 1;
	}	
	
	if (!channel_enable_status)
		ficses_start(channel_enable);

	sprintf(file,"%s",binary_file);
	fh = open(file, O_CREAT | O_RDWR, 0666);
	if(fh < 0) {
	        printf("Error : can't open file\n");
	        return -1;
	}

	fd = open(DEVICE_FILE,O_RDWR);
	if (fd < 0){
		fprintf(stderr,"Cannot Open /dev/mem\n");
		return 1;
	}

	st=PCIE_BAR_DATA_ADDR;	// base address
	st += ficses_channel*length;
	page_size = getpagesize();
	map_size = ((length-1) /page_size + 1) * page_size;

	lseek(fh, map_size-1, SEEK_SET);
	write(fh, &c, sizeof(char));
	lseek(fh, 0, SEEK_SET);


	if ((mmaped = (char *)mmap(0, map_size, PROT_READ, MAP_SHARED, fd, st))==MAP_FAILED)
	{
		fprintf(stderr,"cannot mmap\n");
       		return 1;
	}
	mmaped_tmp = mmaped;

	if ((fmaped = (char *)mmap(0, map_size, PROT_WRITE, MAP_SHARED, fh, 0))==MAP_FAILED)
	{
		fprintf(stderr,"cannot fmap\n");
       		return 1;
	}
	fmaped_tmp = fmaped;

	map_size1 = map_size;
	while(map_size1--){
		*fmaped++ = *mmaped++;
	}
	msync(fmaped_tmp, map_size-1,MS_SYNC);
	msync(mmaped_tmp, map_size,MS_SYNC);
	munmap(fmaped_tmp, map_size-1);
	munmap(mmaped_tmp, map_size);
	close(fd);
	close(fh);
	
	read_binary_file (binary_file, read_data);
	return 0;
}	


// Read/Write control registers in func4x4/func5x5
int ficses_register_access(int BOARD_ID, int command, int address, unsigned long payload, int ficses_channel, int * read_data) {

	unsigned __int128 	data	=0x0; 
    unsigned int 		header	=0x0; 
	unsigned int 		cmd		=0x0;	 		
	unsigned long		filler	=0x0; 

	int channel_enable=0;
	int src_board_id=0xFF;
	int dst_board_id=BOARD_ID;
	
	cmd	= command | 0x200;	

	int i,j;
	
	FILE *fp;
	
	data= payload;	// Bug! (missing assignement to payload) Added by Akram 08-27-2019 


    switch (ficses_channel) {
        case FICSESL0_CH0: 
			channel_enable= WR_START_FICSESL0_CH0;
		break;
			
        case FICSESL0_CH1: 
			channel_enable= WR_START_FICSESL0_CH1;
		break;
			
        case FICSESL0_CH2: 
			channel_enable= WR_START_FICSESL0_CH2;
		break;
			
        case FICSESL0_CH3: 
			channel_enable= WR_START_FICSESL0_CH3;
		break;

		default: 
			printf("Illegal channel ID (Must be 0~3) \n");
		return 1;
	}	


	/* open binary file to write */
	fp = fopen(WRITE_BINARY_FILE, "wb");
	if (fp == NULL) {
			 printf("Open Error\n");
			 return 1;
	}	
	header= address | src_board_id << 16 | dst_board_id << 24;

	/* Write a single occurence of the desired packet  */
	fwrite(&data, sizeof(data), 1, fp);  
	fwrite(&header, sizeof(header), 1, fp);  
	fwrite(&cmd, sizeof(cmd), 1, fp); 
	fwrite(&filler, sizeof(filler), 1, fp);  

	/* Write remaining dummy packets to fill the write buffer  */
	cmd		= 0x200;
	header	= 0x0;
	data	= 0x0;
	for (i=0; i < PACKETMAX-1; i++)
	{
		fwrite(&data, sizeof(data), 1, fp);    
		fwrite(&address, sizeof(address), 1, fp);  
		fwrite(&cmd, sizeof(cmd), 1, fp);
		fwrite(&filler, sizeof(filler), 1, fp);  
	} 
		
    /* Initialize the read buffer with zeros  */
	for (i=0; i < PACKETMAX; i++)
	{
		fwrite(&filler, sizeof(filler), 1, fp);  
		fwrite(&filler, sizeof(filler), 1, fp);  
		fwrite(&filler, sizeof(filler), 1, fp);  
		fwrite(&filler, sizeof(filler), 1, fp);  
	}
		
	fclose(fp);

	ficses_write_128KB(WRITE_BINARY_FILE, ficses_channel);
	if (command ==REG_RD)
		ficses_read_128KB(READ_BINARY_FILE, ficses_channel, read_data, 0);
		
	return 0;	
	
}

// Write control registers in func4x4/func5x5
int ficses_register_write(int BOARD_ID, int address, unsigned long payload, int ficses_channel) {

	ficses_register_access(BOARD_ID, REG_WR, address, payload, ficses_channel, NULL);

		
	return 0;	
	
}	

// Read control registers in func4x4/func5x5
int ficses_register_read(int BOARD_ID, int address, int ficses_channel) {

	FILE *fp;
	int i;

	int *read_data;
	read_data = (int *)malloc(sizeof(int)*PACKETMAX*4);	
	
	ficses_register_access(BOARD_ID, REG_RD, address, UNDEFINED, ficses_channel, read_data);
		
	printf("Board %d: Register[0x%x]= %08x_%08x_%08x_%08x \n", BOARD_ID, address, read_data[3], read_data[2], read_data[1],read_data[0]);
	
	return 0;	
}

// HLS ap_start
int ficses_ap_start(int BOARD_ID, int ficses_channel) {

	ficses_register_access(BOARD_ID, START, UNDEFINED, UNDEFINED, ficses_channel, NULL);
		
	return 0;	
}

// HLS ap_rst
int ficses_ap_rst(int BOARD_ID, int ficses_channel) {

	ficses_register_access(BOARD_ID, RESET, UNDEFINED, UNDEFINED, ficses_channel, NULL);

	return 0;	
}


// Write table table to func4x4/func5x5
int ficses_table_setting(int BOARD_ID, char* table_file, int ficses_channel) {

	unsigned __int128 	data	=0x0; 
    unsigned int 		header	=0x0; 
	unsigned int 		cmd		=0x0;	 		
	unsigned long		filler	=0x0; 

	int port, slot, tdata;
	int address;
	int channel_enable=0;
	int src_board_id=0xFF;
	int dst_board_id=BOARD_ID;
  	char file[256];
	int i, j, k;
	
	FILE *fp0,*fp1;

    switch (ficses_channel) {
        case FICSESL0_CH0: 
			channel_enable= WR_START_FICSESL0_CH0;
		break;
			
        case FICSESL0_CH1: 
			channel_enable= WR_START_FICSESL0_CH1;
		break;
			
        case FICSESL0_CH2: 
			channel_enable= WR_START_FICSESL0_CH2;
		break;
			
        case FICSESL0_CH3: 
			channel_enable= WR_START_FICSESL0_CH3;
		break;

		default: 
			printf("Illegal channel ID (Must be 0~3) \n");
		return 1;
	}	


	sprintf(file,"%s",table_file);
	/* open binary file to read */
	if ((fp0 = fopen(file, "r")) == NULL){
	       fprintf(stderr,"Cannot open %s\n", file);
             return 1;
	}

	/* open binary file to write */
	fp1 = fopen(WRITE_BINARY_FILE, "wb");
	if (fp1 == NULL) {
			 printf("Open Error\n");
			 return 1;
	}	
	
	fscanf(fp0,"%d %d\n",&port, &slot);
	// Write slot number
	cmd	= REG_WR | 0x200;
	address	= SNUM;
	data	= slot<<1;	
	header	= address | src_board_id << 16 | dst_board_id << 24;
	
	fwrite(&data, sizeof(data), 1, fp1);  
	fwrite(&header, sizeof(header), 1, fp1);  
	fwrite(&cmd, sizeof(cmd), 1, fp1); 
	fwrite(&filler, sizeof(filler), 1, fp1);
	printf("Set table for FIC%d: slot number = %d\n", dst_board_id, slot);
	
	// Write table data
	cmd = TBL_SET| 0x200;
	for(i=0;i<port; i++) {
		fscanf(fp0,"%x \n",&address);
		printf("port:%x\n",i);
		for(k=0; k<slot; k++) {
			fscanf(fp0,"%d\n",&tdata);
			header	= address | src_board_id << 16 | dst_board_id << 24;
			data=tdata;
			
			fwrite(&data, sizeof(data), 1, fp1);  
			fwrite(&header, sizeof(header), 1, fp1);  
			fwrite(&cmd, sizeof(cmd), 1, fp1); 
			fwrite(&filler, sizeof(filler), 1, fp1);
			
			printf("add:0x%04x, data:%d\n",address,tdata);
			address++;
		}
      }

    /* write remaining dummy packets for write buffer */
	cmd		= 0x200;
	data	= 0x0;
	header	= 0x0;
	
	int remaining= port*slot +1;
	for (i=0; i < PACKETMAX-remaining; i++)
	{
		fwrite(&data, sizeof(data), 1, fp1);    
		fwrite(&header, sizeof(header), 1, fp1);  
		fwrite(&cmd, sizeof(cmd), 1, fp1);
		fwrite(&filler, sizeof(filler), 1, fp1);  
	} 

	
    /* Initialize the receiving buffer with zeros */
	for (i=0; i < PACKETMAX; i++)
	{
		fwrite(&filler, sizeof(filler), 1, fp1);  
		fwrite(&filler, sizeof(filler), 1, fp1);  
		fwrite(&filler, sizeof(filler), 1, fp1);  
		fwrite(&filler, sizeof(filler), 1, fp1);  
	}
	/* close file */
    fclose(fp0);
    fclose(fp1);

	ficses_write_128KB(WRITE_BINARY_FILE, ficses_channel);
	
    return 0;
	
}

// User function: write data_countx32bit to DDR data via FICSES
int ddr_write_data(int *array, int BOARD_ID, int data_count, int ficses_channel) {

    int 	data1; 			// 32bit: [31:0]
    int 	data2; 			// 32bit: [127:32]
    int 	data3; 			// 32bit: [159:64]
    int 	data4; 			// 32bit: [127:96]
	
    unsigned int 	header; 		// 32bit: [159:128]
    unsigned int 	cmd=0x2AA;		// 32bit: [191:160] : "02" MUST be provided; "AA" can be random but must be !=0
    unsigned long	filler=0x0; 	// 64bit: [255:192]

	int address=0x00;
	int src_board_id=0xFF;
	int dst_board_id=BOARD_ID;
	
	int i,j;
	int transaction_128KB_count=0;
	int tmp_data_count=0;
	int counter=0;
	
	FILE *fp1;
	
	// Convert the the array pointer size to a multiple of 128KB 	
	if (data_count%PACKETMAX ==0){	
		transaction_128KB_count = data_count/PACKETMAX;
		tmp_data_count=data_count;
	}
	else{
		transaction_128KB_count = 1 + data_count/PACKETMAX;
		tmp_data_count= transaction_128KB_count * PACKETMAX;
		array = (int *)realloc(array, sizeof(int)*tmp_data_count);
	}
	
		
	// Construct the FICSES packet header 32bit: [159:128]	
	header= address | src_board_id << 16 | dst_board_id << 24;

	for ( i = 0; i < transaction_128KB_count; i++){

		/* open binary file to write */
		fp1 = fopen(WRITE_BINARY_FILE, "wb");
		if (fp1 == NULL) {
				 printf("Open Error\n");
				 return 1;
		}
		
		for (j=0; j < PACKETMAX; j++)
		   {
			  counter = (i*PACKETMAX)+j;
			  data1 = array[counter];		   
			  fwrite(&data1, sizeof(data1), 1, fp1);  
			  data2 = 0;		   
			  fwrite(&data2, sizeof(data2), 1, fp1);  
			  data3 = 0;		   
			  fwrite(&data3, sizeof(data3), 1, fp1);  
			  data4 = 0;		   
			  fwrite(&data4, sizeof(data4), 1, fp1);  

			  fwrite(&header, sizeof(header), 1, fp1);  

			  if (counter < data_count)
	  			cmd=0x2AA;
			  else
	  			cmd=0x200;

			  fwrite(&cmd, sizeof(cmd), 1, fp1); 

			  fwrite(&filler, sizeof(filler), 1, fp1);
		   }
		for (j=0; j < PACKETMAX; j++)
		   {
			  fwrite(&filler, sizeof(filler), 1, fp1); 
			  fwrite(&filler, sizeof(filler), 1, fp1); 
			  fwrite(&filler, sizeof(filler), 1, fp1); 
			  fwrite(&filler, sizeof(filler), 1, fp1); 
		   }	
		
    	fclose(fp1);

		ficses_write_128KB(WRITE_BINARY_FILE, ficses_channel);
		
	}
return 0;		
}

// User function: read data_countx32bit from DDR data via FICSES
int ddr_read_data(int BOARD_ID, int address, unsigned long payload, int *array) {	
	
	ficses_start(RD_START_FICSESL0_CH2);

	int *read_data;
	read_data = (int *)malloc(sizeof(int)*PACKETMAX*4);	
	
	ficses_register_write(BOARD_ID, address, payload, FICSESL0_CH1);
	
	ficses_read_128KB(READ_BINARY_FILE, FICSESL0_CH2, read_data, 1);	
	
	for (int i=0; i < PACKETMAX; i++)
	{
  		array[i] = read_data[i*4];
//		printf("array[%d] =%d \n", i, array[i]);
	}	

return 0;		
}
