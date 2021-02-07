#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define MAXLEN 100 //max length of one string

MODULE_LICENSE("GPL");
struct message_node{
	char message[MAXLEN];
	int length;
	struct list_head list;
};
static LIST_HEAD(message_list);
static struct dentry *dir, *inputdir, *ptreedir;
static struct task_struct *curr;

//char output[MAXPROC*MAXLEN];
ssize_t total_len = 0;

static ssize_t write_pid_to_input(struct file *fp, 
                                const char __user *user_buffer, 
                                size_t length, 
                                loff_t *position)
{
	//char tempout[MAXPROC][MAXLEN];
	struct message_node *node;
	//LIST_HEAD(message_list);

        pid_t input_pid;
	
        sscanf(user_buffer, "%u", &input_pid);
	//memset(output,0,MAXPROC*MAXLEN);
        curr = pid_task(find_get_pid(input_pid), PIDTYPE_PID);// Find task_struct using input_pid. 

        // Tracing process tree from input_pid to init(1) process
	while(curr->pid!=0){
		
		node = kmalloc(sizeof(struct message_node),GFP_KERNEL);
		if(!node){
			printk("memory allocation failed");
			return NULL;
		}
		memset(node->message,0,MAXLEN);
		node->length=sprintf(node->message,"%s (%d)\n",curr->comm,curr->pid);
		total_len += node->length;
		INIT_LIST_HEAD(&node->list);
		list_add(&node->list,&message_list);
		//printk("in write %s %d",node->message,node->length);
		curr = curr->parent;
	}
        
        return length;
}

static ssize_t read_output(struct file *fp, 
                                char __user *user_buffer, 
                                size_t length, 
                                loff_t *position){
	ssize_t temp_len;
	struct list_head *ptr, *ptrn;
	struct message_node *node;
	char temp[total_len+1];
	memset(temp,0,total_len+1);
	list_for_each(ptr,&message_list){
		node = list_entry(ptr,struct message_node, list);
		//printk("in read : %s",node->message);
		strcat(temp,node->message);
		//temp_len+= simple_read_from_buffer(user_buffer,length,position,node->message,node->length);
	}
	temp_len = simple_read_from_buffer(user_buffer,length,position,temp,total_len);
	list_for_each_safe(ptr,ptrn,&message_list){ // free all node
		node = list_entry(ptr,struct message_node, list);
		list_del(ptr);
		kfree(node);
	}
	return temp_len;
}
static const struct file_operations dbfs_fops = {
        .write = write_pid_to_input,
};
static const struct file_operations dbfs_fops_for_read = {
	.read = read_output,
};

static int __init dbfs_module_init(void)
{
        // Implement init module code


        dir = debugfs_create_dir("ptree", NULL);
        
        if (!dir) {
                printk("Cannot create ptree dir\n");
                return -1;
        }

        inputdir = debugfs_create_file("input",00700 ,dir ,NULL ,&dbfs_fops );
        ptreedir = debugfs_create_file("ptree",00700 ,dir ,NULL ,&dbfs_fops_for_read);
	if(!ptreedir)
		printk("ptreedir not created\n");
	
	printk("dbfs_ptree module initialize done\n");

        return 0;
}

static void __exit dbfs_module_exit(void)
{
        // Implement exit module code
	debugfs_remove_recursive(dir);
	printk("dbfs_ptree module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);
