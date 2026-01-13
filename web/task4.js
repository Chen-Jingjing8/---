// 任务4：紧急调度系统

// 提交任务4
function submitTask4() {
    const action = document.getElementById('task4-action').value;
    
    if (action === 'u') {
        const aCount = document.getElementById('emergency-a').value;
        const bCount = document.getElementById('emergency-b').value;
        const cCount = document.getElementById('emergency-c').value;
        const totalWork = document.getElementById('emergency-x').value;
        
        if (!validateEmergencyInput(aCount, bCount, cCount, totalWork)) return;
        // 后端改为通过 'e' 进入紧急任务并随后读取参数
        const input = `e\n${aCount} ${bCount} ${cCount} ${totalWork}`;
        processInput(input, 4);
    } else {
        const id = document.getElementById('task4-id').value;
        const type = document.getElementById('task4-type').value;
        const time = document.getElementById('task4-time').value;
        
        // 后端命令格式：
        // a: a id type time (必须)
        // d: d id [time] (time可选，不提供则使用系统当前时间)
        let input;
        if (action === 'a') {
            if (!validateInput(id, time)) return;
            input = `a ${id} ${type} ${time}`;
        } else if (action === 'd') {
            // d命令：id必须，time可选
            if (!id) {
                addOutputMessage('请输入无人机编号！', 'error');
                return;
            }
            if (time && parseInt(time) < 0) {
                addOutputMessage('时间不能为负数！', 'error');
                return;
            }
            if (time) {
                input = `d ${id} ${time}`;
            } else {
                input = `d ${id}`;  // 不提供时间，后端使用系统当前时间
            }
        } else {
            input = `${action}`; // 预留其他命令
        }
        processInput(input, 4);
    }
}

// 更新任务4的系统状态
function updateTask4State(input, backendText) {
    // 处理包含换行符的输入（紧急调度）
    const normalizedInput = input.trim().replace(/\n/g, ' ');
    const parts = normalizedInput.split(' ').filter(p => p.length > 0);
    const action = parts[0];
    const targetState = taskStates[4];
    
    if (action === 'a') {
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
        
        accumulateAllEnergy(targetState, time, 4);
        
        const newDrone = {
            id: id,
            type: type,
            arrivalTime: time,
            enterStationTime: null,
            inStation: false,
            inQueue: false,
            position: 1,
            energyAccumulated: 0,
            lastUpdateTime: time
        };
        
        const enterStation = /进入充电站/.test(backendText);
        const waitQueue = /在便道等待/.test(backendText);
        if (enterStation) {
            newDrone.inStation = true;
            newDrone.enterStationTime = time;
            const m = backendText.match(/位置为(\d+)/);
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
        // 时间参数可选，可能在parts[2]位置
        const time = parts.length >= 3 ? Number(parts[2]) : null;
        
        // 从后端输出中提取实际的离开时间
        const timeMatch = backendText.match(/在时刻(\d+)离开/);
        const actualTime = timeMatch ? Number(timeMatch[1]) : (time || targetState.currentTime);
        
        accumulateAllEnergy(targetState, actualTime, 4);
        
        if (targetState.drones.has(id)) {
            targetState.drones.delete(id);
            targetState.totalDrones--;
            targetState.currentTime = Math.max(targetState.currentTime, actualTime);
            
            // 检查便道无人机进入充电站（后端输出格式已更改）
            const queueToStationMatch = backendText.match(/编号为(\d+)的无人机从便道进入充电站，位置为(\d+)/);
            if (queueToStationMatch) {
                const promotedId = Number(queueToStationMatch[1]);
                const newPosition = Number(queueToStationMatch[2]);
                
                if (targetState.drones.has(promotedId)) {
                    const promotedDrone = targetState.drones.get(promotedId);
                    accumulateEnergy(promotedDrone, actualTime, 'waiting', 4);
                    promotedDrone.inQueue = false;
                    promotedDrone.inStation = true;
                    promotedDrone.position = newPosition;
                    promotedDrone.enterStationTime = actualTime;
                    promotedDrone.lastUpdateTime = actualTime;
                }
            }
        }
    } else if (action === 'u' || action === 'e') {
        // 紧急调度操作不更新状态
        return;
    }
    
    targetState.stationOccupied = Array.from(targetState.drones.values()).filter(d => d.inStation).length;
    targetState.queueCount = Array.from(targetState.drones.values()).filter(d => d.inQueue).length;
    
    updateStatusPanel();
}

// 显示紧急调度结果弹窗
function showEmergencyDispatchResult() {
    const modal = document.getElementById('emergency-dispatch-modal');
    const content = document.getElementById('emergency-dispatch-content');
    
    if (!modal || !content) return;
    
    // 检查是否有调度结果
    if (!emergencyDispatchResult) {
        content.innerHTML = '<div style="text-align: center; padding: 20px; color: #666;">暂无调度结果，请先执行紧急调度操作。</div>';
        modal.style.display = 'block';
        return;
    }
    
    // 调试：打印实际结果
    console.log('Emergency dispatch result:', emergencyDispatchResult);
    console.log('Result type:', typeof emergencyDispatchResult);
    console.log('Result length:', emergencyDispatchResult ? emergencyDispatchResult.length : 0);
    
    // 格式化显示调度结果 - 使用HTML转义防止XSS
    const escapeHtml = (text) => {
        if (!text) return '';
        const div = document.createElement('div');
        div.textContent = text;
        return div.innerHTML;
    };
    
    // 将制表符分隔的表格转换为HTML表格
    const formatTableContent = (text) => {
        const lines = text.split('\n');
        const result = [];
        let tableRows = [];
        
        // 生成HTML表格的函数
        const generateTable = (rows) => {
            if (rows.length === 0) return '';
            
            const headerRow = rows[0];
            const dataRows = rows.slice(1);
            
            let tableHtml = '<table style="width: 100%; border-collapse: collapse; margin: 15px 0; font-family: \'Courier New\', monospace; font-size: 13px; background: white;">';
            
            // 表头
            if (headerRow.length > 0) {
                tableHtml += '<thead><tr style="background-color: #667eea; color: white;">';
                headerRow.forEach(cell => {
                    tableHtml += `<th style="border: 1px solid #ddd; padding: 10px; text-align: left; font-weight: bold;">${escapeHtml(cell)}</th>`;
                });
                tableHtml += '</tr></thead>';
            }
            
            // 数据行
            if (dataRows.length > 0) {
                tableHtml += '<tbody>';
                dataRows.forEach((row, idx) => {
                    const bgColor = idx % 2 === 0 ? '#f8f9fa' : 'white';
                    tableHtml += `<tr style="background-color: ${bgColor};">`;
                    // 确保列数一致
                    for (let j = 0; j < headerRow.length; j++) {
                        const cell = row[j] || '';
                        tableHtml += `<td style="border: 1px solid #ddd; padding: 8px;">${escapeHtml(cell)}</td>`;
                    }
                    tableHtml += '</tr>';
                });
                tableHtml += '</tbody>';
            }
            
            tableHtml += '</table>';
            return tableHtml;
        };
        
        for (let i = 0; i < lines.length; i++) {
            const line = lines[i];
            // 检测表格行（包含制表符，且不是空行）
            const isTableRow = line.includes('\t') && line.trim().length > 0;
            
            if (isTableRow) {
                // 解析表格行（过滤掉空字符串，处理连续制表符的情况）
                const cells = line.split('\t').map(cell => cell.trim()).filter(cell => cell.length > 0);
                tableRows.push(cells);
            } else {
                // 遇到非表格行
                if (tableRows.length > 0) {
                    // 先输出之前收集的表格
                    result.push(generateTable(tableRows));
                    tableRows = [];
                }
                // 输出非表格内容
                if (line.trim() || i === 0 || i === lines.length - 1) {
                    result.push(escapeHtml(line));
                }
            }
        }
        
        // 处理最后可能还在表格中的内容
        if (tableRows.length > 0) {
            result.push(generateTable(tableRows));
        }
        
        return result.join('<br>');
    };
    
    // 确保结果是字符串
    const resultText = String(emergencyDispatchResult || '');
    
    // 如果结果很短且只包含提示信息，显示更友好的提示
    const trimmedResult = resultText.trim();
    if (trimmedResult.length < 50 && 
        (trimmedResult.includes('执行完成') || trimmedResult.includes('请点击'))) {
        content.innerHTML = `
            <div style="text-align: center; padding: 30px; color: #666;">
                <p style="margin-bottom: 15px; font-size: 16px;">${escapeHtml(resultText).replace(/\n/g, '<br>')}</p>
                <p style="font-size: 14px; color: #999;">如果这是您看到的消息，可能是后端返回的结果不完整。请检查后端是否正确输出了调度结果。</p>
            </div>
        `;
    } else {
        // 格式化内容，将表格转换为HTML表格
        const formattedContent = formatTableContent(resultText);
        
        content.innerHTML = `
            <div style="font-family: 'Courier New', monospace; background: #f5f5f5; padding: 20px; border-radius: 8px; line-height: 1.8; font-size: 14px; color: #333;">
                ${formattedContent}
            </div>
        `;
    }
    
    modal.style.display = 'block';
}

// 关闭紧急调度结果弹窗
function closeEmergencyDispatchModal() {
    const modal = document.getElementById('emergency-dispatch-modal');
    if (modal) {
        modal.style.display = 'none';
    }
}

// 点击弹窗外部关闭
document.addEventListener('DOMContentLoaded', function() {
    const modal = document.getElementById('emergency-dispatch-modal');
    if (modal) {
        modal.addEventListener('click', function(event) {
            if (event.target === modal) {
                closeEmergencyDispatchModal();
            }
        });
    }
});

