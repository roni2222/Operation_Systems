#undef __KERNEL__
#define __KERNEL__ //קוד קרנל
#undef MODULE
#define MODULE //קוד של מודול שנטען לקרנל

//ספריות שמאפשרות לקרוא לפונקציות מיוחדות
#include <linux/kernel.h>   
#include <linux/module.h>   
#include <linux/fs.h>       
#include <linux/uaccess.h>  
#include <linux/string.h> 
#include <linux/slab.h>

MODULE_LICENSE("GPL"); //רישיון
#include "message_slot.h"
channel_list devices_data[257]; //כל תא מייצג קובץ התקן שונה

//פונקצית הניקיון למניעת זליגות זיכרון בקרנל
static int device_release(struct inode* inode, struct file*  file) {
	printk("Invoking device_release(%p,%p)\n", inode, file);
	kfree(file->private_data);
	return SUCCESS;
}

static int device_open(struct inode* inode, struct file*  file) {
	int minor = iminor(inode); //לאיזה minor number המשתמש מנסה להתחבר
	file_data *data;
	printk("Invoking device_open(%p) minor num is: %d\n", file,minor);
	data = kmalloc(sizeof(file_data), GFP_KERNEL); //הקצאת זיכרון דינאמית בקרנל
	if (data==NULL) {
		printk("file data malloc failed device_open(%p)\n", file);
		return -ENOMEM; //אין זיכרון
	}
	data->minor_num = minor;
	data->working_channel_id = 0;
	data->censorship_mode = 0;
	file->private_data = (void*)data;
	return SUCCESS;
}

//קריאת הודעה מהדרייבר
static ssize_t device_read(struct file* file, char __user* buffer, size_t length, loff_t* offset) {
	int i;
	int message_size;
	int fail_flag = 0;
	char *last_message;
	file_data *data = (file_data*)(file->private_data);
	printk("Invoking device_read(%p,%ld)\n",file, length);
	if (data->working_channel_id == 0 || buffer == NULL) {
		return -EINVAL;
	}
	last_message = data->working_channel->message;
	message_size = data->working_channel->msg_size;
	if (message_size==0) {
		return -EWOULDBLOCK;
	}
	if (length < message_size) {
		return -ENOSPC;
	}
	for (i = 0; i < message_size; ++i) {
		if (put_user(last_message[i], buffer + i) != 0) {
			fail_flag = 1;
		}
	}
	if (fail_flag) {
		return -EFAULT;
	}
	return i;
}

//כתיבת הודעה לדרייבר
static ssize_t device_write(struct file* file, const char __user* buffer, size_t length, loff_t* offset) {
	int i,fail_flag=0;
	char kbuffer[BUF_LEN];
	file_data *data = (file_data*)(file->private_data);
	printk("Invoking device_write(%p,%ld)\n", file, length);
	if ((data->working_channel_id == 0)||buffer == NULL){
		return -EINVAL;
	}
	if (length == 0||length > BUF_LEN) {
		return -EMSGSIZE;
	}
	for (i = 0; i < length && i < BUF_LEN; ++i) {
		if (get_user(kbuffer[i], &buffer[i]) != 0) {
			fail_flag = 1;
		}
		if (data->censorship_mode == 1 && (i % 4) == 3) {
			kbuffer[i] = '#';
		}
	}
	if (fail_flag) {
		return -EFAULT;
	}
	data->working_channel->msg_size = length;
	for (i = 0; i < length && i < BUF_LEN; ++i) {
		data->working_channel->message[i] = kbuffer[i];
	}
	return i;
}

//ניהול הרשימה המקושרת של הערוצים עבור ה-Minor הנוכחי
static long device_ioctl(struct file* file, unsigned int ioctl_command_id, unsigned long ioctl_param) {
	file_data *data = (file_data*)(file->private_data);
	channel *head = devices_data[data->minor_num].head;
	channel *temp;
	printk("Invoking ioctl: setting channel id to %ld\n", ioctl_param);
	if (ioctl_command_id == MSG_SLOT_SET_CEN) {
		if (ioctl_param != 0 && ioctl_param != 1) {
			return -EINVAL;
		}
		data->censorship_mode = (unsigned int)ioctl_param;
		return SUCCESS;
	}
	if(ioctl_command_id != MSG_SLOT_CHANNEL || ioctl_param == 0){
		return -EINVAL;
	}
	head = devices_data[data->minor_num].head;
	if (head == NULL) {
		head = kmalloc(sizeof(channel), GFP_KERNEL);
		if (head==NULL) {
			printk("new channel malloc failed in ioctl");
			return -ENOMEM;
		}
		head->channel_id = ioctl_param;
		head->msg_size = 0;
		head->next = NULL;
		data->working_channel_id = ioctl_param;
		data->working_channel = head;
		devices_data[data->minor_num].head=head;
		return SUCCESS;
	}
	else {
		temp = head;
		while (temp->next != NULL) {
			if (temp->channel_id== ioctl_param) {
				data->working_channel_id = ioctl_param;
				data->working_channel = temp;
				return SUCCESS;
			}
			temp = temp->next;
		}
		if (temp->channel_id == ioctl_param) {
			data->working_channel_id = ioctl_param;
			data->working_channel = temp;
			return SUCCESS;
		}
		temp->next = kmalloc(sizeof(channel), GFP_KERNEL);
		if (temp->next==NULL) {
			printk("new channel malloc failed in ioctl");
			return -ENOMEM;
		}
		temp->next->channel_id = ioctl_param;
		temp->next->msg_size = 0;
		temp->next->next = NULL;
		data->working_channel_id = ioctl_param;
		data->working_channel = temp->next;
		return SUCCESS;
	}
}


struct file_operations Fops =
 {
	.owner = THIS_MODULE,
	.read = device_read,
	.write = device_write,
	.open = device_open,
	.unlocked_ioctl = device_ioctl,
	.release = device_release,
 };

//אתחול המודול
static int __init simple_init(void)
{
	int rc = -1;
	int i;
	rc = register_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME, &Fops);
	if (rc < 0)
	{
		//ערך שלילי זו שגיאה
		printk(KERN_ERR "%s registraion failed for  %d\n",DEVICE_RANGE_NAME, MAJOR_NUM);
		return rc;
	}
	for(i=0;i<257;i++) {
		devices_data[i].head = NULL;
	}
	printk("Registeration is successful. ");
	return SUCCESS;
}

//מחיקת המודול
static void __exit simple_cleanup(void)
{
	channel *head;
	channel *temp;
	int i;
	for(i=0;i<257;i++){
		head=devices_data[i].head;
		while (head != NULL)
		{
			temp = head;
			head = head->next;
			kfree(temp);
		}
	}
	unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME);
}

module_init(simple_init);
module_exit(simple_cleanup);
