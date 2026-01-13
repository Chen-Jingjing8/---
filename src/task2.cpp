#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_STACK_SIZE 2
#define BUFFER_SIZE 1024  // 用于输出的缓冲区大小

typedef struct Drone {
    int id;
    char type;  // 无人机种类：'A'，'B'，'C'
    int arrival_time;
    struct Drone* next;  // 用于队列的链表实现
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
int isValidType(char type);
int isDroneIdExists(int id, Stack *chargingStation, Queue *waitingPath);
void updateTypeStatistics(char type, int operation);
void displayTypeStatistics(char* buffer);
void displayChargingEfficiency(char* buffer);
int changeDroneType(int id, char new_type, Stack *chargingStation, Queue *waitingPath);

// 类型统计
static int typeA_count = 0;
static int typeB_count = 0;
static int typeC_count = 0;

extern "C" {
    const char* simulateChargingStation(const char* input);
}

// 初始化和操作实现
void initStack(Stack* s) { s->top = 0; }
int isStackFull(Stack* s) { return s->top == MAX_STACK_SIZE; }
int isStackEmpty(Stack* s) { return s->top == 0; }

void push(Stack* s, Drone d) {
    if (isStackFull(s)) {
        printf("充电站已满！\n");
        return;
    }
    s->stack[s->top++] = d;
}

Drone pop(Stack* s) {
    if (isStackEmpty(s)) {
        printf("充电站为空！\n");
        Drone emptyDrone = {-1, 'X', -1, NULL};
        return emptyDrone;
    }
    return s->stack[--s->top];
}

void initTempStack(TempStack* s) { s->top = 0; }
int isTempStackEmpty(TempStack* s) { return s->top == 0; }

void tempPush(TempStack* s, Drone d) {
    s->stack[s->top++] = d;
}

Drone tempPop(TempStack* s) {
    if (isTempStackEmpty(s)) {
        printf("临时栈为空！\n");
        Drone emptyDrone = {-1, 'X', -1, NULL};
        return emptyDrone;
    }
    return s->stack[--s->top];
}

void initQueue(Queue* q) { q->front = q->rear = NULL; }
int isQueueEmpty(Queue* q) { return q->front == NULL; }

void enqueue(Queue* q, Drone d) {
    Drone* newNode = (Drone*)malloc(sizeof(Drone));
    newNode->id = d.id;
    newNode->type = d.type;
    newNode->arrival_time = d.arrival_time;
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
        printf("便道为空！\n");
        Drone emptyDrone = {-1, 'X', -1, NULL};
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

// 验证无人机类型是否有效
int isValidType(char type) {
    return type == 'A' || type == 'B' || type == 'C';
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

// 更新类型统计
void updateTypeStatistics(char type, int operation) {
    // operation: 1=增加, -1=减少
    switch (type) {
        case 'A': typeA_count += operation; break;
        case 'B': typeB_count += operation; break;
        case 'C': typeC_count += operation; break;
    }
}

// 显示类型统计
void displayTypeStatistics(char* buffer) {
    snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
             "=== 类型统计 ===\n");
    snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
             "A型无人机: %d架\n", typeA_count);
    snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
             "B型无人机: %d架\n", typeB_count);
    snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
             "C型无人机: %d架\n", typeC_count);
    snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
             "总计: %d架\n", typeA_count + typeB_count + typeC_count);
}

// 显示充电效率对比
void displayChargingEfficiency(char* buffer) {
    snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
             "=== 充电效率对比 ===\n");
    snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
             "A型: 基准充电速率 (1.0x)\n");
    snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
             "B型: 快速充电 (1.5x A型速度)\n");
    snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
             "C型: 超快充电 (3.0x A型速度)\n");
    snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
             "说明: 同样时间内，B型充电量是A型的1.5倍，C型是A型的3倍\n");
}

// 改变无人机类型
int changeDroneType(int id, char new_type, Stack *chargingStation, Queue *waitingPath) {
    if (!isValidType(new_type)) {
        return 0; // 无效类型
    }
    
    // 在充电站中查找
    for (int i = 0; i < chargingStation->top; i++) {
        if (chargingStation->stack[i].id == id) {
            char old_type = chargingStation->stack[i].type;
            chargingStation->stack[i].type = new_type;
            updateTypeStatistics(old_type, -1);
            updateTypeStatistics(new_type, 1);
            return 1;
        }
    }
    
    // 在便道中查找
    Drone *temp = waitingPath->front;
    while (temp != NULL) {
        if (temp->id == id) {
            char old_type = temp->type;
            temp->type = new_type;
            updateTypeStatistics(old_type, -1);
            updateTypeStatistics(new_type, 1);
            return 1;
        }
        temp = temp->next;
    }
    
    return 0; // 未找到
}

const char* simulateChargingStation(const char* input) {
    // 定义静态变量保持充电站状态
    static Stack chargingStation;
    static TempStack tempStack;
    static Queue waitingPath;

    // 第一次调用时初始化
    static int initialized = 0;
    if (!initialized) {
        initStack(&chargingStation);
        initTempStack(&tempStack);
        initQueue(&waitingPath);
        initialized = 1;
    }


    // 定义缓冲区
    char* buffer = (char*)malloc(BUFFER_SIZE);
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

    int id, time;
    char type;

    if (action == 'a') {
        // 到达操作解析和验证
        parsed = sscanf(input, " %c %d %c %d", &action, &id, &type, &time);
        if (parsed != 4) {
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "错误：到达操作格式不正确！正确格式：a 编号 类型 时间\n");
            return buffer;
        }
        
        // 验证类型
        if (!isValidType(type)) {
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "错误：无效的无人机类型'%c'！只支持A、B、C类型\n", type);
            return buffer;
        }
        
    } else if (action == 'd') {
        // 离开操作解析和验证
        parsed = sscanf(input, " %c %d %d", &action, &id, &time);
        if (parsed != 3) {
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "错误：离开操作格式不正确！正确格式：d 编号 时间\n");
            return buffer;
        }
    } else if (action == 's') {
        // 统计和显示操作
        displayTypeStatistics(buffer);
        displayChargingEfficiency(buffer);
        return buffer;
    } else if (action == 'c') {
        // 类型转换操作
        parsed = sscanf(input, " %c %d %c", &action, &id, &type);
        if (parsed != 3) {
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "错误：类型转换操作格式不正确！正确格式：c 编号 新类型\n");
            return buffer;
        }
        
        if (!isValidType(type)) {
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "错误：无效的无人机类型'%c'！只支持A、B、C类型\n", type);
            return buffer;
        }
        
        if (changeDroneType(id, type, &chargingStation, &waitingPath)) {
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "成功将无人机%d的类型转换为%c\n", id, type);
        } else {
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "错误：未找到无人机%d或类型转换失败\n", id);
        }
        return buffer;
    } else {
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "错误：未知操作'%c'！支持操作：a(到达) d(离开) s(统计) c(类型转换)\n", action);
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

    if (action == 'a') {
        // 检查重复ID
        if (isDroneIdExists(id, &chargingStation, &waitingPath)) {
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "错误：无人机%d已存在！\n", id);
            return buffer;
        }

        Drone newDrone = {id, type, time, NULL};
        if (!isStackFull(&chargingStation)) {
            push(&chargingStation, newDrone);
            int position = chargingStation.top;
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "编号为%d，种类为%c的无人机在时刻%d到达，进入充电站，停靠位置为%d。\n", id, type, time, position);
        } else {
            enqueue(&waitingPath, newDrone);
            int waitingPosition = 0;
            Drone* temp = waitingPath.front;
            while (temp != NULL) {
                waitingPosition++;
                if (temp->id == id) break;
                temp = temp->next;
            }
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "编号为%d，种类为%c的无人机在时刻%d到达，充电站已满，在便道上等待，位置为%d。\n", id, type, time, waitingPosition);
        }
        
        // 更新类型统计
        updateTypeStatistics(type, 1);
    } else if (action == 'd') {
        int found = 0;
        int bypass_count = 0;  // 让路计数
        char drone_type = 'X';
        
        while (!isStackEmpty(&chargingStation)) {
            Drone topDrone = pop(&chargingStation);
            if (topDrone.id == id) {
                found = 1;
                drone_type = topDrone.type;
                int chargingTime = time - topDrone.arrival_time;
                float rate = 1.0;  // 默认速率
                if (topDrone.type == 'B') rate = 1.5;
                else if (topDrone.type == 'C') rate = 3.0;
                float chargingAmount = chargingTime * rate;
                
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "编号为%d，种类为%c的无人机在时刻%d离开，停留时间为%d，充电量为%.1f。\n", id, topDrone.type, time, chargingTime, chargingAmount);
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
                     "错误：无人机%d不在充电站内！\n", id);
        } else {
            // 更新类型统计
            updateTypeStatistics(drone_type, -1);
        }
        
        while (!isTempStackEmpty(&tempStack)) {
            Drone tempDrone = tempPop(&tempStack);
            push(&chargingStation, tempDrone);
        }
        
        if (!isStackFull(&chargingStation) && !isQueueEmpty(&waitingPath)) {
            Drone nextDrone = dequeue(&waitingPath);
            push(&chargingStation, nextDrone);
            int position = chargingStation.top;
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "编号为%d，种类为%c的无人机从便道进入充电站，停靠位置为%d。\n", nextDrone.id, nextDrone.type, position);
        }
    }

    return buffer;  // 返回缓冲区内容
}
