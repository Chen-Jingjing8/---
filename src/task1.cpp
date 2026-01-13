#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_STACK_SIZE 2
#define BUFFER_SIZE 1024  // 用于输出的缓冲区大小

typedef struct Drone {
    int id;
    int arrival_time;
    struct Drone *next; // 用于队列的链表实现
} Drone;

typedef struct {
    Drone stack[MAX_STACK_SIZE];
    int top;
} Stack;

typedef struct {
    Drone stack[MAX_STACK_SIZE];
    int top;
} TempStack;

typedef struct {
    Drone *front;
    Drone *rear;
} Queue;

// 函数声明
void initStack(Stack *s);
int isStackFull(Stack *s);
int isStackEmpty(Stack *s);
void push(Stack *s, Drone d);
Drone pop(Stack *s);
void initTempStack(TempStack *s);
int isTempStackEmpty(TempStack *s);
void tempPush(TempStack *s, Drone d);
Drone tempPop(TempStack *s);
void initQueue(Queue *q);
int isQueueEmpty(Queue *q);
void enqueue(Queue *q, Drone d);
Drone dequeue(Queue *q);
int isDroneIdExists(int id, Stack *chargingStation, Queue *waitingPath);

extern "C" {
    const char* simulateChargingStation(const char* input);
}

void initStack(Stack *s) {
    s->top = 0;
}

int isStackFull(Stack *s) {
    return s->top == MAX_STACK_SIZE;
}

int isStackEmpty(Stack *s) {
    return s->top == 0;
}

void push(Stack *s, Drone d) {
    if (isStackFull(s)) {
        printf("充电站已满！\n");
        return;
    }
    s->stack[s->top++] = d;
}

Drone pop(Stack *s) {
    if (isStackEmpty(s)) {
        printf("充电站为空！\n");
        Drone emptyDrone = {-1, -1, NULL};
        return emptyDrone;
    }
    return s->stack[--s->top];
}

void initTempStack(TempStack *s) {
    s->top = 0;
}

int isTempStackEmpty(TempStack *s) {
    return s->top == 0;
}

void tempPush(TempStack *s, Drone d) {
    s->stack[s->top++] = d;
}

Drone tempPop(TempStack *s) {
    if (isTempStackEmpty(s)) {
        printf("临时栈为空！\n");
        Drone emptyDrone = {-1, -1, NULL};
        return emptyDrone;
    }
    return s->stack[--s->top];
}

void initQueue(Queue *q) {
    q->front = q->rear = NULL;
}

int isQueueEmpty(Queue *q) {
    return q->front == NULL;
}

void enqueue(Queue *q, Drone d) {
    Drone *newNode = (Drone *)malloc(sizeof(Drone));
    newNode->id = d.id;
    newNode->arrival_time = d.arrival_time;
    newNode->next = NULL;
    if (q->rear == NULL) {
        q->front = q->rear = newNode;
    } else {
        q->rear->next = newNode;
        q->rear = newNode;
    }
}

Drone dequeue(Queue *q) {
    if (isQueueEmpty(q)) {
        printf("便道为空！\n");
        Drone emptyDrone = {-1, -1, NULL};
        return emptyDrone;
    }
    Drone *temp = q->front;
    Drone d = *temp;
    q->front = q->front->next;
    if (q->front == NULL)
        q->rear = NULL;
    free(temp);
    return d;
}

// 检查无人机ID是否已存在
int isDroneIdExists(int id, Stack *chargingStation, Queue *waitingPath) {
    // 检查充电站
    for (int i = 0; i < chargingStation->top; i++) {
        if (chargingStation->stack[i].id == id) return 1;
    }
    // 检查便道
    Drone *temp = waitingPath->front;
    while (temp != NULL) {
        if (temp->id == id) return 1;
        temp = temp->next;
    }
    return 0;
}

const char* simulateChargingStation(const char* input) {
    // 定义为静态变量，以保持函数调用间的状态
    static Stack chargingStation;
    static TempStack tempStack;
    static Queue waitingPath;

    // 统计变量
    static int total_arrivals = 0;
    static int total_departures = 0;
    static int total_bypass = 0;
    static int max_bypass_depth = 0;
    static int last_time = 0;

    // 第一次调用时初始化
    static int initialized = 0;
    if (!initialized) {
        initStack(&chargingStation);
        initTempStack(&tempStack);
        initQueue(&waitingPath);
        initialized = 1;
    }


    int position;
    int waitingPosition;

    // 输出缓冲区
    char *buffer = (char*)malloc(BUFFER_SIZE);
    if (!buffer) {
        return "内存分配失败！";
    }
    memset(buffer, 0, BUFFER_SIZE);

    snprintf(buffer, BUFFER_SIZE, "正在处理操作：%s\n", input);
    
    // 基础输入解析
    char action;
    int parsed = sscanf(input, " %c", &action);
    if (parsed != 1) {
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "错误：输入格式不正确！\n");
        return buffer;
    }

    // 处理特殊命令
    if (action == 't') {
        // 统计信息命令
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "=== 系统统计 ===\n");
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "总到达无人机: %d\n", total_arrivals);
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "总离开无人机: %d\n", total_departures);
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "总让路次数: %d\n", total_bypass);
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "最大让路深度: %d\n", max_bypass_depth);
        if (total_departures > 0) {
            float avg_bypass = (float)total_bypass / total_departures;
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "平均让路深度: %.1f\n", avg_bypass);
        }
        return buffer;
    }

    // 常规操作解析
    int id, time;
    if (action == 'a') {
        parsed = sscanf(input, " %c %d %d", &action, &id, &time);
    } else if (action == 'd') {
        parsed = sscanf(input, " %c %d %d", &action, &id, &time);
    } else {
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "错误：未知操作'%c'！\n", action);
        return buffer;
    }

    if (parsed != 3) {
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "错误：输入格式不正确！正确格式：a/d 编号 时间\n");
        return buffer;
    }

    // 时间顺序验证
    if (time < last_time) {
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "错误：时间%d小于上一次操作时间%d！\n", time, last_time);
        return buffer;
    }
    last_time = time;

    if (action == 'a') {
        // 检查重复ID
        if (isDroneIdExists(id, &chargingStation, &waitingPath)) {
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "错误：无人机%d已存在！\n", id);
            return buffer;
        }
        
        Drone newDrone = {id, time, NULL};
        if (!isStackFull(&chargingStation)) {
            push(&chargingStation, newDrone);
            position = chargingStation.top;
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "编号为%d的无人机在时刻%d到达，进入充电站，停靠位置为%d。\n", id, time, position);
        } else {
            enqueue(&waitingPath, newDrone);
            waitingPosition = 1;
            Drone *temp = waitingPath.front;
            while (temp != NULL && temp->id != id) {
                temp = temp->next;
                waitingPosition++;
            }
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "编号为%d的无人机在时刻%d到达，充电站已满，在便道上等待，位置为%d。\n", id, time, waitingPosition);
        }
        total_arrivals++;
        
    } else if (action == 'd') {
        int found = 0;
        int bypass_count = 0;  // 让路计数
        
        while (!isStackEmpty(&chargingStation)) {
            Drone topDrone = pop(&chargingStation);
            if (topDrone.id == id) {
                found = 1;
                int chargingTime = time - topDrone.arrival_time;
                int chargingAmount = chargingTime;
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "编号为%d的无人机在时刻%d离开，停留时间为%d，充电量为%d。\n", id, time, chargingTime, chargingAmount);
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "本次让路无人机数量：%d\n", bypass_count);
                break;
            } else {
                tempPush(&tempStack, topDrone);
                bypass_count++;
            }
        }
        
        if (!found) {
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "无人机%d不在充电站内！\n", id);
        } else {
            total_departures++;
            total_bypass += bypass_count;
            if (bypass_count > max_bypass_depth) {
                max_bypass_depth = bypass_count;
            }
        }
        
        while (!isTempStackEmpty(&tempStack)) {
            Drone tempDrone = tempPop(&tempStack);
            push(&chargingStation, tempDrone);
        }
        
        if (!isStackFull(&chargingStation) && !isQueueEmpty(&waitingPath)) {
            Drone nextDrone = dequeue(&waitingPath);
            push(&chargingStation, nextDrone);
            position = chargingStation.top;
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "编号为%d的无人机从便道进入充电站，停靠位置为%d。\n", nextDrone.id, position);
        }
    }

    return buffer;  // 返回输出缓冲区
}
