// 任务5：双入口充电站管理

// 提交任务5
function submitTask5() {
    const action = document.getElementById('task5-action').value;
    
    if (action === 's') {
        const input = 's';
        processInput(input, 5);
    } else {
        const id = document.getElementById('task5-id').value;
        const type = document.getElementById('task5-type').value;
        const time = document.getElementById('task5-time').value;
        
        if (!validateInput(id, time)) return;
        
        // 构造命令：a命令需要type，d命令不需要type
        const input = action === 'a' 
            ? `${action} ${id} ${type} ${time}` 
            : `${action} ${id} ${time}`;
        processInput(input, 5);
    }
}

// 更新任务5的系统状态
function updateTask5State(input, backendText) {
    const parts = input.trim().split(' ');
    const action = parts[0];
    const targetState = taskStates[5];
    
    // 初始化任务5专用状态
    if (!targetState.westStack) targetState.westStack = [];
    if (!targetState.eastStack) targetState.eastStack = [];
    if (!targetState.task5Queue) targetState.task5Queue = [];
    if (!targetState.maxBypassDepth) targetState.maxBypassDepth = 0;
    
    if (action === 'a') {
        const id = Number(parts[1]);
        const type = normalizeType(parts[2]);
        const time = Number(parts[3]);
        
        accumulateAllEnergy(targetState, time, 5);
        
        const newDrone = {
            id: id,
            type: type,
            arrivalTime: time,
            inWestStack: false,
            inEastStack: false,
            inQueue: false,
            position: 1,
            energyAccumulated: 0,
            lastUpdateTime: time
        };
        
        // 解析后端输出，确定无人机位置
        const enterQueue = /进入便道/.test(backendText);
        const toEast = /转移到东口/.test(backendText);
        
        if (enterQueue) {
            // 进入便道
            newDrone.inQueue = true;
            targetState.task5Queue.push(id);
        } else if (toEast) {
            // 如果最终转移到了东口，则只在东口显示
            newDrone.inEastStack = true;
            targetState.eastStack.push(id);
        } else {
            // 只在西口
            newDrone.inWestStack = true;
            targetState.westStack.push(id);
        }
        
        targetState.drones.set(id, newDrone);
        targetState.totalDrones++;
        targetState.currentTime = Math.max(targetState.currentTime, time);
        
    } else if (action === 'd') {
        const id = Number(parts[1]);
        const time = Number(parts[2]); // d命令格式：d id time
        
        accumulateAllEnergy(targetState, time, 5);
        
        // 检查后端是否返回错误
        const hasError = /错误[：:]/.test(backendText);
        
        if (hasError) {
            // 后端返回错误，不修改状态，保持原样
            targetState.currentTime = Math.max(targetState.currentTime, time);
            updateTask5Visualization();
            return;
        }
        
        // 成功执行，删除离开的无人机
        if (targetState.drones.has(id)) {
            targetState.drones.delete(id);
            targetState.totalDrones--;
        }
        
        // 提取最大让路深度
        const bypassMatch = backendText.match(/让路深度[：:]\s*(\d+)/g);
        if (bypassMatch) {
            const depths = bypassMatch.map(m => parseInt(m.match(/\d+/)[0]));
            const maxDepth = Math.max(...depths);
            if (maxDepth > targetState.maxBypassDepth) {
                targetState.maxBypassDepth = maxDepth;
            }
        }
        
        // d命令执行成功后，需要同步前端状态
        // 初始化为执行d命令前除了离开无人机外的所有无人机
        const allDroneIds = Array.from(targetState.drones.keys());
        
        // 清空当前跟踪的栈状态
        targetState.westStack = [];
        targetState.eastStack = [];
        targetState.task5Queue = [];
        
        // 记录每个无人机的最后一次转移
        const lastLocation = new Map();
        
        // 解析所有转移消息，按顺序更新位置
        const lines = backendText.split('\n');
        for (const line of lines) {
            // 匹配"无人机X从临时栈返回西口"
            let match = line.match(/无人机(\d+)从临时栈返回西口/);
            if (match) {
                lastLocation.set(parseInt(match[1]), 'west');
                continue;
            }
            
            // 匹配"无人机X从西口转移到东口"
            match = line.match(/无人机(\d+)从西口转移到东口/);
            if (match) {
                lastLocation.set(parseInt(match[1]), 'east');
                continue;
            }
            
            // 匹配"无人机X从便道进入西口"
            match = line.match(/无人机(\d+)从便道进入西口/);
            if (match) {
                lastLocation.set(parseInt(match[1]), 'west');
                continue;
            }
        }
        
        // 收集所有转移到东口栈的无人机，按转移顺序排序
        const eastTransfers = [];
        const westTransfers = [];
        const queueTransfers = [];
        
        // 解析转移消息，记录转移顺序
        const transferOrder = [];
        for (const line of lines) {
            let match = line.match(/无人机(\d+)从西口转移到东口/);
            if (match) {
                const droneId = parseInt(match[1]);
                transferOrder.push({id: droneId, type: 'west_to_east', line: line});
            }
            
            match = line.match(/无人机(\d+)从临时栈返回西口/);
            if (match) {
                const droneId = parseInt(match[1]);
                transferOrder.push({id: droneId, type: 'temp_to_west', line: line});
            }
            
            match = line.match(/无人机(\d+)从便道进入西口/);
            if (match) {
                const droneId = parseInt(match[1]);
                transferOrder.push({id: droneId, type: 'queue_to_west', line: line});
            }
        }
        
        // 根据最后位置分配无人机
        for (const droneId of allDroneIds) {
            const location = lastLocation.get(droneId);
            const drone = targetState.drones.get(droneId);
            
            if (location === 'east') {
                eastTransfers.push(droneId);
            } else if (location === 'west') {
                westTransfers.push(droneId);
            } else if (location === 'queue') {
                queueTransfers.push(droneId);
            } else {
                // 如果没有记录转移，说明无人机保持在原位置
                if (drone) {
                    if (drone.inEastStack) {
                        eastTransfers.push(droneId);
                    } else if (drone.inWestStack) {
                        westTransfers.push(droneId);
                    } else if (drone.inQueue) {
                        queueTransfers.push(droneId);
                    }
                }
            }
        }
        
        // 按照转移顺序排列东口栈的无人机
        const orderedEastStack = [];
        for (const transfer of transferOrder) {
            if (transfer.type === 'west_to_east' && eastTransfers.includes(transfer.id)) {
                orderedEastStack.push(transfer.id);
            }
        }
        
        // 添加没有转移记录的东口栈无人机（保持原位置）
        for (const droneId of eastTransfers) {
            if (!orderedEastStack.includes(droneId)) {
                orderedEastStack.push(droneId);
            }
        }
        
        // 按照转移顺序排列西口栈的无人机
        const orderedWestStack = [];
        for (const transfer of transferOrder) {
            if ((transfer.type === 'temp_to_west' || transfer.type === 'queue_to_west') && westTransfers.includes(transfer.id)) {
                orderedWestStack.push(transfer.id);
            }
        }
        
        // 添加没有转移记录的西口栈无人机（保持原位置）
        for (const droneId of westTransfers) {
            if (!orderedWestStack.includes(droneId)) {
                orderedWestStack.push(droneId);
            }
        }
        
        targetState.eastStack = orderedEastStack;
        targetState.westStack = orderedWestStack;
        targetState.task5Queue = queueTransfers;
        
        // 更新每个无人机的位置标记
        targetState.drones.forEach((drone, droneId) => {
            drone.inWestStack = targetState.westStack.includes(droneId);
            drone.inEastStack = targetState.eastStack.includes(droneId);
            drone.inQueue = targetState.task5Queue.includes(droneId);
        });
        
        targetState.currentTime = Math.max(targetState.currentTime, time);
        
    } else if (action === 's') {
        // s命令只显示状态，不修改前端跟踪的数据
        const displayState = parseTask5StatusForDisplay(backendText, targetState);
        updateTask5VisualizationWithDisplayState(displayState, targetState);
        return;
    }
    
    updateTask5Visualization();
}

// 解析状态显示（用于s命令，只显示不修改状态）
function parseTask5StatusForDisplay(text, targetState) {
    const displayState = {
        westStack: [],
        eastStack: [],
        task5Queue: [],
        maxBypassDepth: targetState.maxBypassDepth || 0
    };
    
    // 解析西口栈 - 使用[ ]*只匹配空格，[^\n]*匹配到行尾
    const westMatch = text.match(/西口栈\[(\d+)\/\d+\]:[ ]*([^\n]*)/);
    if (westMatch && westMatch[2]) {
        const dronesText = westMatch[2].trim();
        if (dronesText.length > 0) {
            const droneMatches = dronesText.matchAll(/(\d+)\(([A-C])\)/g);
            for (const match of droneMatches) {
                const id = parseInt(match[1]);
                displayState.westStack.push(id);
            }
        }
    }
    
    // 解析东口栈 - 使用[ ]*只匹配空格，[^\n]*匹配到行尾
    const eastMatch = text.match(/东口栈\[(\d+)\/\d+\]:[ ]*([^\n]*)/);
    if (eastMatch && eastMatch[2]) {
        const dronesText = eastMatch[2].trim();
        if (dronesText.length > 0) {
            const droneMatches = dronesText.matchAll(/(\d+)\(([A-C])\)/g);
            for (const match of droneMatches) {
                const id = parseInt(match[1]);
                displayState.eastStack.push(id);
            }
        }
    }
    
    // 解析便道 - 使用[ ]*只匹配空格，[^\n]*匹配到行尾
    const queueMatch = text.match(/便道\[(\d+)\]:[ ]*([^\n]*)/);
    if (queueMatch && queueMatch[2]) {
        const dronesText = queueMatch[2].trim();
        if (dronesText.length > 0) {
            const droneMatches = dronesText.matchAll(/(\d+)\(([A-C])\)/g);
            for (const match of droneMatches) {
                const id = parseInt(match[1]);
                displayState.task5Queue.push(id);
            }
        }
    }
    
    // 解析最大让路深度
    const bypassMatch = text.match(/最大让路深度[：:]\s*(\d+)/);
    if (bypassMatch) {
        displayState.maxBypassDepth = parseInt(bypassMatch[1]);
    }
    
    return displayState;
}

// 使用显示状态更新可视化（用于s命令）
function updateTask5VisualizationWithDisplayState(displayState, targetState) {
    // 更新西口栈
    updateStackList('west-stack-list', 'west-stack-count', displayState.westStack, targetState);
    
    // 更新东口栈
    updateStackList('east-stack-list', 'east-stack-count', displayState.eastStack, targetState);
    
    // 更新便道队列
    updateTask5Queue(displayState.task5Queue, targetState);
    
    // 更新最大让路深度
    const bypassEl = document.getElementById('max-bypass-depth');
    if (bypassEl) {
        bypassEl.textContent = displayState.maxBypassDepth;
    }
    
    // 更新当前时间
    const currentTimeEl = document.getElementById('current-time-value');
    if (currentTimeEl) {
        currentTimeEl.textContent = targetState.currentTime || 0;
    }
}

// 更新任务5的可视化
function updateTask5Visualization() {
    const targetState = taskStates[5];
    
    // 更新西口栈
    updateStackList('west-stack-list', 'west-stack-count', targetState.westStack || [], targetState);
    
    // 更新东口栈
    updateStackList('east-stack-list', 'east-stack-count', targetState.eastStack || [], targetState);
    
    // 更新便道队列
    updateTask5Queue(targetState.task5Queue || [], targetState);
    
    // 更新最大让路深度
    const bypassEl = document.getElementById('max-bypass-depth');
    if (bypassEl) {
        bypassEl.textContent = targetState.maxBypassDepth || 0;
    }
    
    // 更新当前时间
    const currentTimeEl = document.getElementById('current-time-value');
    if (currentTimeEl) {
        currentTimeEl.textContent = targetState.currentTime || 0;
    }
}

// 更新栈列表
function updateStackList(listId, countId, droneIds, state) {
    const container = document.getElementById(listId);
    const countElement = document.getElementById(countId);
    
    if (!container || !countElement) return;
    
    countElement.textContent = `${droneIds.length}/2`;
    
    container.style.opacity = '0.7';
    
    setTimeout(() => {
        container.innerHTML = '';
        
        // 判断是西口栈还是东口栈
        const isWestStack = listId === 'west-stack-list';
        const isEastStack = listId === 'east-stack-list';
        
        if (isWestStack || isEastStack) {
            // DOM显示：先appendChild的在上方，后appendChild的在下方
            // 西口栈：栈顶（高索引）在上方，栈底（索引0）在下方
            // 东口栈：远离出口（高索引）在上方，出口（索引0）在下方
            
            // 如果droneIds为空，显示两个空位
            if (droneIds.length === 0) {
                for (let i = 0; i < 2; i++) {
                    const emptyElement = document.createElement('div');
                    emptyElement.className = 'empty-slot';
                    emptyElement.textContent = isEastStack && i === 1 ? '空位（出口）' : '空位';
                    container.appendChild(emptyElement);
                }
            } else {
                // 先添加空位（如果有）到上方
                for (let i = 2 - 1; i >= droneIds.length; i--) {
                    const emptyElement = document.createElement('div');
                    emptyElement.className = 'empty-slot';
                    emptyElement.textContent = '空位';
                    container.appendChild(emptyElement);
                }
                
                // 从高索引到低索引显示（栈顶到栈底）
                for (let i = droneIds.length - 1; i >= 0; i--) {
                    const droneId = droneIds[i];
                    const drone = state.drones.get(droneId);
                    if (drone) {
                        // 东口栈索引0是出口位置
                        const isExit = isEastStack && i === 0;
                        const droneElement = createTask5DroneElement({
                            id: droneId,
                            type: drone.type,
                            arrival_time: drone.arrivalTime,
                            position: i
                        }, 'stack', isExit);
                        container.appendChild(droneElement);
                    }
                }
            }
        }
        
        container.style.opacity = '1';
    }, 150);
}

// 更新任务5便道队列
function updateTask5Queue(droneIds, state) {
    const container = document.getElementById('task5-queue-list');
    const countElement = document.getElementById('task5-queue-count');
    
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
                const drone = state.drones.get(droneId);
                if (drone) {
                    const droneElement = createTask5DroneElement({
                        id: droneId,
                        type: drone.type,
                        arrival_time: drone.arrivalTime,
                        position: index + 1
                    }, 'queue');
                    container.appendChild(droneElement);
                }
            });
        }
        
        container.style.opacity = '1';
    }, 150);
}

// 创建任务5无人机元素
function createTask5DroneElement(drone, location, isExitPosition = false) {
    const droneElement = document.createElement('div');
    droneElement.className = 'drone-item';
    droneElement.style.cursor = 'pointer';
    
    // 如果是出口位置，添加特殊标记
    if (isExitPosition) {
        droneElement.classList.add('exit-position');
    }
    
    // 添加点击事件，显示详细信息
    droneElement.onclick = () => showTask5DroneDetails(drone, location);
    
    const droneInfo = document.createElement('div');
    droneInfo.className = 'drone-info';
    
    const droneId = document.createElement('div');
    droneId.className = 'drone-id';
    droneId.textContent = `#${drone.id} (${drone.type || 'A'})`;
    
    const droneDetails = document.createElement('div');
    droneDetails.className = 'drone-details';
    const positionLabel = isExitPosition ? '出口位置' : `到达: ${drone.arrival_time || 0}`;
    droneDetails.textContent = positionLabel;
    
    droneInfo.appendChild(droneId);
    droneInfo.appendChild(droneDetails);
    
    const dronePosition = document.createElement('div');
    dronePosition.className = 'drone-position';
    dronePosition.textContent = `索引 ${drone.position !== undefined ? drone.position : '?'}`;
    
    droneElement.appendChild(droneInfo);
    droneElement.appendChild(dronePosition);
    
    return droneElement;
}

// 显示任务5无人机详细信息
function showTask5DroneDetails(drone, location) {
    const modal = document.getElementById('drone-detail-modal');
    const content = document.getElementById('drone-detail-content');
    
    const targetState = taskStates[5];
    const currentTime = targetState.currentTime || 0;
    const stored = targetState.drones.get(drone.id) || drone;
    const arrivalTime = stored.arrivalTime || drone.arrival_time || 0;
    const stayTime = Math.max(0, currentTime - arrivalTime);
    
    // 确定位置名称
    let locationName = '未知位置';
    if (location === 'stack') {
        if (stored.inWestStack) {
            locationName = '西口栈（入口A）';
        } else if (stored.inEastStack) {
            locationName = '东口栈（出口B）';
        }
    } else if (location === 'queue') {
        locationName = '链式便道';
    }
    
    // 计算充电量（简化版）
    const typeRate = getTypeRate(stored.type || drone.type || 'A');
    const chargingAmount = (stayTime * typeRate * 1.0).toFixed(2);
    
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
                <div class="detail-label">栈内位置</div>
                <div class="detail-value">${drone.position || 1}</div>
            </div>
            <div class="detail-item">
                <div class="detail-label">到达时间</div>
                <div class="detail-value">${arrivalTime}</div>
            </div>
            <div class="detail-item">
                <div class="detail-label">停留时间</div>
                <div class="detail-value">${stayTime} 时间单位</div>
            </div>
            <div class="detail-item">
                <div class="detail-label">累计充电量</div>
                <div class="detail-value">${chargingAmount}</div>
            </div>
            <div class="detail-item">
                <div class="detail-label">充电倍率</div>
                <div class="detail-value">${typeRate}x</div>
            </div>
            <div class="detail-item">
                <div class="detail-label">状态</div>
                <div class="detail-value">${stored.inEastStack ? '准备离站' : stored.inWestStack ? '等待转移' : '队列等待'}</div>
            </div>
        </div>
    `;
    
    modal.style.display = 'block';
}

// 防止数字输入框滚轮事件冒泡到页面
document.addEventListener('DOMContentLoaded', function() {
    // 为所有任务5的数字输入框添加滚轮事件处理
    const task5Inputs = [
        document.getElementById('task5-id'),
        document.getElementById('task5-time')
    ];
    
    task5Inputs.forEach(input => {
        if (input) {
            input.addEventListener('wheel', function(e) {
                // 阻止滚轮事件冒泡到页面
                e.stopPropagation();
            }, { passive: true });
            
            // 当输入框获得焦点时，临时禁用父容器滚动
            input.addEventListener('focus', function() {
                const inputSection = this.closest('.input-section');
                if (inputSection) {
                    inputSection.style.overflowY = 'hidden';
                }
            });
            
            input.addEventListener('blur', function() {
                const inputSection = this.closest('.input-section');
                if (inputSection) {
                    inputSection.style.overflowY = 'auto';
                }
            });
        }
    });
    
    // 为整个输入区域添加滚轮事件处理
    const inputSections = document.querySelectorAll('.input-section');
    inputSections.forEach(section => {
        section.addEventListener('wheel', function(e) {
            // 检查是否滚动到顶部或底部
            const atTop = this.scrollTop === 0;
            const atBottom = this.scrollHeight - this.scrollTop === this.clientHeight;
            
            // 如果滚动到边界，阻止事件冒泡
            if ((atTop && e.deltaY < 0) || (atBottom && e.deltaY > 0)) {
                e.preventDefault();
                e.stopPropagation();
            }
        }, { passive: false });
    });
    
    // 为输出区域添加滚轮事件处理
    const outputSections = document.querySelectorAll('.output-section');
    outputSections.forEach(section => {
        section.addEventListener('wheel', function(e) {
            // 阻止滚动冒泡到页面
            e.stopPropagation();
        }, { passive: true });
    });
    
    // 为输出内容区域添加滚轮事件处理
    const outputContents = document.querySelectorAll('.output-content');
    outputContents.forEach(content => {
        content.addEventListener('wheel', function(e) {
            const atTop = this.scrollTop === 0;
            const atBottom = Math.abs(this.scrollHeight - this.scrollTop - this.clientHeight) < 1;
            
            // 如果滚动到边界，阻止事件冒泡
            if ((atTop && e.deltaY < 0) || (atBottom && e.deltaY > 0)) {
                e.preventDefault();
                e.stopPropagation();
            }
        }, { passive: false });
    });
    
    // 为可视化区域添加滚轮事件处理
    const visualizationSections = document.querySelectorAll('.visualization-section');
    visualizationSections.forEach(section => {
        section.addEventListener('wheel', function(e) {
            // 阻止滚动冒泡到页面
            e.stopPropagation();
        }, { passive: true });
    });
    
    // 为系统状态容器添加滚轮事件处理
    const systemStateContainers = document.querySelectorAll('.system-state-container');
    systemStateContainers.forEach(container => {
        container.addEventListener('wheel', function(e) {
            // 阻止滚动冒泡到页面
            e.stopPropagation();
        }, { passive: true });
    });
    
    // 为无人机列表添加滚轮事件处理
    const droneLists = document.querySelectorAll('.drone-list');
    droneLists.forEach(list => {
        list.addEventListener('wheel', function(e) {
            const atTop = this.scrollTop === 0;
            const atBottom = Math.abs(this.scrollHeight - this.scrollTop - this.clientHeight) < 1;
            
            // 如果滚动到边界，阻止事件冒泡
            if ((atTop && e.deltaY < 0) || (atBottom && e.deltaY > 0)) {
                e.preventDefault();
                e.stopPropagation();
            }
        }, { passive: false });
    });
});

