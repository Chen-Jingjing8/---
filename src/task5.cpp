#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_STACK_SIZE 2
#define MAX_TEMP_SIZE 10
#define MAX_DRONES 100
#define BUFFER_SIZE 1024

typedef struct Drone {
    int id;
    char type;
    int arrival_time;
    float power_level;
    struct Drone* next;
} Drone;

typedef struct {
    Drone west_stack[MAX_STACK_SIZE];
    int west_top;
    Drone east_stack[MAX_STACK_SIZE];
    int east_top;
    Drone temp_stack[MAX_TEMP_SIZE];
    int temp_top;
    int max_bypass_depth;
} DualEntryStation;

typedef struct {
    Drone* front;
    Drone* rear;
    int count;
} WaitingQueue;

DualEntryStation station;
WaitingQueue queue;
Drone allDrones[MAX_DRONES];
int totalDrones = 0;

extern "C" {
    const char* simulateChargingStation(const char* input);
}

void initSystem() {
    memset(&station, 0, sizeof(station));
    station.west_top = -1;
    station.east_top = -1;
    station.temp_top = -1;
    station.max_bypass_depth = 0;
    queue.front = queue.rear = NULL;
    queue.count = 0;
}

int isWestFull() { return station.west_top == MAX_STACK_SIZE - 1; }
int isEastFull() { return station.east_top == MAX_STACK_SIZE - 1; }
int isTempFull() { return station.temp_top == MAX_TEMP_SIZE - 1; }

void enterWest(Drone drone, char* outputBuffer, size_t bufferSize) {
    if (isWestFull()) {
        Drone* newDrone = (Drone*)malloc(sizeof(Drone));
        *newDrone = drone;
        newDrone->next = NULL;
        
        if (queue.rear == NULL) {
            queue.front = queue.rear = newDrone;
        } else {
            queue.rear->next = newDrone;
            queue.rear = newDrone;
        }
        queue.count++;
        snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
                 "西口已满！无人机%d进入便道\n", drone.id);
    } else {
        station.west_stack[++station.west_top] = drone;
        snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
                 "无人机%d从西口进入，西口当前数量：%d\n", drone.id, station.west_top + 1);
        
        // 自动转移到东口（保持FIFO：先进西口的先到东口）
        if (!isEastFull() && station.west_top >= 0) {
            // 从西口栈底（索引0）取，保证先进先出
            Drone moved = station.west_stack[0];
            // 将剩余元素前移
            for (int k = 0; k < station.west_top; k++) {
                station.west_stack[k] = station.west_stack[k + 1];
            }
            station.west_top--;
            
            station.east_stack[++station.east_top] = moved;
            snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
                     "无人机%d从西口自动转移到东口\n", moved.id);
        }
    }
}

void departEast(int id, int time, char* outputBuffer, size_t bufferSize) {
    // 在东口栈查找
    for (int i = station.east_top; i >= 0; i--) {
        if (station.east_stack[i].id == id) {
            // 检查是否需要让路（不在东出口位置，即索引0）
            int bypass_count = 0;
            if (i != 0) {
                snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
                         "触发让路机制...\n");
                
                // 将目标无人机前方（索引0到i-1）的所有无人机移到临时栈
                // 按照索引顺序让路：索引0先让路，索引1再让路
                for (int j = 0; j < i; j++) {
                    if (isTempFull()) {
                        snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
                                 "错误：临时栈已满！\n");
                        return;
                    }
                    station.temp_stack[++station.temp_top] = station.east_stack[j];
                    bypass_count++;
                    snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
                             "无人机%d移到临时栈（让路深度：%d）\n", 
                             station.temp_stack[station.temp_top].id, bypass_count);
                }
                
                // 将目标无人机及后面的无人机向前移动到索引0
                for (int j = 0; j <= station.east_top - i; j++) {
                    station.east_stack[j] = station.east_stack[i + j];
                }
                station.east_top -= bypass_count;
                
                // 更新最大让路深度
                if (bypass_count > station.max_bypass_depth) {
                    station.max_bypass_depth = bypass_count;
                }
            }
            
            // 执行离站（目标无人机现在在索引0）
            Drone d = station.east_stack[0];
            snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
                     "无人机%d从东口离开，停留时间：%d\n", d.id, time - d.arrival_time);
            
            // 移除索引0的无人机
            for (int j = 0; j < station.east_top; j++) {
                station.east_stack[j] = station.east_stack[j + 1];
            }
            station.east_top--;
            
            // 步骤1：让路无人机从临时栈返回西口，按照FIFO顺序（先让路的先返回）
            // 先保存临时栈的数量
            int temp_count = station.temp_top + 1;
            
            // 按照FIFO顺序处理：从栈底（索引0）开始，先让路的先返回
            for (int idx = 0; idx < temp_count; idx++) {
                // 直接按索引顺序处理，保证FIFO
                int actual_idx = idx;
                
                Drone returning = station.temp_stack[actual_idx];
                
                // 如果西口满了，先转移西口无人机到东口
                while (isWestFull() && !isEastFull()) {
                    Drone westMoved = station.west_stack[0];
                    for (int k = 0; k < station.west_top; k++) {
                        station.west_stack[k] = station.west_stack[k + 1];
                    }
                    station.west_top--;
                    station.east_stack[++station.east_top] = westMoved;
                    snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
                             "无人机%d从西口转移到东口\n", westMoved.id);
                }
                
                // 让路无人机进入西口
                if (!isWestFull()) {
                    station.west_stack[++station.west_top] = returning;
                    snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
                             "无人机%d从临时栈返回西口\n", returning.id);
                    
                    // 立即尝试从西口流动到东口
                    while (station.west_top >= 0 && !isEastFull()) {
                        Drone westMoved = station.west_stack[0];
                        for (int k = 0; k < station.west_top; k++) {
                            station.west_stack[k] = station.west_stack[k + 1];
                        }
                        station.west_top--;
                        station.east_stack[++station.east_top] = westMoved;
                        snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
                                 "无人机%d从西口转移到东口\n", westMoved.id);
                    }
                } else {
                    snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
                             "错误：西口和东口都满，让路无人机%d无法返回！\n", returning.id);
                }
            }
            
            // 清空临时栈
            station.temp_top = -1;
            
            // 步骤2：从便道补充到西口，并立即流动到东口
            while (queue.front != NULL && !isWestFull()) {
                Drone* moved = queue.front;
                queue.front = queue.front->next;
                if (queue.front == NULL) queue.rear = NULL;
                queue.count--;
                
                station.west_stack[++station.west_top] = *moved;
                snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
                         "无人机%d从便道进入西口\n", moved->id);
                free(moved);
                
                // 立即尝试从西口流动到东口
                while (station.west_top >= 0 && !isEastFull()) {
                    Drone westMoved = station.west_stack[0];
                    for (int k = 0; k < station.west_top; k++) {
                        station.west_stack[k] = station.west_stack[k + 1];
                    }
                    station.west_top--;
                    station.east_stack[++station.east_top] = westMoved;
                    snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
                             "无人机%d从西口转移到东口\n", westMoved.id);
                }
            }
            return;
        }
    }
    
    // 未在东口找到，检查西口
    for (int i = station.west_top; i >= 0; i--) {
        if (station.west_stack[i].id == id) {
            snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
                     "无人机%d在西口栈索引%d位置\n", id, i);
            
            int bypass_count = 0;
            
            // 如果东口满了，需要先让东口的无人机让路腾出空间
            if (isEastFull()) {
                snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
                         "东口已满，让东口无人机让路腾出空间...\n");
                
                // 将东口所有无人机暂时移到临时栈
                // 按照索引顺序让路：索引0先让路，索引1再让路
                for (int j = 0; j <= station.east_top; j++) {
                    if (isTempFull()) {
                        snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
                                 "错误：临时栈已满！\n");
                        return;
                    }
                    station.temp_stack[++station.temp_top] = station.east_stack[j];
                    bypass_count++;
                    snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
                             "无人机%d移到临时栈（让路深度：%d）\n", 
                             station.temp_stack[station.temp_top].id, bypass_count);
                }
                station.east_top = -1;
            }
            
            // 西口中目标无人机前面的无人机也需要让路
            if (i > 0) {
                snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
                         "西口栈中前方有%d个无人机需要让路...\n", i);
                
                // 将西口目标无人机前面的无人机（索引0到i-1）移到临时栈
                // 按照索引顺序让路：索引0先让路，索引1再让路
                for (int j = 0; j < i; j++) {
                    if (isTempFull()) {
                        snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
                                 "错误：临时栈已满！\n");
                        return;
                    }
                    station.temp_stack[++station.temp_top] = station.west_stack[j];
                    bypass_count++;
                    snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
                             "无人机%d移到临时栈（让路深度：%d）\n", 
                             station.temp_stack[station.temp_top].id, bypass_count);
                }
                
                // 将目标无人机移到索引0
                station.west_stack[0] = station.west_stack[i];
                // 调整栈顶，移除了i个让路的无人机
                station.west_top = station.west_top - i;
            }
            
            // 更新最大让路深度
            if (bypass_count > station.max_bypass_depth) {
                station.max_bypass_depth = bypass_count;
            }
            
            // 从西口取出目标无人机（现在在索引0）
            Drone moved = station.west_stack[0];
            for (int j = 0; j < station.west_top; j++) {
                station.west_stack[j] = station.west_stack[j + 1];
            }
            station.west_top--;
            
            // 转移到东口
            station.east_stack[++station.east_top] = moved;
            snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
                     "无人机%d从西口转移到东口\n", moved.id);
            
            // 从东口离开（递归调用，会处理让路无人机的返回）
            departEast(id, time, outputBuffer, bufferSize);
            return;
        }
    }
    
    snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
             "错误：无人机%d不在充电站内！\n", id);
}

const char* simulateChargingStation(const char* input) {
    static int initialized = 0;
    if (!initialized) {
        initSystem();
        initialized = 1;
    }

    char* buffer = (char*)malloc(BUFFER_SIZE);
    if (!buffer) {
        return "内存分配失败！";
    }
    memset(buffer, 0, BUFFER_SIZE);

    char action;
    int id, time;
    char type;

    snprintf(buffer, BUFFER_SIZE, "正在处理操作：%s\n", input);
    
    // 先尝试解析a命令（需要4个参数）
    if (sscanf(input, " %c %d %c %d", &action, &id, &type, &time) == 4 && action == 'a') {
        // 检查是否已存在相同ID的无人机
        int droneExists = 0;
        for (int i = 0; i < totalDrones; i++) {
            if (allDrones[i].id == id) {
                droneExists = 1;
                break;
            }
        }
        
        if (droneExists) {
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "错误：编号为%d的无人机已经在系统中，无法重复到达！\n", id);
        } else {
            Drone drone = {id, type, time, 20.0, NULL};
            allDrones[totalDrones++] = drone;
            
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "编号为%d，种类为%c的无人机在时刻%d到达。\n", 
                     id, type, time);
            
            enterWest(drone, buffer, BUFFER_SIZE);
        }
    } else if (sscanf(input, " %c %d %d", &action, &id, &time) == 3 && action == 'd') {
        // d命令只需要3个参数：action id time
        departEast(id, time, buffer, BUFFER_SIZE);
    } else if (sscanf(input, " %c", &action) == 1 && action == 's') {
        // 显示状态
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "\n=== 系统状态 ===\n");
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "西口栈[%d/%d]: ", station.west_top + 1, MAX_STACK_SIZE);
        for (int i = 0; i <= station.west_top; i++) {
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "%d(%c) ", station.west_stack[i].id, station.west_stack[i].type);
        }
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer), "\n");
        
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "东口栈[%d/%d]: ", station.east_top + 1, MAX_STACK_SIZE);
        for (int i = 0; i <= station.east_top; i++) {
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "%d(%c) ", station.east_stack[i].id, station.east_stack[i].type);
        }
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer), "\n");
        
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "便道[%d]: ", queue.count);
        Drone* current = queue.front;
        while (current != NULL) {
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "%d(%c) ", current->id, current->type);
            current = current->next;
        }
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer), "\n");
        
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "历史最大让路深度: %d\n", station.max_bypass_depth);
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "================\n");
    }

    return buffer;
}

