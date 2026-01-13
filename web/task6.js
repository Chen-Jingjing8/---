// 任务6：安全认证充电站管理

// 提交任务6
function submitTask6() {
    const action = document.getElementById('task6-action').value;
    
    if (action === 'm') {
        const password = document.getElementById('task6-password').value;
        if (!password) {
            addOutputMessage('请输入管理员密码！', 'error');
            return;
        }
        const input = `m ${password}`;
        processInput(input, 6);
    } else if (action === 'r') {
        const id = document.getElementById('task6-id').value;
        const type = document.getElementById('task6-type').value;
        
        if (!id) {
            addOutputMessage('请输入无人机编号！', 'error');
            return;
        }
        
        const input = `r ${id} ${type}`;
        processInput(input, 6);
    } else if (action === 's') {
        // s命令不需要参数
        const input = 's';
        processInput(input, 6);
    } else {
        const id = document.getElementById('task6-id').value;
        const time = document.getElementById('task6-time').value;
        
        if (!validateInput(id, time)) return;
        
        const input = `${action} ${id} ${time}`;
        processInput(input, 6);
    }
}

// 更新任务6的系统状态
function updateTask6State(input, backendText) {
    const parts = input.trim().split(' ');
    const action = parts[0];
    const targetState = taskStates[6];
    
    if (action === 'a') {
        const id = Number(parts[1]);
        const time = Number(parts[2]);
        
        accumulateAllEnergy(targetState, time, 6);
        
        // 初始化无人机存储
        if (!targetState.drones) {
            targetState.drones = new Map();
        }
        
        // 创建或更新无人机信息
        if (!targetState.drones.has(id)) {
            targetState.drones.set(id, {
                id: id,
                type: 'A', // 默认类型，实际应该从后端获取
                arrivalTime: time,
                inWestStack: false,
                inEastStack: false,
                inQueue: false
            });
        } else {
            const drone = targetState.drones.get(id);
            drone.arrivalTime = time;
        }
        
        // 解析双入口栈状态
        parseTask6DualStackStatus(backendText, targetState);
        
        targetState.currentTime = Math.max(targetState.currentTime, time);
        
    } else if (action === 'd') {
        const id = Number(parts[1]);
        const time = Number(parts[2]);
        
        accumulateAllEnergy(targetState, time, 6);
        
        // 检查后端是否返回错误
        const hasError = /错误[：:]/.test(backendText);
        
        if (hasError) {
            // 后端返回错误，不修改状态，保持原样
            targetState.currentTime = Math.max(targetState.currentTime, time);
            updateTask6Visualization();
            return;
        }
        
        // 成功执行，显示离开提示信息
        const drone = targetState.drones.get(id);
        if (drone) {
            const arrivalTime = drone.arrivalTime || 0;
            const stayTime = Math.max(0, time - arrivalTime);
            
            // 确定离开位置
            let leaveLocation = '系统';
            if (targetState.westStack && targetState.westStack.includes(id)) {
                leaveLocation = '西口';
            } else if (targetState.eastStack && targetState.eastStack.includes(id)) {
                leaveLocation = '东口';
            } else if (targetState.task6Queue && targetState.task6Queue.includes(id)) {
                leaveLocation = '便道队列';
            }
            
            addOutputMessage(`无人机${id}从${leaveLocation}离开，停留时间：${stayTime}`, 'success');
            
            // 从系统状态中删除无人机
            targetState.drones.delete(id);
            targetState.totalDrones--;
        }
        
        // 解析双入口栈状态和移动逻辑（参照任务5）
        parseTask6DualStackStatusWithMovement(backendText, targetState);
        
        targetState.currentTime = Math.max(targetState.currentTime, time);
    } else if (action === 's') {
        // 解析状态信息
        parseTask6StatusForDisplay(backendText, targetState);
        return;
    } else if (action === 'i') {
        // 显示注册信息弹窗
        handleRegistrationInfo(backendText);
        return;
    } else if (action === 'r') {
        const id = Number(parts[1]);
        const type = parts[2];
        
        // 保存到本地存储
        saveRegistrationToLocal(id, type);
        return;
    } else if (action === 'm') {
        return;
    }
    
    updateTask6Visualization();
}

// 解析双入口栈状态和移动逻辑（参照任务5）
function parseTask6DualStackStatusWithMovement(backendText, targetState) {
    // 获取所有剩余的无人机ID
    const allDroneIds = Array.from(targetState.drones.keys());
    
    // 清空当前跟踪的栈状态
    targetState.westStack = [];
    targetState.eastStack = [];
    targetState.task6Queue = [];
    
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
        
        // 匹配"无人机X从便道进入东口"
        match = line.match(/无人机(\d+)从便道进入东口/);
        if (match) {
            lastLocation.set(parseInt(match[1]), 'east');
            continue;
        }
    }
    
    // 收集所有转移到东口栈和西口栈的无人机
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
    targetState.task6Queue = queueTransfers;
    
    // 更新每个无人机的位置标记
    targetState.drones.forEach((drone, droneId) => {
        drone.inWestStack = targetState.westStack.includes(droneId);
        drone.inEastStack = targetState.eastStack.includes(droneId);
        drone.inQueue = targetState.task6Queue.includes(droneId);
    });
    
    // 提取最大让路深度
    const bypassMatch = backendText.match(/让路深度[：:]\s*(\d+)/g);
    if (bypassMatch) {
        const depths = bypassMatch.map(m => parseInt(m.match(/\d+/)[0]));
        const maxDepth = Math.max(...depths);
        if (maxDepth > targetState.maxBypassDepth) {
            targetState.maxBypassDepth = maxDepth;
        }
    }
}

// 解析任务6双入口栈状态（参考任务5的解析逻辑）
function parseTask6DualStackStatus(backendText, targetState) {
    // 初始化双入口栈状态
    if (!targetState.westStack) targetState.westStack = [];
    if (!targetState.eastStack) targetState.eastStack = [];
    if (!targetState.task6Queue) targetState.task6Queue = [];
    if (!targetState.maxBypassDepth) targetState.maxBypassDepth = 0;
    
    // 不清空当前状态，只根据后端输出更新状态
    
    // 解析移动信息（参考任务5的解析逻辑）
    const lines = backendText.split('\n');
    for (const line of lines) {
        // 匹配"无人机X从西口进入"
        let match = line.match(/无人机(\d+)从西口进入/);
        if (match) {
            const droneId = parseInt(match[1]);
            if (!targetState.westStack.includes(droneId)) {
                targetState.westStack.push(droneId);
            }
            continue;
        }
        
        // 匹配"无人机X从西口自动转移到东口"
        match = line.match(/无人机(\d+)从西口自动转移到东口/);
        if (match) {
            const droneId = parseInt(match[1]);
            // 从西口栈移除
            const westIndex = targetState.westStack.indexOf(droneId);
            if (westIndex !== -1) {
                targetState.westStack.splice(westIndex, 1);
            }
            // 添加到东口栈
            if (!targetState.eastStack.includes(droneId)) {
                targetState.eastStack.push(droneId);
            }
            continue;
        }
        
        // 匹配"无人机X从西口转移到东口"
        match = line.match(/无人机(\d+)从西口转移到东口/);
        if (match) {
            const droneId = parseInt(match[1]);
            // 从西口栈移除
            const westIndex = targetState.westStack.indexOf(droneId);
            if (westIndex !== -1) {
                targetState.westStack.splice(westIndex, 1);
            }
            // 添加到东口栈
            if (!targetState.eastStack.includes(droneId)) {
                targetState.eastStack.push(droneId);
            }
            continue;
        }
        
        // 匹配"无人机X从便道进入西口"
        match = line.match(/无人机(\d+)从便道进入西口/);
        if (match) {
            const droneId = parseInt(match[1]);
            // 从便道移除
            const queueIndex = targetState.task6Queue.indexOf(droneId);
            if (queueIndex !== -1) {
                targetState.task6Queue.splice(queueIndex, 1);
            }
            // 添加到西口栈
            if (!targetState.westStack.includes(droneId)) {
                targetState.westStack.push(droneId);
            }
            continue;
        }
        
        // 匹配"无人机X进入便道"
        match = line.match(/无人机(\d+)进入便道/);
        if (match) {
            const droneId = parseInt(match[1]);
            // 从西口栈移除
            const westIndex = targetState.westStack.indexOf(droneId);
            if (westIndex !== -1) {
                targetState.westStack.splice(westIndex, 1);
            }
            // 添加到便道
            if (!targetState.task6Queue.includes(droneId)) {
                targetState.task6Queue.push(droneId);
            }
            continue;
        }
        
        // 匹配"无人机X从东口离开"
        match = line.match(/无人机(\d+)从东口离开/);
        if (match) {
            const droneId = parseInt(match[1]);
            // 从东口栈移除
            const eastIndex = targetState.eastStack.indexOf(droneId);
            if (eastIndex !== -1) {
                targetState.eastStack.splice(eastIndex, 1);
            }
            continue;
        }
    }
    
    
    // 更新无人机位置信息
    if (targetState.drones) {
        // 重置所有无人机的位置标记
        targetState.drones.forEach((drone, droneId) => {
            drone.inWestStack = false;
            drone.inEastStack = false;
            drone.inQueue = false;
        });
        
        // 更新西口栈无人机
        targetState.westStack.forEach(droneId => {
            if (targetState.drones.has(droneId)) {
                targetState.drones.get(droneId).inWestStack = true;
            }
        });
        
        // 更新东口栈无人机
        targetState.eastStack.forEach(droneId => {
            if (targetState.drones.has(droneId)) {
                targetState.drones.get(droneId).inEastStack = true;
            }
        });
        
        // 更新便道无人机
        targetState.task6Queue.forEach(droneId => {
            if (targetState.drones.has(droneId)) {
                targetState.drones.get(droneId).inQueue = true;
            }
        });
    }
}

// 解析任务6状态显示（用于s命令）
function parseTask6StatusForDisplay(backendText, targetState) {
    console.log('[Task6] 解析状态显示，输入文本:', backendText);
    
    // 初始化双入口栈状态
    if (!targetState.westStack) targetState.westStack = [];
    if (!targetState.eastStack) targetState.eastStack = [];
    if (!targetState.task6Queue) targetState.task6Queue = [];
    if (!targetState.maxBypassDepth) targetState.maxBypassDepth = 0;
    
    // 创建临时显示状态，不影响实际状态
    const displayState = {
        westStack: [],
        eastStack: [],
        task6Queue: [],
        maxBypassDepth: targetState.maxBypassDepth || 0
    };
    
    // 解析西口栈
    const westMatch = backendText.match(/西口栈\[\d+\/\d+\]:\s*([^\n]*)/);
    console.log('[Task6] 西口栈匹配结果:', westMatch);
    if (westMatch && westMatch[1].trim() !== '空') {
        const westContent = westMatch[1].trim();
        console.log('[Task6] 西口栈内容:', westContent);
        const westDrones = westContent.match(/\d+\([ABC]\)/g);
        if (westDrones) {
            displayState.westStack = westDrones.map(drone => {
                const match = drone.match(/(\d+)\(([ABC])\)/);
                return match ? parseInt(match[1]) : null;
            }).filter(id => id !== null);
        }
    }
    console.log('[Task6] 西口栈最终结果:', displayState.westStack);
    
    // 解析东口栈
    const eastMatch = backendText.match(/东口栈\[\d+\/\d+\]:\s*([^\n]*)/);
    console.log('[Task6] 东口栈匹配结果:', eastMatch);
    if (eastMatch && eastMatch[1].trim() !== '空') {
        const eastContent = eastMatch[1].trim();
        console.log('[Task6] 东口栈内容:', eastContent);
        const eastDrones = eastContent.match(/\d+\([ABC]\)/g);
        if (eastDrones) {
            displayState.eastStack = eastDrones.map(drone => {
                const match = drone.match(/(\d+)\(([ABC])\)/);
                return match ? parseInt(match[1]) : null;
            }).filter(id => id !== null);
        }
    }
    console.log('[Task6] 东口栈最终结果:', displayState.eastStack);
    
    // 解析便道
    const queueMatch = backendText.match(/便道\[\d+\]:\s*([^\n]*)/);
    console.log('[Task6] 便道匹配结果:', queueMatch);
    if (queueMatch && queueMatch[1].trim()) {
        const queueContent = queueMatch[1].trim();
        const queueDrones = queueContent.match(/\d+\([ABC]\)/g);
        if (queueDrones) {
            displayState.task6Queue = queueDrones.map(drone => {
                const match = drone.match(/(\d+)\(([ABC])\)/);
                return match ? parseInt(match[1]) : null;
            }).filter(id => id !== null);
        }
    }
    console.log('[Task6] 便道最终结果:', displayState.task6Queue);
    
    // 解析最大让路深度
    const bypassMatch = backendText.match(/历史最大让路深度:\s*(\d+)/);
    if (bypassMatch) {
        displayState.maxBypassDepth = parseInt(bypassMatch[1]);
    }
    
    // 使用显示状态更新可视化
    updateTask6VisualizationWithDisplayState(displayState, targetState);
}

// 使用显示状态更新任务6可视化（用于s命令）
function updateTask6VisualizationWithDisplayState(displayState, targetState) {
    // 更新西口栈
    updateTask6StackList('task6-west-stack-list', 'task6-west-stack-count', displayState.westStack, targetState);
    
    // 更新东口栈
    updateTask6StackList('task6-east-stack-list', 'task6-east-stack-count', displayState.eastStack, targetState);
    
    // 更新便道队列
    updateTask6Queue(displayState.task6Queue, targetState);
    
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

// 更新任务6的可视化
function updateTask6Visualization() {
    const targetState = taskStates[6];
    
    // 更新西口栈
    updateTask6StackList('task6-west-stack-list', 'task6-west-stack-count', targetState.westStack || [], targetState);
    
    // 更新东口栈
    updateTask6StackList('task6-east-stack-list', 'task6-east-stack-count', targetState.eastStack || [], targetState);
    
    // 更新便道队列
    updateTask6Queue(targetState.task6Queue || [], targetState);
    
    // 更新最大让路深度
    const bypassEl = document.getElementById('max-bypass-depth');
    if (bypassEl) {
        bypassEl.textContent = targetState.maxBypassDepth || 0;
    }
}

// 更新栈列表（参照任务5的动态展示）
function updateTask6StackList(listId, countId, droneIds, state) {
    const container = document.getElementById(listId);
    const countElement = document.getElementById(countId);
    
    if (!container || !countElement) {
        console.log('[Task6] 容器未找到:', listId, countId);
        return;
    }
    
    countElement.textContent = `${droneIds.length}/2`;
    
    container.style.opacity = '0.7';
    
    setTimeout(() => {
        container.innerHTML = '';
        
        // 判断是西口栈还是东口栈
        const isWestStack = listId === 'task6-west-stack-list';
        const isEastStack = listId === 'task6-east-stack-list';
        
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
                    const drone = state.drones ? state.drones.get(droneId) : null;
                    if (drone) {
                        // 东口栈索引0是出口位置
                        const isExit = isEastStack && i === 0;
                        const droneElement = createTask6DroneElement({
                            id: droneId,
                            type: drone.type,
                            arrival_time: drone.arrivalTime,
                            position: i
                        }, 'stack', isExit);
                        container.appendChild(droneElement);
                    } else {
                        // 如果没有找到无人机信息，创建简单的显示
                        const droneElement = createTask6DroneElement({
                            id: droneId,
                            type: 'A',
                            arrival_time: 0,
                            position: i
                        }, 'stack', isEastStack && i === 0);
                        container.appendChild(droneElement);
                    }
                }
            }
        }
        
        container.style.opacity = '1';
    }, 150);
}

// 创建任务6无人机元素（参照任务5）
function createTask6DroneElement(drone, location, isExitPosition = false) {
    const droneElement = document.createElement('div');
    droneElement.className = 'drone-item';
    droneElement.style.cursor = 'pointer';
    
    // 如果是出口位置，添加特殊标记
    if (isExitPosition) {
        droneElement.classList.add('exit-position');
    }
    
    // 添加点击事件，显示详细信息
    droneElement.onclick = () => showTask6DroneDetails(drone, location);
    
    const droneInfo = document.createElement('div');
    droneInfo.className = 'drone-info';
    
    const droneId = document.createElement('div');
    droneId.className = 'drone-id';
    droneId.textContent = `#${drone.id} (${drone.type || 'A'})`;
    
    const droneDetails = document.createElement('div');
    droneDetails.className = 'drone-details';
    droneDetails.textContent = `到达: ${drone.arrival_time || 0}`;
    
    droneInfo.appendChild(droneId);
    droneInfo.appendChild(droneDetails);
    
    const dronePosition = document.createElement('div');
    dronePosition.className = 'drone-position';
    dronePosition.textContent = `位置 ${drone.position || 'N/A'}`;
    
    droneElement.appendChild(droneInfo);
    droneElement.appendChild(dronePosition);
    
    return droneElement;
}

// 更新便道队列
function updateTask6Queue(droneIds, state) {
    const container = document.getElementById('task6-queue-list');
    const countElement = document.getElementById('task6-queue-count');
    
    if (!container || !countElement) return;
    
    countElement.textContent = droneIds.length;
    
    container.style.opacity = '0.7';
    
    setTimeout(() => {
        container.innerHTML = '';
        
        if (droneIds.length === 0) {
            const emptyElement = document.createElement('div');
            emptyElement.className = 'empty-queue';
            emptyElement.textContent = '暂无排队';
            container.appendChild(emptyElement);
        } else {
            droneIds.forEach((droneId, index) => {
                const drone = state.drones ? state.drones.get(droneId) : null;
                if (drone) {
                    const droneElement = createTask6DroneElement({
                        id: droneId,
                        type: drone.type,
                        arrival_time: drone.arrivalTime,
                        position: index
                    }, 'queue');
                    container.appendChild(droneElement);
                } else {
                    // 如果没有找到无人机信息，创建简单的显示
                    const droneElement = createTask6DroneElement({
                        id: droneId,
                        type: 'A',
                        arrival_time: 0,
                        position: index
                    }, 'queue');
                    container.appendChild(droneElement);
                }
            });
        }
        
        container.style.opacity = '1';
    }, 150);
}

// 创建任务6无人机元素
function createTask6DroneElement(drone, location, isExitPosition = false) {
    const droneElement = document.createElement('div');
    droneElement.className = 'drone-item';
    droneElement.style.cursor = 'pointer';
    
    // 如果是出口位置，添加特殊标记
    if (isExitPosition) {
        droneElement.classList.add('exit-position');
    }
    
    // 添加点击事件，显示详细信息
    droneElement.onclick = () => showTask6DroneDetails(drone, location);
    
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

// 显示任务6无人机详细信息
function showTask6DroneDetails(drone, location) {
    const modal = document.getElementById('drone-detail-modal');
    const content = document.getElementById('drone-detail-content');
    
    const targetState = taskStates[6];
    const currentTime = targetState.currentTime || 0;
    const stored = targetState.drones ? targetState.drones.get(drone.id) : null;
    const arrivalTime = stored ? stored.arrivalTime : drone.arrival_time || 0;
    const stayTime = Math.max(0, currentTime - arrivalTime);
    
    // 计算充电时长和充电量
    const droneType = stored ? stored.type : drone.type || 'A';
    const typeRate = getTypeRate(droneType);
    const typeLabel = normalizeType(droneType) + '类';
    
    // 根据位置确定充电效率
    let chargingEfficiency = 0;
    let chargingAmount = 0;
    let chargingTime = 0;
    
    if (stored) {
        if (stored.inEastStack) {
            // 在东口栈，充电效率为1.0
            chargingEfficiency = 1.0;
            chargingTime = stayTime;
        } else if (stored.inWestStack) {
            // 在西口栈，充电效率为1.0（与东口栈相同）
            chargingEfficiency = 1.0;
            chargingTime = stayTime;
        } else if (stored.inQueue) {
            // 在便道，充电效率为0.5
            chargingEfficiency = 0.5;
            chargingTime = stayTime;
        }
    }
    
    chargingAmount = (chargingTime * typeRate * chargingEfficiency).toFixed(2);
    
    // 确定位置名称
    let positionName = '';
    let statusText = '';
    switch (location) {
        case 'stack':
            if (stored && stored.inEastStack) {
                positionName = '东口栈（出口B）';
                statusText = '准备离站';
            } else if (stored && stored.inWestStack) {
                positionName = '西口栈（入口A）';
                statusText = '等待转移';
            } else {
                positionName = '栈中';
                statusText = '充电中';
            }
            break;
        case 'queue':
            positionName = '便道队列';
            statusText = '队列等待';
            break;
        default:
            positionName = '未知位置';
            statusText = '未知状态';
    }
    
    if (modal && content) {
        content.innerHTML = `
            <h3>无人机 #${drone.id} 详细信息</h3>
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
                    <div class="detail-value">${positionName}</div>
                </div>
                <div class="detail-item">
                    <div class="detail-label">栈内位置</div>
                    <div class="detail-value">${drone.position !== undefined ? drone.position : '?'}</div>
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
                    <div class="detail-label">充电时长</div>
                    <div class="detail-value">${chargingTime} 时间单位</div>
                </div>
                <div class="detail-item">
                    <div class="detail-label">充电效率</div>
                    <div class="detail-value">${chargingEfficiency}x</div>
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
                    <div class="detail-value">${statusText}</div>
                </div>
                <div class="detail-item">
                    <div class="detail-label">当前时间</div>
                    <div class="detail-value">${currentTime}</div>
                </div>
            </div>
        `;
        modal.style.display = 'block';
    }
}

// 获取无人机颜色
function getDroneColor(droneId) {
    const colors = ['#ff6b6b', '#4ecdc4', '#45b7d1', '#96ceb4', '#feca57', '#ff9ff3'];
    return colors[droneId % colors.length];
}

// 清空任务6的系统状态，但保留注册信息
function clearTask6SystemState() {
    const targetState = taskStates[6];
    if (!targetState) return;
    
    // 清空系统状态
    targetState.westStack = [];
    targetState.eastStack = [];
    targetState.task6Queue = [];
    targetState.maxBypassDepth = 0;
    targetState.currentTime = 0;
    
    // 保留注册信息（JSON文件自动保留）
    console.log('[Task6] 系统状态已清空，JSON文件中的注册信息已保留');
}

// 检查无人机是否在系统中（西口栈、东口栈或队列）
function isDroneInSystem(droneId) {
    const targetState = taskStates[6];
    if (!targetState) return false;
    
    // 检查西口栈
    if (targetState.westStack && targetState.westStack.includes(droneId)) {
        return true;
    }
    
    // 检查东口栈
    if (targetState.eastStack && targetState.eastStack.includes(droneId)) {
        return true;
    }
    
    // 检查队列
    if (targetState.task6Queue && targetState.task6Queue.includes(droneId)) {
        return true;
    }
    
    return false;
}

// 显示注册信息弹窗
function showRegistrationInfoModal(backendText) {
    // 解析后端返回的完整注册信息
    const registrationData = parseRegistrationInfo(backendText);
    
    // 创建弹窗
    const modal = document.createElement('div');
    modal.className = 'modal';
    modal.style.display = 'block';
    modal.style.position = 'fixed';
    modal.style.zIndex = '1000';
    modal.style.left = '0';
    modal.style.top = '0';
    modal.style.width = '100%';
    modal.style.height = '100%';
    modal.style.backgroundColor = 'rgba(0,0,0,0.5)';
    
    // 创建弹窗内容
    const modalContent = document.createElement('div');
    modalContent.style.backgroundColor = '#fefefe';
    modalContent.style.margin = '5% auto';
    modalContent.style.padding = '20px';
    modalContent.style.border = '1px solid #888';
    modalContent.style.width = '80%';
    modalContent.style.maxHeight = '80%';
    modalContent.style.overflow = 'auto';
    modalContent.style.borderRadius = '10px';
    modalContent.style.boxShadow = '0 4px 6px rgba(0, 0, 0, 0.1)';
    
    // 创建标题
    const title = document.createElement('h2');
    title.textContent = '无人机注册信息';
    title.style.marginTop = '0';
    title.style.color = '#333';
    title.style.borderBottom = '2px solid #007bff';
    title.style.paddingBottom = '10px';
    
    // 创建内容区域
    const content = document.createElement('div');
    content.style.fontFamily = 'Arial, sans-serif';
    content.style.fontSize = '14px';
    content.style.lineHeight = '1.6';
    content.style.color = '#333';
    
    if (registrationData.length === 0) {
        content.innerHTML = '<p style="text-align: center; color: #666;">暂无注册的无人机</p>';
    } else {
        // 显示注册的无人机列表
        registrationData.forEach(drone => {
            const droneItem = document.createElement('div');
            droneItem.style.border = '1px solid #ddd';
            droneItem.style.borderRadius = '8px';
            droneItem.style.padding = '20px';
            droneItem.style.margin = '15px 0';
            droneItem.style.backgroundColor = '#f9f9f9';
            droneItem.style.position = 'relative';
            
            // 创建详细信息区域
            const droneInfo = document.createElement('div');
            droneInfo.style.marginBottom = '15px';
            
            // 基本信息
            const basicInfo = document.createElement('div');
            basicInfo.innerHTML = `
                <div style="font-weight: bold; font-size: 18px; color: #333; margin-bottom: 10px;">无人机 #${drone.id}</div>
                <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 10px; font-size: 14px;">
                    <div><strong>类型:</strong> ${drone.type}类</div>
                    <div><strong>信任等级:</strong> ${drone.trustLevel}</div>
                    <div><strong>最后更新:</strong> ${drone.lastUpdate || '未知'}</div>
                    <div><strong>注册时间:</strong> ${drone.registrationTime}</div>
                </div>
            `;
            
            // 安全信息
            const securityInfo = document.createElement('div');
            securityInfo.style.marginTop = '15px';
            securityInfo.style.padding = '10px';
            securityInfo.style.backgroundColor = '#f0f0f0';
            securityInfo.style.borderRadius = '5px';
            securityInfo.innerHTML = `
                <div style="font-weight: bold; margin-bottom: 8px; color: #555;">安全信息</div>
                <div style="font-size: 12px; line-height: 1.4;">
                    <div><strong>ID哈希:</strong> ${drone.idHash || '未设置'}</div>
                    <div><strong>类型哈希:</strong> ${drone.typeHash || '未设置'}</div>
                    <div><strong>密钥哈希:</strong> ${drone.keyHash || '未设置'}</div>
                </div>
            `;
            
            // 管理员原始信息（如果有）
            let adminInfo = '';
            if (drone.rawInfo && Object.keys(drone.rawInfo).length > 0) {
                adminInfo = `
                    <div style="margin-top: 15px; padding: 10px; background-color: #e8f4fd; border-radius: 5px;">
                        <div style="font-weight: bold; margin-bottom: 8px; color: #0066cc;">[管理员] 原始信息</div>
                        <div style="font-size: 12px; line-height: 1.4;">
                            <div><strong>原始ID:</strong> ${drone.rawInfo.rawId || '未设置'}</div>
                            <div><strong>原始类型:</strong> ${drone.rawInfo.rawType || '未设置'}</div>
                            <div><strong>原始密钥:</strong> ${drone.rawInfo.rawKey || '未设置'}</div>
                        </div>
                    </div>
                `;
            }
            
            droneInfo.innerHTML = basicInfo.outerHTML + securityInfo.outerHTML + adminInfo;
            
            // 删除按钮
            const deleteBtn = document.createElement('button');
            deleteBtn.textContent = '删除';
            deleteBtn.style.backgroundColor = '#dc3545';
            deleteBtn.style.color = 'white';
            deleteBtn.style.border = 'none';
            deleteBtn.style.padding = '8px 16px';
            deleteBtn.style.borderRadius = '4px';
            deleteBtn.style.cursor = 'pointer';
            deleteBtn.style.fontSize = '12px';
            deleteBtn.style.position = 'absolute';
            deleteBtn.style.top = '15px';
            deleteBtn.style.right = '15px';
            
            deleteBtn.onclick = function() {
                // 检查无人机是否在充电栈或队列中
                if (isDroneInSystem(drone.id)) {
                    alert(`无法删除无人机 #${drone.id} 的注册信息！\n该无人机当前正在充电栈或队列中，请先让其离开系统。`);
                    return;
                }
                
                if (confirm(`确定要删除无人机 #${drone.id} 的注册信息吗？`)) {
                    // 从本地存储删除
                    removeRegistrationFromLocal(drone.id);
                    
                    // 删除操作
                    deleteDroneRegistration(drone.id);
                    
                    // 关闭当前弹窗
                    document.body.removeChild(modal);
                    
                    // 重新获取最新的注册信息
                    setTimeout(() => {
                        const input = 'i';
                        processInput(input, 6);
                    }, 100); // 稍微延迟确保后端删除操作完成
                }
            };
            
            droneItem.appendChild(droneInfo);
            droneItem.appendChild(deleteBtn);
            content.appendChild(droneItem);
        });
    }
    
    // 创建关闭按钮
    const closeBtn = document.createElement('button');
    closeBtn.textContent = '关闭';
    closeBtn.style.backgroundColor = '#007bff';
    closeBtn.style.color = 'white';
    closeBtn.style.border = 'none';
    closeBtn.style.padding = '10px 20px';
    closeBtn.style.borderRadius = '5px';
    closeBtn.style.cursor = 'pointer';
    closeBtn.style.float = 'right';
    closeBtn.style.marginTop = '20px';
    
    closeBtn.onclick = function() {
        document.body.removeChild(modal);
    };
    
    // 点击背景关闭
    modal.onclick = function(event) {
        if (event.target === modal) {
            document.body.removeChild(modal);
        }
    };
    
    // 组装弹窗
    modalContent.appendChild(title);
    modalContent.appendChild(content);
    modalContent.appendChild(closeBtn);
    modal.appendChild(modalContent);
    
    // 添加到页面
    document.body.appendChild(modal);
}

// 获取所有注册信息
function showAllRegistrationInfo() {
    // 调用后端获取注册信息
    const input = 'i';
    processInput(input, 6);
}

// 解析注册信息
function parseRegistrationInfo(backendText) {
    console.log('[Task6] 解析后端返回的注册信息:', backendText);
    const drones = [];
    const lines = backendText.split('\n');
    
    // 检查是否有注册信息
    const totalMatch = backendText.match(/总数:\s*(\d+)架/);
    if (totalMatch && parseInt(totalMatch[1]) === 0) {
        console.log('[Task6] 后端没有注册信息，返回空数组');
        return [];
    }
    
    console.log('[Task6] 总行数:', lines.length);
    console.log('[Task6] 包含"无人机ID:"的行数:', lines.filter(line => line.includes('无人机ID:')).length);
    
    for (let i = 0; i < lines.length; i++) {
        const line = lines[i];
        
        // 匹配无人机ID
        if (line.includes('无人机ID:')) {
            console.log('[Task6] 找到无人机ID行:', line);
            const idMatch = line.match(/无人机ID:\s*(\d+)/);
            if (idMatch) {
                console.log('[Task6] 解析无人机ID:', idMatch[1]);
                const drone = {
                    id: parseInt(idMatch[1]),
                    type: 'A',
                    registrationTime: new Date().toLocaleString(),
                    lastUpdate: '',
                    idHash: '',
                    typeHash: '',
                    keyHash: '',
                    trustLevel: 0,
                    rawInfo: {}
                };
                
                // 解析后续的详细信息
                let j = i + 1;
                while (j < lines.length && !lines[j].includes('----------------------')) {
                    const currentLine = lines[j];
                    
                    if (currentLine.includes('类型:')) {
                        const typeMatch = currentLine.match(/类型:\s*([ABC])/);
                        if (typeMatch) {
                            drone.type = typeMatch[1];
                        }
                    } else if (currentLine.includes('最后更新:')) {
                        drone.lastUpdate = currentLine.replace('最后更新:', '').trim();
                    } else if (currentLine.includes('ID哈希:')) {
                        drone.idHash = currentLine.replace('ID哈希:', '').trim();
                    } else if (currentLine.includes('类型哈希:')) {
                        drone.typeHash = currentLine.replace('类型哈希:', '').trim();
                    } else if (currentLine.includes('密钥哈希:')) {
                        drone.keyHash = currentLine.replace('密钥哈希:', '').trim();
                    } else if (currentLine.includes('信任等级:')) {
                        const trustMatch = currentLine.match(/信任等级:\s*(\d+)/);
                        if (trustMatch) {
                            drone.trustLevel = parseInt(trustMatch[1]);
                        }
                    } else if (currentLine.includes('[管理员] 原始信息：')) {
                        // 解析管理员原始信息
                        j++;
                        while (j < lines.length && !lines[j].includes('信任等级:') && !lines[j].includes('----------------------')) {
                            const adminLine = lines[j];
                            if (adminLine.includes('ID:')) {
                                drone.rawInfo.rawId = adminLine.replace('ID:', '').trim();
                            } else if (adminLine.includes('类型:')) {
                                drone.rawInfo.rawType = adminLine.replace('类型:', '').trim();
                            } else if (adminLine.includes('密钥:')) {
                                drone.rawInfo.rawKey = adminLine.replace('密钥:', '').trim();
                            }
                            j++;
                        }
                        j--; // 回退一行，因为外层循环会自增
                    }
                    
                    j++;
                }
                
                // 调试输出
                console.log('[Task6] 解析完成，无人机信息:', drone);
                drones.push(drone);
            }
        }
    }
    
    console.log('[Task6] 最终解析结果，无人机数量:', drones.length);
    console.log('[Task6] 解析的无人机ID列表:', drones.map(d => d.id));
    return drones;
}

// 保存注册信息到本地存储（已禁用，改用JSON文件）
function saveRegistrationToLocal(droneId, droneType) {
    // 不再使用本地存储，改为使用JSON文件
    console.log('[Task6] 注册信息已保存到JSON文件');
}

// 从本地存储获取注册信息
function getLocalRegistrations() {
    const stored = localStorage.getItem('task6_registrations');
    return stored ? JSON.parse(stored) : [];
}

// 从本地存储删除注册信息
function removeRegistrationFromLocal(droneId) {
    const registrations = getLocalRegistrations();
    const filtered = registrations.filter(drone => drone.id !== droneId);
    localStorage.setItem('task6_registrations', JSON.stringify(filtered));
}

// 删除无人机注册信息
function deleteDroneRegistration(droneId) {
    // 调用后端删除注册信息
    const input = `delete ${droneId}`;
    processInput(input, 6);
}

// 处理注册信息显示
function handleRegistrationInfo(backendText) {
    // 检查是否是删除操作后的刷新
    if (backendText.includes('注册信息已删除') || backendText.includes('未注册')) {
        // 删除操作完成，重新获取最新信息
        setTimeout(() => {
            const input = 'i';
            processInput(input, 6);
        }, 200);
        return;
    }
    
    showRegistrationInfoModal(backendText);
}

// 已禁用自动恢复注册信息功能

// 已禁用系统状态恢复功能

// 清除本地存储的注册信息（用于调试）
function clearTask6LocalStorage() {
    localStorage.removeItem('task6_registrations');
    console.log('[Task6] 已清除本地存储的注册信息');
}

// 完全清除任务6的所有数据（用于调试）
function clearTask6AllData() {
    localStorage.removeItem('task6_registrations');
    sessionStorage.removeItem('task6_registrations_restored');
    sessionStorage.removeItem('task6_page_loaded');
    sessionStorage.removeItem('task6_system_restored');
    console.log('[Task6] 已清除所有任务6数据');
}

// 清除恢复标记（用于调试）
function clearTask6RestoreFlags() {
    sessionStorage.removeItem('task6_registrations_restored');
    sessionStorage.removeItem('task6_page_loaded');
    console.log('[Task6] 已清除所有恢复标记');
}

// 已移除自动恢复功能

// 已禁用所有自动恢复功能

// ==================== 组网攻击可视化功能 ====================

// 显示组网攻击可视化
async function showNetworkAttackVisualization() {
    // 显示组网可视化容器
    const container = document.getElementById('network-visualization-container');
    if (container) {
        container.style.display = 'block';
    }
    
    // 获取组网状态
    try {
        const result = await callBackend('n', 6);
        updateNetworkVisualization(result);
        
        // 如果组网中有无人机，显示可视化
        if (result && result.includes('组网中无人机数量')) {
            addOutputMessage('组网状态已更新', 'success');
        }
    } catch (error) {
        addOutputMessage('获取组网状态失败：' + error.message, 'error');
    }
}

// 更新组网可视化
function updateNetworkVisualization(backendText) {
    if (!backendText) return;
    
    const canvas = document.getElementById('network-canvas');
    const statusText = document.getElementById('network-status-text');
    const networkCountDisplay = document.getElementById('network-count-display');
    const maliciousInfo = document.getElementById('malicious-drone-info');
    const maliciousId = document.getElementById('malicious-drone-id');
    const poisonStatus = document.getElementById('poison-status');
    const poisonText = document.getElementById('poison-status-text');
    const crashStatus = document.getElementById('crash-status');
    const crashText = document.getElementById('crash-status-text');
    
    if (!canvas) return;
    
    // 解析组网状态
    const networkMatch = backendText.match(/组网中无人机数量:\s*(\d+)\/6/);
    const networkCount = networkMatch ? parseInt(networkMatch[1]) : 0;
    
    // 解析组网中的无人机ID
    const dronesMatch = backendText.match(/组网中的无人机:\s*([^\n]+)/);
    const droneIds = dronesMatch ? dronesMatch[1].trim().split(/\s+/).map(id => parseInt(id)) : [];
    
    // 解析恶意无人机ID
    const maliciousMatch = backendText.match(/恶意无人机ID:\s*(\d+)/);
    const maliciousDroneId = maliciousMatch ? parseInt(maliciousMatch[1]) : -1;
    
    // 解析组网状态
    const stateMatch = backendText.match(/组网状态:\s*([^\n]+)/);
    const networkState = stateMatch ? stateMatch[1].trim() : '正常';
    
    // 解析坠毁的无人机
    const crashedMatch = backendText.match(/坠毁的无人机:\s*([^\n]+)/);
    const crashedDrones = crashedMatch ? crashedMatch[1].trim().split(/\s+/).map(id => parseInt(id)) : [];
    
    // 更新显示
    if (networkCountDisplay) {
        networkCountDisplay.textContent = `${networkCount}/6`;
    }
    
    if (statusText) {
        statusText.textContent = networkState;
        if (networkState === '已投毒') {
            statusText.style.color = '#ffc107';
        } else if (networkState === '已坠毁') {
            statusText.style.color = '#dc3545';
        } else {
            statusText.style.color = '#28a745';
        }
    }
    
    // 显示恶意无人机信息
    if (maliciousDroneId > 0) {
        if (maliciousInfo) maliciousInfo.style.display = 'flex';
        if (maliciousId) maliciousId.textContent = `#${maliciousDroneId}`;
    } else {
        if (maliciousInfo) maliciousInfo.style.display = 'none';
    }
    
    // 显示投毒状态
    if (networkState === '已投毒' || networkState === '已坠毁') {
        if (poisonStatus) poisonStatus.style.display = 'flex';
        if (poisonText) {
            poisonText.textContent = networkState === '已坠毁' ? '已投毒（已触发坠毁）' : '已投毒';
            poisonText.style.color = '#ffc107';
        }
    } else {
        if (poisonStatus) poisonStatus.style.display = 'none';
    }
    
    // 显示坠毁状态
    if (networkState === '已坠毁' || crashedDrones.length > 0) {
        if (crashStatus) crashStatus.style.display = 'flex';
        if (crashText) {
            crashText.textContent = `已坠毁 (${crashedDrones.length}架)`;
            crashText.style.color = '#dc3545';
        }
    } else {
        if (crashStatus) crashStatus.style.display = 'none';
    }
    
    // 清空画布
    canvas.innerHTML = '';
    
    // 绘制组网节点
    if (networkCount > 0) {
        droneIds.forEach((droneId, index) => {
            const node = createNetworkNode(droneId, maliciousDroneId === droneId, 
                                         networkState === '已投毒' || networkState === '已坠毁',
                                         crashedDrones.includes(droneId));
            canvas.appendChild(node);
        });
        
        // 绘制连接线
        if (droneIds.length > 1) {
            drawNetworkConnections(canvas, droneIds, networkState === '已投毒' || networkState === '已坠毁',
                                  networkState === '已坠毁');
        }
    } else {
        const emptyMsg = document.createElement('div');
        emptyMsg.style.textAlign = 'center';
        emptyMsg.style.color = '#666';
        emptyMsg.style.padding = '40px';
        emptyMsg.innerHTML = '<i class="fas fa-network-wired" style="font-size: 3rem; opacity: 0.3;"></i><br><br>暂无无人机加入组网';
        canvas.appendChild(emptyMsg);
    }
}

// 创建组网节点
function createNetworkNode(droneId, isMalicious, isPoisoned, isCrashed) {
    const node = document.createElement('div');
    node.className = 'network-node';
    node.id = `network-node-${droneId}`;
    
    if (isCrashed) {
        node.classList.add('crashed');
    } else if (isMalicious) {
        node.classList.add('malicious');
    } else if (isPoisoned) {
        node.classList.add('poisoned');
    } else {
        node.classList.add('normal');
    }
    
    const idDiv = document.createElement('div');
    idDiv.className = 'network-node-id';
    idDiv.textContent = `#${droneId}`;
    
    const labelDiv = document.createElement('div');
    labelDiv.className = 'network-node-label';
    if (isMalicious) {
        labelDiv.textContent = '恶意';
        labelDiv.style.color = '#fff';
    } else if (isCrashed) {
        labelDiv.textContent = '已坠毁';
        labelDiv.style.color = '#fff';
    } else if (isPoisoned) {
        labelDiv.textContent = '已投毒';
        labelDiv.style.color = '#fff';
    } else {
        labelDiv.textContent = '正常';
        labelDiv.style.color = '#fff';
    }
    
    node.appendChild(idDiv);
    node.appendChild(labelDiv);
    
    return node;
}

// 绘制组网连接线
function drawNetworkConnections(canvas, droneIds, isPoisoned, isCrashed) {
    const nodes = droneIds.map(id => document.getElementById(`network-node-${id}`));
    const validNodes = nodes.filter(node => node !== null);
    
    if (validNodes.length < 2) return;
    
    // 创建中心节点（用于星型连接）
    const centerX = canvas.offsetWidth / 2;
    const centerY = canvas.offsetHeight / 2;
    
    validNodes.forEach((node, index) => {
        const rect = node.getBoundingClientRect();
        const canvasRect = canvas.getBoundingClientRect();
        const nodeX = rect.left - canvasRect.left + rect.width / 2;
        const nodeY = rect.top - canvasRect.top + rect.height / 2;
        
        // 创建连接线到中心
        const line = document.createElement('div');
        line.className = 'network-connection';
        
        const dx = centerX - nodeX;
        const dy = centerY - nodeY;
        const length = Math.sqrt(dx * dx + dy * dy);
        const angle = Math.atan2(dy, dx) * 180 / Math.PI;
        
        line.style.width = `${length}px`;
        line.style.left = `${nodeX}px`;
        line.style.top = `${nodeY}px`;
        line.style.transform = `rotate(${angle}deg)`;
        line.style.transformOrigin = '0 0';
        
        if (isCrashed) {
            line.classList.add('crashed');
        } else if (isPoisoned) {
            line.classList.add('poisoned');
        }
        
        canvas.appendChild(line);
    });
}

// 触发攻击演示
async function triggerAttackDemo(maliciousDroneId) {
    if (!maliciousDroneId) {
        maliciousDroneId = prompt('请输入恶意无人机编号：');
        if (!maliciousDroneId) return;
    }
    
    try {
        // 发送攻击命令
        const result = await callBackend(`x ${maliciousDroneId}`, 6);
        addOutputMessage(result, 'warning');
        
        // 更新组网可视化
        setTimeout(async () => {
            const networkResult = await callBackend('n', 6);
            updateNetworkVisualization(networkResult);
        }, 500);
    } catch (error) {
        addOutputMessage('攻击演示失败：' + error.message, 'error');
    }
}
