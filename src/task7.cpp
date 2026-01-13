#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

#define MAX_DRONES 100
#define CHARGING_STATION_SIZE 2
#define MAX_TIME 1000  // 假设最大模拟时间
#define BUFFER_SIZE 4096  // 用于输出的缓冲区大小

// 无人机结构
typedef struct {
    int id;
    char type;
    int arrival_time;
    float current_power;
    float needed_time;
    int start_time;
    int finish_time;
    int completed;
} Drone;

// 快照：记录某一时刻的状态
typedef struct {
    // 充电站中的无人机ID（-1 表示空）
    int station_ids[CHARGING_STATION_SIZE];
    float station_power[CHARGING_STATION_SIZE]; // 当前电量
    // 便道中的无人机ID列表
    int waiting_ids[MAX_DRONES];
    int waiting_count;
} TimeSnapshot;

// 全局数据
Drone all_drones[MAX_DRONES];
int drone_count = 0;
TimeSnapshot snapshots[MAX_TIME]; // 保存 0 ~ MAX_TIME-1 的状态
int total_completion_time = 0;

// 辅助函数
float get_charge_rate(char type) {
    switch (type) {
        case 'A': return 1.0f;
        case 'B': return 1.5f;
        case 'C': return 3.0f;
        default: return 1.0f;
    }
}

// 初始化快照
void init_snapshot(TimeSnapshot* s) {
    for (int i = 0; i < CHARGING_STATION_SIZE; i++) {
        s->station_ids[i] = -1;
        s->station_power[i] = 0;
    }
    s->waiting_count = 0;
    for (int i = 0; i < MAX_DRONES; i++) {
        s->waiting_ids[i] = -1;
    }
}

// 高响应比优先（HRRN）模拟主函数
void simulate_hrrn() {
    // 初始化所有无人机状态
    for (int i = 0; i < drone_count; i++) {
        all_drones[i].start_time = -1;
        all_drones[i].finish_time = -1;
        all_drones[i].completed = 0;
    }

    // 系统状态
    int station_ids[CHARGING_STATION_SIZE];
    int station_count = 0;
    int waiting_ids[MAX_DRONES];
    int waiting_count = 0;

    // 初始化系统状态
    for (int i = 0; i < CHARGING_STATION_SIZE; i++) station_ids[i] = -1;
    waiting_count = 0;

    int current_time = 0;

    // 辅助：记录每个无人机是否已在系统中（已到达）
    int in_system[MAX_DRONES] = {0};

    while (current_time < MAX_TIME) {
        // 1. 检查是否有无人机完成充电（先检查完成，因为完成是在时刻开始时检查的）
        for (int i = 0; i < station_count; i++) {
            int id = station_ids[i];
            int idx = -1;
            for (int j = 0; j < drone_count; j++) {
                if (all_drones[j].id == id) { idx = j; break; }
            }
            if (idx != -1 && !all_drones[idx].completed && all_drones[idx].start_time != -1) {
                int charged_time = current_time - all_drones[idx].start_time;
                if (charged_time >= all_drones[idx].needed_time) {
                    all_drones[idx].completed = 1;
                    all_drones[idx].finish_time = current_time;
                    // 移除该无人机
                    for (int k = i; k < station_count - 1; k++) {
                        station_ids[k] = station_ids[k + 1];
                    }
                    station_count--;
                    i--; // 重新检查当前位置
                }
            }
        }

        // 3. 新到达的无人机先加入队列（保持HRRN公平性）
        for (int i = 0; i < drone_count; i++) {
            if (!in_system[i] && all_drones[i].arrival_time == current_time) {
                in_system[i] = 1;
                // 所有新到达的无人机先加入队列，统一在后面按HRRN处理
                waiting_ids[waiting_count++] = all_drones[i].id;
            }
        }

        // 4. 如果充电站有空位，从便道选择最高响应比的进入（HRRN）
        // 这一步要确保队列中的无人机和新到达的无人机一起按响应比竞争
        while (station_count < CHARGING_STATION_SIZE && waiting_count > 0) {
            // 计算每个等待无人机的响应比
            int best_index = 0;
            float best_ratio = -1.0f;
            int earliest_arrival = MAX_TIME;
            int earliest_id = MAX_DRONES;
            for (int i = 0; i < waiting_count; i++) {
                int id = waiting_ids[i];
                int idx = -1;
                for (int j = 0; j < drone_count; j++) {
                    if (all_drones[j].id == id) { idx = j; break; }
                }
                if (idx == -1) continue;
                int waiting_time = current_time - all_drones[idx].arrival_time;
                if (waiting_time < 0) waiting_time = 0;
                float ratio = (waiting_time + all_drones[idx].needed_time) / all_drones[idx].needed_time;
                // 选择响应比最高的，如果响应比相同（考虑浮点误差），选择到达时间最早的，再相同选择ID最小的
                const float EPSILON = 0.0001f;
                int should_update = 0;
                if (best_ratio < 0 || ratio > best_ratio + EPSILON) {
                    // 响应比明显更高，直接更新
                    should_update = 1;
                } else if (ratio > best_ratio - EPSILON && ratio < best_ratio + EPSILON) {
                    // 响应比相等（考虑浮点误差），比较到达时间和ID
                    if (all_drones[idx].arrival_time < earliest_arrival ||
                        (all_drones[idx].arrival_time == earliest_arrival && id < earliest_id)) {
                        should_update = 1;
                    }
                }
                if (should_update) {
                    best_ratio = ratio;
                    earliest_arrival = all_drones[idx].arrival_time;
                    earliest_id = id;
                    best_index = i;
                }
            }

            // 选中的无人机进入充电站
            int selected_id = waiting_ids[best_index];
            int drone_idx = -1;
            for (int j = 0; j < drone_count; j++) {
                if (all_drones[j].id == selected_id) { drone_idx = j; break; }
            }
            if (drone_idx != -1) {
                station_ids[station_count++] = selected_id;
                all_drones[drone_idx].start_time = current_time;
                // 从便道移除
                for (int k = best_index; k < waiting_count - 1; k++) {
                    waiting_ids[k] = waiting_ids[k + 1];
                }
                waiting_count--;
            }
        }

        // 5. 记录当前时刻结束时的快照（在事件处理后记录）
        TimeSnapshot* snap = &snapshots[current_time];
        init_snapshot(snap);
        for (int i = 0; i < station_count; i++) {
            snap->station_ids[i] = station_ids[i];
            // 计算当前电量（时刻结束时的电量）
            int idx = -1;
            for (int j = 0; j < drone_count; j++) {
                if (all_drones[j].id == station_ids[i]) {
                    idx = j; break;
                }
            }
            if (idx != -1 && all_drones[idx].start_time != -1) {
                // charged_time = current_time - start_time
                // 如果start_time = current_time（刚刚开始），charged_time = 0
                // 如果start_time < current_time，charged_time = 充电时间
                int charged_time = current_time - all_drones[idx].start_time;
                float current = all_drones[idx].current_power + 
                                get_charge_rate(all_drones[idx].type) * charged_time;
                if (current > 10.0) current = 10.0;
                snap->station_power[i] = current;
            } else {
                snap->station_power[i] = 0;
            }
        }
        for (int i = 0; i < waiting_count; i++) {
            snap->waiting_ids[i] = waiting_ids[i];
        }
        snap->waiting_count = waiting_count;

        // 6. 检查是否全部完成
        int all_done = 1;
        for (int i = 0; i < drone_count; i++) {
            if (!all_drones[i].completed) {
                all_done = 0;
                break;
            }
        }
        if (all_done) {
            total_completion_time = current_time;
            break;
        }

        current_time++;
    }

    // 补全剩余时刻的快照（如果提前结束）
    for (int t = current_time; t < MAX_TIME; t++) {
        init_snapshot(&snapshots[t]);
    }
}

// 先来先服务（FCFS）模拟主函数
void simulate_fcfs() {
    // 初始化所有无人机状态
    for (int i = 0; i < drone_count; i++) {
        all_drones[i].start_time = -1;
        all_drones[i].finish_time = -1;
        all_drones[i].completed = 0;
    }

    // 系统状态
    int station_ids[CHARGING_STATION_SIZE];
    int station_count = 0;
    int waiting_ids[MAX_DRONES];
    int waiting_count = 0;

    // 初始化系统状态
    for (int i = 0; i < CHARGING_STATION_SIZE; i++) station_ids[i] = -1;
    waiting_count = 0;

    int current_time = 0;

    // 辅助：记录每个无人机是否已在系统中（已到达）
    int in_system[MAX_DRONES] = {0};

    while (current_time < MAX_TIME) {
        // 1. 检查是否有无人机完成充电（先检查完成，因为完成是在时刻开始时检查的）
        for (int i = 0; i < station_count; i++) {
            int id = station_ids[i];
            int idx = -1;
            for (int j = 0; j < drone_count; j++) {
                if (all_drones[j].id == id) { idx = j; break; }
            }
            if (idx != -1 && !all_drones[idx].completed && all_drones[idx].start_time != -1) {
                int charged_time = current_time - all_drones[idx].start_time;
                if (charged_time >= all_drones[idx].needed_time) {
                    all_drones[idx].completed = 1;
                    all_drones[idx].finish_time = current_time;
                    // 移除该无人机
                    for (int k = i; k < station_count - 1; k++) {
                        station_ids[k] = station_ids[k + 1];
                    }
                    station_count--;
                    i--; // 重新检查当前位置
                }
            }
        }

        // 3. 新到达的无人机先加入队列（保持FCFS顺序）
        for (int i = 0; i < drone_count; i++) {
            if (!in_system[i] && all_drones[i].arrival_time == current_time) {
                in_system[i] = 1;
                // 所有新到达的无人机先加入队列，统一在后面按FCFS处理
                waiting_ids[waiting_count++] = all_drones[i].id;
            }
        }

        // 4. 如果充电站有空位，从便道选择最早到达的进入（FCFS）
        // 这一步要确保队列中的无人机优先于新到达的无人机
        while (station_count < CHARGING_STATION_SIZE && waiting_count > 0) {
            // FCFS: 从队列中选择到达时间最早的无人机
            // 如果多个无人机同时到达，选择ID最小的
            int best_index = 0;
            int earliest_arrival = MAX_TIME;
            int earliest_id = MAX_DRONES;
            for (int i = 0; i < waiting_count; i++) {
                int id = waiting_ids[i];
                int idx = -1;
                for (int j = 0; j < drone_count; j++) {
                    if (all_drones[j].id == id) { idx = j; break; }
                }
                if (idx != -1) {
                    if (all_drones[idx].arrival_time < earliest_arrival ||
                        (all_drones[idx].arrival_time == earliest_arrival && id < earliest_id)) {
                        earliest_arrival = all_drones[idx].arrival_time;
                        earliest_id = id;
                        best_index = i;
                    }
                }
            }
            
            int selected_id = waiting_ids[best_index];
            int drone_idx = -1;
            for (int j = 0; j < drone_count; j++) {
                if (all_drones[j].id == selected_id) { drone_idx = j; break; }
            }
            if (drone_idx != -1) {
                station_ids[station_count++] = selected_id;
                all_drones[drone_idx].start_time = current_time;
                // 从便道移除
                for (int k = best_index; k < waiting_count - 1; k++) {
                    waiting_ids[k] = waiting_ids[k + 1];
                }
                waiting_count--;
            }
        }

        // 5. 记录当前时刻结束时的快照（在事件处理后记录）
        TimeSnapshot* snap = &snapshots[current_time];
        init_snapshot(snap);
        for (int i = 0; i < station_count; i++) {
            snap->station_ids[i] = station_ids[i];
            // 计算当前电量（时刻结束时的电量）
            int idx = -1;
            for (int j = 0; j < drone_count; j++) {
                if (all_drones[j].id == station_ids[i]) {
                    idx = j; break;
                }
            }
            if (idx != -1 && all_drones[idx].start_time != -1) {
                int charged_time = current_time - all_drones[idx].start_time;
                float current = all_drones[idx].current_power + 
                                get_charge_rate(all_drones[idx].type) * charged_time;
                if (current > 10.0) current = 10.0;
                snap->station_power[i] = current;
            } else {
                snap->station_power[i] = 0;
            }
        }
        for (int i = 0; i < waiting_count; i++) {
            snap->waiting_ids[i] = waiting_ids[i];
        }
        snap->waiting_count = waiting_count;

        // 6. 检查是否全部完成
        int all_done = 1;
        for (int i = 0; i < drone_count; i++) {
            if (!all_drones[i].completed) {
                all_done = 0;
                break;
            }
        }
        if (all_done) {
            total_completion_time = current_time;
            break;
        }

        current_time++;
    }

    // 补全剩余时刻的快照（如果提前结束）
    for (int t = current_time; t < MAX_TIME; t++) {
        init_snapshot(&snapshots[t]);
    }
}

// 短作业优先（SJF）模拟主函数
void simulate_sjf() {
    // 初始化所有无人机状态
    for (int i = 0; i < drone_count; i++) {
        all_drones[i].start_time = -1;
        all_drones[i].finish_time = -1;
        all_drones[i].completed = 0;
    }

    // 系统状态
    int station_ids[CHARGING_STATION_SIZE];
    int station_count = 0;
    int waiting_ids[MAX_DRONES];
    int waiting_count = 0;

    // 初始化系统状态
    for (int i = 0; i < CHARGING_STATION_SIZE; i++) station_ids[i] = -1;
    waiting_count = 0;

    int current_time = 0;

    // 辅助：记录每个无人机是否已在系统中（已到达）
    int in_system[MAX_DRONES] = {0};

    while (current_time < MAX_TIME) {
        // 1. 检查是否有无人机完成充电（先检查完成，因为完成是在时刻开始时检查的）
        for (int i = 0; i < station_count; i++) {
            int id = station_ids[i];
            int idx = -1;
            for (int j = 0; j < drone_count; j++) {
                if (all_drones[j].id == id) { idx = j; break; }
            }
            if (idx != -1 && !all_drones[idx].completed && all_drones[idx].start_time != -1) {
                int charged_time = current_time - all_drones[idx].start_time;
                if (charged_time >= all_drones[idx].needed_time) {
                    all_drones[idx].completed = 1;
                    all_drones[idx].finish_time = current_time;
                    // 移除该无人机
                    for (int k = i; k < station_count - 1; k++) {
                        station_ids[k] = station_ids[k + 1];
                    }
                    station_count--;
                    i--; // 重新检查当前位置
                }
            }
        }

        // 3. 新到达的无人机先加入队列（保持SJF公平性）
        for (int i = 0; i < drone_count; i++) {
            if (!in_system[i] && all_drones[i].arrival_time == current_time) {
                in_system[i] = 1;
                // 所有新到达的无人机先加入队列，统一在后面按SJF处理
                waiting_ids[waiting_count++] = all_drones[i].id;
            }
        }

        // 4. 如果充电站有空位，从便道选择需充电时间最短的进入（SJF）
        // 这一步要确保队列中的无人机和新到达的无人机一起按需充电时间竞争
        while (station_count < CHARGING_STATION_SIZE && waiting_count > 0) {
            // SJF: 选择需充电时间最短的无人机
            // 如果需充电时间相同，选择到达时间最早的，再相同选择ID最小的
            int best_index = 0;
            float shortest_time = -1.0f;
            int earliest_arrival = MAX_TIME;
            int earliest_id = MAX_DRONES;
            for (int i = 0; i < waiting_count; i++) {
                int id = waiting_ids[i];
                int idx = -1;
                for (int j = 0; j < drone_count; j++) {
                    if (all_drones[j].id == id) { idx = j; break; }
                }
                if (idx == -1) continue;
                const float EPSILON = 0.0001f;
                int should_update = 0;
                if (shortest_time < 0 || all_drones[idx].needed_time < shortest_time - EPSILON) {
                    // 需充电时间明显更短，直接更新
                    should_update = 1;
                } else if (all_drones[idx].needed_time > shortest_time - EPSILON && 
                          all_drones[idx].needed_time < shortest_time + EPSILON) {
                    // 需充电时间相等（考虑浮点误差），比较到达时间和ID
                    if (all_drones[idx].arrival_time < earliest_arrival ||
                        (all_drones[idx].arrival_time == earliest_arrival && id < earliest_id)) {
                        should_update = 1;
                    }
                }
                if (should_update) {
                    shortest_time = all_drones[idx].needed_time;
                    earliest_arrival = all_drones[idx].arrival_time;
                    earliest_id = id;
                    best_index = i;
                }
            }

            // 选中的无人机进入充电站
            int selected_id = waiting_ids[best_index];
            int drone_idx = -1;
            for (int j = 0; j < drone_count; j++) {
                if (all_drones[j].id == selected_id) { drone_idx = j; break; }
            }
            if (drone_idx != -1) {
                station_ids[station_count++] = selected_id;
                all_drones[drone_idx].start_time = current_time;
                // 从便道移除
                for (int k = best_index; k < waiting_count - 1; k++) {
                    waiting_ids[k] = waiting_ids[k + 1];
                }
                waiting_count--;
            }
        }

        // 5. 记录当前时刻结束时的快照（在事件处理后记录）
        TimeSnapshot* snap = &snapshots[current_time];
        init_snapshot(snap);
        for (int i = 0; i < station_count; i++) {
            snap->station_ids[i] = station_ids[i];
            // 计算当前电量（时刻结束时的电量）
            int idx = -1;
            for (int j = 0; j < drone_count; j++) {
                if (all_drones[j].id == station_ids[i]) {
                    idx = j; break;
                }
            }
            if (idx != -1 && all_drones[idx].start_time != -1) {
                int charged_time = current_time - all_drones[idx].start_time;
                float current = all_drones[idx].current_power + 
                                get_charge_rate(all_drones[idx].type) * charged_time;
                if (current > 10.0) current = 10.0;
                snap->station_power[i] = current;
            } else {
                snap->station_power[i] = 0;
            }
        }
        for (int i = 0; i < waiting_count; i++) {
            snap->waiting_ids[i] = waiting_ids[i];
        }
        snap->waiting_count = waiting_count;

        // 6. 检查是否全部完成
        int all_done = 1;
        for (int i = 0; i < drone_count; i++) {
            if (!all_drones[i].completed) {
                all_done = 0;
                break;
            }
        }
        if (all_done) {
            total_completion_time = current_time;
            break;
        }

        current_time++;
    }

    // 补全剩余时刻的快照（如果提前结束）
    for (int t = current_time; t < MAX_TIME; t++) {
        init_snapshot(&snapshots[t]);
    }
}

void print_status_at_time(int t) {
    if (t < 0 || t > total_completion_time) {
        printf("无效时间！请输入 0 到 %d 之间的整数。\n", total_completion_time);
        return;
    }

    TimeSnapshot* snap = &snapshots[t];
    printf("\n--- 时间 %d 的状态 ---\n", t);
    printf("充电站（容量%d）:\n", CHARGING_STATION_SIZE);
    int has_station = 0;
    for (int i = 0; i < CHARGING_STATION_SIZE; i++) {
        if (snap->station_ids[i] != -1) {
            printf("  位置%d: 无人机%d (电量%.1f)\n", i + 1, snap->station_ids[i], snap->station_power[i]);
            has_station = 1;
        }
    }
    if (!has_station) {
        printf("  空\n");
    }

    printf("便道:\n");
    if (snap->waiting_count == 0) {
        printf("  空\n");
    } else {
        for (int i = 0; i < snap->waiting_count; i++) {
            printf("  位置%d: 无人机%d\n", i + 1, snap->waiting_ids[i]);
        }
    }
    printf("-------------------\n");
}

void print_final_statistics() {
    printf("\n========== 充电完成统计 ==========\n");
    float total_turnaround = 0, total_waiting = 0;

    for (int i = 0; i < drone_count; i++) {
        Drone* d = &all_drones[i];
        int turnaround = d->finish_time - d->arrival_time;
        int waiting_time = d->start_time - d->arrival_time;

        printf("无人机%d:\n", d->id);
        printf("  到达时间: %d\n", d->arrival_time);
        printf("  开始充电: %d\n", d->start_time);
        printf("  完成时间: %d\n", d->finish_time);
        printf("  周转时间: %d\n", turnaround);
        printf("  等待时间: %d\n", waiting_time);
        printf("  需充电时间: %.1f\n", d->needed_time);

        total_turnaround += turnaround;
        total_waiting += waiting_time;
    }

    printf("\n平均周转时间: %.2f\n", total_turnaround / drone_count);
    printf("平均等待时间: %.2f\n", total_waiting / drone_count);
    printf("总执行时间（完成所有）: %d\n", total_completion_time);
    printf("==================================\n");
}

// DLL接口函数：simulateChargingStation
extern "C" const char* simulateChargingStation(const char* input) {
    // 使用静态变量保持状态
    static Drone static_drones[MAX_DRONES];
    static int static_drone_count = 0;
    static TimeSnapshot static_snapshots[MAX_TIME];
    static int static_total_completion_time = 0;
    static int initialized = 0;
    static int simulation_done = 0;
    
    // 第一次调用时初始化
    if (!initialized) {
        static_drone_count = 0;
        static_total_completion_time = 0;
        simulation_done = 0;
        for (int i = 0; i < MAX_TIME; i++) {
            init_snapshot(&static_snapshots[i]);
        }
        initialized = 1;
    }
    
    // 输出缓冲区
    char *buffer = (char*)malloc(BUFFER_SIZE);
    if (!buffer) {
        return "内存分配失败！";
    }
    memset(buffer, 0, BUFFER_SIZE);
    
    snprintf(buffer, BUFFER_SIZE, "正在处理操作：%s\n", input);
    
    // 解析输入
    char action;
    int parsed = sscanf(input, " %c", &action);
    if (parsed != 1) {
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "错误：输入格式不正确！\n");
        return buffer;
    }
    
    if (action == 'i') {
        // 录入无人机：i id type arrival_time current_power
        int id;
        char type;
        int arrival_time;
        float current_power;
        
        parsed = sscanf(input, " %c %d %c %d %f", &action, &id, &type, &arrival_time, &current_power);
        if (parsed != 5) {
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "错误：输入格式不正确！正确格式：i id type arrival_time current_power\n");
            return buffer;
        }
        
        if (static_drone_count >= MAX_DRONES) {
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "错误：无人机数量已达上限！\n");
            return buffer;
        }
        
        // 检查ID是否已存在
        for (int i = 0; i < static_drone_count; i++) {
            if (static_drones[i].id == id) {
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "错误：无人机%d已存在！\n", id);
                return buffer;
            }
        }
        
        // 添加无人机
        int idx = static_drone_count++;
        static_drones[idx].id = id;
        static_drones[idx].type = type;
        static_drones[idx].arrival_time = arrival_time;
        static_drones[idx].current_power = (current_power < 0) ? 0 : (current_power > 10 ? 10 : current_power);
        float needed = 10.0f - static_drones[idx].current_power;
        static_drones[idx].needed_time = needed / get_charge_rate(type);
        if (static_drones[idx].needed_time < 0) static_drones[idx].needed_time = 0;
        static_drones[idx].start_time = -1;
        static_drones[idx].finish_time = -1;
        static_drones[idx].completed = 0;
        
        simulation_done = 0; // 录入新无人机后需要重新调度
        
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "无人机%d已录入（类型:%c，到达时间:%d，当前电量:%.1f，需充电时间:%.1f）\n",
                 id, type, arrival_time, static_drones[idx].current_power, static_drones[idx].needed_time);
        return buffer;
        
    } else if (action == 's') {
        // 开始调度（高响应比优先算法）
        if (static_drone_count == 0) {
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "错误：没有录入任何无人机！\n");
            return buffer;
        }
        
        // 使用全局变量进行模拟（临时复制到全局变量）
        drone_count = static_drone_count;
        for (int i = 0; i < drone_count; i++) {
            all_drones[i] = static_drones[i];
        }
        
        // 执行高响应比优先算法
        simulate_hrrn();
        
        // 将结果复制回静态变量
        for (int i = 0; i < drone_count; i++) {
            static_drones[i] = all_drones[i];
        }
        for (int i = 0; i < MAX_TIME; i++) {
            static_snapshots[i] = snapshots[i];
        }
        static_total_completion_time = total_completion_time;
        simulation_done = 1;
        
        // 构建统计信息到buffer
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "\n========== 充电完成统计（高响应比优先算法）==========\n");
        float total_turnaround = 0, total_waiting = 0, total_weighted_turnaround = 0;
        for (int i = 0; i < drone_count; i++) {
            Drone* d = &static_drones[i];
            int turnaround = d->finish_time - d->arrival_time;
            int waiting_time = d->start_time - d->arrival_time;
            float weighted_turnaround = (float)turnaround / d->needed_time;
            
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "无人机%d:\n", d->id);
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "  到达时间: %d\n", d->arrival_time);
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "  开始充电: %d\n", d->start_time);
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "  完成时间: %d\n", d->finish_time);
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "  周转时间: %d\n", turnaround);
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "  等待时间: %d\n", waiting_time);
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "  需充电时间: %.1f\n", d->needed_time);
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "  带权周转时间: %.2f\n", weighted_turnaround);
            
            total_turnaround += turnaround;
            total_waiting += waiting_time;
            total_weighted_turnaround += weighted_turnaround;
        }
        
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "\n平均周转时间: %.2f\n", total_turnaround / drone_count);
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "平均等待时间: %.2f\n", total_waiting / drone_count);
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "平均带权周转时间: %.2f\n", total_weighted_turnaround / drone_count);
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "总执行时间（完成所有）: %d\n", static_total_completion_time);
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "==================================\n");
        return buffer;
        
    } else if (action == 'f') {
        // 开始调度（先来先服务算法）
        if (static_drone_count == 0) {
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "错误：没有录入任何无人机！\n");
            return buffer;
        }
        
        // 使用全局变量进行模拟（临时复制到全局变量）
        drone_count = static_drone_count;
        for (int i = 0; i < drone_count; i++) {
            all_drones[i] = static_drones[i];
        }
        
        // 执行先来先服务算法
        simulate_fcfs();
        
        // 将结果复制回静态变量
        for (int i = 0; i < drone_count; i++) {
            static_drones[i] = all_drones[i];
        }
        for (int i = 0; i < MAX_TIME; i++) {
            static_snapshots[i] = snapshots[i];
        }
        static_total_completion_time = total_completion_time;
        simulation_done = 1;
        
        // 构建统计信息到buffer
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "\n========== 充电完成统计（先来先服务算法）==========\n");
        float total_turnaround = 0, total_waiting = 0, total_weighted_turnaround = 0;
        for (int i = 0; i < drone_count; i++) {
            Drone* d = &static_drones[i];
            int turnaround = d->finish_time - d->arrival_time;
            int waiting_time = d->start_time - d->arrival_time;
            float weighted_turnaround = (float)turnaround / d->needed_time;
            
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "无人机%d:\n", d->id);
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "  到达时间: %d\n", d->arrival_time);
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "  开始充电: %d\n", d->start_time);
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "  完成时间: %d\n", d->finish_time);
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "  周转时间: %d\n", turnaround);
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "  等待时间: %d\n", waiting_time);
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "  需充电时间: %.1f\n", d->needed_time);
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "  带权周转时间: %.2f\n", weighted_turnaround);
            
            total_turnaround += turnaround;
            total_waiting += waiting_time;
            total_weighted_turnaround += weighted_turnaround;
        }
        
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "\n平均周转时间: %.2f\n", total_turnaround / drone_count);
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "平均等待时间: %.2f\n", total_waiting / drone_count);
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "平均带权周转时间: %.2f\n", total_weighted_turnaround / drone_count);
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "总执行时间（完成所有）: %d\n", static_total_completion_time);
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "==================================\n");
        return buffer;
        
    } else if (action == 'j') {
        // 开始调度（短作业优先算法）
        if (static_drone_count == 0) {
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "错误：没有录入任何无人机！\n");
            return buffer;
        }
        
        // 使用全局变量进行模拟（临时复制到全局变量）
        drone_count = static_drone_count;
        for (int i = 0; i < drone_count; i++) {
            all_drones[i] = static_drones[i];
        }
        
        // 执行短作业优先算法
        simulate_sjf();
        
        // 将结果复制回静态变量
        for (int i = 0; i < drone_count; i++) {
            static_drones[i] = all_drones[i];
        }
        for (int i = 0; i < MAX_TIME; i++) {
            static_snapshots[i] = snapshots[i];
        }
        static_total_completion_time = total_completion_time;
        simulation_done = 1;
        
        // 构建统计信息到buffer
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "\n========== 充电完成统计（短作业优先算法）==========\n");
        float total_turnaround = 0, total_waiting = 0, total_weighted_turnaround = 0;
        for (int i = 0; i < drone_count; i++) {
            Drone* d = &static_drones[i];
            int turnaround = d->finish_time - d->arrival_time;
            int waiting_time = d->start_time - d->arrival_time;
            float weighted_turnaround = (float)turnaround / d->needed_time;
            
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "无人机%d:\n", d->id);
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "  到达时间: %d\n", d->arrival_time);
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "  开始充电: %d\n", d->start_time);
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "  完成时间: %d\n", d->finish_time);
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "  周转时间: %d\n", turnaround);
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "  等待时间: %d\n", waiting_time);
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "  需充电时间: %.1f\n", d->needed_time);
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "  带权周转时间: %.2f\n", weighted_turnaround);
            
            total_turnaround += turnaround;
            total_waiting += waiting_time;
            total_weighted_turnaround += weighted_turnaround;
        }
        
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "\n平均周转时间: %.2f\n", total_turnaround / drone_count);
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "平均等待时间: %.2f\n", total_waiting / drone_count);
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "平均带权周转时间: %.2f\n", total_weighted_turnaround / drone_count);
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "总执行时间（完成所有）: %d\n", static_total_completion_time);
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "==================================\n");
        return buffer;
        
    } else if (action == 't') {
        // 查看时刻状态：t time
        int time;
        parsed = sscanf(input, " %c %d", &action, &time);
        if (parsed != 2) {
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "错误：输入格式不正确！正确格式：t time\n");
            return buffer;
        }
        
        if (!simulation_done) {
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "错误：请先执行调度（s/f/j命令）！\n");
            return buffer;
        }
        
        if (time < 0 || time > static_total_completion_time) {
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "无效时间！请输入 0 到 %d 之间的整数。\n", static_total_completion_time);
            return buffer;
        }
        
        // 输出时刻状态
        TimeSnapshot* snap = &static_snapshots[time];
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "\n--- 时间 %d 的状态 ---\n", time);
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "充电站（容量%d）:\n", CHARGING_STATION_SIZE);
        int has_station = 0;
        for (int i = 0; i < CHARGING_STATION_SIZE; i++) {
            if (snap->station_ids[i] != -1) {
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "  位置%d: 无人机%d (电量%.1f)\n", i + 1, snap->station_ids[i], snap->station_power[i]);
                has_station = 1;
            }
        }
        if (!has_station) {
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "  空\n");
        }
        
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "便道:\n");
        if (snap->waiting_count == 0) {
            snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                     "  空\n");
        } else {
            for (int i = 0; i < snap->waiting_count; i++) {
                snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                         "  位置%d: 无人机%d\n", i + 1, snap->waiting_ids[i]);
            }
        }
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "-------------------\n");
        return buffer;
        
    } else if (action == 'r') {
        // 重置系统
        static_drone_count = 0;
        static_total_completion_time = 0;
        simulation_done = 0;
        for (int i = 0; i < MAX_TIME; i++) {
            init_snapshot(&static_snapshots[i]);
        }
        initialized = 0; // 下次调用时重新初始化
        
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "系统已重置\n");
        return buffer;
        
    } else {
        snprintf(buffer + strlen(buffer), BUFFER_SIZE - strlen(buffer),
                 "未知操作！支持的操作：i(录入), s(高响应比优先), f(先来先服务), j(短作业优先), t(查看时刻), r(重置)\n");
        return buffer;
    }
}

int main() {
    printf("=== 高响应比优先（HRRN）无人机充电站模拟系统 ===\n");
    printf("请输入无人机数量（最多%d）: ", MAX_DRONES);
    scanf("%d", &drone_count);
    if (drone_count <= 0 || drone_count > MAX_DRONES) {
        printf("无效数量！\n");
        return 1;
    }

    for (int i = 0; i < drone_count; i++) {
        float power;
        printf("请输入无人机%d的编号、类型(A/B/C)、到达时间、当前电量(0~10): ", i + 1);
        scanf("%d %c %d %f", &all_drones[i].id, &all_drones[i].type,
              &all_drones[i].arrival_time, &power);
        all_drones[i].current_power = (power < 0) ? 0 : (power > 10 ? 10 : power);
        float needed = 10.0f - all_drones[i].current_power;
        all_drones[i].needed_time = needed / get_charge_rate(all_drones[i].type);
        if (all_drones[i].needed_time < 0) all_drones[i].needed_time = 0;
    }

    printf("\n正在模拟充电过程（高响应比优先算法）...\n");
    simulate_hrrn();

    print_final_statistics();

    char choice;
    do {
        printf("\n是否查看某时刻状态？(y/n): ");
        scanf(" %c", &choice);
        if (choice == 'y' || choice == 'Y') {
            int t;
            printf("请输入时刻（0 ~ %d）: ", total_completion_time);
            scanf("%d", &t);
            print_status_at_time(t);
        }
    } while (choice == 'y' || choice == 'Y');

    printf("模拟结束。\n");
    return 0;
}