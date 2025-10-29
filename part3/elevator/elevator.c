#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/sched.h>

struct mutex mut;
mutex_init(&mut);


void stopOperation() {

}

void procRequest() {

}

struct list_head floorPets[5];
struct list_head elevatorPets;

// not sure yet
typedef struct pet {
    struct list_head list;
    int type;
    int destination;
} pet


// dumb way of getting weight based on using 'type' as just an int
// theres better ways to do this, this is placeholder
int weightCheck(pet) {
    if (pet -> type == 0) {
        return 3;
    }
    if (pet -> type == 1) {
        return 14;
    }
    if (pet -> type == 2) {
        return 10;
    }
    else {
        return 16
    }
}

struct task_struct * kthread_run(threadfin, data, namefmt);



void runningElevator() {
    maxWeight = 0;
    currFloor = 0;
    currPets =0;
    //temp scheduler
    boolUp = true;
    while (true) {
        mutex_lock(&mut);
        
        // unload first
        list_for_each_safe(temp, dummy, &elevatorPets) {
            item = list_entry(temp, Item, list);
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
                item = list_entry(temp, Item, &floorPets[currFloor]);
                if (weightCheck(item->type) + maxWeight <= 50){
                    list_add_tail(&item->left, &elevatorPets);
                    list_del(temp);
                    maxWeight += weightCheck(item->type);
                    currPets += 1;
                }
             }
        }
        // temp scheduler before scheduling is actually made.
        if (currFloor == 4) {
            boolUp = False;
        }
        if (currFloor == 0)
        {
            boolUp = true;
        }
        if (boolUp == true) {
            currFloor+=1;
        }
        else {
            currFloor-=1;
        }
        mutex_unlock(&mut);

    }

}

void initailizeThreads() {
    
}