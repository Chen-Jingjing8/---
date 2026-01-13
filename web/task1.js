// 任务1：基础充电站管理

// 提交任务1
function submitTask1() {
    const action = document.getElementById('task1-action').value;
    const id = document.getElementById('task1-id').value;
    const time = document.getElementById('task1-time').value;
    
    let input;
    
    if (action === 't') {
        // 统计查询不需要其他参数
        input = 't';
    } else {
        // 到达和离开需要验证输入
        if (!validateInput(id, time)) return;
        input = `${action} ${id} ${time}`;
    }
    
    processInput(input, 1);
}

// 更新任务1的系统状态
function updateTask1State(input, backendText) {
    const parts = input.trim().split(' ');
    const action = parts[0];
    const targetState = taskStates[1];
    
    if (action === 'a') {
        // 到达操作
        const id = Number(parts[1]);
        const time = Number(parts[2]);
        
        // 检查后端是否返回错误
        const hasError = /错误[：:]/.test(backendText);
        
        if (hasError) {
            // 后端返回错误，不修改状态，保持原样
            targetState.currentTime = Math.max(targetState.currentTime, time);
            return;
        }
        
        // 先把所有已有无人机按当前事件时间结算能量
        accumulateAllEnergy(targetState, time, 1);
        
        // 创建新无人机
        const newDrone = {
            id: id,
            type: 'A',
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
        // 离开操作
        const id = Number(parts[1]);
        const time = Number(parts[2]);
        
        accumulateAllEnergy(targetState, time, 1);
        
        if (targetState.drones.has(id)) {
            targetState.drones.delete(id);
            targetState.totalDrones--;
            targetState.currentTime = Math.max(targetState.currentTime, time);
            
            // 检查是否有便道无人机进入充电站
            const queueToStationMatch = backendText.match(/编号为(\d+)的无人机从便道进入充电站，停靠位置为(\d+)/);
            if (queueToStationMatch) {
                const promotedId = Number(queueToStationMatch[1]);
                const newPosition = Number(queueToStationMatch[2]);
                
                if (targetState.drones.has(promotedId)) {
                    const promotedDrone = targetState.drones.get(promotedId);
                    
                    accumulateEnergy(promotedDrone, time, 'waiting', 1);
                    
                    promotedDrone.inQueue = false;
                    promotedDrone.inStation = true;
                    promotedDrone.position = newPosition;
                    promotedDrone.enterStationTime = time;
                    promotedDrone.lastUpdateTime = time;
                    
                    console.log(`无人机${promotedId}从便道进入充电站，位置${newPosition}`);
                }
            }
        }
    } else if (action === 't') {
        // 统计操作不更新状态
        return;
    }
    
    // 重新统计
    targetState.stationOccupied = Array.from(targetState.drones.values()).filter(d => d.inStation).length;
    targetState.queueCount = Array.from(targetState.drones.values()).filter(d => d.inQueue).length;
    
    updateStatusPanel();
}

