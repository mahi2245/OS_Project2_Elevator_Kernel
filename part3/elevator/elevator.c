#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/slab.h>

#define ENTRY_NAME "elevator"
#define PERMS 0666
#define PARENT NULL
#define BUF_LEN 100

static int __init elevator_init(void);
static int __init elevator_exit(void);

static int currPets;
static int maxWeight;
static int currFloor;
static bool boolUp = 1;
static bool stop = 0;

static struct task_struct *elevator_thread;
static struct task_struct *stop_thread;

struct mutex mut;
static int runningElevator(void *data);
static int stopOperation(void *data);


struct list_head floorPets[5];
struct list_head elevatorPets;

// not sure yet
typedef struct pet {
    struct list_head list;
    int type;
    int destination;
} pet;

int weightCheck(pet *p);


// dumb way of getting weight based on using 'type' as just an int
// theres better ways to do this, this is placeholder
int weightCheck(pet *p) {
    if (p -> type == 0) {
        return 3;
    }
    if (p -> type == 1) {
        return 14;
    }
    if (p -> type == 2) {
        return 10;
    }
    else {
        return 16;
    }
}

static int runningElevator(void *data) {

    struct list_head *temp, *dummy;
    pet * item;
    while (true) {
        mutex_lock(&mut);
        
        // unload first
        list_for_each_safe(temp, dummy, &elevatorPets) {
            item = list_entry(temp, pet, list);
            if (item->destination == currFloor){
                maxWeight -= weightCheck(item->type);
                currPets -=1;
                list_del(temp);
                kfree(item);
            }
        }

        // move floors according to scheduling algorithm
        while (maxWeight < 50 && currPets < 5){
             list_for_each_safe(temp, dummy, &floorPets[currFloor]){
                item = list_entry(temp, pet, list);
                if (weightCheck(item->type) + maxWeight <= 50){
                    list_add_tail(&item->list, &elevatorPets);
                    list_del(temp);
                    maxWeight += weightCheck(item->type);
                    currPets += 1;
                }
             }
        }
        // temp scheduler before scheduling is actually made.
        if (currFloor == 4) {
            boolUp = 0;
        }
        if (currFloor == 0)
        {
            boolUp = 1;
        }
        if (boolUp == 1) {
            currFloor+=1;
        }
        else {
            currFloor-=1;
        }
        mutex_unlock(&mut);

    }
    return 0;
}


int stopOperation() {
    struct list_head *temp, *dummy;
    pet * item;
    while (true){
        if (stop == 1) {
            while (!list_empty(&elevatorPets)) {
                mutex_lock(&mut);
                list_for_each_safe(temp, dummy, &elevatorPets) {
                    item = list_entry(temp, pet, list);
                    if (item->destination == currFloor){
                        maxWeight -= weightCheck(item->type);
                        currPets -=1;
                        list_del(temp);
                        kfree(item);
                    }
                }


            //in actuality, either follow scheduling algo
            // or go directly to
            if (currFloor == 4) {
                boolUp = 0;
            }
            if (currFloor == 0)
            {
                boolUp = 1;
            }
            if (boolUp == 1) {
                currFloor+=1;
            }
            else {
                currFloor-=1;
            }
            mutex_unlock(&mut);
        }
    }
}
    return 0;
    
}

// void procRequest() {

// }



static int __init elevator_init(void) {
    mutex_init(&mut);
    printk(KERN_INFO, "setting up threads?");
    elevator_thread = kthread_run(runningElevator, NULL, "elevator_thread");
    stop_thread = kthread_run(stopOperation, NULL, "stop_thread");
    return 0;
}

static void __exit elevator_exit() {
    kthread_stop(elevator_thread);
    kthread_stop(stop_thread);
}