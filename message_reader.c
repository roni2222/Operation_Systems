#include "message_slot.h"
#include <fcntl.h>      
#include <unistd.h>     
#include <sys/ioctl.h> 


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>



int main(int argc, char** argv)
{
	int returned_val;
	char buffer[BUF_LEN];
	int file_descriptor;
	unsigned long channel_id; 
	if (argc != 3) {
		fprintf(stderr, "invalid number of arguments Error: %s\n", strerror(EINVAL));
		exit(1);
	}
	channel_id = atoi(argv[2]);
	file_descriptor = open(argv[1], O_RDWR);
	if (file_descriptor < 0) {
		fprintf(stderr, "Can't open device file - %s\n Error: %s\n", DEVICE_RANGE_NAME, strerror(errno));
		exit(1);
	}
	returned_val = ioctl(file_descriptor, MSG_SLOT_CHANNEL, channel_id);
	if (returned_val != 0) {
		fprintf(stderr, "ioctl failed Error: %s\n", strerror(errno));
		exit(1);
	}
	returned_val = read(file_descriptor, buffer, BUF_LEN);
	if (returned_val <= 0) {
		fprintf(stderr, "read failed Error: %s\n", strerror(errno));
		exit(1);
	}
	if (write(1, buffer, returned_val) != returned_val) {
		fprintf(stderr, "print out message failed Error: %s\n", strerror(errno));
		exit(1);
	}
	close(file_descriptor);
	exit(0);
}