@echo off
chcp 65001 

REM 切换到脚本所在目录，避免相对路径问题导致闪退
cd /d "%~dp0"

echo ========== 无人机管理与网络威胁预警系统 ==========
echo.
echo 正在启动后端服务...
echo.

REM 检查Python可执行文件（优先使用py -3，其次python）
set "PYEXE="
py -3 --version >nul 2>&1 && set "PYEXE=py -3"
if not defined PYEXE (
    python --version >nul 2>&1 && set "PYEXE=python"
)
if not defined PYEXE (
    echo 错误：未找到Python，请先安装 Python 3.6+ 并将其加入PATH
    echo 可从 Microsoft Store 或 python.org 安装。
    pause
    exit /b 1
)

REM 安装必要的Python包（使用国内镜像，避免SSL重试告警）
echo 检查并安装必要的Python包...
set "PIP_INDEX_URL=https://pypi.tuna.tsinghua.edu.cn/simple"
%PYEXE% -m pip install --timeout 60 --retries 2 -i %PIP_INDEX_URL% flask flask-cors
if %errorlevel% neq 0 (
    echo 依赖安装失败，请检查网络或镜像源设置。
    pause
    exit /b 1
)

REM 启动后端服务（端口为 5002，与 backend_bridge.py 保持一致）
echo 启动后端桥接服务...
echo 访问地址: http://127.0.0.1:5002
echo API状态: http://127.0.0.1:5002/api/status
echo.
echo 按 Ctrl+C 停止服务
echo.

%PYEXE% backend_bridge.py --host 127.0.0.1 --port 5002

echo.
echo 服务已停止。
pause
