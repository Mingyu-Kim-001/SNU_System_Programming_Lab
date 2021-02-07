#include <linux/debugfs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <asm/pgtable.h>
#define MAXLEN 100
MODULE_LICENSE("GPL");

static struct dentry *dir, *output;
static struct task_struct *task;

static ssize_t read_output(struct file *fp,
                        char __user *user_buffer,
                        size_t length,
                        loff_t *position)
{
	pid_t input_pid;
	unsigned long vaddr = 0;
	int i;
	//unsigned char copied_pckt[MAXLEN];
	//memset(copied_pckt,0,MAXLEN);
	unsigned char copied[MAXLEN];
	int length_copied = length>MAXLEN?MAXLEN:length;
        copy_from_user(copied,user_buffer,length_copied);
	
	//input_pid = (((int)copied[1])<<8) + (int)copied[0]; // pid is at most 2 bytes(little endian).
	memcpy(&input_pid,copied,sizeof(pid_t));
	//for(i=8;i<16;i++){
	//	vaddr+= (((long)copied[i])<<(8*(i-8)));//little endian
	//}
	memcpy(&vaddr,copied+8,8);
	task = pid_task(find_get_pid(input_pid), PIDTYPE_PID);

	pgd_t *pgdp;
	p4d_t *p4dp;
	pud_t *pudp;
	pmd_t *pmdp;
	pte_t *ptep;
	phys_addr_t pfn;
	unsigned char output_str[MAXLEN];

	pgdp = pgd_offset(task->mm,vaddr);
	p4dp = p4d_offset(pgdp,vaddr);
	pudp = pud_offset(p4dp,vaddr);
	pmdp = pmd_offset(pudp,vaddr);
	ptep = pte_offset_kernel(pmdp,vaddr);
	//pte = pte_val(ptep) & PHYS_MASK;
	pfn = pte_pfn(*ptep);
	//printk("pfn : %x",pfn);
	//pfn = 0x1234567;
	//vaddr = 0x1234567111;
	//printk("pfn %x",pfn);
	//printk("vaddr %lx",vaddr);
	//printk("pfn<<12 %llx",((unsigned long long)pfn)<<12);
	//printk("vaddr 12 : %llx",(unsigned long long)(vaddr % 4096));
	unsigned long long paddr = (((unsigned long long)pfn)<<12) + ((unsigned long long)(vaddr % 4096));
	//printk("paddr : %llx ",paddr);
	//printk("end");
	memcpy(copied+16,&paddr,8);
	return (ssize_t)simple_read_from_buffer(user_buffer, length, position, copied, length_copied);
}

static const struct file_operations dbfs_fops = {
        .read = read_output,
};

static int __init dbfs_module_init(void)
{
        // Implement init module


        dir = debugfs_create_dir("paddr", NULL);

        if (!dir) {
                printk("Cannot create paddr dir\n");
                return -1;
        }

        output = debugfs_create_file("output", 00700 , dir , NULL , &dbfs_fops);


	printk("dbfs_paddr module initialize done\n");

        return 0;
}

static void __exit dbfs_module_exit(void)
{
        // Implement exit module
	debugfs_remove_recursive(dir);
	printk("dbfs_paddr module exit\n");
}

module_init(dbfs_module_init);
module_exit(dbfs_module_exit);
