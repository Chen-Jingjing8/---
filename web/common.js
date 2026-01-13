// 公共变量
let currentTask = 1;
let outputHistory = [];
// 存储任务4的紧急调度结果
let emergencyDispatchResult = null;

// 为每个任务维护独立的系统状态
function createEmptyState() {
    return {
        stationOccupied: 0,
        queueCount: 0,
        currentTime: 0,
        totalDrones: 0,
        drones: new Map(),
        timeOriginSim: 0,
        timeOriginMs: 0
    };
}

const taskStates = {
    1: createEmptyState(),
    2: createEmptyState(),
    3: createEmptyState(),
    4: createEmptyState(),
    5: createEmptyState(),
    6: createEmptyState(),
    7: createEmptyState()
};

// 指向当前任务的活动状态对象
let systemState = taskStates[currentTask];

// 通用工具函数
function normalizeType(t) {
    if (t === undefined || t === null) return 'A';
    const s = String(t).toUpperCase();
    if (s === '1') return 'A';
    if (s === '2') return 'B';
    if (s === '3') return 'C';
    if (s === 'A' || s === 'B' || s === 'C') return s;
    return 'A';
}

function getTypeRate(type) {
    if (type === 'B') return 1.5;
    if (type === 'C') return 3.0;
    return 1.0;
}

function getLocationMultiplier(location, taskNum, type) {
    if (location === 'charging') return 1.0;
    if (location === 'waiting') {
        return (taskNum === 1 || taskNum === 2) ? 0.0 : 0.5;
    }
    if (location === 'temp') {
        return 0.3;
    }
    return 0.0;
}

function accumulateEnergy(drone, nowTime, location, taskNum) {
    if (drone.lastUpdateTime === undefined) {
        drone.lastUpdateTime = nowTime;
        return;
    }
    const delta = Math.max(0, nowTime - drone.lastUpdateTime);
    if (delta === 0) return;
    const rate = getTypeRate(drone.type || 'A');
    const mult = getLocationMultiplier(location, taskNum, drone.type || 'A');
    drone.energyAccumulated = (drone.energyAccumulated || 0) + delta * rate * mult;
    drone.lastUpdateTime = nowTime;
}

function accumulateAllEnergy(state, nowTime, taskNum) {
    state.drones.forEach(d => {
        const loc = d.inStation ? 'charging' : (d.inQueue ? 'waiting' : (d.inTempStack ? 'temp' : 'unknown'));
        accumulateEnergy(d, nowTime, loc, taskNum);
    });
}

// 验证输入
function validateInput(id, time) {
    if (!id || !time) {
        addOutputMessage('请输入完整的参数！', 'error');
        return false;
    }
    if (parseInt(id) < 1 || parseInt(time) < 0) {
        addOutputMessage('编号必须大于0，时间不能为负数！', 'error');
        return false;
    }
    return true;
}

// 验证紧急调度输入
function validateEmergencyInput(a, b, c, x) {
    if (!a || !b || !c || !x) {
        addOutputMessage('请输入完整的紧急调度参数！', 'error');
        return false;
    }
    if (parseInt(a) < 0 || parseInt(b) < 0 || parseInt(c) < 0 || parseInt(x) < 0) {
        addOutputMessage('所有参数都不能为负数！', 'error');
        return false;
    }
    return true;
}

// 处理输入（调用真实后端）
async function processInput(input, taskNum) {
    // 检查是否是任务4的紧急调度操作
    const isTask4Emergency = (taskNum === 4 && (input.startsWith('e') || input.includes('e\n')));
    
    if (!isTask4Emergency) {
        addOutputMessage(`正在处理操作：${input}`, 'info');
    }
    
    try {
        const result = await callBackend(input, taskNum);
        
        // 检查是否是任务4的紧急调度操作
        if (isTask4Emergency) {
            // 保存紧急调度结果，不显示在系统输出中
            // 调试：记录原始结果
            console.log('原始紧急调度结果:', result);
            console.log('结果长度:', result ? result.length : 0);
            emergencyDispatchResult = result;
            
            // 检查结果是否有效（包含实际的调度内容）
            const hasValidResult = result && result.trim().length > 0 && 
                                   (result.includes('紧急任务调度') || 
                                    result.includes('充电阶段') || 
                                    result.includes('任务执行') ||
                                    result.includes('任务结果'));
            
            if (hasValidResult) {
                addOutputMessage('紧急调度任务已提交，请点击"查看调度结果"按钮查看详情。', 'success');
            } else {
                addOutputMessage('紧急调度任务已提交，但返回结果可能不完整。请点击"查看调度结果"按钮查看。', 'warning');
            }
        } else {
            // 检查是否是任务6的i操作，如果是则不显示在系统输出中
            const isTask6Info = (taskNum === 6 && input.trim() === 'i');
            
            if (!isTask6Info) {
                addOutputMessage(result, 'success');
            }
        }
        
        // 调用任务特定的状态更新函数
        if (window[`updateTask${taskNum}State`]) {
            window[`updateTask${taskNum}State`](input, result);
        }
        
        refreshSystemState();
        
    } catch (error) {
        addOutputMessage(`处理失败：${error.message}`, 'error');
        return;
    }
}

// 实际的后端调用函数
async function callBackend(input, taskNum) {
    try {
        const response = await fetch('http://127.0.0.1:5002/api/simulate', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
            },
            body: JSON.stringify({
                input: input,
                task: taskNum
            })
        });
        
        if (!response.ok) {
            throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }
        
        const data = await response.json();
        
        if (data && data.success) {
            return data.result;
        }
        throw new Error((data && data.error) || '后端返回错误');
        
    } catch (error) {
        console.error('Backend call failed:', error);
        throw new Error('后端调用失败：' + error.message);
    }
}

// 添加输出消息
function addOutputMessage(message, type = 'info') {
    const outputContent = document.getElementById('output-content');
    const timestamp = new Date().toLocaleTimeString();
    
    const outputItem = document.createElement('div');
    outputItem.className = `output-item ${type}`;
    
    const timeSpan = document.createElement('div');
    timeSpan.className = 'output-time';
    timeSpan.textContent = `[${timestamp}]`;
    
    const textSpan = document.createElement('div');
    textSpan.className = 'output-text';
    textSpan.innerHTML = message.replace(/\n/g, '<br>');
    
    outputItem.appendChild(timeSpan);
    outputItem.appendChild(textSpan);
    
    const welcomeMsg = outputContent.querySelector('.welcome-message');
    if (welcomeMsg) {
        welcomeMsg.remove();
    }
    
    outputContent.appendChild(outputItem);
    outputContent.scrollTop = outputContent.scrollHeight;
    
    outputHistory.push({
        timestamp: timestamp,
        message: message,
        type: type
    });
}

// 清空输出
function clearOutput() {
    const outputContent = document.getElementById('output-content');
    outputContent.innerHTML = `
        <div class="welcome-message">
            <i class="fas fa-info-circle"></i>
            <p>输出已清空</p>
            <p>请输入新的操作继续测试</p>
        </div>
    `;
    outputHistory = [];

    // 任务7特殊处理：调用后端重置命令
    if (currentTask === 7) {
        processInput('r', 7).then(() => {
            refreshSystemState();
        }).catch(() => {});
    }
    // 任务6特殊处理：保留注册信息，只清空系统状态
    else if (currentTask === 6) {
        clearTask6SystemState();
        refreshSystemState();
    } else {
        taskStates[currentTask] = createEmptyState();
        systemState = taskStates[currentTask];
        refreshSystemState();
    }

    fetch('http://127.0.0.1:5002/api/reset', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ task: currentTask })
    }).catch(() => {});
}

// 导出输出
function exportOutput() {
    if (outputHistory.length === 0) {
        addOutputMessage('没有可导出的内容！', 'warning');
        return;
    }
    
    let exportText = '无人机管理与网络威胁预警系统 - 操作记录\n';
    exportText += '='.repeat(50) + '\n\n';
    
    outputHistory.forEach(item => {
        exportText += `[${item.timestamp}] ${item.message}\n`;
    });
    
    const blob = new Blob([exportText], { type: 'text/plain' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `drone_system_log_${new Date().toISOString().slice(0, 10)}.txt`;
    document.body.appendChild(a);
    a.click();
    document.body.removeChild(a);
    URL.revokeObjectURL(url);
    
    addOutputMessage('操作记录已导出！', 'success');
}

// 检查后端服务状态
async function checkBackendStatus() {
    try {
        const response = await fetch('http://127.0.0.1:5002/api/health');
        if (response.ok) {
            addOutputMessage('后端服务已连接', 'success');
            return true;
        }
    } catch (error) {
        addOutputMessage('后端服务未启动，将使用模拟数据', 'warning');
        return false;
    }
}

// 刷新系统状态可视化
function refreshSystemState() {
    if (currentTask === 5) {
        // 任务5使用专用的更新函数
        if (window.updateTask5Visualization) {
            window.updateTask5Visualization();
        }
    } else if (currentTask === 6) {
        // 任务6使用专用的更新函数
        if (window.updateTask6Visualization) {
            window.updateTask6Visualization();
        }
    } else {
        updateSystemVisualization(systemState);
    }
}

// 更新状态面板
function updateStatusPanel() {
    const stationCountEl = document.getElementById('station-count');
    if (stationCountEl) {
        stationCountEl.textContent = `${Math.min(systemState.stationOccupied, 2)}/2`;
    }
    const queueListCountEl = document.getElementById('queue-list-count');
    if (queueListCountEl) {
        queueListCountEl.textContent = String(systemState.queueCount);
    }
    const currentTimeEl = document.getElementById('current-time-value');
    if (currentTimeEl) {
        currentTimeEl.textContent = systemState.currentTime || 0;
    }
}

// 使用前端systemState数据更新系统可视化
function updateSystemVisualization(stateData) {
    try {
        const chargingStation = [];
        const waitingQueue = [];
        const tempStack = [];
        
        stateData.drones.forEach((drone, id) => {
            const typeNorm = normalizeType(drone.type);
            const droneInfo = {
                id: id,
                type: typeNorm,
                arrival_time: Number.isFinite(drone.arrivalTime) ? drone.arrivalTime : 0,
                position: drone.position || 1
            };
            
            if (drone.inStation) {
                chargingStation.push(droneInfo);
            } else if (drone.inQueue) {
                waitingQueue.push(droneInfo);
            } else if (drone.inTempStack) {
                tempStack.push(droneInfo);
            }
        });
        
        updateChargingStationList(chargingStation);
        updateWaitingQueueList(waitingQueue);
        updateStatusPanel();
        
    } catch (error) {
        console.error('更新系统状态失败:', error);
    }
}

// 更新充电站栈显示
function updateChargingStationList(drones) {
    const container = document.getElementById('charging-station-list');
    const countElement = document.getElementById('station-count');
    
    if (!container || !countElement) return;
    
    countElement.textContent = `${drones.length}/2`;
    
    container.style.opacity = '0.7';
    
    setTimeout(() => {
        container.innerHTML = '';
        
        for (let i = 0; i < 2; i++) {
            if (i < drones.length) {
                const drone = drones[i];
                const droneElement = createDroneElement(drone, 'charging');
                container.appendChild(droneElement);
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

// 更新便道队列显示
function updateWaitingQueueList(drones) {
    const container = document.getElementById('waiting-queue-list');
    const countElement = document.getElementById('queue-list-count');
    
    if (!container || !countElement) return;
    
    countElement.textContent = drones.length;
    
    container.style.opacity = '0.7';
    
    setTimeout(() => {
        container.innerHTML = '';
        
        if (drones.length === 0) {
            const emptyElement = document.createElement('div');
            emptyElement.className = 'empty-queue';
            emptyElement.textContent = '暂无排队无人机';
            container.appendChild(emptyElement);
        } else {
            drones.forEach(drone => {
                const droneElement = createDroneElement(drone, 'waiting');
                container.appendChild(droneElement);
            });
        }
        
        container.style.opacity = '1';
    }, 150);
}

// 创建无人机元素
function createDroneElement(drone, location) {
    const droneElement = document.createElement('div');
    droneElement.className = 'drone-item';
    droneElement.onclick = () => showDroneDetails(drone, location);
    
    const droneInfo = document.createElement('div');
    droneInfo.className = 'drone-info';
    
    const droneId = document.createElement('div');
    droneId.className = 'drone-id';
    const stored = systemState.drones.get(drone.id) || drone;
    let idText = `#${drone.id}`;
    if (!(currentTask === 1 || currentTask === 3)) {
        idText += ` (${stored.type || drone.type || 'A'})`;
    }
    droneId.textContent = idText;
    
    const droneDetails = document.createElement('div');
    droneDetails.className = 'drone-details';
    const arrivalTime = stored.arrivalTime || drone.arrival_time || 0;
    droneDetails.textContent = `到达: ${arrivalTime}`;
    
    droneInfo.appendChild(droneId);
    droneInfo.appendChild(droneDetails);
    
    const dronePosition = document.createElement('div');
    dronePosition.className = 'drone-position';
    dronePosition.textContent = `位置 ${drone.position || 'N/A'}`;
    
    droneElement.appendChild(droneInfo);
    droneElement.appendChild(dronePosition);
    
    return droneElement;
}

// 显示无人机详情弹窗
function showDroneDetails(drone, location) {
    const modal = document.getElementById('drone-detail-modal');
    const content = document.getElementById('drone-detail-content');
    
    const currentTime = systemState.currentTime || 0;
    const stored = systemState.drones.get(drone.id) || drone;
    const arrivalTime = stored.arrivalTime || drone.arrival_time || 0;
    const stayTime = Math.max(0, currentTime - arrivalTime);
    const chargingAmount = calculateChargingAmount(drone, location, stayTime);
    
    const typeLabel = normalizeType(stored.type || drone.type) + '类';
    const typeRow = (currentTask === 1 || currentTask === 3) ? '' : `
            <div class="detail-item">
                <div class="detail-label">无人机类型</div>
                <div class="detail-value">${typeLabel}</div>
            </div>`;

    content.innerHTML = `
        <div class="detail-grid">
            <div class="detail-item">
                <div class="detail-label">无人机编号</div>
                <div class="detail-value highlight">#${drone.id}</div>
            </div>
            <div class="detail-item">
                <div class="detail-label">当前位置</div>
                <div class="detail-value">${getLocationName(location)}</div>
            </div>
            <div class="detail-item">
                <div class="detail-label">位置编号</div>
                <div class="detail-value">${drone.position || 'N/A'}</div>
            </div>
            <div class="detail-item">
                <div class="detail-label">到达时间</div>
                <div class="detail-value">${arrivalTime}</div>
            </div>
            <div class="detail-item">
                <div class="detail-label">停留时间</div>
                <div class="detail-value">${stayTime} 时间单位</div>
            </div>
            ${typeRow}
            <div class="detail-item">
                <div class="detail-label">充电量</div>
                <div class="detail-value">${chargingAmount}</div>
            </div>
            <div class="detail-item">
                <div class="detail-label">状态</div>
                <div class="detail-value">${getStatusText(location)}</div>
            </div>
        </div>
    `;
    
    modal.style.display = 'block';
}

// 计算充电量
function calculateChargingAmount(drone, location, stayTime) {
    const stored = systemState.drones.get(drone.id) || drone;
    let type = stored.type || drone.type || 'A';
    if (type === 1) type = 'A';
    if (type === 2) type = 'B';
    if (type === 3) type = 'C';
    const rate = (type === 'B') ? 1.5 : (type === 'C') ? 3.0 : 1.0;
    const multiplier = (location === 'charging') ? 1.0 : (location === 'waiting') ? ((currentTask === 1 || currentTask === 2) ? 0.0 : 0.5) : (location === 'temp') ? 0.3 : 0.0;
    const nowSim = systemState.currentTime || 0;
    const base = stored.energyAccumulated || 0;
    const last = (stored.lastUpdateTime != null) ? stored.lastUpdateTime : nowSim;
    const deltaTime = Math.max(0, nowSim - last);
    const value = base + deltaTime * rate * multiplier;
    return value.toFixed(2);
}

// 获取位置名称
function getLocationName(location) {
    switch (location) {
        case 'charging': return '充电站内';
        case 'waiting': return '便道队列';
        case 'temp': return '临时停靠';
        default: return '未知位置';
    }
}

// 获取状态文本
function getStatusText(location) {
    switch (location) {
        case 'charging': return '正在充电';
        case 'waiting': return '等待充电';
        case 'temp': return '临时停靠';
        default: return '未知状态';
    }
}

// 关闭无人机详情弹窗
function closeDroneDetailModal() {
    const modal = document.getElementById('drone-detail-modal');
    modal.style.display = 'none';
}

// 点击弹窗外部关闭
window.onclick = function(event) {
    const modal = document.getElementById('drone-detail-modal');
    if (event.target === modal) {
        modal.style.display = 'none';
    }
}

// 定时刷新相关变量
let refreshInterval = null;

function startPeriodicRefresh() {
    refreshInterval = setInterval(() => {
        refreshSystemState();
    }, 1000);
}

function stopPeriodicRefresh() {
    if (refreshInterval) {
        clearInterval(refreshInterval);
        refreshInterval = null;
    }
}

window.addEventListener('beforeunload', function() {
    stopPeriodicRefresh();
});

// 页面加载时恢复系统状态（适用于所有任务）
function restoreSystemStateOnLoad() {
    // 检查是否已经恢复过系统状态
    if (sessionStorage.getItem(`task${currentTask}_system_restored`) === 'true') {
        console.log(`[Task${currentTask}] 系统状态已经恢复过，跳过`);
        return;
    }
    
    // 获取当前系统状态
    const input = 's';
    processInput(input, currentTask).then(() => {
        console.log(`[Task${currentTask}] 系统状态已恢复`);
        sessionStorage.setItem(`task${currentTask}_system_restored`, 'true');
    }).catch(error => {
        console.log(`[Task${currentTask}] 恢复系统状态失败:`, error);
    });
}

// 已禁用全局自动恢复，让每个任务自己决定是否恢复

