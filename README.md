# MemoryMonitor - 跨平台内存监控工具

## 简介

MemoryMonitor 是一个跨平台的命令行工具，用于监控指定子进程的内存，并根据配置文件中的规则自动修改内存中的值。它支持 Windows 和 Linux 操作系统，以及 32 位和 64 位架构。

## 功能

*   **跨平台支持：** 支持 Windows 和 Linux 操作系统。
*   **配置文件驱动：** 通过配置文件定义需要监控的内存地址、数据类型和期望值。
*   **自动修改：** 当监控的内存地址的值与配置文件中的期望值不一致时，自动将其修改为期望值。
*   **命令行参数：** 接受一个命令行参数，作为要启动的子进程的命令。

## 构建

本项目使用 CMake 进行构建。请确保你的系统中已安装 CMake。

1.  **克隆代码库：**

    ```bash
    git clone [https://github.com/](https://github.com/)<your_username>/MemoryMonitor.git # 替换为你的仓库地址
    cd MemoryMonitor
    ```

2.  **创建构建目录：**

    ```bash
    mkdir build
    cd build
    ```

3.  **配置项目：**

    *   **Windows (Visual Studio):**

        ```bash
        cmake -G "Visual Studio 17 2022" .. # 根据你的VS版本选择生成器
        # 或者使用 Ninja 生成器
        cmake -G "Ninja" ..
        ```

    *   **Linux (Makefiles):**

        ```bash
        cmake ..
        # 或者使用 Ninja 生成器
        cmake -G "Ninja" ..
        ```

4.  **构建项目：**

    *   **Windows (Visual Studio):** 打开 `build` 目录下的 `.sln` 文件，在 Visual Studio 中构建项目。或者使用命令行：

        ```bash
        cmake --build . --config Release # 构建 Release 版本
        cmake --build . --config Debug   # 构建 Debug 版本
        ```

    *   **Linux (Makefiles):**

        ```bash
        make
        # 或者使用 Ninja 生成器
        ninja
        ```

构建完成后，可执行文件将在 `build` 目录下生成（Windows 下为 `Debug` 或 `Release` 子目录）。

## 使用方法

1.  **创建配置文件 `config.txt`：**

    `config.txt` 文件定义了需要监控的内存地址、数据类型和期望值。每行定义一个监控项，格式如下：

    ```
    <地址> <数据类型> <值>
    ```

    *   `<地址>`：要监控的内存地址，以十六进制表示（例如 `0x00401000`）。
    *   `<数据类型>`：数据类型，可以是 `DWORD`、`BYTE` 或 `WORD`。
    *   `<值>`：期望值，以十进制表示。

    例如：

    ```
    0x00401000 DWORD 100
    0x00402000 BYTE 255
    0x00403000 WORD 300
    ```

2.  **运行程序：**

    ```bash
    ./MemoryMonitor "<要启动的子进程的命令>"
    ```

    例如，在 Windows 下：

    ```
    MemoryMonitor "notepad.exe"
    ```

    在 Linux 下：

    ```bash
    ./MemoryMonitor "gedit"
    ```

    或者运行一个bash命令
    ```bash
    ./MemoryMonitor "ls -l"
    ```

## 注意事项

*   **权限：** 在 Linux 下，由于使用了 `ptrace` 系统调用，运行此程序需要 root 权限，或者需要设置 `ptrace_scope`。建议使用 `setcap cap_sys_ptrace+ep ./MemoryMonitor` 命令给程序设置 `CAP_SYS_PTRACE` capability，以避免使用 root 权限。
*   **地址有效性：** 确保配置文件中指定的内存地址在子进程的地址空间中是有效的。错误的地址可能导致程序崩溃或产生未定义的行为。
*   **类型匹配：** 配置文件中指定的数据类型必须与子进程中实际的数据类型匹配。
*   **错误处理：** 程序包含基本的错误处理，但可能仍存在未考虑到的情况。
*   **子进程退出** 程序会等待子进程结束后再退出。

## 示例

假设 `config.txt` 内容如下：
0x00401000 DWORD 100
0x00402000 BYTE 255
0x00403000 WORD 300
0x404000 DWORD 12345

运行命令 `./MemoryMonitor "./my_program"`，其中 `./my_program` 是一个会使用地址 `0x404000` 的程序。如果 `my_program` 将该地址的值修改为其他值（例如 67890），MemoryMonitor 将会立即将其改回 12345，并在控制台输出：

Address: 0x404000 changed to: 12345

## 许可证

Apache License

## 贡献

欢迎提交 issue 和 pull request。