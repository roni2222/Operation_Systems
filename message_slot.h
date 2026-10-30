#ifndef _MESSAGE_SLOT_H_ //נועד לקריאת התוכן של הקובץ הזה רק פעם אחת במהלך הקימפול
#define _MESSAGE_SLOT_H_ //נועד לקריאת התוכן של הקובץ הזה רק פעם אחת במהלך הקימפול
#include <linux/ioctl.h>
#define MAJOR_NUM 235 //מספר שנועד כדי לדעת לאיזה מודול להפנות את הבקשות
#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUM, 0, unsigned int)
#define MSG_SLOT_SET_CEN _IOW(MAJOR_NUM, 1, unsigned int)
//קבועים כלליים
#define DEVICE_RANGE_NAME "message_slot"
#define BUF_LEN 128 //אורך מקסימלי של הודעה
#define SUCCESS 0

typedef struct channel {
	long channel_id; //מספר הערוץ
	int msg_size; //האורך של ההודעה ששמורה כרגע
	char message[BUF_LEN]; //תוכן ההודעה האחרונה שנכתבה
	struct channel *next;
} channel;
typedef struct channel_list {
	struct channel *head;
} channel_list;
typedef struct file_data {
	int minor_num;
	long working_channel_id;
	channel *working_channel;
	unsigned int censorship_mode;
} file_data;

#endif //נועד לקריאת התוכן של הקובץ הזה רק פעם אחת במהלך הקימפול