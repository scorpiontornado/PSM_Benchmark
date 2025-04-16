#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import time
import subprocess
import resource
import shutil
from tqdm import tqdm  # 引入 tqdm 库
import psutil  # 导入 psutil 库

memory_file = "../C_output/C_memory.txt"

def monitor_memory(process, schedule, split_type, data_graph, query_graph):
    """
    监控 SubgraphMatching.out 进程的内存占用并记录到指定的文件中。
    如果内存占用超过 50GB，终止进程并终止 timeout 进程。
    """
    # 首先检查主进程是否已经结束，如果已经结束，则直接退出
    if process.poll() is not None:  # 如果主进程已结束
        with open(memory_file, 'a') as f:
            f.write(f"Max memory usage: 1.00 MB | "
                    f"Schedule: {schedule} | "
                    f"Split Type: {split_type} | "
                    f"Data Graph: {data_graph} | "
                    f"Query Graph: {query_graph}\n")
        return

    # 使用 psutil 获取子进程对象
    ps_process = psutil.Process(process.pid)  # 获取 timeout 进程的对象
    subgraph_matching_pid = None

    # 寻找 SubgraphMatching.out 的 PID
    for child in ps_process.children(recursive=True):  # 获取所有子进程
        try:
            if 'SubgraphMatching.out' in child.name():  # 判断子进程是否是 SubgraphMatching.out
                subgraph_matching_pid = child.pid
                break
        except psutil.NoSuchProcess:
            # 如果子进程已经终止，跳过这个子进程
            continue

    if subgraph_matching_pid is None:
        with open(memory_file, 'a') as f:
            f.write(f"Max memory usage: 1.00 MB | "
                    f"Schedule: {schedule} | "
                    f"Split Type: {split_type} | "
                    f"Data Graph: {data_graph} | "
                    f"Query Graph: {query_graph}\n")
        return

    # 监控 SubgraphMatching.out 的内存使用
    ps_subgraph = psutil.Process(subgraph_matching_pid)
    max_memory = 0  # 初始内存使用量
    memory_limit = 50 * 1024 * 1024 * 1024  # 50GB

    while True:
        try:
            # 检查进程是否仍在运行
            if not ps_subgraph.is_running():
                break

            memory_usage = ps_subgraph.memory_info().rss  # 获取进程的内存占用
            max_memory = max(max_memory, memory_usage)  # 更新最大内存占用

            if memory_usage > memory_limit:
                ps_subgraph.terminate()  # 终止进程
                ps_process.terminate()  # 终止 timeout 进程
                break

        except psutil.NoSuchProcess:
            # 如果进程已经结束，直接跳出循环
            break

        time.sleep(0.1)  # 每秒检查一次

    # 进程结束后，写入最大内存占用到文件，并记录传入的参数信息
    with open(memory_file, 'a') as f:
        f.write(f"Max memory usage: {max_memory / (1024 * 1024):.2f} MB | "
                f"Schedule: {schedule} | "
                f"Split Type: {split_type} | "
                f"Data Graph: {data_graph} | "
                f"Query Graph: {query_graph}\n")

def handle_exit_code(exit_code, graph, label_size, size, i, scripts_file, stats, joinOrSchedule, patternOrSplit):
    """
    """
    # 新增：指定错误日志文件
    error_file = "../C_output/error.txt"
    
    if exit_code == 0:
        stats["success_count"] += 1
    elif exit_code == 1:
        stats["error_count"] += 1
        msg = f"{graph}, {label_size}, {size}, {i}, {joinOrSchedule}, {patternOrSplit}, Error exit code 1"
        with open(scripts_file, 'a') as sf:
            sf.write(msg + "\n")
        # 同步写入 error_file
        with open(error_file, 'a') as ef:
            ef.write(msg + "\n")
    elif exit_code == 124:
        stats["timeout_count"] += 1
        msg = f"{graph}, {label_size}, {size}, {i}, {joinOrSchedule}, {patternOrSplit}, Timeout exit code 124"
        with open(scripts_file, 'a') as sf:
            sf.write(msg + "\n")
        # 同步写入 error_file
        with open(error_file, 'a') as ef:
            ef.write(msg + "\n")
    elif exit_code == 134:
        stats["abort_count"] += 1
        msg = f"{graph}, {label_size}, {size}, {i}, {joinOrSchedule}, {patternOrSplit}, Aborted (SIGABRT) exit code 134"
        with open(scripts_file, 'a') as sf:
            sf.write(msg + "\n")
        # 同步写入 error_file
        with open(error_file, 'a') as ef:
            ef.write(msg + "\n")
    elif exit_code == 137:
        stats["kill_mem_count"] += 1
        msg = f"{graph}, {label_size}, {size}, {i}, {joinOrSchedule}, {patternOrSplit}, Killed by SIGKILL (likely OOM) exit code 137"
        with open(scripts_file, 'a') as sf:
            sf.write(msg + "\n")
        # 同步写入 error_file
        with open(error_file, 'a') as ef:
            ef.write(msg + "\n")
    else:
        stats["unknown_count"] += 1
        msg = f"{graph}, {label_size}, {size}, {i}, {joinOrSchedule}, {patternOrSplit}, Unknown exit code {exit_code}"
        with open(scripts_file, 'a') as sf:
            sf.write(msg + "\n")
        # 同步写入 error_file
        with open(error_file, 'a') as ef:
            ef.write(msg + "\n")

def print_stats(stats):
    """
    输出各个 exit_code 的统计信息
    """
    print("Success Count:", stats["success_count"])
    print("Error Count:", stats["error_count"])
    print("Timeout Count:", stats["timeout_count"])
    print("Abort Count:", stats["abort_count"])
    print("Killed by SIGKILL Count:", stats["kill_mem_count"])
    print("Unknown Count:", stats["unknown_count"])

# 在主程序或调用时初始化 stats 字典
stats = {
    "success_count": 0,
    "error_count": 0,
    "timeout_count": 0,
    "abort_count": 0,
    "kill_mem_count": 0,
    "unknown_count": 0
}


def clear_output_dir(output_dir):
    """
    如果 output_dir 存在，删除该目录中的所有文件；如果不存在，则创建该目录。
    """
    # print(f"clear output_dir: {output_dir}")
    if os.path.exists(output_dir):
        # 删除目录下的所有文件
        # print(f"remove all files in {output_dir}")
        for filename in os.listdir(output_dir):
            file_path = os.path.join(output_dir, filename)
            if os.path.isfile(file_path):
                os.remove(file_path)
            elif os.path.isdir(file_path):
                shutil.rmtree(file_path)
    else:
        # 如果目录不存在，则创建
        print(f"create output_dir: {output_dir}")
        os.makedirs(output_dir, exist_ok=True)


def kill_prsf_processes():
    """
    与 shell 中的 ps -ef | grep PRSF | grep -v grep | awk '{print $2}' | xargs -r kill -9 对应
    这里直接调用 shell 命令
    """
    cmd = r"ps -ef | grep PRSF | grep -v grep | awk '{print $2}' | xargs -r kill -9"
    subprocess.run(cmd, shell=True)

def set_memory_limit():
    """
    示例: 在这里设置子进程(及其子进程)的最大可用内存大小
    """
    soft_limit = 50 * 1024 * 1024 * 1024    # 50 GB
    hard_limit = 50 * 1024 * 1024 * 1024    # 50 GB
    resource.setrlimit(resource.RLIMIT_AS, (soft_limit, hard_limit))

def split_C_test(
    exepath,
    dataspace,
    file_suffix,
    label_size,
    size,
    query_num,
    graph,
    qidx,
    time_limit,
    max_num,
    nums_threads,
    types_backMethods,
    stats,
    testspace,
    types_split,
    types_schedule,
    valid_combinations
):
    """
    C 分支:
      for schedule in types_schedule:
        for split_type in types_split:
            收集多条查询文件 -> 用分隔符拼接成字符串 -> 作为 -qs "..." 传给 C++
    """

    timeout_duration = (time_limit + 0.5) * len(types_backMethods) * len(nums_threads)
    timeout_duration += 15 # 10 是为了加载data_graph准备的

    joined_nums_threads = ",".join(nums_threads)
    joined_types_backMethods = ",".join(types_backMethods)

    for schedule in types_schedule:
        for split_type in types_split:

            if (split_type, schedule) not in valid_combinations:
                continue  # 跳过无效组合

            data_graph = f"{graph}-{label_size}"
            query_graph = f"Q{size}/Q{size}-{qidx}{file_suffix}"

            output_dir = os.path.join(testspace, "C_output", graph, f"L{label_size}", f"Q{size}/")
            outputFile = os.path.join(output_dir, f"Q{size}-{qidx}.txt")
            # print(f"outputFile: {outputFile}")
            scripts_file = os.path.join(output_dir, "scripts.txt")
            if not os.path.exists(scripts_file):
                with open(scripts_file, 'w') as sf:
                    pass

            # 构造命令
            cmd = [
                # "valgrind", "--leak-check=full", "--track-origins=yes", "--read-var-info=yes", "--show-leak-kinds=all",
                "timeout", str(timeout_duration),
                exepath,
                # "ASAN_OPTIONS=detect_leaks=1", exepath,  # 启用 AddressSanitizer
                "-d", os.path.join(dataspace, f"{data_graph}{file_suffix}"),
                "-q", os.path.join(dataspace, query_graph),
                "-threadnums", joined_nums_threads,
                "-num", max_num,
                "-time_limit", str(time_limit),
                "-OutputFile", outputFile,
                "-QorCandi", "C",
                "-split", split_type,
                "-schedule", schedule,
                "-BackMethods", joined_types_backMethods,
            ]

            # # print(cmd)

            # 执行命令并获取返回的进程对象
            process = subprocess.Popen(cmd)
            # 启动一个新的线程来监控内存使用
            monitor_memory(process, schedule, split_type, data_graph, query_graph)
            # print(process)
            # 等待 C++ 程序执行完成
            process.wait()

            # 记录退出码
            exit_code = process.returncode
            handle_exit_code(exit_code, graph, label_size, size, qidx, scripts_file, stats, schedule, split_type)


            # result = subprocess.run(cmd, preexec_fn=set_memory_limit)
            # exit_code = result.returncode
            
def main():
    # 1. 清理可能残留的 PRSF 进程

    # 2. 一些配置与路径
    testspace = "../"
    src_space = "../../build/matching"
    exepath = os.path.join(src_space, "SubgraphMatching.out")
    data_folder = "../../datasets/"

    # 先清空 error_file
    error_file = "../C_output/error.txt"
    with open(error_file, 'w') as ef:
        pass  # 以 'w' 模式打开后关闭，文件就被清空了
    with open(memory_file, 'w') as mf:
        pass

    # 你需要遍历的参数
    types_QorCandis = ["C"]

    # Q 分支配置
    types_Qpattern = ["SKETCHTREE", "STAR", "TWINTWIG", "SEED"]
    types_Joinmethod = ["LEFTDEEP"]

    # C 分支配置
    types_schedule = ["static", "staticworkload", "busy2idlenostop", "busy2idledepthstop", "timeout"]
    types_schedule = ["busy2idlenostop"]
    types_split = ["linear", "upperone", "workload", "layer"]
    types_split = ["linear"]

    # 保证 valid_combinations 中的项属于 types_split 和 types_schedule 的交集
    # 这是scalability实验的setting
    valid_combinations = [
        (split, schedule)
        for split, schedule in [
            ('linear', 'busy2idlenostop'),
            ('linear', 'busy2idledepthstop'),
            ('linear', 'timeout'),

            ('upperone', 'busy2idlenostop'),
            ('upperone', 'busy2idledepthstop'),
            ('upperone', 'timeout'),

            ('workload', 'busy2idlenostop'),
            ('workload', 'busy2idledepthstop'),
            ('workload', 'timeout'),

            ('layer', 'busy2idlenostop'),
            ('layer', 'busy2idledepthstop'),
            ('layer', 'timeout'),

            ('layer', 'staticworkload'),
            ('layer', 'static')
        ]
        if split in types_split and schedule in types_schedule
    ]

    types_backMethods = [
        "DPiso_DPiso_LFTJ",
        "CFL_CFL_EXPLORE",
        "GQL_GQL_GQL",
        "CECI_CECI_CECI",
        "DPiso_DPiso_DPiso",
        "DPiso_CIRCINUS_CIRCINUS",
    ]

    graphs = ["HPRD", "dblp", "citeseer", "human", "maayan-figeys", "twitch", "web-Stanford", "wordnet-words", "YeastS", "youtube"]
    graphs = ["HPRD"]
    file_suffix = ".txt"
    query_num = 10
    max_num = "MAX"
    # max_nums = ["10000", "100000", "1000000", "10000000"]
    label_sizes = ["15", "30", "45", "60"]
    label_sizes = ["15"]
    query_sizes = ["10", "20", "30", "40", "50"]
    # query_sizes = ["5", "6", "7", "8", "9", "10"]
    query_sizes = ["10"]

    target_pairs = [
        ("15", "10"), ("30", "10"), ("45", "10"), ("60", "10"),
        ("15", "20"), ("15", "30"), ("15", "40"), ("15", "50")
    ]


    nums_threads = ["1", "2", "4", "8", "16", "32", "64"]
    nums_threads = ["1", "4", "16", "64"]
    time_limit = 2

    # 3. 初始化统计
    stats = {
        "success_count": 0,
        "error_count": 0,
        "timeout_count": 0,
        "abort_count": 0,
        "kill_mem_count": 0,
        "unknown_count": 0
    }

    offset_qidx = 0

    start_time_total = time.time()

    timeout_duration_perquery = time_limit * len(types_backMethods) * len(nums_threads) # 0.2 是为了加载query_graph准备的
    timeout_duration_perquery += 10 # 10 是为了加载data_graph准备的
    timeout_duration_perquery *= len(types_schedule)
    timeout_duration_perquery *= len(types_split)

    # 4. 外层根据 Q 或 C，分别调用函数
    for QorCandiType in types_QorCandis:
        for label_size in label_sizes:
            for size in query_sizes:
                if (label_size, size) not in target_pairs:
                    continue

                for graph in graphs:
                    outputdir = os.path.join(testspace, "C_output", graph, f"L{label_size}", f"Q{size}/")
                    clear_output_dir(outputdir)
                    # 使用 tqdm 创建进度条
                    print(f"Processing {graph} with label_size={label_size}, query_size={size}, QorCandiType={QorCandiType}, timeout_duration={timeout_duration_perquery}")
                    for qidx in tqdm(range(1, query_num + 1), desc="Query Progress", unit="query"):
                        dataspace = os.path.join(data_folder, f"{graph}/L{label_size}")
                        # print(f"dataspace: {dataspace}")
                        if QorCandiType == "Q":
                            exit(1)
                        else:
                            split_C_test(
                                exepath,
                                dataspace,
                                file_suffix,
                                label_size,
                                size,
                                query_num,
                                graph,
                                qidx + offset_qidx,
                                time_limit,
                                max_num,
                                nums_threads,
                                types_backMethods,
                                stats,
                                testspace,
                                types_split,
                                types_schedule,
                                valid_combinations
                            )

    end_time_total = time.time()
    elapsed_time_total = end_time_total - start_time_total
    print(f"Execution time: {elapsed_time_total:.2f} seconds")
    # 在程序结束时输出统计信息
    print_stats(stats)

if __name__ == "__main__":
    start_time = time.time()  # 开始时间

    main()

    end_time = time.time()  # 结束时间
    print(f"Execution time: {end_time - start_time:.2f} seconds")
