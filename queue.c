#include <stdlib.h>
#include <stdio.h>
#include <threads.h> 
#include <stdatomic.h> 
#include <stdbool.h>

typedef struct ItemNode {
    void* data;
    struct ItemNode* next;
} ItemNode;

typedef struct WaiterNode {
    cnd_t cv;
    void* item_ready; 
    struct WaiterNode* next;
} WaiterNode;

static mtx_t queue_mutex; 

static ItemNode* item_head = NULL; 
static ItemNode* item_tail = NULL; 

static WaiterNode* waiter_head = NULL; 
static WaiterNode* waiter_tail = NULL; 

static atomic_size_t visited_count = 0;


void initQueue(void) {
    mtx_init(&queue_mutex, mtx_plain);
    
    item_head = NULL;
    item_tail = NULL;
    
    waiter_head = NULL;
    waiter_tail = NULL;
    
    atomic_store(&visited_count, 0);
}

void destroyQueue(void) {
    while (item_head != NULL) {
        ItemNode* temp = item_head;
        item_head = item_head->next;
        free(temp);
    }
    mtx_destroy(&queue_mutex);
}

void enqueue(void* item) {
    mtx_lock(&queue_mutex);

    if (waiter_head != NULL) { 
        WaiterNode* w = waiter_head; 
        waiter_head = w->next; 
        if (waiter_head == NULL) {
            waiter_tail = NULL;
        }
        w->item_ready = item;
        cnd_signal(&w->cv);
    } 
    else {
        ItemNode* new_node = malloc(sizeof(ItemNode));
        new_node->data = item;
        new_node->next = NULL;

        if (item_tail != NULL) {
            item_tail->next = new_node;
        } else {
            item_head = new_node;
        }
        item_tail = new_node;
    }

    mtx_unlock(&queue_mutex); 
}

void* dequeue(void) {
    mtx_lock(&queue_mutex);
    void* result = NULL;

    if (item_head != NULL) {
        ItemNode* node = item_head;
        item_head = node->next;
        if (item_head == NULL) {
            item_tail = NULL;
        }
        result = node->data;
        free(node);

        atomic_fetch_add(&visited_count, 1);
        
        mtx_unlock(&queue_mutex);
        return result;
    } 
    else {
        WaiterNode my_waiter;
        cnd_init(&my_waiter.cv);
        my_waiter.item_ready = NULL;
        my_waiter.next = NULL;

        if (waiter_tail != NULL) {
            waiter_tail->next = &my_waiter;
        } else {
            waiter_head = &my_waiter;
        }
        waiter_tail = &my_waiter;

        cnd_wait(&my_waiter.cv, &queue_mutex);

        result = my_waiter.item_ready;
        
        cnd_destroy(&my_waiter.cv);

        atomic_fetch_add(&visited_count, 1);

        mtx_unlock(&queue_mutex);
        return result;
    }
}

size_t visited(void) {
    return atomic_load(&visited_count);
}
