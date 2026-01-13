// 主控制脚本

// 初始化页面
document.addEventListener('DOMContentLoaded', function() {
    initializeApp();
    setupEventListeners();
    updateStatusPanel();
    refreshSystemState();
});

// 初始化应用
async function initializeApp() {
    showTaskForm(1);
    switchVisualization(1); // 初始化时显示任务1的可视化
    addOutputMessage('系统已启动，欢迎使用无人机管理与网络威胁预警系统！', 'success');
    
    await checkBackendStatus();
    startPeriodicRefresh();
}

// 设置事件监听器
function setupEventListeners() {
    // 任务选择按钮
    document.querySelectorAll('.task-btn').forEach(btn => {
        btn.addEventListener('click', function() {
            const taskNum = parseInt(this.dataset.task);
            switchTask(taskNum);
        });
    });

    // 任务2的操作类型切换
    document.getElementById('task2-action').addEventListener('change', function() {
        const typeLabel = document.getElementById('task2-type-label');
        if (this.value === 'c') {
            typeLabel.textContent = '新类型 (转换后的类型)';
        } else if (this.value === 'a') {
            typeLabel.textContent = '无人机类型';
        } else {
            typeLabel.textContent = '无人机类型';
        }
    });

    // 任务4的紧急调度切换
    document.getElementById('task4-action').addEventListener('change', function() {
        const emergencyParams = document.getElementById('emergency-params');
        if (this.value === 'u') {
            emergencyParams.classList.remove('hidden');
        } else {
            emergencyParams.classList.add('hidden');
        }
    });
}

// 切换任务
function switchTask(taskNum) {
    currentTask = taskNum;
    systemState = taskStates[currentTask];
    
    document.querySelectorAll('.task-btn').forEach(btn => {
        btn.classList.remove('active');
    });
    document.querySelector(`[data-task="${taskNum}"]`).classList.add('active');
    
    showTaskForm(taskNum);
    updateTaskTitle(taskNum);
    switchVisualization(taskNum);
    
    // 切换任务时刷新系统状态
    refreshSystemState();
    addOutputMessage(`已切换到任务${taskNum}`, 'success');
}

// 切换可视化显示
function switchVisualization(taskNum) {
    const generalViz = document.getElementById('general-visualization');
    const task5Viz = document.getElementById('task5-visualization');
    const task6Viz = document.getElementById('task6-visualization');
    const task7Viz = document.getElementById('task7-visualization');
    
    // 隐藏所有可视化
    if (generalViz) generalViz.classList.add('hidden');
    if (task5Viz) task5Viz.classList.add('hidden');
    if (task6Viz) task6Viz.classList.add('hidden');
    if (task7Viz) task7Viz.classList.add('hidden');
    
    if (taskNum === 5) {
        // 显示任务5的双栈可视化
        if (task5Viz) task5Viz.classList.remove('hidden');
    } else if (taskNum === 6) {
        // 显示任务6的双栈可视化
        if (task6Viz) task6Viz.classList.remove('hidden');
    } else if (taskNum === 7) {
        // 显示任务7的优先级调度可视化
        if (task7Viz) task7Viz.classList.remove('hidden');
    } else {
        // 显示通用可视化
        if (generalViz) generalViz.classList.remove('hidden');
    }
}

// 显示任务表单
function showTaskForm(taskNum) {
    document.querySelectorAll('.input-form').forEach(form => {
        form.classList.add('hidden');
    });
    
    document.getElementById(`form-task${taskNum}`).classList.remove('hidden');
}

// 更新任务描述
function updateTaskTitle(taskNum) {
    const title = document.getElementById('task-title');
    
    switch(taskNum) {
        case 1:
            title.textContent = '任务1：基础充电站管理';
            break;
        case 2:
            title.textContent = '任务2：类型化充电管理';
            break;
        case 3:
            title.textContent = '任务3：精确时间管理';
            break;
        case 4:
            title.textContent = '任务4：紧急调度系统';
            break;
        case 5:
            title.textContent = '任务5：双入口充电站管理';
            break;
        case 6:
            title.textContent = '任务6：安全认证充电站管理';
            break;
        case 7:
            title.textContent = '任务7：高响应比优先调度算法';
            break;
    }
}

