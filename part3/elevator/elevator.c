#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include<linux/module.h>
#include<linux/delay.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("OSPROJ2GROUP24");
MODULE_DESCRIPTION("elevator");
MODULE_VERSION("1.0");

#define ENTRY_NAME "elevator"
#define PERMS 0666
#define PARENT NULL
#define BUF_LEN 100

static int __init elevator_init(void);
static void __init elevator_exit(void);
static void __init proc_init(void);

static int currPets;
static int maxWeight;
static int currFloor;
static bool boolUp = 1;
static bool stop = 0;

static struct task_struct *elevator_thread;
static struct task_struct *stop_thread;
static struct task_struct *proc_thread;

struct mutex mut;
static int runningElevator(void *data);
static int stopOperation(void *data);
static int procRequest(void *data);

struct list_head floorPets[5];
struct list_head elevatorPets;
//not on a floor yet, just brought in from a request
struct list_head waitingPets;

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
                ssleep(1);
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
                    ssleep(1);
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
        ssleep(2);
        mutex_unlock(&mut);

    }
    return 0;
}


static int stopOperation(void *data) {
    struct list_head *temp, *dummy;
    pet * item;
    while (!kthread_should_stop()){
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
                        ssleep(1);
                    }
                }
                mutex_unlock(&mut);

            mutex_lock(&mut);
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
            ssleep(2);
            mutex_unlock(&mut);
        }
    }
}
    return 0;
    
}



static int procRequest(void *data) {
    struct list_head *temp, *dummy;
    pet * item;
    while (!kthread_should_stop()){
        // when request happens
        // or while queue is occupied with pets
        // should work through this queue, adding them onto floors
        mutex_lock(&mut);
        list_for_each_safe(temp, dummy, &waitingPets){
            item = list_entry(temp, pet, list);
            list_add_tail(&item->list, &floorPets[item->destination]);
            list_del(temp);      
        }
        mutex_unlock(&mut);
        
    }
    return 0;
    
}



static int __init elevator_init(void) {
    // set up lists 
    INIT_LIST_HEAD(&elevatorPets);
    INIT_LIST_HEAD(&waitingPets);
    for (int i = 0; i < 5; i++){
        INIT_LIST_HEAD(&floorPets[i]);
    }
    mutex_init(&mut);
    elevator_thread = kthread_run(runningElevator, NULL, "elevator_thread");
    stop_thread = kthread_run(stopOperation, NULL, "stop_thread");
    proc_thread = kthread_run(procRequest, NULL, "proc_thread");
    return 0;
}

static void __exit elevator_exit() {

    kthread_stop(elevator_thread);
    kthread_stop(stop_thread);
    kthread_stop(proc_thread);
}

module_init(elevator_init);
module_exit(elevator_exit);