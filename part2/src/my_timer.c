#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/timekeeping.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("project2group14");
MODULE_DESCRIPTION("Linux kernel timer module");
MODULE_VERSION("1.0");

#define ENTRY_NAME "timer"
#define PERMS 0666
#define PARENT NULL
#define BUF_LEN 100

static struct proc_dir_entry* proc_entry;
static char msg[BUF_LEN];
static int procfs_buf_len;
static int called_before = 0;
static struct timespec64 previous;


static ssize_t procfile_read(struct file* file, char* ubuf, size_t count, loff_t *ppos) {
	struct timespec64 time;
	ktime_get_real_ts64(&time);
	//procfs_buf_len = snprintf(msg, BUF_LEN, "current time: %lld.%9ld\n", time->tv_sec, time->tv_nsec);
	if (called_before) {
		long long secs = (time.tv_sec - previous.tv_sec);
		long nsecs = (time.tv_nsec - previous.tv_nsec);
		if (nsecs < 0 && secs > 0) {
			secs -=1;
			nsecs +=1000000000;
		} else if (nsecs < 0) {
			nsecs +=1000000000;
		}
		procfs_buf_len = snprintf(msg, BUF_LEN, "current time: %lld.%9ld\nelapsed time: %lld.%9ld\n", time.tv_sec, time.tv_nsec, secs, nsecs);
	} else {
		procfs_buf_len = snprintf(msg, BUF_LEN, "current time: %lld.%9ld\n", time.tv_sec, time.tv_nsec);
	}
	called_before = 1;
	previous = time;
    if (*ppos > 0 || count < procfs_buf_len)
        return 0;
    if (copy_to_user(ubuf, msg, procfs_buf_len))
        return -EFAULT;
    *ppos = procfs_buf_len;
    return procfs_buf_len;
}


static const struct proc_ops procfile_fops = {
    .proc_read = procfile_read,
};

static int __init timer_init(void)  {
    proc_entry = proc_create(ENTRY_NAME, PERMS, PARENT, &procfile_fops);
    if (proc_entry == NULL)
        return -ENOMEM;
    return 0;
}

static void __exit timer_exit(void) {
    proc_remove(proc_entry);
}

module_init(timer_init);
module_exit(timer_exit);