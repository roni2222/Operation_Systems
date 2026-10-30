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
	char *message;
	int message_size;
	unsigned long channel_id; 
	int file_descriptor;
	int returned_val;
	unsigned int cen_mode;
	if (argc != 5) {
		fprintf(stderr, "invalid number of arguments Error: %s\n", strerror(EINVAL));
		exit(1);
	}
	channel_id = atoi(argv[2]); 
	cen_mode = atoi(argv[3]);
	message = argv[4];
	message_size = strlen(message);
	file_descriptor = open(argv[1], O_RDWR);
	if (file_descriptor < 0) {
		fprintf(stderr, "Can't open device file: %s\n Error: %s\n", argv[1], strerror(errno));
		exit(1);
	}
	returned_val = ioctl(file_descriptor, MSG_SLOT_SET_CEN, cen_mode);
	if (returned_val != 0) {
		fprintf(stderr, "ioctl MSG_SLOT_SET_CEN failed Error: %s\n", strerror(errno));
		exit(1);
	}
	returned_val = ioctl(file_descriptor, MSG_SLOT_CHANNEL, channel_id);
	if (returned_val != 0) {
		fprintf(stderr, "ioctl failed Error: %s\n", strerror(errno));
		exit(1);
	}
	returned_val = write(file_descriptor, message, message_size);
	if (returned_val != message_size) {
		fprintf(stderr, "write failed Error: %s\n", strerror(errno));
		exit(1);
	}
	close(file_descriptor);
	exit(0);
}