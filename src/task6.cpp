#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_STACK_SIZE 2
#define MAX_DRONES 100
#define DRONE_ID_LEN 32
#define HASH_LEN 65
#define SIMPLE_KEY 0xAA
#define BUFFER_SIZE 8192
#define JSON_DATA_FILE "../data/task6_registrations.json"

int admin_mode = 0;

// 函数声明
void saveDroneDataToJSON();
void loadDroneDataFromJSON();

typedef struct {
    char id_hash[HASH_LEN];
    char type_hash[HASH_LEN];
    char key_hash[HASH_LEN];
    char raw_id[DRONE_ID_LEN];
    char raw_type;
    char raw_key[128];
    int auth_attempts;
    time_t last_auth_time;
    int trust_level;
} SecurityContext;

typedef struct Drone {
    int id;
    char type;
    int arrival_time;
    float power_level;
    SecurityContext security;
    struct Drone* next;
    time_t last_update;
} Drone;

// 双入口栈结构（参考任务5）
typedef struct {
    Drone west_stack[MAX_STACK_SIZE];
    int west_top;
    Drone east_stack[MAX_STACK_SIZE];
    int east_top;
    Drone temp_stack[MAX_STACK_SIZE];
    int temp_top;
    int max_bypass_depth;
} DualEntryStation;

typedef struct QueueNode {
    Drone drone;
    struct QueueNode* next;
} QueueNode;

typedef struct {
    QueueNode* front;
    QueueNode* rear;
    int count;
} WaitingQueue;

DualEntryStation station;
WaitingQueue queue;
Drone allDrones[MAX_DRONES];
int totalDrones = 0;

// 组网相关数据结构
int networkDrones[MAX_DRONES];  // 组网中的无人机ID列表
int networkCount = 0;           // 组网中无人机数量
int maliciousDroneId = -1;      // 恶意无人机ID
int networkPoisoned = 0;         // 组网是否被投毒（0=未投毒，1=已投毒）
int networkCrashed = 0;         // 组网是否坠毁（0=正常，1=已坠毁）

// ==================== 工具函数 ====================
void simpleHash(const char* input, char* output) {
    unsigned long hash = 5381;
    int c;
    
    while ((c = *input++)) {
        hash = ((hash << 5) + hash) + c;
    }
    
    snprintf(output, HASH_LEN, "%016lx", hash);
}

void simpleEncrypt(char* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        data[i] ^= SIMPLE_KEY;
    }
}

void simpleDecrypt(char* data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        data[i] ^= SIMPLE_KEY;
    }
}

// ==================== 栈操作辅助函数 ====================
int isWestFull() { 
    return station.west_top == MAX_STACK_SIZE - 1; 
}

int isEastFull() { 
    return station.east_top == MAX_STACK_SIZE - 1; 
}

int isTempFull() { 
    return station.temp_top >= MAX_STACK_SIZE - 1; 
}

int getWestCount() {
    return station.west_top + 1;
}

int getEastCount() {
    return station.east_top + 1;
}

// ==================== 系统初始化 ====================
void initSystem() {
    memset(&station, 0, sizeof(station));
    station.west_top = -1;
    station.east_top = -1;
    station.temp_top = -1;
    station.max_bypass_depth = 0;
    
    queue.front = queue.rear = NULL;
    queue.count = 0;
    totalDrones = 0;
    
    // 初始化组网状态
    networkCount = 0;
    maliciousDroneId = -1;
    networkPoisoned = 0;
    networkCrashed = 0;
    memset(networkDrones, 0, sizeof(networkDrones));
    
    // 自动加载JSON文件中的注册信息
    loadDroneDataFromJSON();
}

// ==================== 无人机注册与管理 ====================
void registerDrone(int id, char type) {
    if (totalDrones >= MAX_DRONES) {
        printf("错误：无人机数量已达上限！\n");
        return;
    }
    
    // 检查是否已存在
    for (int i = 0; i < totalDrones; i++) {
        if (allDrones[i].id == id) {
            printf("错误：无人机%d已注册！\n", id);
            return;
        }
    }
    
    Drone newDrone;
    memset(&newDrone, 0, sizeof(Drone));
    newDrone.id = id;
    newDrone.type = type;
    newDrone.arrival_time = 0;
    newDrone.power_level = 20.0;
    newDrone.next = NULL;
    newDrone.last_update = time(NULL);
    
    snprintf(newDrone.security.raw_id, DRONE_ID_LEN, "DRONE%03d", id);
    newDrone.security.raw_type = type;
    time_t now = time(NULL);
    snprintf(newDrone.security.raw_key, 128, "KEY_%d_%lu", id, (unsigned long)now);
    
    simpleHash(newDrone.security.raw_id, newDrone.security.id_hash);
    simpleHash(&newDrone.security.raw_type, newDrone.security.type_hash);
    simpleHash(newDrone.security.raw_key, newDrone.security.key_hash);
    
    newDrone.security.auth_attempts = 0;
    newDrone.security.last_auth_time = time(NULL);
    newDrone.security.trust_level = 60;
    
    allDrones[totalDrones++] = newDrone;
    
    printf("无人机%d注册成功！类型：%c\n", id, type);
    
    // 自动保存到JSON文件
    saveDroneDataToJSON();
}

// ==================== 组网相关函数 ====================
int isDroneInNetwork(int drone_id) {
    for (int i = 0; i < networkCount; i++) {
        if (networkDrones[i] == drone_id) {
            return 1;
        }
    }
    return 0;
}

void addDroneToNetwork(int drone_id, char* outputBuffer, size_t bufferSize) {
    if (isDroneInNetwork(drone_id)) {
        return; // 已在组网中
    }
    
    if (networkCount < MAX_DRONES) {
        networkDrones[networkCount++] = drone_id;
        snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
                 "无人机%d已加入组网 (当前组网: %d/6)\n", drone_id, networkCount);
    }
}

void detectMaliciousDrone(int drone_id, char* outputBuffer, size_t bufferSize) {
    // 检查无人机是否注册
    int isRegistered = 0;
    for (int i = 0; i < totalDrones; i++) {
        if (allDrones[i].id == drone_id) {
            isRegistered = 1;
            break;
        }
    }
    
    if (!isRegistered) {
        maliciousDroneId = drone_id;
        snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
                 "▲警告: 检测到未注册无人机%d! 视为恶意无人机!\n", drone_id);
    }
}

void poisonNetwork(char* outputBuffer, size_t bufferSize) {
    if (maliciousDroneId > 0 && networkCount > 0 && !networkPoisoned) {
        networkPoisoned = 1;
        snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
                 "💥恶意无人机%d已向组网投毒! 组网处于危险状态!\n", maliciousDroneId);
    }
}

void triggerNetworkCrash(char* outputBuffer, size_t bufferSize) {
    if (maliciousDroneId > 0 && networkPoisoned && !networkCrashed) {
        networkCrashed = 1;
        snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
                 "💥恶意无人机%d起飞! 触发组网连锁坠毁!\n", maliciousDroneId);
        snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
                 "💥组网内%d架无人机全部坠毁!\n", networkCount);
        
        for (int i = 0; i < networkCount; i++) {
            snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
                     "- 无人机%d已坠毁\n", networkDrones[i]);
        }
    }
}

// ==================== 核心到达函数（参考任务5） ====================
void droneArrive(int drone_id, int current_time, char* outputBuffer, size_t bufferSize) {
    Drone* drone = NULL;
    for (int i = 0; i < totalDrones; i++) {
        if (allDrones[i].id == drone_id) {
            drone = &allDrones[i];
            break;
        }
    }
    
    // 检测恶意无人机（未注册的无人机）
    if (drone == NULL) {
        detectMaliciousDrone(drone_id, outputBuffer, bufferSize);
        // 恶意无人机不进入充电站，直接返回
        return;
    }
    
    drone->arrival_time = current_time;
    drone->last_update = time(NULL);
    
    // 加入组网
    addDroneToNetwork(drone_id, outputBuffer, bufferSize);
    
    // 如果组网中有恶意无人机且未投毒，执行投毒攻击
    if (maliciousDroneId > 0 && !networkPoisoned) {
        poisonNetwork(outputBuffer, bufferSize);
    }
    
    // 所有无人机都从西口进入，然后自动转移到东口（与任务5一致）
    if (isWestFull()) {
        // 进入便道
        QueueNode* newDrone = (QueueNode*)malloc(sizeof(QueueNode));
        newDrone->drone = *drone;
        newDrone->next = NULL;
        
        if (queue.rear == NULL) {
            queue.front = queue.rear = newDrone;
        } else {
            queue.rear->next = newDrone;
            queue.rear = newDrone;
        }
        queue.count++;
        snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
                 "西口已满！无人机%d进入便道\n", drone_id);
    } else {
        station.west_stack[++station.west_top] = *drone;
        snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
                 "无人机%d从西口进入，西口当前数量：%d\n", 
                 drone_id, station.west_top + 1);
        
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

// ==================== 核心离站函数（参考任务5，添加详细输出） ====================
void droneDepart(int drone_id, int current_time, char* outputBuffer, size_t bufferSize) {
    // 1. 首先在东口栈查找目标无人机
    for (int i = station.east_top; i >= 0; i--) {
        if (station.east_stack[i].id == drone_id) {
            // 检查是否需要让路（不在东出口位置，即索引0）
            int bypass_count = 0;
            if (i != 0) {
                snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
                         "触发让路机制...\n");
                
                // 将目标无人机前方（索引0到i-1）的所有无人机移到临时栈
                // 按照索引顺序让路：索引0先让路，索引1再让路
                for (int j = 0; j < i; j++) {
                    if (station.temp_top + 1 >= MAX_STACK_SIZE) {
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
                     "无人机%d从东口离开，停留时间：%d\n", d.id, current_time - d.arrival_time);
            
            // 检查是否是恶意无人机起飞，触发组网坠毁
            if (d.id == maliciousDroneId && networkPoisoned && !networkCrashed) {
                triggerNetworkCrash(outputBuffer, bufferSize);
            }
            
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
            
            // 步骤2：便道队列中的无人机进入西口
            while (queue.front != NULL && !isWestFull()) {
                QueueNode* moved = queue.front;
                queue.front = queue.front->next;
                if (queue.front == NULL) queue.rear = NULL;
                queue.count--;
                
                station.west_stack[++station.west_top] = moved->drone;
                snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
                         "无人机%d从便道进入西口\n", moved->drone.id);
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
    
    // 2. 如果不在东口栈，检查是否在西口栈
    for (int i = station.west_top; i >= 0; i--) {
        if (station.west_stack[i].id == drone_id) {
            snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
                     "无人机%d在西口栈索引%d位置\n", drone_id, i);
            
            int bypass_count = 0;
            
            // 如果东口满了，需要先让东口的无人机让路腾出空间
            if (isEastFull()) {
                snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
                         "东口已满，让东口无人机让路腾出空间...\n");
                
                // 将东口所有无人机暂时移到临时栈
                // 按照索引顺序让路：索引0先让路，索引1再让路
                for (int j = 0; j <= station.east_top; j++) {
                    if (station.temp_top + 1 >= MAX_STACK_SIZE) {
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
                    if (station.temp_top + 1 >= MAX_STACK_SIZE) {
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
            droneDepart(drone_id, current_time, outputBuffer, bufferSize);
            return;
        }
    }
    
    snprintf(outputBuffer + strlen(outputBuffer), bufferSize - strlen(outputBuffer),
             "错误：无人机%d不在充电站内！\n", drone_id);
}

// ==================== 状态显示函数 ====================
void displayStatus() {
    printf("\n=== 系统状态 ===\n");
    
    // 西口栈状态
    printf("西口栈[%d/%d]: ", getWestCount(), MAX_STACK_SIZE);
    if (station.west_top >= 0) {
        for (int i = 0; i <= station.west_top; i++) {
            printf("%d(%c) ", station.west_stack[i].id, station.west_stack[i].type);
        }
    } else {
        printf("空");
    }
    printf("\n");
    
    // 东口栈状态
    printf("东口栈[%d/%d]: ", getEastCount(), MAX_STACK_SIZE);
    if (station.east_top >= 0) {
        for (int i = station.east_top; i >= 0; i--) {
            printf("%d(%c) ", station.east_stack[i].id, station.east_stack[i].type);
        }
    } else {
        printf("空");
    }
    printf("\n");
    
    // 便道状态
    printf("便道[%d]: ", queue.count);
    QueueNode* current = queue.front;
    while (current != NULL) {
        printf("%d(%c) ", current->drone.id, current->drone.type);
        current = current->next;
    }
    printf("\n");
    
    printf("历史最大让路深度: %d\n", station.max_bypass_depth);
    printf("================\n");
}

// ==================== 注册信息显示 ====================
void displayAllRegistrationInfo() {
    printf("\n=== 所有无人机注册信息 ===\n");
    printf("总数: %d架\n\n", totalDrones);
    
    for (int i = 0; i < totalDrones; i++) {
        char time_str[26];
        ctime_s(time_str, sizeof(time_str), &allDrones[i].last_update);
        
        printf("无人机ID: %d\n", allDrones[i].id);
        printf("类型: %c\n", allDrones[i].type);
        printf("最后更新: %s", time_str);
        printf("ID哈希: %s\n", allDrones[i].security.id_hash);
        printf("类型哈希: %s\n", allDrones[i].security.type_hash);
        printf("密钥哈希: %s\n", allDrones[i].security.key_hash);
        
        if (admin_mode) {
            printf("[管理员] 原始信息：\n");
            printf("ID: %s\n", allDrones[i].security.raw_id);
            printf("类型: %c\n", allDrones[i].security.raw_type);
            printf("密钥: %s\n", allDrones[i].security.raw_key);
        }
        
        printf("信任等级: %d\n", allDrones[i].security.trust_level);
        printf("----------------------\n");
    }
}

void toggleAdminMode(const char* password) {
    if (strcmp(password, "admin123") == 0) {
        admin_mode = !admin_mode;
        printf("管理员模式已%s\n", admin_mode ? "开启" : "关闭");
    } else {
        printf("错误：密码不正确！\n");
    }
}

// ==================== 前端接口函数 ====================
extern "C" {
    const char* simulateChargingStation(const char* input);
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
    char password[50];
    
    snprintf(buffer, BUFFER_SIZE, "正在处理操作：%s\n", input);
    
    if (sscanf(input, " %c", &action) != 1) {
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "错误：输入格式不正确！\n");
        return buffer;
    }
    
    // 检查是否是delete操作
    if (strncmp(input, "delete ", 7) == 0) {
        if (sscanf(input, "delete %d", &id) == 1) {
            // 查找并删除无人机
            int found = 0;
            for (int i = 0; i < totalDrones; i++) {
                if (allDrones[i].id == id) {
                    // 移动后面的元素向前
                    for (int j = i; j < totalDrones - 1; j++) {
                        allDrones[j] = allDrones[j + 1];
                    }
                    totalDrones--;
                    found = 1;
                    snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                             "无人机%d注册信息已删除\n", id);
                    // 保存更新后的数据到JSON文件
                    saveDroneDataToJSON();
                    break;
                }
            }
            if (!found) {
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "错误：无人机%d未注册！\n", id);
            }
        } else {
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "格式：delete 编号\n");
        }
    } else {
        switch (action) {
            case 'r':
                if (sscanf(input, "r %d %c", &id, &type) == 2) {
                    registerDrone(id, type);
                    snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                             "无人机%d注册成功！类型：%c\n", id, type);
                } else {
                    snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                             "格式：r 编号 类型\n");
                }
                break;
            
        case 'a':
            if (sscanf(input, "a %d %d", &id, &time) == 2) {
                // 查找无人机信息（如果已注册，则输出正常到达信息）
                Drone* drone = NULL;
                for (int i = 0; i < totalDrones; i++) {
                    if (allDrones[i].id == id) {
                        drone = &allDrones[i];
                        break;
                    }
                }
                
                if (drone != NULL) {
                    // 已注册无人机的到达提示（与任务5一致）
                    snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                             "编号为%d，种类为%c的无人机在时刻%d到达。\n", 
                             drone->id, drone->type, time);
                }

                // 无论是否注册，都交由 droneArrive 处理：
                // - 已注册无人机：正常入站并加入组网
                // - 未注册无人机：在 droneArrive 中被识别为恶意无人机并触发告警/投毒
                droneArrive(id, time, buffer, BUFFER_SIZE);
            } else {
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "格式：a 编号 时间\n");
            }
            break;
            
        case 'd':
            if (sscanf(input, "d %d %d", &id, &time) == 2) {
                droneDepart(id, time, buffer, BUFFER_SIZE);
                // 输出状态信息
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "西口栈[%d/%d]: ", getWestCount(), MAX_STACK_SIZE);
                if (station.west_top >= 0) {
                    for (int i = 0; i <= station.west_top; i++) {
                        char temp[20];
                        snprintf(temp, sizeof(temp), "%d(%c) ", station.west_stack[i].id, station.west_stack[i].type);
                        strcat(buffer, temp);
                    }
                } else {
                    strcat(buffer, "空");
                }
                strcat(buffer, "\n");
                
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "东口栈[%d/%d]: ", getEastCount(), MAX_STACK_SIZE);
                if (station.east_top >= 0) {
                    for (int i = station.east_top; i >= 0; i--) {
                        char temp[20];
                        snprintf(temp, sizeof(temp), "%d(%c) ", station.east_stack[i].id, station.east_stack[i].type);
                        strcat(buffer, temp);
                    }
                } else {
                    strcat(buffer, "空");
                }
                strcat(buffer, "\n");
                
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "便道[%d]: ", queue.count);
                QueueNode* current = queue.front;
                while (current != NULL) {
                    char temp[20];
                    snprintf(temp, sizeof(temp), "%d(%c) ", current->drone.id, current->drone.type);
                    strcat(buffer, temp);
                    current = current->next;
                }
                strcat(buffer, "\n");
            } else {
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "格式：d 编号 时间\n");
            }
            break;
            
        case 's': {
            // 显示状态
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "\n=== 系统状态 ===\n");
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "西口栈[%d/%d]: ", getWestCount(), MAX_STACK_SIZE);
            if (station.west_top >= 0) {
                for (int i = 0; i <= station.west_top; i++) {
                    snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                             "%d(%c) ", station.west_stack[i].id, station.west_stack[i].type);
                }
            } else {
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer), "空");
            }
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer), "\n");
            
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "东口栈[%d/%d]: ", getEastCount(), MAX_STACK_SIZE);
            if (station.east_top >= 0) {
                for (int i = 0; i <= station.east_top; i++) {
                    snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                             "%d(%c) ", station.east_stack[i].id, station.east_stack[i].type);
                }
            } else {
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer), "空");
            }
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer), "\n");
            
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "便道[%d]: ", queue.count);
            QueueNode* current = queue.front;
            while (current != NULL) {
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "%d(%c) ", current->drone.id, current->drone.type);
                current = current->next;
            }
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer), "\n");
            
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "历史最大让路深度: %d\n", station.max_bypass_depth);
            break;
        }
            
        case 'm':
            if (sscanf(input, "m %s", password) == 1) {
                toggleAdminMode(password);
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "管理员模式已%s\n", admin_mode ? "开启" : "关闭");
            } else {
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "格式：m 密码\n");
            }
            break;
            
        case 'i':
            displayAllRegistrationInfo();
            // 将注册信息添加到buffer
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "=== 所有无人机注册信息 ===\n");
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "总数: %d架\n\n", totalDrones);
            
            for (int i = 0; i < totalDrones; i++) {
                char time_str[26];
                ctime_s(time_str, sizeof(time_str), &allDrones[i].last_update);
                
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "无人机ID: %d\n", allDrones[i].id);
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "类型: %c\n", allDrones[i].type);
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "最后更新: %s", time_str);
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "ID哈希: %s\n", allDrones[i].security.id_hash);
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "类型哈希: %s\n", allDrones[i].security.type_hash);
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "密钥哈希: %s\n", allDrones[i].security.key_hash);
                
                if (admin_mode) {
                    snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                             "[管理员] 原始信息：\n");
                    snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                             "ID: %s\n", allDrones[i].security.raw_id);
                    snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                             "类型: %c\n", allDrones[i].security.raw_type);
                    snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                             "密钥: %s\n", allDrones[i].security.raw_key);
                }
                
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "信任等级: %d\n", allDrones[i].security.trust_level);
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "----------------------\n");
            }
            break;
            
        case 'n':
            // 获取组网状态
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "\n=== 组网状态 ===\n");
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "组网中无人机数量: %d/6\n", networkCount);
            if (networkCount > 0) {
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "组网中的无人机: ");
                for (int i = 0; i < networkCount; i++) {
                    snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                             "%d ", networkDrones[i]);
                }
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer), "\n");
            }
            if (maliciousDroneId > 0) {
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "恶意无人机ID: %d\n", maliciousDroneId);
            }
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "组网状态: %s\n", networkPoisoned ? "已投毒" : (networkCrashed ? "已坠毁" : "正常"));
            if (networkCrashed) {
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "坠毁的无人机: ");
                for (int i = 0; i < networkCount; i++) {
                    snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                             "%d ", networkDrones[i]);
                }
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer), "\n");
            }
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "================\n");
            break;
            
        case 'x':
            // 触发攻击演示（模拟恶意无人机到达并投毒）
            if (sscanf(input, "x %d", &id) == 1) {
                // 模拟恶意无人机到达
                detectMaliciousDrone(id, buffer, BUFFER_SIZE);
                maliciousDroneId = id;
                // 如果组网中已有无人机，立即投毒
                if (networkCount > 0 && !networkPoisoned) {
                    poisonNetwork(buffer, BUFFER_SIZE);
                }
            } else {
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "格式：x 恶意无人机编号\n");
            }
            break;
            
            default:
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "错误：未知操作'%c'！支持操作：r(注册) a(到达) d(离开) m(管理员) i(信息) n(组网状态) x(攻击演示) delete(删除)\n", action);
        }
    }
    
    return buffer;
}

// ==================== JSON数据持久化 ====================
void saveDroneDataToJSON() {
    printf("保存JSON文件: %s\n", JSON_DATA_FILE);
    
    FILE* fp = fopen(JSON_DATA_FILE, "w");
    if (fp == NULL) {
        printf("无法创建JSON数据文件: %s\n", JSON_DATA_FILE);
        printf("请检查data目录是否存在\n");
        return;
    }

    fprintf(fp, "{\n");
    fprintf(fp, "  \"registrations\": [\n");
    
    for (int i = 0; i < totalDrones; i++) {
        fprintf(fp, "    {\n");
        fprintf(fp, "      \"id\": %d,\n", allDrones[i].id);
        fprintf(fp, "      \"type\": \"%c\",\n", allDrones[i].type);
        fprintf(fp, "      \"id_hash\": \"%s\",\n", allDrones[i].security.id_hash);
        fprintf(fp, "      \"type_hash\": \"%s\",\n", allDrones[i].security.type_hash);
        fprintf(fp, "      \"key_hash\": \"%s\",\n", allDrones[i].security.key_hash);
        fprintf(fp, "      \"raw_id\": \"%s\",\n", allDrones[i].security.raw_id);
        fprintf(fp, "      \"raw_type\": \"%c\",\n", allDrones[i].security.raw_type);
        fprintf(fp, "      \"raw_key\": \"%s\",\n", allDrones[i].security.raw_key);
        fprintf(fp, "      \"trust_level\": %d,\n", allDrones[i].security.trust_level);
        fprintf(fp, "      \"last_update\": %ld\n", (long)allDrones[i].last_update);
        
        if (i < totalDrones - 1) {
            fprintf(fp, "    },\n");
        } else {
            fprintf(fp, "    }\n");
        }
    }
    
    fprintf(fp, "  ],\n");
    fprintf(fp, "  \"total_count\": %d,\n", totalDrones);
    fprintf(fp, "  \"save_time\": %ld\n", (long)time(NULL));
    fprintf(fp, "}\n");

    fclose(fp);
    printf("注册信息已保存到JSON文件: %s\n", JSON_DATA_FILE);
}

void loadDroneDataFromJSON() {
    printf("加载JSON文件: %s\n", JSON_DATA_FILE);
    
    FILE* fp = fopen(JSON_DATA_FILE, "r");
    if (fp == NULL) {
        printf("未找到JSON数据文件: %s\n", JSON_DATA_FILE);
        return;
    }

    // 改进的JSON解析
    char line[1024];
    int inRegistrations = 0;
    int droneCount = 0;
    int inDrone = 0;
    Drone currentDrone;
    memset(&currentDrone, 0, sizeof(Drone));
    
    while (fgets(line, sizeof(line), fp) != NULL) {
        // 移除换行符和空格
        line[strcspn(line, "\n")] = '\0';
        // 移除前导空格
        char* trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        
        if (strstr(trimmed, "\"registrations\"")) {
            inRegistrations = 1;
            continue;
        }
        
        if (inRegistrations && strstr(trimmed, "{")) {
            inDrone = 1;
            memset(&currentDrone, 0, sizeof(Drone));
            continue;
        }
        
        if (inDrone && strstr(trimmed, "}")) {
            // 完成一个无人机的解析
            if (droneCount < MAX_DRONES) {
                allDrones[droneCount] = currentDrone;
                droneCount++;
            }
            inDrone = 0;
            continue;
        }
        
        if (inDrone) {
            // 解析无人机字段
            if (strstr(trimmed, "\"id\"")) {
                sscanf(trimmed, "\"id\": %d,", &currentDrone.id);
            } else if (strstr(trimmed, "\"type\"")) {
                char type;
                sscanf(trimmed, "\"type\": \"%c\",", &type);
                currentDrone.type = type;
            } else if (strstr(trimmed, "\"id_hash\"")) {
                sscanf(trimmed, "\"id_hash\": \"%64[^\"]\",", currentDrone.security.id_hash);
            } else if (strstr(trimmed, "\"type_hash\"")) {
                sscanf(trimmed, "\"type_hash\": \"%64[^\"]\",", currentDrone.security.type_hash);
            } else if (strstr(trimmed, "\"key_hash\"")) {
                sscanf(trimmed, "\"key_hash\": \"%64[^\"]\",", currentDrone.security.key_hash);
            } else if (strstr(trimmed, "\"raw_id\"")) {
                sscanf(trimmed, "\"raw_id\": \"%31[^\"]\",", currentDrone.security.raw_id);
            } else if (strstr(trimmed, "\"raw_type\"")) {
                char raw_type;
                sscanf(trimmed, "\"raw_type\": \"%c\",", &raw_type);
                currentDrone.security.raw_type = raw_type;
            } else if (strstr(trimmed, "\"raw_key\"")) {
                sscanf(trimmed, "\"raw_key\": \"%127[^\"]\",", currentDrone.security.raw_key);
            } else if (strstr(trimmed, "\"trust_level\"")) {
                sscanf(trimmed, "\"trust_level\": %d,", &currentDrone.security.trust_level);
            } else if (strstr(trimmed, "\"last_update\"")) {
                long update_time;
                sscanf(trimmed, "\"last_update\": %ld", &update_time);
                currentDrone.last_update = (time_t)update_time;
            }
        }
        
        if (strstr(trimmed, "]")) {
            inRegistrations = 0;
        }
    }
    
    totalDrones = droneCount;
    fclose(fp);
    printf("从JSON文件加载了 %d 架无人机注册信息\n", totalDrones);
}