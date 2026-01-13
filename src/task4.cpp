#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <stdarg.h>

#define MAX_STACK_SIZE 2
#define MAX_DRONES 100

typedef struct Drone {
    int id;
    char type;
    int arrival_time;
    int enter_station_time;
    int depart_time;
    int in_station;
    float power_level;
    float work_capacity;
    float energy_consumed;
    float energy_per_work;
    int is_available;
    int is_charged;
    struct Drone* next;
} Drone;

typedef struct TaskDrone {
    int id;
    char type;
    float current_power;
    float work_efficiency;
    float consumption_rate;
    int is_active;
    float total_work_done;
} TaskDrone;

Drone allDrones[MAX_DRONES];
int totalDrones = 0;
int system_current_time = 0;  // 全局系统时间

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
    int count;
} Queue;

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
        Drone emptyDrone = {-1, 'X', -1, -1, -1, 0, 0.0, 0.0, 0.0, 0.0, 0, 0, NULL};
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
        Drone emptyDrone = {-1, 'X', -1, -1, -1, 0, 0.0, 0.0, 0.0, 0.0, 0, 0, NULL};
        return emptyDrone;
    }
    return s->stack[--s->top];
}

void initQueue(Queue* q) {
    q->front = q->rear = NULL;
    q->count = 0;
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
    q->count++;
}

Drone dequeue(Queue* q) {
    if (isQueueEmpty(q)) {
        Drone emptyDrone = {-1, 'X', -1, -1, -1, 0, 0.0, 0.0, 0.0, 0.0, 0, 0, NULL};
        return emptyDrone;
    }
    Drone* temp = q->front;
    Drone d = *temp;
    q->front = q->front->next;
    if (q->front == NULL)
        q->rear = NULL;
    q->count--;
    free(temp);
    return d;
}

int isDuplicateId(int id) {
    for (int i = 0; i < totalDrones; i++) {
        if (allDrones[i].id == id) {
            return 1;
        }
    }
    return 0;
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
            q->count--;
            free(current);
            return 1;
        }
        prev = current;
        current = current->next;
    }
    return 0;
}

float getChargingRate(char type, int in_station) {
    // 充电站内基础充电速率
    const float STATION_BASE_RATE = 10.0;
    // 便道基础充电速率（充电站的一半）
    const float QUEUE_BASE_RATE = STATION_BASE_RATE / 2.0;  // 5.0
    
    float base_rate = in_station ? STATION_BASE_RATE : QUEUE_BASE_RATE;
    switch (type) {
        case 'A': return base_rate * 1.0;
        case 'B': return base_rate * 1.5;
        case 'C': return base_rate * 3.0;
        default: return base_rate;
    }
}

float getConsumptionRate(char type, int in_station) {
    return getChargingRate(type, in_station) * 1.5;
}

void chargeDronesBeforeTask(Drone* drones, int count) {
    printf("\n=== 充电阶段 ===\n");
    printf("编号\t类型\t位置\t充电前电量\t充电小时\t充电后电量\t充电状态\n");
    
    for (int i = 0; i < count; i++) {
        // 充电前电量统一设置为20%
        const float INITIAL_POWER = 20.0;
        drones[i].power_level = INITIAL_POWER;
        
        // 计算实际充电时间（从到达时间到当前系统时间）
        float charging_hours = system_current_time - drones[i].arrival_time;
        if (charging_hours < 0) charging_hours = 0;
        
        float initial_power = INITIAL_POWER;
        float charging_rate = getChargingRate(drones[i].type, drones[i].in_station);
        float power_gain = charging_rate * charging_hours;
        
        drones[i].power_level += power_gain;
        if (drones[i].power_level > 100.0) {
            drones[i].power_level = 100.0;
        }
        
        drones[i].is_charged = 1;
        
        const char* location = drones[i].in_station ? "充电站" : "便道";
        const char* charged_status = drones[i].is_charged ? "已充电" : "未充电";
        printf("%d\t%c\t%s\t%.1f%%\t%.1f\t%.1f%%\t%s\n",
               drones[i].id, drones[i].type, location,
               initial_power, charging_hours, drones[i].power_level, charged_status);
    }
}

void registerDrone(int id, char type) {
    if (totalDrones >= MAX_DRONES) {
        printf("错误：无人机数量已达上限！\n");
        return;
    }
    
    if (type != 'A' && type != 'B' && type != 'C') {
        printf("错误：无效的无人机类型！\n");
        return;
    }
    
    if (isDuplicateId(id)) {
        printf("错误：无人机%d已存在！\n", id);
        return;
    }
    
    Drone newDrone;
    memset(&newDrone, 0, sizeof(Drone));
    newDrone.id = id;
    newDrone.type = type;
    newDrone.arrival_time = 0;
    newDrone.enter_station_time = 0;
    newDrone.depart_time = -1;
    newDrone.in_station = 0;
    newDrone.power_level = 20.0;
    newDrone.is_available = 1;
    newDrone.is_charged = 0;
    newDrone.next = NULL;
    
    allDrones[totalDrones++] = newDrone;
    printf("无人机%d注册成功！类型：%c，初始电量：20.0%%，充电状态：未充电\n", id, type);
}

void droneArrive(Drone newDrone, Stack* chargingStation, Queue* waitingPath) {
    newDrone.arrival_time = system_current_time;  // 使用系统当前时间
    
    if (!isStackFull(chargingStation)) {
        newDrone.enter_station_time = system_current_time;
        newDrone.in_station = 1;
        push(chargingStation, newDrone);
        printf("编号为%d，类型为%c的无人机在时刻%d到达，进入充电站，位置为%d。\n", 
               newDrone.id, newDrone.type, newDrone.arrival_time, chargingStation->top);
    } else {
        newDrone.in_station = 0;
        enqueue(waitingPath, newDrone);
        printf("编号为%d，类型为%c的无人机在时刻%d到达，充电站已满，在便道等待，位置为%d。\n", 
               newDrone.id, newDrone.type, newDrone.arrival_time, waitingPath->count);
    }
    
    for (int i = 0; i < totalDrones; i++) {
        if (allDrones[i].id == newDrone.id) {
            allDrones[i] = newDrone;
            break;
        }
    }
}

void droneDepart(int id, int depart_time, Stack* chargingStation, Queue* waitingPath) {
    int found = 0;
    TempStack tempStack;
    initTempStack(&tempStack);

    // 更新系统时间为离开时间
    if (depart_time > system_current_time) {
        system_current_time = depart_time;
    }

    while (!isStackEmpty(chargingStation)) {
        Drone topDrone = pop(chargingStation);
        if (topDrone.id == id) {
            found = 1;
            int waitingTime = topDrone.enter_station_time - topDrone.arrival_time;
            int chargingTime = system_current_time - topDrone.enter_station_time;
            float totalCharging = waitingTime * getChargingRate(topDrone.type, 0) + 
                                 chargingTime * getChargingRate(topDrone.type, 1);
            
            printf("编号为%d的无人机在时刻%d离开，\n在便道等待时间：%d，\n在充电站充电时间：%d，\n总充电量：%.1f%%。\n", 
                   id, system_current_time, waitingTime, chargingTime, totalCharging);
            
            for (int i = 0; i < totalDrones; i++) {
                if (allDrones[i].id == id) {
                    allDrones[i].depart_time = system_current_time;
                    allDrones[i].in_station = 0;
                    allDrones[i].power_level += totalCharging;
                    if (allDrones[i].power_level > 100.0)
                        allDrones[i].power_level = 100.0;
                    allDrones[i].is_charged = 1;
                    break;
                }
            }
            break;
        } else {
            tempPush(&tempStack, topDrone);
        }
    }

    if (!found) {
        Drone leavingDrone;
        if (removeFromQueue(waitingPath, id, &leavingDrone)) {
            found = 1;
            int waitingTime = system_current_time - leavingDrone.arrival_time;
            float totalCharging = waitingTime * getChargingRate(leavingDrone.type, 0);
            
            printf("编号为%d的无人机在时刻%d离开便道，\n在便道等待时间：%d，\n总充电量：%.1f%%。\n", 
                   id, system_current_time, waitingTime, totalCharging);
            
            for (int i = 0; i < totalDrones; i++) {
                if (allDrones[i].id == id) {
                    allDrones[i].depart_time = system_current_time;
                    allDrones[i].in_station = 0;
                    allDrones[i].power_level += totalCharging;
                    if (allDrones[i].power_level > 100.0)
                        allDrones[i].power_level = 100.0;
                    allDrones[i].is_charged = 1;
                    break;
                }
            }
        }
    }

    if (!found) {
        printf("无人机%d不在充电站或便道内！\n", id);
    }

    while (!isTempStackEmpty(&tempStack)) {
        Drone tempDrone = tempPop(&tempStack);
        push(chargingStation, tempDrone);
    }

    if (!isStackFull(chargingStation) && !isQueueEmpty(waitingPath)) {
        Drone nextDrone = dequeue(waitingPath);
        nextDrone.enter_station_time = system_current_time;
        nextDrone.in_station = 1;
        push(chargingStation, nextDrone);
        printf("编号为%d的无人机从便道进入充电站，位置为%d。\n", nextDrone.id, chargingStation->top);
        
        for (int i = 0; i < totalDrones; i++) {
            if (allDrones[i].id == nextDrone.id) {
                allDrones[i] = nextDrone;
                break;
            }
        }
    }
}

void displayStatus(Stack* chargingStation, Queue* waitingPath) {
    printf("\n=== 系统状态 ===\n");
    printf("当前系统时间：%d\n", system_current_time);
    printf("充电站 [%d/%d]: ", chargingStation->top, MAX_STACK_SIZE);
    if (isStackEmpty(chargingStation)) {
        printf("空");
    } else {
        for (int i = 0; i < chargingStation->top; i++) {
            printf("%d(%c,%s) ", 
                   chargingStation->stack[i].id, 
                   chargingStation->stack[i].type,
                   chargingStation->stack[i].is_charged ? "已充电" : "未充电");
        }
    }
    printf("\n");
    
    printf("便道 [%d]: ", waitingPath->count);
    if (isQueueEmpty(waitingPath)) {
        printf("空");
    } else {
        Drone* temp = waitingPath->front;
        while (temp != NULL) {
            printf("%d(%c,%s) ", 
                   temp->id, 
                   temp->type,
                   temp->is_charged ? "已充电" : "未充电");
            temp = temp->next;
        }
    }
    printf("\n");
    
    int chargedCount = 0;
    for (int i = 0; i < totalDrones; i++) {
        if (allDrones[i].is_charged) chargedCount++;
    }
    printf("总无人机: %d (已充电: %d, 未充电: %d)\n", 
           totalDrones, chargedCount, totalDrones - chargedCount);
    printf("================\n");
}

void resetSystem(Stack* chargingStation, Queue* waitingPath) {
    while (!isStackEmpty(chargingStation)) {
        pop(chargingStation);
    }
    while (!isQueueEmpty(waitingPath)) {
        dequeue(waitingPath);
    }
    totalDrones = 0;
    system_current_time = 0;
    printf("系统已重置！\n");
}

void displayHelp() {
    printf("\n=== 帮助信息 ===\n");
    printf("a 编号 类型 时间 - 无人机到达 (类型: A,B,C)\n");
    printf("d 编号 时间      - 无人机离开\n");
    printf("s                - 显示系统状态\n");
    printf("r                - 重置系统\n");
    printf("h                - 显示帮助\n");
    printf("e                - 退出并处理紧急任务\n");
    printf("================\n");
}

void executeTaskWithPowerManagement(int x, int y, int z, int m, 
                                   Drone* selectedDrones, int selectedCount) {
    if (selectedCount == 0) {
        printf("没有选择任何无人机执行任务！\n");
        return;
    }

    // 检查充电状态，使用系统当前时间进行充电
    for (int i = 0; i < selectedCount; i++) {
        if (!selectedDrones[i].is_charged) {
            printf("警告：无人机%d未充电！将根据实际停留时间进行充电...\n", selectedDrones[i].id);
            chargeDronesBeforeTask(&selectedDrones[i], 1);
        }
    }

    // 初始化任务无人机
    TaskDrone taskDrones[MAX_DRONES];
    for (int i = 0; i < selectedCount; i++) {
        taskDrones[i].id = selectedDrones[i].id;
        taskDrones[i].type = selectedDrones[i].type;
        taskDrones[i].current_power = selectedDrones[i].power_level;
        
        switch (selectedDrones[i].type) {
            case 'A': taskDrones[i].work_efficiency = 1.0; break;
            case 'B': taskDrones[i].work_efficiency = 2.0; break;
            case 'C': taskDrones[i].work_efficiency = 2.5; break;
        }
        
        taskDrones[i].consumption_rate = getConsumptionRate(
            selectedDrones[i].type, selectedDrones[i].in_station);
        taskDrones[i].is_active = 1;
        taskDrones[i].total_work_done = 0.0;
    }

    float remainingWork = m;
    int timeStep = 0;
    
    printf("\n=== 任务执行阶段 ===\n");
    while (remainingWork > 0) {
        timeStep++;
        float workThisStep = 0;
        int activeDrones = 0;
        
        printf("\n时间步 %d:\n", timeStep);
        for (int i = 0; i < selectedCount; i++) {
            if (taskDrones[i].is_active && taskDrones[i].current_power > 20.0) {
                float workDone = taskDrones[i].work_efficiency;
                workThisStep += workDone;
                taskDrones[i].total_work_done += workDone;
                taskDrones[i].current_power -= taskDrones[i].consumption_rate;
                
                if (taskDrones[i].current_power <= 20.0) {
                    taskDrones[i].is_active = 0;
                    taskDrones[i].current_power = 20.0;
                    printf("  无人机%d(%c) 电量低于20%%，停止工作（已完成%.1f）\n",
                           taskDrones[i].id, taskDrones[i].type, 
                           taskDrones[i].total_work_done);
                } else {
                    activeDrones++;
                    printf("  无人机%d(%c): 工作%.1f, 剩余电量%.1f%%, 累计%.1f\n",
                           taskDrones[i].id, taskDrones[i].type, workDone,
                           taskDrones[i].current_power, taskDrones[i].total_work_done);
                }
            }
        }
        
        remainingWork -= workThisStep;
        if (remainingWork < 0) remainingWork = 0;
        
        printf("  本步完成: %.1f, 剩余: %.1f, 活跃无人机: %d/%d\n",
               workThisStep, remainingWork, activeDrones, selectedCount);
        
        if (activeDrones == 0 && remainingWork > 0) {
            printf("\n所有无人机电量不足，任务中断！\n");
            break;
        }
    }

    printf("\n=== 任务结果 ===\n");
    printf("总工作量: %.1f/%d (%.1f%%)\n", m - remainingWork, m, 
           (m - remainingWork)/m*100);
    
    float totalEnergyUsed = 0;
    printf("\n各无人机工作详情:\n");
    printf("编号\t类型\t初始电量\t最终电量\t工作量\t能耗\n");
    for (int i = 0; i < selectedCount; i++) {
        float energyUsed = selectedDrones[i].power_level - taskDrones[i].current_power;
        totalEnergyUsed += energyUsed;
        printf("%d\t%c\t%.1f%%\t%.1f%%\t%.1f\t%.1f%%\n",
               taskDrones[i].id, taskDrones[i].type,
               selectedDrones[i].power_level, taskDrones[i].current_power,
               taskDrones[i].total_work_done, energyUsed);
    }
    
    printf("\n统计摘要:\n");
    printf("总能耗: %.1f%%\n", totalEnergyUsed);
    printf("平均效率: %.2f 工作量/%%电量\n", 
           (m - remainingWork)/totalEnergyUsed);
    printf("用时: %d 时间单位\n", timeStep);
}

void handleUrgentTask() {
    int x, y, z, m;
    printf("\n=== 紧急任务调度 ===\n");
    printf("请输入需要的无人机数量（x架A型，y架B型，z架C型）及总工作量m：");
    scanf("%d %d %d %d", &x, &y, &z, &m);
    
    printf("当前系统时间：%d\n", system_current_time);
    
    // 1. 计算所有可用无人机的工作能力和能耗效率（基于系统当前时间）
    for (int i = 0; i < totalDrones; i++) {
        if (allDrones[i].is_available && allDrones[i].depart_time == -1) {
            // 充电前电量统一设置为20%
            const float INITIAL_POWER = 20.0;
            allDrones[i].power_level = INITIAL_POWER;
            
            // 计算实际充电时间
            float charging_hours = system_current_time - allDrones[i].arrival_time;
            if (charging_hours < 0) charging_hours = 0;
            
            // 根据实际充电时间计算充电后的电量（从20%开始充电）
            float charging_rate = getChargingRate(allDrones[i].type, allDrones[i].in_station);
            float charged_power = INITIAL_POWER + charging_rate * charging_hours;
            if (charged_power > 100.0) charged_power = 100.0;
            
            // 更新充电状态
            if (charging_hours > 0) {
                allDrones[i].is_charged = 1;
                allDrones[i].power_level = charged_power;
            }
            
            // 计算可用工作时间（电量从当前电量到20%）
            float usable_power = charged_power - 20.0;
            if (usable_power < 0) usable_power = 0;
            
            float consumption_rate = getConsumptionRate(allDrones[i].type, allDrones[i].in_station);
            float available_time = usable_power / consumption_rate;
            
            // 计算工作能力和能耗效率
            switch (allDrones[i].type) {
                case 'A': 
                    allDrones[i].work_capacity = 1.0 * available_time;
                    break;
                case 'B': 
                    allDrones[i].work_capacity = 2.0 * available_time;
                    break;
                case 'C': 
                    allDrones[i].work_capacity = 2.5 * available_time;
                    break;
            }
            
            allDrones[i].energy_consumed = usable_power;
            allDrones[i].energy_per_work = (allDrones[i].work_capacity > 0) ? 
                allDrones[i].energy_consumed / allDrones[i].work_capacity : 9999.0;
        }
    }
    
    // 2. 按优先级排序可用无人机
    // 排序标准：1.类型(C>B>A) 2.能耗效率(低能耗优先) 3.电量(高电量优先)
    for (int i = 0; i < totalDrones - 1; i++) {
        for (int j = i + 1; j < totalDrones; j++) {
            if (allDrones[i].is_available && allDrones[i].depart_time == -1 &&
                allDrones[j].is_available && allDrones[j].depart_time == -1) {
                
                // 比较优先级
                int swap = 0;
                
                // 首先按类型优先级
                if (allDrones[i].type == 'A' && allDrones[j].type != 'A') swap = 1;
                else if (allDrones[i].type == 'B' && allDrones[j].type == 'C') swap = 1;
                // 同类型比较能耗效率
                else if (allDrones[i].type == allDrones[j].type) {
                    if (allDrones[i].energy_per_work > allDrones[j].energy_per_work) swap = 1;
                    // 能耗效率相同比较电量
                    else if (allDrones[i].energy_per_work == allDrones[j].energy_per_work &&
                             allDrones[i].power_level < allDrones[j].power_level) swap = 1;
                }
                
                if (swap) {
                    Drone temp = allDrones[i];
                    allDrones[i] = allDrones[j];
                    allDrones[j] = temp;
                }
            }
        }
    }
    
    // 3. 选择满足类型需求的最优无人机
    Drone selectedDrones[MAX_DRONES];
    int selectedCount = 0;
    int a_selected = 0, b_selected = 0, c_selected = 0;
    float total_capacity = 0;
    
    for (int i = 0; i < totalDrones; i++) {
        if (allDrones[i].is_available && allDrones[i].depart_time == -1) {
            // 检查是否满足类型需求
            if (allDrones[i].type == 'A' && a_selected < x) {
                selectedDrones[selectedCount++] = allDrones[i];
                a_selected++;
                total_capacity += allDrones[i].work_capacity;
                allDrones[i].is_available = 0; // 标记为已选择
            }
            else if (allDrones[i].type == 'B' && b_selected < y) {
                selectedDrones[selectedCount++] = allDrones[i];
                b_selected++;
                total_capacity += allDrones[i].work_capacity;
                allDrones[i].is_available = 0;
            }
            else if (allDrones[i].type == 'C' && c_selected < z) {
                selectedDrones[selectedCount++] = allDrones[i];
                c_selected++;
                total_capacity += allDrones[i].work_capacity;
                allDrones[i].is_available = 0;
            }
            
            // 如果已经满足类型需求，停止选择
            if (a_selected >= x && b_selected >= y && c_selected >= z) break;
        }
    }
    
    // 4. 如果已选无人机的工作能力不足，补充更多高效无人机
    if (total_capacity < m) {
        for (int i = 0; i < totalDrones && total_capacity < m; i++) {
            if (allDrones[i].is_available && allDrones[i].depart_time == -1) {
                // 检查是否已选择该无人机
                int already_selected = 0;
                for (int j = 0; j < selectedCount; j++) {
                    if (selectedDrones[j].id == allDrones[i].id) {
                        already_selected = 1;
                        break;
                    }
                }
                
                if (!already_selected) {
                    selectedDrones[selectedCount++] = allDrones[i];
                    total_capacity += allDrones[i].work_capacity;
                    allDrones[i].is_available = 0;
                }
            }
        }
    }
    
    // 5. 显示选择结果
    printf("\n=== 最优调度选择结果 ===\n");
    printf("当前系统时间：%d\n", system_current_time);
    printf("需求：A型%d架，B型%d架，C型%d架，工作量%d\n", x, y, z, m);
    printf("实际选择：A型%d架，B型%d架，C型%d架\n", a_selected, b_selected, c_selected);
    printf("预计总工作能力：%.1f\n", total_capacity);
    
    if (total_capacity < m) {
        printf("警告：预计工作能力不足！\n");
    }
    
    // 显示选择的无人机详情
    printf("\n选择的无人机列表：\n");
    printf("ID\t类型\t位置\t停留时间\t当前电量\t工作能力\t能耗效率\n");
    for (int i = 0; i < selectedCount; i++) {
        float stay_time = system_current_time - selectedDrones[i].arrival_time;
        printf("%d\t%c\t%s\t%.1f小时\t%.1f%%\t%.1f\t%.3f\n",
               selectedDrones[i].id,
               selectedDrones[i].type,
               selectedDrones[i].in_station ? "充电站" : "便道",
               stay_time,
               selectedDrones[i].power_level,
               selectedDrones[i].work_capacity,
               selectedDrones[i].energy_per_work);
    }
    
    // 执行任务
    executeTaskWithPowerManagement(x, y, z, m, selectedDrones, selectedCount);
    
    // 恢复无人机可用状态
    for (int i = 0; i < selectedCount; i++) {
        for (int j = 0; j < totalDrones; j++) {
            if (allDrones[j].id == selectedDrones[i].id) {
                allDrones[j].is_available = 1;
                break;
            }
        }
    }
}

void simulateChargingStationInteractive() {
    Stack chargingStation;
    TempStack tempStack;
    Queue waitingPath;

    initStack(&chargingStation);
    initTempStack(&tempStack);
    initQueue(&waitingPath);

    printf("=== 无人机充电站模拟系统 ===\n");
    printf("支持功能：\n");
    printf("- 多类型无人机（A/B/C）充电管理\n");
    printf("- 便道充电支持\n");
    printf("- 紧急任务最优调度\n");
    printf("- 电量低于20%%自动返回机制\n");
    printf("- 能耗优化算法\n");
    displayHelp();

    char input[100];
    while (1) {
        printf("\n> ");
        if (fgets(input, sizeof(input), stdin) == NULL) break;
        
        input[strcspn(input, "\n")] = 0;
        
        if (strcmp(input, "e") == 0 || strcmp(input, "exit") == 0) {
            printf("处理紧急任务...\n");
            handleUrgentTask();
            break;
        } else if (strcmp(input, "s") == 0) {
            displayStatus(&chargingStation, &waitingPath);
        } else if (strcmp(input, "r") == 0) {
            resetSystem(&chargingStation, &waitingPath);
        } else if (strcmp(input, "h") == 0) {
            displayHelp();
        } else if (input[0] == 'a') {
            int id, time;
            char type;
            if (sscanf(input, "a %d %c %d", &id, &type, &time) == 3) {
                if (type != 'A' && type != 'B' && type != 'C') {
                    printf("错误：无效的无人机类型！\n");
                    continue;
                }
                if (isDuplicateId(id)) {
                    printf("错误：无人机%d已存在！\n", id);
                    continue;
                }
                registerDrone(id, type);
                Drone newDrone = allDrones[totalDrones-1];
                system_current_time = time;  // 更新系统时间
                droneArrive(newDrone, &chargingStation, &waitingPath);
            } else {
                printf("错误：到达操作格式不正确！\n");
            }
        } else if (input[0] == 'd') {
            int id, time = -1;
            // 尝试读取可选的时间参数
            if (sscanf(input, "d %d %d", &id, &time) >= 1) {
                if (time == -1) {
                    // 没有提供时间参数，使用当前系统时间
                    droneDepart(id, system_current_time, &chargingStation, &waitingPath);
                } else {
                    // 提供了时间参数，使用指定时间
                    droneDepart(id, time, &chargingStation, &waitingPath);
                }
            } else {
                printf("错误：离开操作格式不正确！\n");
            }
        } else {
            printf("错误：未知命令！输入'h'查看帮助\n");
        }
    }
}

#if defined(_WIN32)
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

// 供DLL单次调用使用的内部状态
static int dll_initialized = 0;
static Stack dll_chargingStation;
static TempStack dll_tempStack;
static Queue dll_waitingPath;

// 工具：向静态输出缓冲追加格式化文本
static void out_appendf(char* out, size_t out_size, const char* fmt, ...) {
    size_t used = strlen(out);
    if (used >= out_size - 1) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(out + used, out_size - used, fmt, ap);
    va_end(ap);
}

static void ensure_init_state() {
    if (!dll_initialized) {
        initStack(&dll_chargingStation);
        initTempStack(&dll_tempStack);
        initQueue(&dll_waitingPath);
        totalDrones = 0;
        dll_initialized = 1;
    }
}

static void reset_all_state() {
    while (!isStackEmpty(&dll_chargingStation)) pop(&dll_chargingStation);
    while (!isQueueEmpty(&dll_waitingPath)) dequeue(&dll_waitingPath);
    totalDrones = 0;
    system_current_time = 0;
}

extern "C" EXPORT const char* simulateChargingStation(const char* input) {
    static char out[8192];
    out[0] = '\0';
    ensure_init_state();

    if (input == NULL) {
        return out;
    }

    // 将输入复制到可变缓冲
    char inbuf[1024];
    size_t inlen = strlen(input);
    if (inlen >= sizeof(inbuf)) inlen = sizeof(inbuf) - 1;
    memcpy(inbuf, input, inlen);
    inbuf[inlen] = '\0';
    // 去除尾部 \n\r
    while (inlen > 0 && (inbuf[inlen-1] == '\n' || inbuf[inlen-1] == '\r')) { inbuf[--inlen] = '\0'; }

    // 拆分首行与第二行
    char* firstLine = inbuf;
    char* secondLine = NULL;
    char* nl = strchr(inbuf, '\n');
    if (nl) {
        *nl = '\0';
        secondLine = nl + 1;
        // trim secondLine 左右空白
        while (*secondLine == ' ' || *secondLine == '\t') secondLine++;
        size_t slen = strlen(secondLine);
        while (slen > 0 && (secondLine[slen-1] == ' ' || secondLine[slen-1] == '\t' || secondLine[slen-1] == '\r')) {
            secondLine[--slen] = '\0';
        }
    }

    // 读取命令与参数
    char cmd = 0;
    int id = 0, time = -1;  // time初始化为-1，表示未提供
    char typeChar = 'A';
    {
        char c1 = 0, ctype = 0;
        int a = 0, t = -1;
        if (sscanf(firstLine, " %c %d %c %d", &c1, &a, &ctype, &t) == 4) {
            cmd = c1; id = a; typeChar = ctype; time = t;
        } else if (sscanf(firstLine, " %c %d %d", &c1, &a, &t) == 3) {
            cmd = c1; id = a; time = t;
        } else if (sscanf(firstLine, " %c %d", &c1, &a) == 2) {
            cmd = c1; id = a; time = -1;  // 只有id，没有time
        } else if (sscanf(firstLine, " %c", &c1) == 1) {
            cmd = c1;
        }
    }

    // 执行命令
    if (cmd == 'r') {
        reset_all_state();
        snprintf(out + strlen(out), sizeof(out) - strlen(out), "系统已重置！\n");
        return out;
    }

    if (cmd == 'a') {
        if (typeChar != 'A' && typeChar != 'B' && typeChar != 'C') typeChar = 'A';
        if (isDuplicateId(id)) {
            snprintf(out + strlen(out), sizeof(out) - strlen(out), "错误：无人机%d已存在！\n", id);
            return out;
        }
        registerDrone(id, typeChar);
        Drone newDrone = allDrones[totalDrones-1];
        system_current_time = time;  // 更新系统时间
        newDrone.arrival_time = system_current_time;
        if (!isStackFull(&dll_chargingStation)) {
            newDrone.enter_station_time = system_current_time;
            newDrone.in_station = 1;
            push(&dll_chargingStation, newDrone);
            snprintf(out + strlen(out), sizeof(out) - strlen(out),
                     "编号为%d，类型为%c的无人机在时刻%d到达，进入充电站，位置为%d。\n",
                     newDrone.id, newDrone.type, newDrone.arrival_time, dll_chargingStation.top);
        } else {
            newDrone.in_station = 0;
            enqueue(&dll_waitingPath, newDrone);
            snprintf(out + strlen(out), sizeof(out) - strlen(out),
                     "编号为%d，类型为%c的无人机在时刻%d到达，充电站已满，在便道等待，位置为%d。\n",
                     newDrone.id, newDrone.type, newDrone.arrival_time, dll_waitingPath.count);
        }
        for (int i = 0; i < totalDrones; i++) { if (allDrones[i].id == newDrone.id) { allDrones[i] = newDrone; break; } }
        return out;
    }

    if (cmd == 'd') {
        // 处理可选时间参数：如果没有提供时间（time == -1），使用当前系统时间
        int actual_time = (time >= 0) ? time : system_current_time;
        
        // 更新系统时间为离开时间
        if (actual_time > system_current_time) {
            system_current_time = actual_time;
        }
        
        int found = 0;
        initTempStack(&dll_tempStack);
        while (!isStackEmpty(&dll_chargingStation)) {
            Drone topDrone = pop(&dll_chargingStation);
            if (topDrone.id == id) {
                found = 1;
                int waitingTime = topDrone.enter_station_time - topDrone.arrival_time;
                int chargingTime = system_current_time - topDrone.enter_station_time;
                float totalCharging = waitingTime * getChargingRate(topDrone.type, 0) + chargingTime * getChargingRate(topDrone.type, 1);
                snprintf(out + strlen(out), sizeof(out) - strlen(out),
                         "编号为%d的无人机在时刻%d离开，\n在便道等待时间：%d，\n在充电站充电时间：%d，\n总充电量：%.1f%%。\n",
                         id, system_current_time, waitingTime, chargingTime, totalCharging);
                for (int i = 0; i < totalDrones; i++) {
                    if (allDrones[i].id == id) {
                        allDrones[i].depart_time = system_current_time;
                        allDrones[i].in_station = 0;
                        allDrones[i].power_level += totalCharging;
                        if (allDrones[i].power_level > 100.0) allDrones[i].power_level = 100.0;
                        allDrones[i].is_charged = 1;
                        break;
                    }
                }
                break;
            } else {
                tempPush(&dll_tempStack, topDrone);
            }
        }
        while (!isTempStackEmpty(&dll_tempStack)) { Drone t = tempPop(&dll_tempStack); push(&dll_chargingStation, t); }

        if (!found) {
            Drone leavingDrone;
            if (removeFromQueue(&dll_waitingPath, id, &leavingDrone)) {
                found = 1;
                int waitingTime = system_current_time - leavingDrone.arrival_time;
                float totalCharging = waitingTime * getChargingRate(leavingDrone.type, 0);
                snprintf(out + strlen(out), sizeof(out) - strlen(out),
                         "编号为%d的无人机在时刻%d离开便道，\n在便道等待时间：%d，\n总充电量：%.1f%%。\n",
                         id, system_current_time, waitingTime, totalCharging);
                for (int i = 0; i < totalDrones; i++) {
                    if (allDrones[i].id == id) {
                        allDrones[i].depart_time = system_current_time;
                        allDrones[i].in_station = 0;
                        allDrones[i].power_level += totalCharging;
                        if (allDrones[i].power_level > 100.0) allDrones[i].power_level = 100.0;
                        allDrones[i].is_charged = 1;
                        break;
                    }
                }
            } else {
                snprintf(out + strlen(out), sizeof(out) - strlen(out), "无人机%d不在充电站或便道内！\n", id);
            }
        }

        if (!isStackFull(&dll_chargingStation) && !isQueueEmpty(&dll_waitingPath)) {
            Drone nextDrone = dequeue(&dll_waitingPath);
            nextDrone.enter_station_time = system_current_time;
            nextDrone.in_station = 1;
            push(&dll_chargingStation, nextDrone);
            snprintf(out + strlen(out), sizeof(out) - strlen(out), "编号为%d的无人机从便道进入充电站，位置为%d。\n", nextDrone.id, dll_chargingStation.top);
            for (int i = 0; i < totalDrones; i++) { if (allDrones[i].id == nextDrone.id) { allDrones[i] = nextDrone; break; } }
        }

        return out;
    }

    if (cmd == 's') {
        char buf[256];
        snprintf(out + strlen(out), sizeof(out) - strlen(out), "\n=== 系统状态 ===\n");
        snprintf(buf, sizeof(buf), "当前系统时间：%d\n", system_current_time);
        strncat(out, buf, sizeof(out) - strlen(out) - 1);
        snprintf(buf, sizeof(buf), "充电站 [%d/%d]: ", dll_chargingStation.top, MAX_STACK_SIZE);
        strncat(out, buf, sizeof(out) - strlen(out) - 1);
        if (isStackEmpty(&dll_chargingStation)) {
            strncat(out, "空\n", sizeof(out) - strlen(out) - 1);
        } else {
            for (int i = 0; i < dll_chargingStation.top; i++) {
                snprintf(buf, sizeof(buf), "%d(%c,%s) ", dll_chargingStation.stack[i].id, dll_chargingStation.stack[i].type,
                         dll_chargingStation.stack[i].is_charged ? "已充电" : "未充电");
                strncat(out, buf, sizeof(out) - strlen(out) - 1);
            }
            strncat(out, "\n", sizeof(out) - strlen(out) - 1);
        }
        snprintf(buf, sizeof(buf), "便道 [%d]: ", dll_waitingPath.count);
        strncat(out, buf, sizeof(out) - strlen(out) - 1);
        if (isQueueEmpty(&dll_waitingPath)) {
            strncat(out, "空\n", sizeof(out) - strlen(out) - 1);
        } else {
            Drone* temp = dll_waitingPath.front;
            while (temp) {
                snprintf(buf, sizeof(buf), "%d(%c,%s) ", temp->id, temp->type, temp->is_charged?"已充电":"未充电");
                strncat(out, buf, sizeof(out) - strlen(out) - 1);
                temp = temp->next;
            }
            strncat(out, "\n", sizeof(out) - strlen(out) - 1);
        }
        int chargedCount = 0; for (int i = 0; i < totalDrones; i++) if (allDrones[i].is_charged) chargedCount++;
        snprintf(buf, sizeof(buf), "总无人机: %d (已充电: %d, 未充电: %d)\n", totalDrones, chargedCount, totalDrones - chargedCount);
        strncat(out, buf, sizeof(out) - strlen(out) - 1);
        strncat(out, "================\n", sizeof(out) - strlen(out) - 1);
        return out;
    }

    if (cmd == 'e') {
        int x = 0, y = 0, z = 0, m = 0;
        if (secondLine) {
            sscanf(secondLine, " %d %d %d %d", &x, &y, &z, &m);
        }
        if (x < 0) x = 0;
        if (y < 0) y = 0;
        if (z < 0) z = 0;
        if (m < 0) m = 0;

        out_appendf(out, sizeof(out), "\n=== 紧急任务调度 ===\n");
        out_appendf(out, sizeof(out), "当前系统时间：%d\n", system_current_time);
        out_appendf(out, sizeof(out), "需求：A型%d架，B型%d架，C型%d架，工作量%d\n", x, y, z, m);

        // 计算所有可用无人机能力与效率（基于系统当前时间）
        for (int i = 0; i < totalDrones; i++) {
            if (allDrones[i].is_available && allDrones[i].depart_time == -1) {
                // 充电前电量统一设置为20%
                const float INITIAL_POWER = 20.0f;
                allDrones[i].power_level = INITIAL_POWER;
                
                // 计算实际充电时间
                float charging_hours = system_current_time - allDrones[i].arrival_time;
                if (charging_hours < 0) charging_hours = 0;
                
                // 根据实际充电时间计算充电后的电量（从20%开始充电）
                float charging_rate = getChargingRate(allDrones[i].type, allDrones[i].in_station);
                float charged_power = INITIAL_POWER + charging_rate * charging_hours;
                if (charged_power > 100.0f) charged_power = 100.0f;
                
                // 更新充电状态
                if (charging_hours > 0) {
                    allDrones[i].is_charged = 1;
                    allDrones[i].power_level = charged_power;
                }
                // 计算可用工作时间（电量从当前电量到20%）
                float usable_power = charged_power - 20.0f;
                if (usable_power < 0) usable_power = 0;
                float consumption_rate = getConsumptionRate(allDrones[i].type, allDrones[i].in_station);
                float available_time = (consumption_rate > 0) ? (usable_power / consumption_rate) : 0.0f;
                switch (allDrones[i].type) {
                    case 'A': allDrones[i].work_capacity = 1.0f * available_time; break;
                    case 'B': allDrones[i].work_capacity = 2.0f * available_time; break;
                    case 'C': allDrones[i].work_capacity = 2.5f * available_time; break;
                }
                allDrones[i].energy_consumed = usable_power;
                allDrones[i].energy_per_work = (allDrones[i].work_capacity > 0) ? (allDrones[i].energy_consumed / allDrones[i].work_capacity) : 9999.0f;
            }
        }

        // 排序：类型(C>B>A) 优先；同类型能耗效率低优先；再电量高优先
        for (int i = 0; i < totalDrones - 1; i++) {
            for (int j = i + 1; j < totalDrones; j++) {
                if (allDrones[i].is_available && allDrones[i].depart_time == -1 &&
                    allDrones[j].is_available && allDrones[j].depart_time == -1) {
                    int swap = 0;
                    if (allDrones[i].type == 'A' && allDrones[j].type != 'A') swap = 1;
                    else if (allDrones[i].type == 'B' && allDrones[j].type == 'C') swap = 1;
                    else if (allDrones[i].type == allDrones[j].type) {
                        if (allDrones[i].energy_per_work > allDrones[j].energy_per_work) swap = 1;
                        else if (allDrones[i].energy_per_work == allDrones[j].energy_per_work && allDrones[i].power_level < allDrones[j].power_level) swap = 1;
                    }
                    if (swap) { Drone t = allDrones[i]; allDrones[i] = allDrones[j]; allDrones[j] = t; }
                }
            }
        }

        // 选择无人机
        Drone selected[MAX_DRONES];
        int selectedCount = 0, a_sel = 0, b_sel = 0, c_sel = 0; float total_capacity = 0.0f;
        for (int i = 0; i < totalDrones; i++) {
            if (allDrones[i].is_available && allDrones[i].depart_time == -1) {
                if (allDrones[i].type == 'A' && a_sel < x) { selected[selectedCount++] = allDrones[i]; a_sel++; total_capacity += allDrones[i].work_capacity; allDrones[i].is_available = 0; }
                else if (allDrones[i].type == 'B' && b_sel < y) { selected[selectedCount++] = allDrones[i]; b_sel++; total_capacity += allDrones[i].work_capacity; allDrones[i].is_available = 0; }
                else if (allDrones[i].type == 'C' && c_sel < z) { selected[selectedCount++] = allDrones[i]; c_sel++; total_capacity += allDrones[i].work_capacity; allDrones[i].is_available = 0; }
                if (a_sel >= x && b_sel >= y && c_sel >= z) break;
            }
        }
        if (total_capacity < m) {
            for (int i = 0; i < totalDrones && total_capacity < m; i++) {
                if (allDrones[i].is_available && allDrones[i].depart_time == -1) {
                    int exists = 0; for (int j = 0; j < selectedCount; j++) if (selected[j].id == allDrones[i].id) { exists = 1; break; }
                    if (!exists) { selected[selectedCount++] = allDrones[i]; total_capacity += allDrones[i].work_capacity; allDrones[i].is_available = 0; }
                }
            }
        }

        // 充电阶段（根据实际停留时间计算充电并输出表格）
        out_appendf(out, sizeof(out), "\n=== 充电阶段 ===\n");
        out_appendf(out, sizeof(out), "编号\t类型\t位置\t充电前电量\t充电小时\t充电后电量\t充电状态\n");
        for (int i = 0; i < selectedCount; i++) {
            // 充电前电量统一设置为20%
            const float INITIAL_POWER = 20.0f;
            selected[i].power_level = INITIAL_POWER;
            
            // 计算实际充电时间（从到达时间到当前系统时间）
            float charging_hours = system_current_time - selected[i].arrival_time;
            if (charging_hours < 0) charging_hours = 0;
            
            float initial_power = INITIAL_POWER;
            float rate = getChargingRate(selected[i].type, selected[i].in_station);
            float gain = rate * charging_hours;
            float after = initial_power + gain; if (after > 100.0f) after = 100.0f;
            selected[i].power_level = after;
            selected[i].is_charged = 1;
            
            // 基于充电后的电量重新计算工作能力和能耗效率
            float usable_power = after - 20.0f;
            if (usable_power < 0) usable_power = 0;
            float consumption_rate = getConsumptionRate(selected[i].type, selected[i].in_station);
            float available_time = (consumption_rate > 0) ? (usable_power / consumption_rate) : 0.0f;
            
            switch (selected[i].type) {
                case 'A': selected[i].work_capacity = 1.0f * available_time; break;
                case 'B': selected[i].work_capacity = 2.0f * available_time; break;
                case 'C': selected[i].work_capacity = 2.5f * available_time; break;
            }
            
            selected[i].energy_consumed = usable_power;
            selected[i].energy_per_work = (selected[i].work_capacity > 0) ? 
                selected[i].energy_consumed / selected[i].work_capacity : 9999.0f;
            
            // 同步回全局数组
            for (int k = 0; k < totalDrones; k++) {
                if (allDrones[k].id == selected[i].id) {
                    allDrones[k].power_level = selected[i].power_level;
                    allDrones[k].work_capacity = selected[i].work_capacity;
                    allDrones[k].energy_consumed = selected[i].energy_consumed;
                    allDrones[k].energy_per_work = selected[i].energy_per_work;
                    allDrones[k].is_charged = 1;
                    break;
                }
            }
            const char* loc = selected[i].in_station ? "充电站" : "便道";
            out_appendf(out, sizeof(out), "%d\t%c\t%s\t%.1f%%\t%.1f\t%.1f%%\t%s\n",
                        selected[i].id, selected[i].type, loc, initial_power, charging_hours, after, "已充电");
        }

        out_appendf(out, sizeof(out), "实际选择：A型%d架，B型%d架，C型%d架\n", a_sel, b_sel, c_sel);
        out_appendf(out, sizeof(out), "预计总工作能力：%.1f\n", total_capacity);
        if (total_capacity < m) out_appendf(out, sizeof(out), "警告：预计工作能力不足！\n");

        out_appendf(out, sizeof(out), "\n选择的无人机列表：\n");
        out_appendf(out, sizeof(out), "ID\t类型\t位置\t停留时间\t当前电量\t工作能力\t能耗效率\n");
        for (int i = 0; i < selectedCount; i++) {
            float stay_time = system_current_time - selected[i].arrival_time;
            out_appendf(out, sizeof(out), "%d\t%c\t%s\t%.1f小时\t%.1f%%\t%.1f\t%.3f\n",
                        selected[i].id, selected[i].type, selected[i].in_station ? "充电站" : "便道",
                        stay_time, selected[i].power_level, selected[i].work_capacity, selected[i].energy_per_work);
        }

        // 执行任务（非交互，输出过程简化为每步汇总）
        if (selectedCount == 0) {
            out_appendf(out, sizeof(out), "\n没有选择任何无人机执行任务！\n");
            return out;
        }

        // 初始化任务视图
        TaskDrone tds[MAX_DRONES];
        for (int i = 0; i < selectedCount; i++) {
            tds[i].id = selected[i].id;
            tds[i].type = selected[i].type;
            tds[i].current_power = selected[i].power_level;
            tds[i].work_efficiency = (tds[i].type=='A')?1.0f: (tds[i].type=='B')?2.0f:2.5f;
            tds[i].consumption_rate = getConsumptionRate(selected[i].type, selected[i].in_station);
            tds[i].is_active = 1;
            tds[i].total_work_done = 0.0f;
        }

        float remaining = (float)m; int step = 0;
        out_appendf(out, sizeof(out), "\n=== 任务执行阶段 ===\n");
        while (remaining > 0.0001f) {
            step++;
            float thisStep = 0.0f; int active = 0;
            out_appendf(out, sizeof(out), "\n时间步 %d:\n", step);
            for (int i = 0; i < selectedCount; i++) {
                if (tds[i].is_active && tds[i].current_power > 20.0f) {
                    float workDone = tds[i].work_efficiency;
                    thisStep += workDone; tds[i].total_work_done += workDone;
                    tds[i].current_power -= tds[i].consumption_rate;
                    if (tds[i].current_power <= 20.0f) {
                        tds[i].is_active = 0; tds[i].current_power = 20.0f;
                        out_appendf(out, sizeof(out), "  无人机%d(%c) 电量低于20%%，停止工作（已完成%.1f）\n",
                                    tds[i].id, tds[i].type, tds[i].total_work_done);
                    } else {
                        active++;
                        out_appendf(out, sizeof(out), "  无人机%d(%c): 工作%.1f, 剩余电量%.1f%%, 累计%.1f\n",
                                    tds[i].id, tds[i].type, workDone, tds[i].current_power, tds[i].total_work_done);
                    }
                }
            }
            remaining -= thisStep; if (remaining < 0) remaining = 0;
            out_appendf(out, sizeof(out), "  本步完成: %.1f, 剩余: %.1f, 活跃无人机: %d/%d\n", thisStep, remaining, active, selectedCount);
            if (active == 0 && remaining > 0.0001f) { out_appendf(out, sizeof(out), "\n所有无人机电量不足，任务中断！\n"); break; }
            if (step > 10000) break; // 安全上限
        }

        float totalUsed = 0.0f;
        out_appendf(out, sizeof(out), "\n=== 任务结果 ===\n");
        out_appendf(out, sizeof(out), "总工作量: %.1f/%d (%.1f%%)\n", (float)m - remaining, m, ((float)m - remaining) / (float)m * 100.0f);
        out_appendf(out, sizeof(out), "\n各无人机工作详情:\n");
        out_appendf(out, sizeof(out), "编号\t类型\t初始电量\t最终电量\t工作量\t能耗\n");
        for (int i = 0; i < selectedCount; i++) {
            float energyUsed = selected[i].power_level - tds[i].current_power; totalUsed += energyUsed;
            out_appendf(out, sizeof(out), "%d\t%c\t%.1f%%\t%.1f%%\t%.1f\t%.1f%%\n",
                        tds[i].id, tds[i].type, selected[i].power_level, tds[i].current_power, tds[i].total_work_done, energyUsed);
        }
        out_appendf(out, sizeof(out), "\n统计摘要:\n");
        out_appendf(out, sizeof(out), "总能耗: %.1f%%\n", totalUsed);
        if (totalUsed > 0.0001f) out_appendf(out, sizeof(out), "平均效率: %.2f 工作量/%%电量\n", (((float)m - remaining) / totalUsed));
        out_appendf(out, sizeof(out), "用时: %d 时间单位\n", step);

        return out;
    }

    snprintf(out + strlen(out), sizeof(out) - strlen(out), "错误：未知命令！\n");
    return out;
}
// 注意：不在此文件中定义main，避免与main.cpp重复定义