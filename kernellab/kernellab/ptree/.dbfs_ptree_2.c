#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#define LENGTH_MAX 1000

//a doubly linked list can also be used

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ptree : a simple process tree");
MODULE_AUTHOR("Seonkyo Ok");

static struct dentry *dir, *inputdir, *ptreedir;
static struct task_struct *curr;
//output
char out_str[LENGTH_MAX];
ssize_t length_temp, length_total = 0;

static ssize_t write_pid_to_input(struct file *fp, 
                                const char __user *user_buffer, 
                                size_t length, 
                                loff_t *position)
{
    pid_t input_pid;
    char temp_str[LENGTH_MAX];

    //total length of the output
    length_total = 0;
    //initialize to 0
    memset(out_str, 0, LENGTH_MAX);

/*
 * GET AN INPUT
 */
    //check
    if(copy_from_user(temp_str, user_buffer, length))
        return -EFAULT;
    sscanf(temp_str, "%u", &input_pid);

/*
 * MAKE AN OUTPUT STRING
 */
    //find task_struct using input_pid
    curr = pid_task(find_get_pid(input_pid), PIDTYPE_PID);
    //invalid input
    if(!curr)
        return -EINVAL;
    //trace process tree from input_pid to init(1) process
    while(curr->pid != 0) {
        //initialize temp_str to 0
        memset(temp_str, 0, LENGTH_MAX);
        //make output format string: process_command (process_id)
        length_temp = snprintf(temp_str, LENGTH_MAX, "%s (%d)\n", curr->comm, curr->pid);
        //append the last result 
        snprintf(temp_str+length_temp, LENGTH_MAX-length_temp, out_str);
        //add to the total length
        if(length_temp >= 0)
            length_total += length_temp;
        //update output string
        strcpy(out_str, temp_str);
        //go to parent
        curr = curr -> parent;
    }
    return length;
}

static ssize_t read_output(struct file *fp, 
                                char __user *user_buffer, 
                                size_t length, 
                                loff_t *position)
{
    ssize_t length_copied;
    //copy the result to user buffer
    length_copied = simple_read_from_buffer(user_buffer, length, position, out_str, length_total);
    return length_copied;
}

static const struct file_operations dbfs_fops_write = {
    .write = write_pid_to_input,
};

static const struct file_operations dbfs_fops_read = {
    .read = read_output,
};

static int __init dbfs_module_init(void)
{
    dir = debugfs_create_dir("ptree", NULL);
    if (!dir) {
        pr_err("Cannot create ptree dir\n");
        return -1;
    }
    inputdir = debugfs_create_file("input", 00700, dir, NULL, &dbfs_fops_write);
    if (!inputdir) {
        pr_err("Cannot create inputdir file\n");
        return -1;
    }
    ptreedir = debugfs_create_file("ptree", 00700, dir, NULL, &dbfs_fops_read);
    if (!ptreedir) {
        pr_err("Cannot create ptreedir file\n");
        return -1;
    }
    printk("dbfs_ptree module initialize done\n");
    return 0;
}

static void __exit dbfs_module_exit(void)
{
    debugfs_remove_recursive(dir);    
	printk("dbfs_ptree module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);
