#include <linux/init.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/sched.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("project2group14");
MODULE_DESCRIPTION("Linux pet elevator module");
MODULE_VERSION("1.0");

#define ENTRY_NAME "elevator"
#define PERMS 0666
#define PARENT NULL
#define BUF_LEN 1000
#define NUM_FLOORS 5

enum Status {OFFLINE, IDLE, LOADING, UP, DOWN };

typedef struct {
    int goal;
    int type;
    int weight;
    struct list_head node;
} Pet;
typedef struct {
    enum Status status;
    int current_floor;
    int num_pets;
    int weight;
    int total;
    int canpickup;
    int target;
    struct list_head pets;
} Elevator;
typedef struct {
    int num_pets;
    int floor_num;
    struct list_head pets;
    struct list_head node;
} Floor;
typedef struct {
    struct list_head floors;
} Building;

static struct proc_dir_entry* proc_entry;
static Elevator elevator;
static Building building;
static int procfs_buf_len;
static char msg[BUF_LEN];
extern int (*STUB_start_elevator)(void);
extern int (*STUB_issue_request)(int, int, int);
extern int (*STUB_stop_elevator)(void);
static struct task_struct *elevator_thread;
struct mutex mut;


int start_elevator(void);                                                           // starts the elevator to pick up and drop off passengers
int issue_request(int start_floor, int destination_floor, int type);                // add passengers requests to specific floors
int stop_elevator(void);                                                            // stops the elevator



static ssize_t procfile_read(struct file* file, char* ubuf, size_t count, loff_t *ppos) {
    int waiting_pets = 0;
	mutex_lock(&mut);
    procfs_buf_len = snprintf(msg + procfs_buf_len, BUF_LEN, "Elevator state: ");
    switch (elevator.status) {
        case OFFLINE:
            procfs_buf_len += snprintf(msg + procfs_buf_len, BUF_LEN, "OFFLINE\n");
            break;
        case IDLE:
            procfs_buf_len += snprintf(msg + procfs_buf_len, BUF_LEN, "IDLE\n");
            break;
        case UP:
            procfs_buf_len += snprintf(msg + procfs_buf_len, BUF_LEN, "UP\n");
            break;
        case DOWN:
            procfs_buf_len += snprintf(msg + procfs_buf_len, BUF_LEN, "DOWN\n");
            break;
        case LOADING:
            procfs_buf_len += snprintf(msg + procfs_buf_len, BUF_LEN, "LOADING\n");
            break;
    }
    procfs_buf_len += snprintf(msg + procfs_buf_len, BUF_LEN, "Current floor: %d\nCurrent load: %d lbs\nElevator status: ", elevator.current_floor, elevator.weight);
    Pet *p;
    list_for_each_entry(p, &elevator.pets, node) {
        switch (p->type) {
            case 0:
                procfs_buf_len += snprintf(msg + procfs_buf_len, BUF_LEN, "C");
                break;
            case 1:
                procfs_buf_len += snprintf(msg + procfs_buf_len, BUF_LEN, "P");
                break;
            case 2:
                procfs_buf_len += snprintf(msg + procfs_buf_len, BUF_LEN, "H");
                break;
            case 3:
                procfs_buf_len += snprintf(msg + procfs_buf_len, BUF_LEN, "D");
                break;
        }
        procfs_buf_len += snprintf(msg + procfs_buf_len, BUF_LEN, "%d ", p->goal);
    }
    procfs_buf_len += snprintf(msg + procfs_buf_len, BUF_LEN, "\n\n\n");
    Floor *f;
    list_for_each_entry(f, &building.floors, node) {
        procfs_buf_len += snprintf(msg + procfs_buf_len, BUF_LEN, "[");
        if (f->floor_num == elevator.current_floor) {
            procfs_buf_len += snprintf(msg + procfs_buf_len, BUF_LEN, "*");
        } else {
            procfs_buf_len += snprintf(msg + procfs_buf_len, BUF_LEN, " ");
        }
        procfs_buf_len += snprintf(msg + procfs_buf_len, BUF_LEN, "] Floor %d: %d ", f->floor_num, f->num_pets);
        list_for_each_entry(p, &f->pets, node) {
            switch (p->type) {
                case 0:
                    procfs_buf_len += snprintf(msg + procfs_buf_len, BUF_LEN, "C");
                break;
                case 1:
                    procfs_buf_len += snprintf(msg + procfs_buf_len, BUF_LEN, "P");
                break;
                case 2:
                    procfs_buf_len += snprintf(msg + procfs_buf_len, BUF_LEN, "H");
                break;
                case 3:
                    procfs_buf_len += snprintf(msg + procfs_buf_len, BUF_LEN, "D");
                break;
            }
            procfs_buf_len += snprintf(msg + procfs_buf_len, BUF_LEN, "%d ", p->goal);
            waiting_pets++;
        }
        procfs_buf_len += snprintf(msg + procfs_buf_len, BUF_LEN, "\n");
    }
    procfs_buf_len += snprintf(msg + procfs_buf_len, BUF_LEN, "\n\n\nNumber of pets: %d\nNumber of pets waiting: %d\nNumber of pets serviced: %d\n", elevator.num_pets, waiting_pets, elevator.total);
	mutex_unlock(&mut);

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

int elevatorLoop(void* data) {
    while(!kthread_should_stop()) {
        while (elevator.status == OFFLINE && !kthread_should_stop()) {
            msleep_interruptible(1000);
        }
        if (elevator.canpickup == 0 && elevator.num_pets == 0) {
            elevator.status = OFFLINE;
            continue;
        }
        mutex_lock(&mut);
        if (elevator.status == IDLE) {
        	int waiting_pets = 0;
            int d = 99999;
            int close_pet = -1;
        	Floor* f;
        	Pet* p;
        	list_for_each_entry(f, &building.floors, node) {
            	list_for_each_entry(p, &f->pets, node) {
                	if (abs(f->floor_num-elevator.current_floor) < d) {
                        d = abs(f->floor_num-elevator.current_floor);
                        close_pet = f->floor_num;
                    }
             	   	waiting_pets++;
            	}
        	}
            if (waiting_pets > 0) {
				elevator.target = close_pet;
                if (elevator.target == elevator.current_floor) {
                	elevator.status = LOADING;
                    mutex_unlock(&mut);
                    msleep(1000);
                    mutex_lock(&mut);
					Floor *a;
					Pet *y, *z;
                    list_for_each_entry(a, &building.floors, node) {
                        if (elevator.current_floor == a->floor_num) {
                            list_for_each_entry_safe(y, z, &a->pets, node) {
                                if (elevator.num_pets < 5 && elevator.weight + y->weight <= 50 && elevator.canpickup == 1) {
                                    list_del(&y->node);
                                    list_add_tail(&y->node, &elevator.pets);
                                    a->num_pets--;
                                    elevator.num_pets++;
                                    elevator.weight+=y->weight;
                                } else {
                                    break;
                                }
                            }
                        }
                    }

                    int closest_floor = 99999;
                    d = 99999;
                    list_for_each_entry(z, &elevator.pets, node) {
                        if (abs(z->goal-elevator.current_floor) < d) {
                            d = abs(z->goal-elevator.current_floor);
                            closest_floor = z->goal;
                        }
                    }
                    elevator.target = closest_floor;
                    if (elevator.target < elevator.current_floor) {
                        elevator.status = DOWN;
                    }
                    else {
                        elevator.status = UP;
                    }
                    mutex_unlock(&mut);
                    continue;
                } else {
                    if (elevator.target < elevator.current_floor) {
                        elevator.status = DOWN;
                    }
                    else {
                        elevator.status = UP;
                    }
                    mutex_unlock(&mut);
                    continue;
                }
            }
            mutex_unlock(&mut);
        } else if (elevator.status == UP || elevator.status == DOWN) {
            mutex_unlock(&mut);
            msleep(2000);
            mutex_lock(&mut);
            if (elevator.status == UP)
                elevator.current_floor++;
            else
                elevator.current_floor--;
            Floor *a;
			Pet *y, *z;
            if (elevator.target == elevator.current_floor) {
                list_for_each_entry(a, &building.floors, node) {
                    if (elevator.current_floor == a->floor_num) {
                        list_for_each_entry_safe(y, z, &elevator.pets, node) {
                            if (elevator.status != LOADING) {
                                elevator.status = LOADING;
                                mutex_unlock(&mut);
                                msleep(1000);
                                mutex_lock(&mut);
                            }
                            if (y->goal == elevator.current_floor) {
                                elevator.num_pets--;
                                elevator.weight-=y->weight;
                                elevator.total++;
                                list_del(&y->node);
                                kvfree(y);
                            }
                        }
                    }
                }
            }

            list_for_each_entry(a, &building.floors, node) {
                if (elevator.current_floor == a->floor_num) {
                    list_for_each_entry_safe(y, z, &a->pets, node) {
                        if (elevator.num_pets < 5 && elevator.weight + y->weight <= 50 && elevator.canpickup == 1) {
                            if (elevator.status != LOADING) {
                                elevator.status = LOADING;
                                mutex_unlock(&mut);
                                msleep(1000);
                                mutex_lock(&mut);
                            }
                            list_del(&y->node);
                            list_add_tail(&y->node, &elevator.pets);
                            a->num_pets--;
                            elevator.num_pets++;
                            elevator.weight+=y->weight;
                        } else {
                            break;
                        }
                    }
                }
            }
            if (elevator.status == LOADING) {
                if (elevator.num_pets == 0) {
                    elevator.status = IDLE;
                    mutex_unlock(&mut);
                    continue;
                }
                if (elevator.target == elevator.current_floor) {
                    int closest_floor = 99999;
                    int d = 99999;
                    list_for_each_entry(z, &elevator.pets, node) {
                        if (abs(z->goal-elevator.current_floor) < d) {
                            d = abs(z->goal-elevator.current_floor);
                            closest_floor = z->goal;
                        }
                    }
                    elevator.target = closest_floor;
                }
            }
            if (elevator.target < elevator.current_floor)
                elevator.status = DOWN;
            else
                elevator.status = UP;
            mutex_unlock(&mut);
            continue;
        }
    }
    return 0;
}

int start_elevator(void) {
    mutex_lock(&mut);
    if (elevator.status == OFFLINE) {
        wake_up_process(elevator_thread);
        elevator.canpickup = 1;
        elevator.status = IDLE;
    } else {
        mutex_unlock(&mut);
        return 1;
    }
    mutex_unlock(&mut);
    return 0;
}

int issue_request(int start_floor, int destination_floor, int type) {
    if (start_floor > NUM_FLOORS || destination_floor > NUM_FLOORS || type < 0 || type > 3 || start_floor < 1 || destination_floor < 1)
        return 1;

    mutex_lock(&mut);
    Floor* f;
    list_for_each_entry(f, &building.floors, node) {
        if (f->floor_num == start_floor) {
            f->num_pets++;
            Pet* newpet;
            newpet = kzalloc(sizeof(*newpet), GFP_KERNEL);
            if (!newpet)
                return -ENOMEM;
            newpet->goal = destination_floor;
            newpet->type = type;
            switch (type) {
                case 0:
                    newpet->weight = 3;
                    break;
                case 1:
                    newpet->weight = 14;
                    break;
                case 2:
                    newpet->weight = 10;
                    break;
                case 3:
                    newpet->weight = 16;
                    break;
            }
            list_add_tail(&newpet->node, &f->pets);
        }
    }
    mutex_unlock(&mut);
    return 0;
}

int stop_elevator(void) {
    if (elevator.canpickup == 0) return 1;
    mutex_lock(&mut);
    printk("ELEVATOR STOP CALLED");
    if (elevator.status != OFFLINE) {
        printk("ELEVATOR NOT OFFLINE YET");
        while (elevator.num_pets > 0) {
            printk("ELEVATOR STOP WHILE LOOP");
            elevator.canpickup = 0;
            mutex_unlock(&mut);
            msleep(2000);
            mutex_lock(&mut);
        }
        elevator.status = OFFLINE;
        printk("ELEVATOR OFFLINE");
    }
    mutex_unlock(&mut);
    return 0;
}

static int __init elevator_init(void) {
    mutex_init(&mut);
    proc_entry = proc_create(ENTRY_NAME, PERMS, PARENT, &procfile_fops);
    if (proc_entry == NULL)
        return -ENOMEM;
    INIT_LIST_HEAD(&building.floors);
    Floor* t;
    for (int i = 1; i <= NUM_FLOORS; i++) {
        t = kzalloc(sizeof(*t), GFP_KERNEL);
        if (!t)
            return -ENOMEM;
        t->num_pets = 0;
        t->floor_num = i;
        INIT_LIST_HEAD(&t->pets);
        list_add(&t->node, &building.floors);
    }
    elevator.status = OFFLINE;
    elevator.current_floor = 1;
    elevator.num_pets = 0;
    elevator.weight = 0;
    elevator.total = 0;
    elevator.canpickup = 1;
    elevator.target = -1;
    INIT_LIST_HEAD(&elevator.pets);
    STUB_start_elevator = start_elevator;
    STUB_issue_request = issue_request;
    STUB_stop_elevator = stop_elevator;
    elevator_thread = kthread_run(elevatorLoop, NULL, "elevator_thread");
    if (IS_ERR(elevator_thread)) {
        printk("elevator thread init error\n");
        return PTR_ERR(elevator_thread);
    }
    return 0;  // Return 0 to indicate successful loading
}

static void __exit elevator_exit(void) {
    stop_elevator();
    int ret = kthread_stop(elevator_thread);
    if (ret != -EINTR)
        printk("elevator thread stopped\n");
    Floor *a, *b;
    Pet *y, *z;
    list_for_each_entry_safe(a, b, &building.floors, node) {
        list_del(&a->node);
        list_for_each_entry_safe(y, z, &a->pets, node) {
            list_del(&y->node);
            kvfree(y);
        }
        kvfree(a);
    }

    STUB_start_elevator = NULL;
    STUB_issue_request = NULL;
    STUB_stop_elevator = NULL;
    proc_remove(proc_entry);
}

module_init(elevator_init);  // Specify the initialization function
module_exit(elevator_exit);  // Specify the exit/cleanup function
