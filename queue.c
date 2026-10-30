#include <stdlib.h>
#include <stdio.h>
#include <threads.h> //הספריה הסטנדרטית לעבודה עם תהליכונים, מנעולים ומשתני תנאי
#include <stdatomic.h> //מאפשרת ביצוע ברמת החומרה בצורה אטומית
#include <stdbool.h>

// צומת ברשימה מקושרת שומרת את האיברים הרגילים
typedef struct ItemNode {
    void* data;
    struct ItemNode* next;
} ItemNode;

// צומת שמייצגת תהליכון שישן
typedef struct WaiterNode {
    cnd_t cv; //משתנה תנאי פרטי לאותו תהליכון. מאפשר להעיר אותו ולא אף אחד אחר.
    void* item_ready; 
    struct WaiterNode* next;
} WaiterNode;

static mtx_t queue_mutex; //מנעול

static ItemNode* item_head = NULL; //מצביע לתחילת רשימת האיברים
static ItemNode* item_tail = NULL; //מצביע לסוף רשימת האיברים

static WaiterNode* waiter_head = NULL; //מצביע לתחילת רשימת התהליכונים הישנים
static WaiterNode* waiter_tail = NULL; //מצביע לסוף רשימת התהליכונים הישנים

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

    if (waiter_head != NULL) { //האם יש תהליכון שמחכה
        WaiterNode* w = waiter_head; //שולפים את הממתין הראשון
        waiter_head = w->next; //מקדמים את הראש לממתין הבא
        if (waiter_head == NULL) {
            waiter_tail = NULL;
        }
        w->item_ready = item;
        cnd_signal(&w->cv);
    } 
    else {
        //אם אין אף אחד שמחכה לתור
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

    mtx_unlock(&queue_mutex); //שחרור המנעול
}

void* dequeue(void) {
    mtx_lock(&queue_mutex);
    void* result = NULL;

    //אם ישנם איברים בתור
    if (item_head != NULL) {
        //הוצאת האיבר הראשון בתור
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
        //התור ריק - צריך לישון
        WaiterNode my_waiter;
        cnd_init(&my_waiter.cv);
        my_waiter.item_ready = NULL;
        my_waiter.next = NULL;

        //מכניסים את התהליכון לסוף תור הממתינים
        if (waiter_tail != NULL) {
            waiter_tail->next = &my_waiter;
        } else {
            waiter_head = &my_waiter;
        }
        waiter_tail = &my_waiter;

        //מרדימה את התהליכון שלנו, ומשחררת את המנעול כדי שתהליכונים אחרים יוכלו להמשיך להשתמש בתור
        cnd_wait(&my_waiter.cv, &queue_mutex);

        //כשמתקבל סיגנל מתעוררת ונועלת מחדש את המנעול באופן אוטומטי לפני שהיא ממשיכה לשורה הבאה
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