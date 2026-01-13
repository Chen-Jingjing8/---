#!/usr/bin/env python3
"""
后端桥接服务
用于连接Web前端和C++后端动态库
"""

import ctypes
import os
import sys
from flask import Flask, request, jsonify, send_from_directory
from flask_cors import CORS
import threading
import time
import subprocess
import tempfile
import json
import shutil
import uuid

app = Flask(__name__)
CORS(app, origins=['*'], methods=['GET', 'POST'], allow_headers=['Content-Type'])  # 允许跨域请求

# 全局变量存储加载的库
loaded_libs = {}
temp_dll_files = []  # 记录临时DLL文件路径，用于清理
# 任务4回退到EXE时，为了保持“会话状态”，我们在服务端累积已发送命令并在每次调用时重放
task4_input_history = ""

def ensure_windows_dll_search_path():
    try:
        if not sys.platform.startswith('win'):
            return
        web_dir = os.path.dirname(os.path.abspath(__file__))
        project_root = os.path.dirname(web_dir)
        bin_dir = os.path.join(project_root, 'bin')
        if os.path.isdir(bin_dir):
            # 优先使用 add_dll_directory（Py3.8+）
            try:
                os.add_dll_directory(bin_dir)
            except Exception:
                # 退化为修改 PATH
                os.environ['PATH'] = bin_dir + os.pathsep + os.environ.get('PATH', '')
    except Exception:
        pass

def load_library(task_num, force_reload=False):
    """加载指定任务的动态库"""
    if (not force_reload) and task_num in loaded_libs:
        return loaded_libs[task_num]
    
    lib_name = f"libtask{task_num}"
    
    # 根据操作系统选择库文件
    if sys.platform.startswith('win'):
        lib_file = f"{lib_name}.dll"
    else:
        lib_file = f"{lib_name}.so"
    
    try:
        # 确保 Windows 下能找到依赖 DLL（如 libgcc_s_seh-1.dll 等）
        ensure_windows_dll_search_path()
        # 尝试多个可能的路径
        # 以 web 目录为基准，优先从项目 bin 目录加载
        web_dir = os.path.dirname(os.path.abspath(__file__))
        project_root = os.path.dirname(web_dir)
        possible_paths = [
            os.path.join(project_root, "bin", lib_file),
            os.path.join(web_dir, lib_file),
            os.path.join(os.getcwd(), lib_file),
            os.path.join(os.path.dirname(os.getcwd()), "bin", lib_file),
            os.path.join("..", "bin", lib_file),
            lib_file,
        ]
        
        lib_path = None
        for path in possible_paths:
            if os.path.exists(path):
                lib_path = path
                break
        
        if lib_path is None:
            raise FileNotFoundError(f"找不到库文件: {lib_file}")
        
        # 加载动态库（如果需要重新加载，先复制文件）
        if force_reload and sys.platform.startswith('win'):
            # 为重新加载创建临时副本，保存到项目目录下的temp_dlls文件夹
            web_dir = os.path.dirname(os.path.abspath(__file__))
            project_root = os.path.dirname(web_dir)
            temp_dir = os.path.join(project_root, "temp_dlls")
            
            # 确保临时目录存在
            os.makedirs(temp_dir, exist_ok=True)
            
            unique_id = str(uuid.uuid4())[:8]
            base_name = os.path.splitext(os.path.basename(lib_path))[0]
            temp_lib_path = os.path.join(temp_dir, f"{base_name}_{unique_id}.dll")
            
            try:
                shutil.copy2(lib_path, temp_lib_path)
                lib = ctypes.CDLL(temp_lib_path)
                temp_dll_files.append(temp_lib_path)  # 记录临时文件路径
                print(f"使用临时DLL副本: {temp_lib_path}")
                print(f"临时目录: {temp_dir}")
            except Exception as e:
                print(f"复制DLL失败，使用原文件: {e}")
                lib = ctypes.CDLL(lib_path)
        else:
            lib = ctypes.CDLL(lib_path)
        
        # 设置函数签名
        lib.simulateChargingStation.argtypes = [ctypes.c_char_p]
        lib.simulateChargingStation.restype = ctypes.c_char_p
        
        loaded_libs[task_num] = lib
        print(f"成功加载库: {lib_path}")
        return lib
        
    except Exception as e:
        print(f"加载库 {lib_file} 失败: {e}")
        return None

def call_backend(input_str, task_num):
    """调用后端函数"""
    try:
        lib = load_library(task_num)
        if lib is None:
            return f"无法加载任务{task_num}的动态库"
        
        # 调用C++函数
        input_bytes = input_str.encode('utf-8')
        result_bytes = lib.simulateChargingStation(input_bytes)
        result_str = result_bytes.decode('utf-8')
        
        return result_str
        
    except Exception as e:
        return f"后端调用失败: {str(e)}"

def call_backend_fresh(input_str, task_num):
    """通过子进程调用后端，确保每次都是全新状态"""
    try:
        # 找到对应的exe文件
        web_dir = os.path.dirname(os.path.abspath(__file__))
        project_root = os.path.dirname(web_dir)
        exe_path = os.path.join(project_root, "bin", f"task{task_num}.exe")
        
        if not os.path.exists(exe_path):
            return f"找不到任务{task_num}的可执行文件: {exe_path}"
        
        # 通过子进程调用，每次都是全新状态
        # 在Windows下使用GBK编码处理中文输出
        result = subprocess.run([exe_path], input=input_str.encode('utf-8'), 
                              capture_output=True, timeout=5, shell=True)
        
        # 处理编码问题：Windows下C++输出GBK编码
        try:
            stdout_text = result.stdout.decode('gbk') if result.stdout else ""
            stderr_text = result.stderr.decode('gbk') if result.stderr else ""
        except UnicodeDecodeError:
            # 如果GBK解码失败，尝试UTF-8
            try:
                stdout_text = result.stdout.decode('utf-8') if result.stdout else ""
                stderr_text = result.stderr.decode('utf-8') if result.stderr else ""
            except UnicodeDecodeError:
                # 最后尝试latin-1（不会失败）
                stdout_text = result.stdout.decode('latin-1') if result.stdout else ""
                stderr_text = result.stderr.decode('latin-1') if result.stderr else ""
        
        # 调试信息（可选）
        # print(f"子进程调用: {exe_path}")
        # print(f"输入: {input_str}")
        # print(f"返回码: {result.returncode}")
        # print(f"stdout: {repr(stdout_text)}")
        # print(f"stderr: {repr(stderr_text)}")
        
        if result.returncode != 0:
            return f"任务{task_num}执行失败: {stderr_text}"
        
        return stdout_text.strip()
        
    except subprocess.TimeoutExpired:
        return f"任务{task_num}执行超时"
    except Exception as e:
        return f"子进程调用失败: {str(e)}"

@app.route('/')
def index():
    """提供主页面"""
    return send_from_directory('.', 'index.html')

@app.route('/<path:filename>')
def static_files(filename):
    """提供静态文件"""
    return send_from_directory('.', filename)

@app.route('/api/simulate', methods=['POST'])
def simulate():
    """模拟接口"""
    try:
        data = request.get_json()
        input_str = data.get('input', '')
        task_num = data.get('task', 1)
        
        if not input_str:
            return jsonify({'error': '输入不能为空'}), 400
        
        # 调用后端（Task4优先DLL，失败时自动回退到EXE）
        if int(task_num) == 4:
            result = call_backend(input_str, task_num)
            # 调试：记录DLL返回的原始结果
            print(f"[DEBUG] Task4 DLL result: {repr(result[:200])} (length: {len(result) if result else 0})")
            
            if not result or '无法加载任务4的动态库' in result or result.strip() == '':
                # Fallback to fresh exe call with cumulative history
                global task4_input_history
                # 追加本次命令并重放全量输入，确保状态一致
                task4_input_history = (task4_input_history + ("\n" if task4_input_history else "")) + input_str
                raw = call_backend_fresh(task4_input_history, task_num)
                # 调试：记录exe返回的原始结果
                print(f"[DEBUG] Task4 EXE raw output: {repr(raw[:500])} (length: {len(raw) if raw else 0})")
                # 清理可执行文件模板噪声，仅保留最近一次命令结果
                result = clean_task4_exe_output(raw)
                print(f"[DEBUG] Task4 EXE cleaned result: {repr(result[:200])} (length: {len(result) if result else 0})")
            
            # 检查结果是否只包含提示信息
            if result and ('紧急任务调度已执行完成' in result and '请点击' in result and len(result.strip()) < 100):
                print(f"[WARNING] Task4 returned only notification message, not actual result")
        else:
            result = call_backend(input_str, task_num)
        
        return jsonify({
            'success': True,
            'result': result,
            'input': input_str,
            'task': task_num
        })
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

def clean_task4_exe_output(text: str) -> str:
    try:
        if not text:
            return text
        
        # 如果包含紧急调度结果的关键标记，直接返回（避免被过滤）
        if '=== 紧急任务调度 ===' in text or '=== 充电阶段 ===' in text or '=== 任务执行阶段 ===' in text or '=== 任务结果 ===' in text:
            # 这是完整的调度结果，需要提取相关部分
            lines = text.splitlines()
            cleaned = []
            in_emergency_result = False
            for line in lines:
                # 跳过模板噪声
                if line.strip().startswith('Drone Charging Station Test'):
                    continue
                if line.strip().startswith('Input test command'):
                    continue
                if line.strip().startswith('Press any key to exit'):
                    continue
                # 如果是提示信息而不是实际结果，跳过
                if '紧急任务调度已执行完成' in line and '请点击' in line:
                    continue
                # 检测紧急调度结果开始
                if '=== 紧急任务调度 ===' in line or in_emergency_result:
                    in_emergency_result = True
                    cleaned.append(line)
                    # 如果遇到任务结果结束标记，继续保留后续内容
                    if '=== 任务结果 ===' in line:
                        # 继续保留到文件结束
                        continue
                # 处理Result:行（保留兼容性）
                elif 'Result:' in line:
                    cleaned = []
                    in_emergency_result = True
                    idx = line.find('Result:')
                    rest = line[idx + len('Result:'):].strip()
                    if rest:
                        cleaned.append(rest)
                elif in_emergency_result:
                    cleaned.append(line)
                else:
                    # 保留其他正常输出
                    cleaned.append(line)
            
            result = '\n'.join([l for l in cleaned if l is not None and l.strip()])
            result = result.strip('\n')
            
            # 如果结果为空或只包含提示信息，返回原始文本的紧急调度部分
            if not result or (len(result) < 50 and ('执行完成' in result or '请点击' in result)):
                # 尝试从原始文本中提取紧急调度相关的内容
                emergency_parts = []
                in_section = False
                for line in text.splitlines():
                    if '=== 紧急任务调度 ===' in line or '=== 充电阶段 ===' in line or '=== 任务执行阶段 ===' in line or '=== 任务结果 ===' in line:
                        in_section = True
                    if in_section:
                        emergency_parts.append(line)
                if emergency_parts:
                    return '\n'.join(emergency_parts).strip()
            
            return result if result else text
        
        # 原有的处理逻辑（兼容其他输出）
        lines = text.splitlines()
        cleaned = []
        in_result = False
        for line in lines:
            # 跳过模板噪声
            if line.strip().startswith('Drone Charging Station Test'):
                continue
            if line.strip().startswith('Input test command'):
                continue
            if line.strip().startswith('Press any key to exit'):
                continue
            # 跳过提示信息
            if '紧急任务调度已执行完成' in line and '请点击' in line:
                continue
            # 处理Result:行
            if 'Result:' in line:
                cleaned = []
                in_result = True
                idx = line.find('Result:')
                rest = line[idx + len('Result:'):].strip()
                if rest:
                    cleaned.append(rest)
                continue
            if in_result:
                cleaned.append(line)
            else:
                cleaned.append(line)
        
        result = '\n'.join([l for l in cleaned if l is not None])
        result = result.strip('\n')
        return result if result else text
    except Exception:
        return text

@app.route('/api/status', methods=['GET'])
def get_status():
    """获取系统状态"""
    return jsonify({
        'success': True,
        'status': {
            'loaded_libs': list(loaded_libs.keys()),
            'available_tasks': [1, 2, 3, 4, 5, 6, 7]
        }
    })


@app.route('/api/health', methods=['GET'])
def health_check():
    """健康检查"""
    return jsonify({'status': 'healthy'})

@app.route('/api/system-state', methods=['GET'])
def get_system_state():
    """获取系统状态"""
    try:
        # 返回空状态，让前端使用自己的systemState数据
        system_state = {
            'charging_station': [],
            'waiting_queue': [],
            'temp_stack': [],
            'current_time': 0,
            'station_occupied': 0,
            'queue_count': 0,
            'temp_stack_count': 0
        }
        
        return jsonify({
            'success': True,
            'state': system_state
        })
        
    except Exception as e:
        return jsonify({
            'success': False,
            'error': str(e)
        }), 500

# 临时文件清理功能已移除，用户可手动删除 temp_dlls 文件夹

def start_server(host='127.0.0.1', port=5002, debug=False):
    """启动服务器"""
    print(f"启动后端桥接服务...")
    print(f"访问地址: http://{host}:{port}")
    print(f"API文档: http://{host}:{port}/api/status")
    
    # 预加载所有库
    for task_num in range(1, 7):
        load_library(task_num)
    
    app.run(host=host, port=port, debug=debug, threaded=True)

@app.route('/api/reset', methods=['POST'])
def reset_backend():
    """重置指定任务的后端状态（重新加载DLL）"""
    try:
        data = request.get_json() or {}
        task_num = int(data.get('task', 0))
        if task_num not in [1, 2, 3, 4, 5, 6]:
            return jsonify({'success': False, 'error': 'invalid task'}), 400
        
        # 删除旧DLL引用，强制重新加载
        if task_num in loaded_libs:
            del loaded_libs[task_num]
        
        # 重新加载DLL
        new_lib = load_library(task_num, force_reload=True)
        if new_lib is None:
            return jsonify({'success': False, 'error': 'reload failed'}), 500
        
        return jsonify({'success': True, 'task': task_num})
    except Exception as e:
        return jsonify({'success': False, 'error': str(e)}), 500

if __name__ == '__main__':
    import argparse
    
    parser = argparse.ArgumentParser(description='无人机管理与网络威胁预警系统后端桥接服务')
    parser.add_argument('--host', default='127.0.0.1', help='服务器地址')
    parser.add_argument('--port', type=int, default=5002, help='服务器端口')
    parser.add_argument('--debug', action='store_true', help='调试模式')
    
    args = parser.parse_args()
    
    start_server(args.host, args.port, args.debug)
