// 任务2：类型化充电管理

// 提交任务2
function submitTask2() {
    const action = document.getElementById('task2-action').value;
    const id = document.getElementById('task2-id').value;
    const type = document.getElementById('task2-type').value;
    const time = document.getElementById('task2-time').value;
    
    let input;
    
    if (action === 's') {
        // 统计查询不需要其他参数
        input = 's';
    } else if (action === 'c') {
        // 类型转换需要编号和类型（作为新类型）
        if (!id) {
            addOutputMessage('请输入无人机编号！', 'error');
            return;
        }
        input = `c ${id} ${type}`;
    } else {
        // 到达和离开需要验证输入
        if (!validateInput(id, time)) return;
        
        if (action === 'a') {
            input = `${action} ${id} ${type} ${time}`;
        } else if (action === 'd') {
            input = `${action} ${id} ${time}`;
        }
    }
    
    processInput(input, 2);
}

// 更新任务2的系统状态
function updateTask2State(input, backendText) {
    const parts = input.trim().split(' ');
    const action = parts[0];
    const targetState = taskStates[2];
    
    if (action === 'a') {
        // 到达操作: a id type time
        const id = Number(parts[1]);
        const type = normalizeType(parts[2]);
        const time = Number(parts[3]);
        
        // 检查后端是否返回错误
        const hasError = /错误[：:]/.test(backendText);
        
        if (hasError) {
            // 后端返回错误，不修改状态，保持原样
            targetState.currentTime = Math.max(targetState.currentTime, time);
            return;
        }
        
        accumulateAllEnergy(targetState, time, 2);
        
        const newDrone = {
            id: id,
            type: type,
            arrivalTime: time,
            enterStationTime: null,
            inStation: false,
            inQueue: false,
            inTempStack: false,
            position: 1,
            energyAccumulated: 0,
            lastUpdateTime: time
        };
        
        // 根据后端返回文本判断进入充电站或进入便道
        const enterStation = /进入充电站/.test(backendText);
        const waitQueue = /在便道上等待/.test(backendText);
        if (enterStation) {
            newDrone.inStation = true;
            newDrone.enterStationTime = time;
            const m = backendText.match(/停靠位置为(\d+)/);
            if (m) newDrone.position = Number(m[1]);
        } else if (waitQueue) {
            newDrone.inQueue = true;
            const m = backendText.match(/位置为(\d+)/);
            if (m) newDrone.position = Number(m[1]);
        }
        
        targetState.drones.set(id, newDrone);
        targetState.totalDrones++;
        targetState.currentTime = Math.max(targetState.currentTime, time);
        
    } else if (action === 'd') {
        // 离开操作: d id time
        const id = Number(parts[1]);
        const time = Number(parts[2]);
        
        accumulateAllEnergy(targetState, time, 2);
        
        if (targetState.drones.has(id)) {
            targetState.drones.delete(id);
            targetState.totalDrones--;
            targetState.currentTime = Math.max(targetState.currentTime, time);
            
            // 检查是否有便道无人机进入充电站
            // 任务2格式：编号为%d，种类为%c的无人机从便道进入充电站
            const queueToStationMatch = backendText.match(/编号为(\d+)，种类为([A-C])的无人机从便道进入充电站，停靠位置为(\d+)/);
            if (queueToStationMatch) {
                const promotedId = Number(queueToStationMatch[1]);
                const promotedType = queueToStationMatch[2];
                const newPosition = Number(queueToStationMatch[3]);
                
                if (targetState.drones.has(promotedId)) {
                    const promotedDrone = targetState.drones.get(promotedId);
                    
                    accumulateEnergy(promotedDrone, time, 'waiting', 2);
                    
                    promotedDrone.inQueue = false;
                    promotedDrone.inStation = true;
                    promotedDrone.position = newPosition;
                    promotedDrone.enterStationTime = time;
                    promotedDrone.lastUpdateTime = time;
                    promotedDrone.type = promotedType;
                    
                    console.log(`任务2: 无人机${promotedId}(${promotedType}型)从便道进入充电站，位置${newPosition}`);
                }
            }
        }
    } else if (action === 'c') {
        // 类型转换操作: c id newType
        const id = Number(parts[1]);
        const newType = parts[2];
        
        if (targetState.drones.has(id)) {
            const drone = targetState.drones.get(id);
            const oldType = drone.type;
            
            const successMatch = /成功将无人机(\d+)的类型转换为([A-C])/.test(backendText);
            if (successMatch) {
                const currentTime = targetState.currentTime || 0;
                
                // 关键修复：使用进入充电站的时间，而不是到达时间
                let chargingTime = 0;
                const location = drone.inStation ? 'charging' : (drone.inQueue ? 'waiting' : 'unknown');
                
                if (drone.inStation && drone.enterStationTime != null) {
                    // 在充电站：充电时间 = 当前时间 - 进入充电站时间
                    chargingTime = currentTime - drone.enterStationTime;
                } else if (drone.inQueue) {
                    // 在便道：充电时间 = 当前时间 - 到达时间（便道充电速率不同）
                    chargingTime = currentTime - (drone.arrivalTime || 0);
                } else {
                    // 其他情况
                    chargingTime = 0;
                }
                
                const newRate = getTypeRate(newType);
                const locationMultiplier = getLocationMultiplier(location, 2, newType);
                
                // 重新计算能量：充电时间 × 新类型速率 × 位置倍数
                drone.energyAccumulated = chargingTime * newRate * locationMultiplier;
                drone.type = newType;
                drone.lastUpdateTime = currentTime;
                
                console.log(`任务2: 无人机${id}类型从${oldType}转为${newType}`);
                console.log(`  位置: ${location}, 到达时间: ${drone.arrivalTime}, 进入充电站时间: ${drone.enterStationTime || '未进站'}`);
                console.log(`  充电时间: ${chargingTime}, 新速率: ${newRate}, 位置倍数: ${locationMultiplier}`);
                console.log(`  能量重算为: ${drone.energyAccumulated.toFixed(2)}`);
            }
        }
    } else if (action === 's') {
        // 统计操作不更新状态
        return;
    }
    
    // 重新统计
    targetState.stationOccupied = Array.from(targetState.drones.values()).filter(d => d.inStation).length;
    targetState.queueCount = Array.from(targetState.drones.values()).filter(d => d.inQueue).length;
    
    updateStatusPanel();
}

