// 任务7：高响应比优先调度算法（重写版）

// 任务7状态
let task7State = {
    drones: new Map(),
    station: [],
    queue: [],
    statistics: {
        totalExecutionTime: 0,
        avgTurnaroundTime: 0,
        avgWaitTime: 0,
        avgWeightedTurnaroundTime: 0,
        completedDrones: 0
    },
    snapshots: new Map(), // 存储各时刻的快照
    currentTime: 0,
    maxTime: 0
};

// 保存三种算法的结果
let algorithmResults = {
    hrrn: null, // 高响应比优先
    fcfs: null, // 先来先服务
    sjf: null   // 短作业优先
};

// 提交任务7
function submitTask7() {
    const action = document.getElementById('task7-action').value;
    
    if (action === 'i') {
        // 录入无人机 - 发送到后端（格式：i id type arrival_time current_power）
        const id = document.getElementById('task7-id').value;
        const type = document.getElementById('task7-type').value;
        const arrivalTime = document.getElementById('task7-arrival-time').value;
        const currentPower = document.getElementById('task7-current-power').value;
        
        if (!validateTask7Input(id, arrivalTime, currentPower)) return;
        
        // 发送命令格式：i id type arrival_time current_power
        const input = `i ${id} ${type} ${arrivalTime} ${currentPower}`;
        processInput(input, 7);
        
    } else if (action === 's') {
        // 开始调度（高响应比优先算法）
        if (task7State.drones.size === 0) {
            addOutputMessage('错误：没有录入任何无人机！', 'error');
            return;
        }
        
        // 发送命令：s
        const input = 's';
        processInput(input, 7);
        
    } else if (action === 'f') {
        // 开始调度（先来先服务算法）
        if (task7State.drones.size === 0) {
            addOutputMessage('错误：没有录入任何无人机！', 'error');
            return;
        }
        
        // 发送命令：f
        const input = 'f';
        processInput(input, 7);
        
    } else if (action === 'j') {
        // 开始调度（短作业优先算法）
        if (task7State.drones.size === 0) {
            addOutputMessage('错误：没有录入任何无人机！', 'error');
            return;
        }
        
        // 发送命令：j
        const input = 'j';
        processInput(input, 7);
        
    } else if (action === 't') {
        // 查看时刻状态
        const time = document.getElementById('task7-time').value;
        if (!time || parseInt(time) < 0) {
            addOutputMessage('请输入有效的时间！', 'error');
            return;
        }
        
        // 发送命令格式：t time
        const input = `t ${time}`;
        processInput(input, 7);
        
    } else if (action === 'r') {
        // 重置系统
        const input = 'r';
        processInput(input, 7);
    }
}

// 验证任务7输入
function validateTask7Input(id, arrivalTime, currentPower) {
    if (!id || !arrivalTime || currentPower === '') {
        addOutputMessage('请输入完整的参数！', 'error');
        return false;
    }
    
    const idNum = parseInt(id);
    const arrivalNum = parseInt(arrivalTime);
    const powerNum = parseFloat(currentPower);
    
    if (idNum < 1 || arrivalNum < 0 || powerNum < 0 || powerNum > 10) {
        addOutputMessage('参数无效！编号必须大于0，时间不能为负数，电量必须在0-10之间！', 'error');
        return false;
    }
    
    // 不再检查是否已存在，由后端处理
    return true;
}

// 更新任务7的系统状态
function updateTask7State(input, backendText) {
    const parts = input.trim().split(' ');
    const action = parts[0];
    
    if (action === 'i') {
        // 录入无人机
        const id = parseInt(parts[1]);
        const type = normalizeType(parts[2]);
        const arrivalTime = parseInt(parts[3]);
        const currentPower = parseFloat(parts[4]);
        
        // 计算需充电时间
        const chargingRate = getTypeRate(type);
        const neededPower = 10 - currentPower;
        const requiredTime = neededPower <= 0 ? 0 : Math.ceil(neededPower / chargingRate);
        
        const drone = {
            id: id,
            type: type,
            arrivalTime: arrivalTime,
            currentPower: currentPower,
            requiredTime: requiredTime,
            startTime: -1,
            endTime: -1,
            waitTime: 0,
            responseRatio: 0.0,
            inStation: false,
            inQueue: false,
            completed: false
        };
        
        task7State.drones.set(id, drone);
        task7State.currentTime = -1; // 重置为未查看状态
        addOutputMessage(`无人机${id}（${type}型，到达时间${arrivalTime}，当前电量${currentPower}，需充电时间${requiredTime}）已录入`, 'success');
        
    } else if (action === 's') {
        // 开始调度（高响应比优先算法）- 解析调度结果
        parseSchedulingResult(backendText);
        
    } else if (action === 'f') {
        // 开始调度（先来先服务算法）- 解析调度结果
        parseSchedulingResult(backendText);
        
    } else if (action === 'j') {
        // 开始调度（短作业优先算法）- 解析调度结果
        parseSchedulingResult(backendText);
        
    } else if (action === 't') {
        // 查看时刻状态
        const time = parseInt(parts[1]);
        parseTimeState(backendText, time);
        
    } else if (action === 'r') {
        // 重置系统
        task7State.drones.clear();
        task7State.station = [];
        task7State.queue = [];
        task7State.snapshots.clear();
        task7State.statistics = {
            totalExecutionTime: 0,
            avgTurnaroundTime: 0,
            avgWaitTime: 0,
            avgWeightedTurnaroundTime: 0,
            completedDrones: 0
        };
        task7State.currentTime = -1; // 重置为未查看状态
        task7State.maxTime = 0;
        
        // 清空算法结果
        algorithmResults = { hrrn: null, fcfs: null, sjf: null };
        
        addOutputMessage('系统已重置', 'success');
        updateCompareButton();
    }
    
    updateTask7Visualization();
}

// 解析调度结果
function parseSchedulingResult(backendText) {
    const lines = backendText.split('\n');
    
    // 识别算法类型
    let algorithmType = null;
    if (backendText.includes('高响应比优先算法')) {
        algorithmType = 'hrrn';
    } else if (backendText.includes('先来先服务算法')) {
        algorithmType = 'fcfs';
    } else if (backendText.includes('短作业优先算法')) {
        algorithmType = 'sjf';
    }
    
    if (!algorithmType) {
        // 如果无法识别算法类型，使用原有逻辑
        return parseSchedulingResultOld(backendText);
    }
    
    // 解析统计信息
    let inStatsSection = false;
    let currentDrone = null;
    const droneResults = [];
    let statistics = {
        avgTurnaroundTime: 0,
        avgWaitTime: 0,
        avgWeightedTurnaroundTime: 0,
        totalExecutionTime: 0
    };
    
    for (let i = 0; i < lines.length; i++) {
        const line = lines[i];
        
        if (line.includes('========== 充电完成统计') && line.includes('==========')) {
            inStatsSection = true;
            continue;
        }
        
        if (inStatsSection) {
            // 解析无人机信息
            if (line.includes('无人机') && line.includes(':')) {
                // 如果之前有未完成的无人机，先保存它
                if (currentDrone) {
                    droneResults.push(currentDrone);
                }
                
                const droneMatch = line.match(/无人机(\d+):/);
                if (droneMatch) {
                    const droneId = parseInt(droneMatch[1]);
                    currentDrone = {
                        id: droneId,
                        arrival: -1,
                        start: -1,
                        finish: -1,
                        wait: -1,
                        turnaround: -1,
                        service: -1,
                        responseRatio: -1,
                        weightedTurnaround: -1
                    };
                }
            } else if (currentDrone) {
                // 解析无人机各项数据（使用行首锚点，避免误匹配“平均XXX时间”行）
                if (/^\s*到达时间:\s*/.test(line)) {
                    const match = line.match(/^\s*到达时间:\s*(\d+)/);
                    if (match) currentDrone.arrival = parseInt(match[1]);
                } else if (/^\s*开始充电:\s*/.test(line)) {
                    const match = line.match(/^\s*开始充电:\s*(\d+)/);
                    if (match) currentDrone.start = parseInt(match[1]);
                } else if (/^\s*完成时间:\s*/.test(line)) {
                    const match = line.match(/^\s*完成时间:\s*(\d+)/);
                    if (match) currentDrone.finish = parseInt(match[1]);
                } else if (/^\s*等待时间:\s*/.test(line)) {
                    const match = line.match(/^\s*等待时间:\s*([\d.]+)/);
                    if (match) currentDrone.wait = parseFloat(match[1]);
                } else if (/^\s*周转时间:\s*/.test(line)) {
                    const match = line.match(/^\s*周转时间:\s*(\d+)/);
                    if (match) {
                        currentDrone.turnaround = parseInt(match[1]);
                    }
                    // 注意：不要在这里计算，因为可能finish和arrival还没解析完
                } else if (/^\s*需充电时间:\s*/.test(line)) {
                    const match = line.match(/^\s*需充电时间:\s*([\d.]+)/);
                    if (match) currentDrone.service = parseFloat(match[1]);
                } else if (/^\s*带权周转时间:\s*/.test(line)) {
                    const match = line.match(/^\s*带权周转时间:\s*([\d.]+)/);
                    if (match) currentDrone.weightedTurnaround = parseFloat(match[1]);
                }
            }
            
            // 解析统计信息
            if (line.includes('平均周转时间:')) {
                const match = line.match(/平均周转时间:\s*([\d.]+)/);
                if (match) {
                    statistics.avgTurnaroundTime = parseFloat(match[1]);
                    task7State.statistics.avgTurnaroundTime = parseFloat(match[1]);
                }
            } else if (line.includes('平均等待时间:')) {
                const match = line.match(/平均等待时间:\s*([\d.]+)/);
                if (match) {
                    statistics.avgWaitTime = parseFloat(match[1]);
                    task7State.statistics.avgWaitTime = parseFloat(match[1]);
                }
            } else if (line.includes('平均带权周转时间:')) {
                const match = line.match(/平均带权周转时间:\s*([\d.]+)/);
                if (match) {
                    statistics.avgWeightedTurnaroundTime = parseFloat(match[1]);
                    task7State.statistics.avgWeightedTurnaroundTime = parseFloat(match[1]);
                }
            } else if (line.includes('总执行时间（完成所有）:')) {
                const match = line.match(/总执行时间（完成所有）:\s*(\d+)/);
                if (match) {
                    statistics.totalExecutionTime = parseInt(match[1]);
                    task7State.statistics.totalExecutionTime = parseInt(match[1]);
                    task7State.maxTime = parseInt(match[1]);
                }
            }
        }
    }
    
    // 保存最后一个无人机
    if (currentDrone) {
        droneResults.push(currentDrone);
    }
    
    // 计算响应比（仅HRRN算法需要）
    if (algorithmType === 'hrrn') {
        droneResults.forEach(drone => {
            if (drone.wait >= 0 && drone.service > 0) {
                // 响应比 = (等待时间 + 需充电时间) / 需充电时间
                drone.responseRatio = (drone.wait + drone.service) / drone.service;
            }
        });
    }
    
    // 确保所有无人机都有正确的周转时间和带权周转时间
    droneResults.forEach(drone => {
        // 如果周转时间解析失败，通过计算得出
        if (drone.turnaround < 0 && drone.finish >= 0 && drone.arrival >= 0) {
            drone.turnaround = drone.finish - drone.arrival;
        }
        
        // 如果带权周转时间解析失败，通过计算得出
        if (drone.weightedTurnaround < 0 && drone.turnaround >= 0 && drone.service > 0) {
            drone.weightedTurnaround = drone.turnaround / drone.service;
        }
    });
    
    // 保存结果
    algorithmResults[algorithmType] = {
        drones: droneResults,
        statistics: statistics
    };
    
    // 更新任务7状态
    task7State.drones.forEach((drone, id) => {
        const result = droneResults.find(d => d.id === id);
        if (result) {
            drone.completed = true;
        }
    });
    task7State.statistics.completedDrones = droneResults.length;
    
    // 清空当前显示状态
    task7State.station = [];
    task7State.queue = [];
    
    // 更新可视化
    updateTask7Visualization();
    
    // 检查是否有至少一个算法结果，显示对比按钮
    updateCompareButton();
}

// 旧的解析函数（备用）
function parseSchedulingResultOld(backendText) {
    const lines = backendText.split('\n');
    
    // 解析统计信息
    let inStatsSection = false;
    
    for (const line of lines) {
        if (line.includes('========== 充电完成统计') && line.includes('==========')) {
            inStatsSection = true;
            continue;
        }
        
        if (inStatsSection) {
            if (line.includes('平均周转时间:')) {
                const match = line.match(/平均周转时间:\s*([\d.]+)/);
                if (match) {
                    task7State.statistics.avgTurnaroundTime = parseFloat(match[1]);
                }
            } else if (line.includes('平均等待时间:')) {
                const match = line.match(/平均等待时间:\s*([\d.]+)/);
                if (match) {
                    task7State.statistics.avgWaitTime = parseFloat(match[1]);
                }
            } else if (line.includes('平均带权周转时间:')) {
                const match = line.match(/平均带权周转时间:\s*([\d.]+)/);
                if (match) {
                    task7State.statistics.avgWeightedTurnaroundTime = parseFloat(match[1]);
                }
            } else if (line.includes('总执行时间（完成所有）:')) {
                const match = line.match(/总执行时间（完成所有）:\s*(\d+)/);
                if (match) {
                    task7State.statistics.totalExecutionTime = parseInt(match[1]);
                    task7State.maxTime = parseInt(match[1]);
                }
            } else if (line.includes('无人机') && line.includes(':')) {
                // 解析单个无人机统计
                const droneMatch = line.match(/无人机(\d+):/);
                if (droneMatch) {
                    const droneId = parseInt(droneMatch[1]);
                    if (task7State.drones.has(droneId)) {
                        const drone = task7State.drones.get(droneId);
                        drone.completed = true;
                        task7State.statistics.completedDrones++;
                    }
                }
            }
        }
    }
    
    // 更新所有无人机的状态
    task7State.drones.forEach((drone, id) => {
        if (drone.completed) {
            drone.inStation = false;
            drone.inQueue = false;
        }
    });
    
    // 清空当前显示状态
    task7State.station = [];
    task7State.queue = [];
}

// 解析指定时刻的状态
function parseTimeState(backendText, time) {
    const lines = backendText.split('\n');
    let inTimeSection = false;
    let inStationSection = false;
    let inQueueSection = false;
    
    task7State.station = [];
    task7State.queue = [];
    task7State.currentTime = time;
    
    for (const line of lines) {
        if (line.includes(`--- 时间 ${time} 的状态 ---`)) {
            inTimeSection = true;
            continue;
        }
        
        if (inTimeSection && line.includes('充电站（容量')) {
            inStationSection = true;
            inQueueSection = false;
            continue;
        }
        
        if (inStationSection && line.includes('便道:')) {
            inStationSection = false;
            inQueueSection = true;
            continue;
        }
        
        if (inStationSection && line.includes('位置')) {
            // 解析充电站状态
            const match = line.match(/位置(\d+):\s*无人机(\d+)\s*\(电量([\d.]+)\)/);
            if (match) {
                const position = parseInt(match[1]);
                const droneId = parseInt(match[2]);
                const power = parseFloat(match[3]);
                
                task7State.station.push(droneId);
                
                if (task7State.drones.has(droneId)) {
                    const drone = task7State.drones.get(droneId);
                    drone.inStation = true;
                    drone.inQueue = false;
                }
            } else if (line.includes('空')) {
                // 充电站为空
            }
        }
        
        if (inQueueSection && line.includes('位置')) {
            // 解析便道状态
            const match = line.match(/位置(\d+):\s*无人机(\d+)/);
            if (match) {
                const position = parseInt(match[1]);
                const droneId = parseInt(match[2]);
                
                task7State.queue.push(droneId);
                
                if (task7State.drones.has(droneId)) {
                    const drone = task7State.drones.get(droneId);
                    drone.inStation = false;
                    drone.inQueue = true;
                    
                    // 计算响应比
                    const waitingTime = Math.max(0, time - drone.arrivalTime);
                    drone.responseRatio = (waitingTime + drone.requiredTime) / drone.requiredTime;
                }
            } else if (line.includes('空')) {
                // 便道为空
            }
        }
        
        if (inTimeSection && line.includes('-------------------')) {
            break;
        }
    }
}

// 更新任务7的可视化
function updateTask7Visualization() {
    // 更新已录入无人机列表
    updateRegisteredDronesList();
    
    // 只有在查看时刻状态时才显示充电站和便道
    if (task7State.currentTime >= 0) {
        updateStationList('station-list', 'station-count', task7State.station);
        updateQueueList('queue-list', 'queue-count', task7State.queue);
    } else {
        // 清空充电站和便道显示
        clearStationAndQueue();
    }
    
    // 更新统计信息
    updateStatistics();
    
    // 更新当前时间显示
    updateCurrentTimeDisplay();
}

// 更新已录入无人机列表
function updateRegisteredDronesList() {
    const container = document.getElementById('registered-drones-list');
    const countElement = document.getElementById('registered-drones-count');
    
    if (!container || !countElement) return;
    
    const droneCount = task7State.drones.size;
    countElement.textContent = droneCount;
    
    container.style.opacity = '0.7';
    
    setTimeout(() => {
        container.innerHTML = '';
        
        if (droneCount === 0) {
            const emptyElement = document.createElement('div');
            emptyElement.className = 'empty-queue';
            emptyElement.textContent = '暂无录入的无人机';
            container.appendChild(emptyElement);
        } else {
            // 按ID排序显示
            const sortedDrones = Array.from(task7State.drones.values()).sort((a, b) => a.id - b.id);
            sortedDrones.forEach(drone => {
                const droneElement = createRegisteredDroneElement(drone);
                container.appendChild(droneElement);
            });
        }
        
        container.style.opacity = '1';
    }, 150);
}

// 创建已录入无人机元素
function createRegisteredDroneElement(drone) {
    const droneElement = document.createElement('div');
    droneElement.className = 'drone-item';
    droneElement.style.cursor = 'pointer';
    
    droneElement.onclick = () => showTask7DroneDetails(drone, 'registered');
    
    const droneInfo = document.createElement('div');
    droneInfo.className = 'drone-info';
    
    const droneId = document.createElement('div');
    droneId.className = 'drone-id';
    droneId.textContent = `#${drone.id} (${drone.type || 'A'})`;
    
    const droneDetails = document.createElement('div');
    droneDetails.className = 'drone-details';
    droneDetails.textContent = `到达: ${drone.arrivalTime || 0}, 电量: ${(drone.currentPower || 0).toFixed(1)}/10`;
    
    droneInfo.appendChild(droneId);
    droneInfo.appendChild(droneDetails);
    
    const dronePosition = document.createElement('div');
    dronePosition.className = 'drone-position';
    dronePosition.textContent = `需充电: ${drone.requiredTime || 0}h`;
    
    droneElement.appendChild(droneInfo);
    droneElement.appendChild(dronePosition);
    
    return droneElement;
}

// 清空充电站和便道显示
function clearStationAndQueue() {
    const stationContainer = document.getElementById('station-list');
    const queueContainer = document.getElementById('queue-list');
    const stationCount = document.getElementById('station-count');
    const queueCount = document.getElementById('queue-count');
    
    if (stationContainer) {
        stationContainer.innerHTML = `
            <div class="empty-slot">空位（栈顶）</div>
            <div class="empty-slot">空位（栈底）</div>
        `;
    }
    
    if (queueContainer) {
        queueContainer.innerHTML = `
            <div class="empty-queue">暂无排队</div>
        `;
    }
    
    if (stationCount) stationCount.textContent = '0/2';
    if (queueCount) queueCount.textContent = '0';
}

// 更新充电站列表
function updateStationList(listId, countId, droneIds) {
    const container = document.getElementById(listId);
    const countElement = document.getElementById(countId);
    
    if (!container || !countElement) return;
    
    countElement.textContent = `${droneIds.length}/2`;
    
    container.style.opacity = '0.7';
    
    setTimeout(() => {
        container.innerHTML = '';
        
        // 显示充电站中的无人机
        for (let i = 0; i < 2; i++) {
            if (i < droneIds.length) {
                const droneId = droneIds[i];
                const drone = task7State.drones.get(droneId);
                if (drone) {
                    const currentPower = Math.min(10, drone.currentPower + 
                        (task7State.currentTime - drone.startTime) * getTypeRate(drone.type));
                    const droneElement = createTask7DroneElement({
                        id: droneId,
                        type: drone.type,
                        arrivalTime: drone.arrivalTime,
                        currentPower: currentPower,
                        requiredTime: drone.requiredTime,
                        position: i + 1
                    }, 'station');
                    container.appendChild(droneElement);
                }
            } else {
                const emptyElement = document.createElement('div');
                emptyElement.className = 'empty-slot';
                emptyElement.textContent = '空位';
                container.appendChild(emptyElement);
            }
        }
        
        container.style.opacity = '1';
    }, 150);
}

// 更新便道队列
function updateQueueList(listId, countId, droneIds) {
    const container = document.getElementById(listId);
    const countElement = document.getElementById(countId);
    
    if (!container || !countElement) return;
    
    countElement.textContent = droneIds.length;
    
    container.style.opacity = '0.7';
    
    setTimeout(() => {
        container.innerHTML = '';
        
        if (droneIds.length === 0) {
            const emptyElement = document.createElement('div');
            emptyElement.className = 'empty-queue';
            emptyElement.textContent = '暂无排队无人机';
            container.appendChild(emptyElement);
        } else {
            droneIds.forEach((droneId, index) => {
                const drone = task7State.drones.get(droneId);
                if (drone) {
                    const droneElement = createTask7DroneElement({
                        id: droneId,
                        type: drone.type,
                        arrivalTime: drone.arrivalTime,
                        currentPower: drone.currentPower,
                        requiredTime: drone.requiredTime,
                        responseRatio: drone.responseRatio || 0,
                        position: index + 1
                    }, 'queue');
                    container.appendChild(droneElement);
                }
            });
        }
        
        container.style.opacity = '1';
    }, 150);
}

// 创建任务7无人机元素
function createTask7DroneElement(drone, location) {
    const droneElement = document.createElement('div');
    droneElement.className = 'drone-item';
    droneElement.style.cursor = 'pointer';
    
    droneElement.onclick = () => showTask7DroneDetails(drone, location);
    
    const droneInfo = document.createElement('div');
    droneInfo.className = 'drone-info';
    
    const droneId = document.createElement('div');
    droneId.className = 'drone-id';
    droneId.textContent = `#${drone.id} (${drone.type || 'A'})`;
    
    const droneDetails = document.createElement('div');
    droneDetails.className = 'drone-details';
    
    if (location === 'station') {
        droneDetails.textContent = `电量: ${(drone.currentPower || 0).toFixed(1)}/10`;
    } else if (location === 'queue') {
        droneDetails.textContent = `响应比: ${(drone.responseRatio || 0).toFixed(2)}`;
    } else {
        droneDetails.textContent = `到达: ${drone.arrivalTime || 0}`;
    }
    
    droneInfo.appendChild(droneId);
    droneInfo.appendChild(droneDetails);
    
    const dronePosition = document.createElement('div');
    dronePosition.className = 'drone-position';
    dronePosition.textContent = `位置 ${drone.position !== undefined ? drone.position : '?'}`;
    
    droneElement.appendChild(droneInfo);
    droneElement.appendChild(dronePosition);
    
    return droneElement;
}

// 显示任务7无人机详细信息
function showTask7DroneDetails(drone, location) {
    const modal = document.getElementById('drone-detail-modal');
    const content = document.getElementById('drone-detail-content');
    
    const stored = task7State.drones.get(drone.id) || drone;
    
    let locationName = '未知位置';
    if (location === 'station') {
        locationName = '充电站';
    } else if (location === 'queue') {
        locationName = '便道队列';
    }
    
    const typeRate = getTypeRate(stored.type || drone.type || 'A');
    const typeLabel = normalizeType(stored.type || drone.type) + '类';
    
    content.innerHTML = `
        <div class="detail-grid">
            <div class="detail-item">
                <div class="detail-label">无人机编号</div>
                <div class="detail-value highlight">#${drone.id}</div>
            </div>
            <div class="detail-item">
                <div class="detail-label">无人机类型</div>
                <div class="detail-value">${typeLabel}</div>
            </div>
            <div class="detail-item">
                <div class="detail-label">当前位置</div>
                <div class="detail-value">${locationName}</div>
            </div>
            <div class="detail-item">
                <div class="detail-label">到达时间</div>
                <div class="detail-value">${stored.arrivalTime || drone.arrivalTime || 0}</div>
            </div>
            <div class="detail-item">
                <div class="detail-label">当前电量</div>
                <div class="detail-value">${(stored.currentPower || drone.currentPower || 0).toFixed(1)}/10</div>
            </div>
            <div class="detail-item">
                <div class="detail-label">需充电时间</div>
                <div class="detail-value">${stored.requiredTime || drone.requiredTime || 0}h</div>
            </div>
            <div class="detail-item">
                <div class="detail-label">充电倍率</div>
                <div class="detail-value">${typeRate}x</div>
            </div>
            <div class="detail-item">
                <div class="detail-label">响应比</div>
                <div class="detail-value">${(stored.responseRatio || drone.responseRatio || 0).toFixed(2)}</div>
            </div>
            <div class="detail-item">
                <div class="detail-label">状态</div>
                <div class="detail-value">${stored.completed ? '已完成' : (stored.inStation ? '充电中' : stored.inQueue ? '等待中' : '未开始')}</div>
            </div>
        </div>
    `;
    
    modal.style.display = 'block';
}

// 更新统计信息
function updateStatistics() {
    const stats = task7State.statistics;
    
    const totalTimeEl = document.getElementById('total-execution-time');
    if (totalTimeEl) {
        totalTimeEl.textContent = stats.totalExecutionTime || 0;
    }
    
    const avgTurnaroundEl = document.getElementById('avg-turnaround-time');
    if (avgTurnaroundEl) {
        avgTurnaroundEl.textContent = (stats.avgTurnaroundTime || 0).toFixed(2);
    }
    
    const avgWaitEl = document.getElementById('avg-wait-time');
    if (avgWaitEl) {
        avgWaitEl.textContent = (stats.avgWaitTime || 0).toFixed(2);
    }
    
    const avgWeightedTurnaroundEl = document.getElementById('avg-weighted-turnaround-time');
    if (avgWeightedTurnaroundEl) {
        avgWeightedTurnaroundEl.textContent = (stats.avgWeightedTurnaroundTime || 0).toFixed(2);
    }
    
    const completedEl = document.getElementById('completed-drones');
    if (completedEl) {
        completedEl.textContent = stats.completedDrones || 0;
    }
}

// 更新当前时间显示
function updateCurrentTimeDisplay() {
    const currentTimeEl = document.getElementById('current-time-value');
    if (currentTimeEl) {
        if (task7State.currentTime >= 0) {
            currentTimeEl.textContent = task7State.currentTime;
        } else {
            currentTimeEl.textContent = '-';
        }
    }
}

// 获取类型倍率
function getTypeRate(type) {
    switch (type) {
        case 'A': return 1.0;
        case 'B': return 1.5;
        case 'C': return 3.0;
        default: return 1.0;
    }
}

// 数字格式化：整数原样显示，小数保留两位
function formatNumber(value) {
    if (value === null || value === undefined || isNaN(value)) return '-';
    const num = Number(value);
    return Number.isInteger(num) ? num.toString() : num.toFixed(2);
}

// 初始化任务7
function initTask7() {
    // 重置状态
    task7State.drones.clear();
    task7State.station = [];
    task7State.queue = [];
    task7State.snapshots.clear();
    task7State.statistics = {
        totalExecutionTime: 0,
        avgTurnaroundTime: 0,
        avgWaitTime: 0,
        avgWeightedTurnaroundTime: 0,
        completedDrones: 0
    };
    task7State.currentTime = -1; // 初始状态为未查看
    task7State.maxTime = 0;
    
    // 清空算法结果（重置时不清空，保留已执行的结果）
    // 如果需要清空，可以取消下面的注释
    // algorithmResults = { hrrn: null, fcfs: null, sjf: null };
    
    updateTask7Visualization();
    updateCompareButton();
}

// 切换任务7输入字段显示
function toggleTask7Inputs() {
    const action = document.getElementById('task7-action').value;
    
    // 获取所有输入组
    const idGroup = document.getElementById('task7-id-group');
    const typeGroup = document.getElementById('task7-type-group');
    const arrivalGroup = document.getElementById('task7-arrival-group');
    const powerGroup = document.getElementById('task7-power-group');
    const timeGroup = document.getElementById('task7-time-group');
    
    // 重置所有组
    [idGroup, typeGroup, arrivalGroup, powerGroup, timeGroup].forEach(group => {
        if (group) group.classList.add('hidden');
    });
    
    // 根据操作类型显示相应字段
    if (action === 'i') {
        // 录入无人机：显示所有字段
        [idGroup, typeGroup, arrivalGroup, powerGroup].forEach(group => {
            if (group) group.classList.remove('hidden');
        });
    } else if (action === 't') {
        // 查看时刻：只显示时间字段
        if (timeGroup) timeGroup.classList.remove('hidden');
    }
    // s、f和j操作不需要额外字段
}

// 查看所有录入的无人机信息
function showAllRegisteredDrones() {
    if (task7State.drones.size === 0) {
        addOutputMessage('没有录入任何无人机！', 'warning');
        return;
    }
    
    let info = '========== 已录入无人机信息 ==========\n';
    const sortedDrones = Array.from(task7State.drones.values()).sort((a, b) => a.id - b.id);
    
    sortedDrones.forEach(drone => {
        const typeRate = getTypeRate(drone.type);
        info += `无人机${drone.id}:\n`;
        info += `  类型: ${drone.type}类 (充电倍率: ${typeRate}x)\n`;
        info += `  到达时间: ${drone.arrivalTime}\n`;
        info += `  当前电量: ${drone.currentPower.toFixed(1)}/10\n`;
        info += `  需充电时间: ${drone.requiredTime}h\n`;
        info += `  状态: ${drone.completed ? '已完成' : (drone.inStation ? '充电中' : drone.inQueue ? '等待中' : '未开始')}\n`;
        info += `  响应比: ${(drone.responseRatio || 0).toFixed(2)}\n\n`;
    });
    
    info += `总计: ${task7State.drones.size} 架无人机\n`;
    info += '=====================================';
    
    addOutputMessage(info, 'info');
}

// 更新对比按钮显示状态
function updateCompareButton() {
    const btn = document.getElementById('compare-algorithms-btn');
    if (btn) {
        const hasAnyResult = algorithmResults.hrrn || algorithmResults.fcfs || algorithmResults.sjf;
        btn.style.display = hasAnyResult ? 'block' : 'none';
    }
}

// 显示算法对比弹窗
function showAlgorithmComparison() {
    const modal = document.getElementById('algorithm-comparison-modal');
    const content = document.getElementById('algorithm-comparison-content');
    
    if (!modal || !content) return;
    
    // 生成对比内容
    let html = '';
    
    // 三种算法的配置
    const algorithms = [
        { key: 'hrrn', name: '高响应比优先调度算法（HRRN）', result: algorithmResults.hrrn },
        { key: 'fcfs', name: '先来先服务调度算法（FCFS）', result: algorithmResults.fcfs },
        { key: 'sjf', name: '短作业优先调度算法（SJF）', result: algorithmResults.sjf }
    ];
    
    // 为每个算法生成表格
    algorithms.forEach(alg => {
        if (!alg.result) return; // 如果没有结果，跳过
        
        html += `<div class="comparison-section">`;
        html += `<h4>${alg.name}</h4>`;
        html += `<table class="comparison-table">`;
        
        // 表头
        html += `<thead>`;
        html += `<tr class="algorithm-header">`;
        html += `<th>无人机</th>`;
        html += `<th>到达</th>`;
        html += `<th>开始</th>`;
        html += `<th>完成</th>`;
        html += `<th>等待</th>`;
        html += `<th>周转</th>`;
        html += `<th>服务</th>`;
        if (alg.key === 'hrrn') {
            html += `<th>响应比</th>`;
        }
        html += `<th>带权周转</th>`;
        html += `</tr>`;
        html += `</thead>`;
        
        // 计算列数（用于colspan）
        const columnCount = alg.key === 'hrrn' ? 8 : 7;
        
        // 表格内容
        html += `<tbody>`;
        alg.result.drones.forEach(drone => {
            html += `<tr>`;
            html += `<td>UAV${drone.id}</td>`;
            html += `<td>${drone.arrival >= 0 ? drone.arrival : '-'}</td>`;
            html += `<td>${drone.start >= 0 ? drone.start : '-'}</td>`;
            html += `<td>${drone.finish >= 0 ? drone.finish : '-'}</td>`;
            html += `<td>${drone.wait >= 0 ? formatNumber(drone.wait) : '-'}</td>`;
            // 周转时间应该显示为整数，如果解析失败则通过计算得出
            let turnaroundDisplay = '-';
            if (drone.turnaround >= 0) {
                turnaroundDisplay = drone.turnaround.toString();
            } else if (drone.finish >= 0 && drone.arrival >= 0) {
                turnaroundDisplay = (drone.finish - drone.arrival).toString();
            }
            html += `<td>${turnaroundDisplay}</td>`;
            html += `<td>${drone.service >= 0 ? drone.service.toFixed(1) : '-'}</td>`;
            if (alg.key === 'hrrn') {
                html += `<td>${drone.responseRatio >= 0 ? drone.responseRatio.toFixed(2) : '-'}</td>`;
            }
            html += `<td>${drone.weightedTurnaround >= 0 ? drone.weightedTurnaround.toFixed(2) : '-'}</td>`;
            html += `</tr>`;
        });
        
        // 统计信息行
        html += `<tr class="statistics-row">`;
        html += `<td colspan="${columnCount}">`;
        html += `平均等待时间: ${alg.result.statistics.avgWaitTime.toFixed(2)}<br>`;
        html += `平均周转时间: ${alg.result.statistics.avgTurnaroundTime.toFixed(2)}<br>`;
        html += `平均带权周转时间: ${alg.result.statistics.avgWeightedTurnaroundTime.toFixed(2)}<br>`;
        html += `总执行时间: ${alg.result.statistics.totalExecutionTime}`;
        html += `</td>`;
        html += `</tr>`;
        
        html += `</tbody>`;
        html += `</table>`;
        html += `</div>`;
    });
    
    if (!html) {
        html = '<div class="no-data-message">暂无算法结果，请先执行至少一种调度算法。</div>';
    }
    
    content.innerHTML = html;
    modal.style.display = 'block';
}

// 关闭算法对比弹窗
function closeAlgorithmComparisonModal() {
    const modal = document.getElementById('algorithm-comparison-modal');
    if (modal) {
        modal.style.display = 'none';
    }
}

// 点击弹窗外部关闭
window.onclick = function(event) {
    const modal = document.getElementById('algorithm-comparison-modal');
    if (event.target === modal) {
        closeAlgorithmComparisonModal();
    }
}

// 页面加载时初始化
document.addEventListener('DOMContentLoaded', function() {
    initTask7();
    toggleTask7Inputs(); // 初始化输入字段显示
});
