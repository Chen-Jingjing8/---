#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_STACK_SIZE 2
#define BUFFER_SIZE 1024  // 用于输出的缓冲区大小

typedef struct Drone {
    int id;
    int arrival_time;
    int enter_station_time;
    int in_station;
    struct Drone* next;
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
    Drone* front;
    Drone* rear;
} Queue;

// 函数声明
void initStack(Stack* s);
int isStackFull(Stack* s);
int isStackEmpty(Stack* s);
void push(Stack* s, Drone d);
Drone pop(Stack* s);
void initTempStack(TempStack* s);
int isTempStackEmpty(TempStack* s);
void tempPush(TempStack* s, Drone d);
Drone tempPop(TempStack* s);
void initQueue(Queue* q);
int isQueueEmpty(Queue* q);
void enqueue(Queue* q, Drone d);
Drone dequeue(Queue* q);
int removeFromQueue(Queue* q, int id, Drone* removedDrone);

extern "C" {
    const char* simulateChargingStation(const char* input);
}

void initStack(Stack* s) {
    s->top = 0;
}

int isStackFull(Stack* s) {
    return s->top == MAX_STACK_SIZE;
}

int isStackEmpty(Stack* s) {
    return s->top == 0;
}

void push(Stack* s, Drone d) {
    if (isStackFull(s)) {
        printf("充电站已满！\n");
        return;
    }
    s->stack[s->top++] = d;
}

Drone pop(Stack* s) {
    if (isStackEmpty(s)) {
        Drone emptyDrone = {-1, -1, -1, 0, NULL};
        return emptyDrone;
    }
    return s->stack[--s->top];
}

void initTempStack(TempStack* s) {
    s->top = 0;
}

int isTempStackEmpty(TempStack* s) {
    return s->top == 0;
}

void tempPush(TempStack* s, Drone d) {
    s->stack[s->top++] = d;
}

Drone tempPop(TempStack* s) {
    if (isTempStackEmpty(s)) {
        Drone emptyDrone = {-1, -1, -1, 0, NULL};
        return emptyDrone;
    }
    return s->stack[--s->top];
}

void initQueue(Queue* q) {
    q->front = q->rear = NULL;
}

int isQueueEmpty(Queue* q) {
    return q->front == NULL;
}

void enqueue(Queue* q, Drone d) {
    Drone* newNode = (Drone*)malloc(sizeof(Drone));
    *newNode = d;
    newNode->next = NULL;
    if (q->rear == NULL) {
        q->front = q->rear = newNode;
    } else {
        q->rear->next = newNode;
        q->rear = newNode;
    }
}

Drone dequeue(Queue* q) {
    if (isQueueEmpty(q)) {
        Drone emptyDrone = {-1, -1, -1, 0, NULL};
        return emptyDrone;
    }
    Drone* temp = q->front;
    Drone d = *temp;
    q->front = q->front->next;
    if (q->front == NULL)
        q->rear = NULL;
    free(temp);
    return d;
}

int removeFromQueue(Queue* q, int id, Drone* removedDrone) {
    Drone* current = q->front;
    Drone* prev = NULL;
    while (current != NULL) {
        if (current->id == id) {
            if (prev == NULL) {
                q->front = current->next;
            } else {
                prev->next = current->next;
            }
            if (current == q->rear) {
                q->rear = prev;
            }
            *removedDrone = *current;
            free(current);
            return 1;
        }
        prev = current;
        current = current->next;
    }
    return 0;
}

const char* simulateChargingStation(const char* input) {
    static Stack chargingStation;
    static TempStack tempStack;
    static Queue waitingPath;
    static int initialized = 0;
    
    // 统计变量
    static int total_arrivals = 0;
    static int total_departures = 0;
    static int station_departures = 0;
    static int waiting_departures = 0;
    static float total_charging_amount = 0.0;
    static int last_time = 0;

    if (!initialized) {
        initStack(&chargingStation);
        initTempStack(&tempStack);
        initQueue(&waitingPath);
        initialized = 1;
    }

    char action;
    int id, time;
    float charging_rate_station = 1.0;
    float charging_rate_waiting = 0.5;

    char* buffer = (char*)malloc(BUFFER_SIZE);
    if (!buffer) {
        return "内存分配失败！";
    }
    memset(buffer, 0, BUFFER_SIZE);

    snprintf(buffer, BUFFER_SIZE, "正在处理操作：%s\n", input);
    
    // 基础输入解析
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
                 "=== 系统统计（便道充电版） ===\n");
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "总到达无人机: %d\n", total_arrivals);
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "总离开无人机: %d\n", total_departures);
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "  - 充电站离开: %d\n", station_departures);
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "  - 便道离开: %d\n", waiting_departures);
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "总充电量: %.1f\n", total_charging_amount);
        if (total_departures > 0) {
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "平均充电量: %.1f\n", total_charging_amount / total_departures);
        }
        return buffer;
    }

    // 常规操作解析
    parsed = sscanf(input, " %c %d %d", &action, &id, &time);
    if (parsed != 3) {
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "错误：输入格式不正确！正确格式：a/d 编号 时间\n");
        return buffer;
    }

    // 验证数据范围
    if (id <= 0) {
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "错误：无人机编号必须为正整数！\n");
        return buffer;
    }
    if (time < 0) {
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "错误：时间不能为负数！\n");
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
        // 检查是否已存在相同ID的无人机
        int droneExists = 0;
        
        // 检查充电站中是否存在
        for (int i = 0; i < chargingStation.top; i++) {
            if (chargingStation.stack[i].id == id) {
                droneExists = 1;
                break;
            }
        }
        
        // 检查便道中是否存在
        if (!droneExists) {
            Drone* temp = waitingPath.front;
            while (temp != NULL) {
                if (temp->id == id) {
                    droneExists = 1;
                    break;
                }
                temp = temp->next;
            }
        }
        
        if (droneExists) {
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "错误：编号为%d的无人机已经在系统中，无法重复到达！\n", id);
        } else {
            Drone newDrone = {id, time, 0, 0, NULL};

            if (!isStackFull(&chargingStation)) {
                newDrone.enter_station_time = time;
                newDrone.in_station = 1;
                push(&chargingStation, newDrone);
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "编号为%d的无人机在时刻%d到达，进入充电站，停靠位置为%d。\n", id, time, chargingStation.top);
            } else {
                enqueue(&waitingPath, newDrone);
                int waitingPosition = 1;
                Drone* temp = waitingPath.front;
                while (temp != NULL && temp->id != id) {
                    temp = temp->next;
                    waitingPosition++;
                }
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "编号为%d的无人机在时刻%d到达，充电站已满，在便道上等待，位置为%d。\n", id, time, waitingPosition);
            }
            total_arrivals++;
        }
    } else if (action == 'd') {
        int found = 0;
        while (!isStackEmpty(&chargingStation)) {
            Drone topDrone = pop(&chargingStation);
            if (topDrone.id == id) {
                found = 1;
                int waitingTime = topDrone.enter_station_time - topDrone.arrival_time;
                int chargingTime = time - topDrone.enter_station_time;
                float totalCharging = waitingTime * charging_rate_waiting + chargingTime * charging_rate_station;
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "编号为%d的无人机在时刻%d离开，在便道停留时间为%d，充电站停留时间为%d，总充电量为%.1f。\n", id, time, waitingTime, chargingTime, totalCharging);
                total_departures++;
                station_departures++;
                total_charging_amount += totalCharging;
                break;
            } else {
                tempPush(&tempStack, topDrone);
            }
        }
        if (!found) {
            Drone leavingDrone;
            if (removeFromQueue(&waitingPath, id, &leavingDrone)) {
                found = 1;
                int waitingTime = time - leavingDrone.arrival_time;
                float totalCharging = waitingTime * charging_rate_waiting;
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "编号为%d的无人机在时刻%d离开便道，便道停留时间为%d，总充电量为%.1f。\n", id, time, waitingTime, totalCharging);
                total_departures++;
                waiting_departures++;
                total_charging_amount += totalCharging;
            }
        }
        if (!found) {
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "无人机%d不在充电站或便道内！\n", id);
        }

        while (!isTempStackEmpty(&tempStack)) {
            Drone tempDrone = tempPop(&tempStack);
            push(&chargingStation, tempDrone);
        }

        if (!isStackFull(&chargingStation) && !isQueueEmpty(&waitingPath)) {
            Drone nextDrone = dequeue(&waitingPath);
            nextDrone.enter_station_time = time;
            nextDrone.in_station = 1;
            push(&chargingStation, nextDrone);
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "编号为%d的无人机从便道进入充电站，停靠位置为%d。\n", nextDrone.id, chargingStation.top);
        }
    }

    return buffer;
}
