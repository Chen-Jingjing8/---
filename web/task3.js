// 任务3：精确时间管理

// 提交任务3
function submitTask3() {
    const action = document.getElementById('task3-action').value;
    const id = document.getElementById('task3-id').value;
    const time = document.getElementById('task3-time').value;
    
    let input;
    
    if (action === 't') {
        // 统计查询不需要其他参数
        input = 't';
    } else {
        // 到达和离开需要验证输入
        if (!validateInput(id, time)) return;
        input = `${action} ${id} ${time}`;
    }
    
    processInput(input, 3);
}

// 更新任务3的系统状态
function updateTask3State(input, backendText) {
    const parts = input.trim().split(' ');
    const action = parts[0];
    const targetState = taskStates[3];
    
    if (action === 'a') {
        const id = Number(parts[1]);
        const time = Number(parts[2]);
        
        // 检查后端是否返回错误
        const hasError = /错误[：:]/.test(backendText);
        
        if (hasError) {
            // 后端返回错误，不修改状态，保持原样
            targetState.currentTime = Math.max(targetState.currentTime, time);
            return;
        }
        
        accumulateAllEnergy(targetState, time, 3);
        
        const newDrone = {
            id: id,
            type: 'A',
            arrivalTime: time,
            enterStationTime: null,
            inStation: false,
            inQueue: false,
            position: 1,
            energyAccumulated: 0,
            lastUpdateTime: time
        };
        
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
        const id = Number(parts[1]);
        const time = Number(parts[2]);
        
        accumulateAllEnergy(targetState, time, 3);
        
        if (targetState.drones.has(id)) {
            targetState.drones.delete(id);
            targetState.totalDrones--;
            targetState.currentTime = Math.max(targetState.currentTime, time);
            
            // 检查便道无人机进入充电站
            const queueToStationMatch = backendText.match(/编号为(\d+)的无人机从便道进入充电站，停靠位置为(\d+)/);
            if (queueToStationMatch) {
                const promotedId = Number(queueToStationMatch[1]);
                const newPosition = Number(queueToStationMatch[2]);
                
                if (targetState.drones.has(promotedId)) {
                    const promotedDrone = targetState.drones.get(promotedId);
                    accumulateEnergy(promotedDrone, time, 'waiting', 3);
                    promotedDrone.inQueue = false;
                    promotedDrone.inStation = true;
                    promotedDrone.position = newPosition;
                    promotedDrone.enterStationTime = time;
                    promotedDrone.lastUpdateTime = time;
                }
            }
        }
    } else if (action === 't') {
        // 统计操作不更新状态
        return;
    }
    
    targetState.stationOccupied = Array.from(targetState.drones.values()).filter(d => d.inStation).length;
    targetState.queueCount = Array.from(targetState.drones.values()).filter(d => d.inQueue).length;
    
    updateStatusPanel();
}

