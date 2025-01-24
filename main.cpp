#include <cstdint>  // 添加uintptr_t支持
#include <D:/Program/keystone-0.9.2-win32/include/keystone/keystone.h>  // Keystone引擎头文件

// 使用标准UTF-8编码
#ifdef _WIN32
#include <windows.h>
#include <tlhelp32.h>
#include <psapi.h>
#elif __linux__
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/reg.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <cstring>
#include <sstream>
#include <cstdlib>
#include <pthread.h>
#include <sys/uio.h>
#include <errno.h>
#else
#error "Unsupported platform."
#endif

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>

// 内存配置结构体 (处理 config.txt)
struct MemoryConfig {
    uintptr_t address;  // 使用uintptr_t
    size_t typeSize;
    uintptr_t value;    // 使用uintptr_t
};

// 汇编指令配置结构体 (处理 config_asm.txt)
struct AsmConfig {
    uintptr_t address;  // 使用uintptr_t
    std::string asmInstruction;
};

// 解析命令行参数
std::string getCommandLine(int argc, char* argv[]) {
    if (argc > 1) {
        return argv[1];
    } else {
        std::cerr << "Usage: program.exe <command>\n";
        exit(1);
    }
}

// 读取内存配置文件 (config.txt)
std::vector<MemoryConfig> readConfigFile(const std::string& filename) {
    std::vector<MemoryConfig> configs;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening config file: " << filename << "\n";
        exit(1);
    }

    std::string line;
    while (std::getline(file, line)) {
        MemoryConfig config;
        std::stringstream ss(line);
        std::string addressStr, typeStr, valueStr;
        ss >> std::hex >> config.address >> typeStr >> std::dec >> config.value;

        if (typeStr == "DWORD") {
            config.typeSize = sizeof(unsigned int);
        } else if (typeStr == "BYTE") {
            config.typeSize = sizeof(unsigned char);
        } else if (typeStr == "WORD") {
            config.typeSize = sizeof(unsigned short);
        } else {
            std::cerr << "Unknown type: " << typeStr << std::endl;
            continue;
        }
        configs.push_back(config);
    }
    return configs;
}

// 读取汇编指令配置文件 (config_asm.txt)
std::vector<AsmConfig> readAsmConfigFile(const std::string& filename) {
    std::vector<AsmConfig> asmConfigs;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening asm config file: " << filename << "\n";
        exit(1);
    }

    std::string line;
    while (std::getline(file, line)) {
        AsmConfig asmConfig;

        // 使用空格分割地址和指令部分
        std::istringstream ss(line);
        ss >> std::hex >> asmConfig.address;  // 读取地址
        std::getline(ss, asmConfig.asmInstruction);  // 读取剩余的汇编指令部分

        // 去除指令前的空格
        if (!asmConfig.asmInstruction.empty() && asmConfig.asmInstruction[0] == ' ') {
            asmConfig.asmInstruction.erase(0, 1);  // 删除开头的空格
        }

        asmConfigs.push_back(asmConfig);
    }
    return asmConfigs;
}

// 使用Keystone编译汇编指令
std::vector<uint8_t> compileAsmToMachineCode(const std::string& asmCode, size_t originalSize) {
    ks_engine *ks;
    ks_err err;
    size_t count;
    unsigned char *encode;
    size_t size;

    // 初始化Keystone引擎
    err = ks_open(KS_ARCH_X86, KS_MODE_32, &ks);
    if (err != KS_ERR_OK) {
        std::cerr << "Failed to initialize Keystone engine: " << ks_strerror(err) << "\n";
        exit(1);
    }

    // 编译汇编指令
    if (ks_asm(ks, asmCode.c_str(), 0, &encode, &size, &count) != KS_ERR_OK) {
        std::cerr << "Failed to assemble code: " << ks_strerror(ks_errno(ks)) << "\n";
        ks_close(ks);
        exit(1);
    }

    // 处理生成的机器码长度
    std::vector<uint8_t> machineCode(encode, encode + size);
    
    if (size < originalSize) {
        // 如果生成的机器码较短，用NOP填充
        machineCode.resize(originalSize, 0x90); // 0x90 = NOP
    } else if (size > originalSize) {
        // 如果生成的机器码较长，使用JMP跳转
        // 计算跳转偏移量（假设空闲区域在0x10000000）
        uint32_t jumpOffset = 0x10000000 - (0x1000 + size);
        std::vector<uint8_t> jumpCode = {0xE9}; // JMP指令
        // 添加跳转偏移量（小端序）
        jumpCode.push_back(jumpOffset & 0xFF);
        jumpCode.push_back((jumpOffset >> 8) & 0xFF);
        jumpCode.push_back((jumpOffset >> 16) & 0xFF);
        jumpCode.push_back((jumpOffset >> 24) & 0xFF);
        
        // 将跳转指令放在原位置
        machineCode = jumpCode;
        // 在空闲区域写入新指令
        // （实际实现中需要先分配内存并写入）
    }

    // 输出调试信息
    std::cout << "Generated machine code (" << machineCode.size() << " bytes):\n";
    for (size_t i = 0; i < machineCode.size(); ++i) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') 
                 << (int)machineCode[i] << " ";
        if ((i + 1) % 16 == 0) {
            std::cout << std::endl;
        }
    }
    std::cout << std::dec << std::endl;

    // 释放资源
    ks_free(encode);
    ks_close(ks);

    return machineCode;
}


// 将机器码写入进程的内存
void writeMachineCodeToMemory(HANDLE hProcess, uintptr_t address, const std::string& machineCodeFile) {
    // 读取机器码文件
    std::ifstream file(machineCodeFile, std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open machine code file: " << machineCodeFile << std::endl;
        return;
    }

    // 读取文件内容到字节数组
    std::vector<BYTE> code((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // 将机器码写入目标进程的内存
    SIZE_T bytesWritten;
    if (!WriteProcessMemory(hProcess, reinterpret_cast<LPVOID>(address), code.data(), code.size(), &bytesWritten)) {
        std::cerr << "Failed to write machine code to memory.\n";
    } else {
        std::cout << "Successfully wrote machine code to memory.\n";
    }
}

// 执行汇编指令 (仅执行一次)
void executeAsmInstruction(HANDLE hProcess, uintptr_t address, const std::string& asmInstruction) {
    // 获取原始机器码长度
    SIZE_T bytesRead;
    BYTE originalCode[16];
    if (!ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(address), originalCode, sizeof(originalCode), &bytesRead)) {
        std::cerr << "Failed to read original machine code.\n";
        return;
    }

    // 编译汇编指令为机器码
    auto machineCode = compileAsmToMachineCode(asmInstruction, bytesRead);

    // 将机器码写入目标进程的内存
    SIZE_T bytesWritten;
    if (!WriteProcessMemory(hProcess, reinterpret_cast<LPVOID>(address), machineCode.data(), machineCode.size(), &bytesWritten)) {
        std::cerr << "Failed to write machine code to memory.\n";
    } else {
        std::cout << "Successfully wrote machine code to memory.\n";
    }

    // 删除生成的机器码文件
    //remove(outputFile.c_str());
}

// 监控内存值修改的线程 (Windows)
#ifdef _WIN32
DWORD WINAPI monitorThread(LPVOID lpParam) {
    struct ThreadData {
        HANDLE hProcess;
        std::vector<MemoryConfig> configs;
        uintptr_t baseAddress;  // 使用uintptr_t
    };
    ThreadData* data = static_cast<ThreadData*>(lpParam);
    HANDLE hProcess = data->hProcess;
    std::vector<MemoryConfig> configs = data->configs;
    uintptr_t baseAddress = data->baseAddress;

    while (true) {
        for (const auto& config : configs) {
            BYTE buffer[sizeof(unsigned int)];
            SIZE_T bytesRead;
            if (!ReadProcessMemory(hProcess, reinterpret_cast<LPCVOID>(config.address + baseAddress), buffer, config.typeSize, &bytesRead)) {
                std::cerr << "ReadProcessMemory failed (" << GetLastError() << ").\n";
                continue;
            }
            uintptr_t actualValue;
            memcpy(&actualValue, buffer, config.typeSize);

            if (actualValue != config.value) {
                SIZE_T bytesWritten;
                if (!WriteProcessMemory(hProcess, reinterpret_cast<LPVOID>(config.address + baseAddress), &config.value, config.typeSize, &bytesWritten)) {
                    std::cerr << "WriteProcessMemory failed (" << GetLastError() << ").\n";
                } else {
                    std::cout << "Address: 0x" << std::hex << (config.address + baseAddress) << " changed to: " << std::dec << config.value << std::endl;
                }
            }
        }
        Sleep(100);
    }
    return 0;
}
#endif

// 创建子进程 (Windows)
#ifdef _WIN32
PROCESS_INFORMATION createChildProcess(const std::string& commandLine) {
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // 使用ANSI版本的CreateProcess
    if (!CreateProcessA(NULL, const_cast<char*>(commandLine.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        std::cerr << "CreateProcess failed (" << GetLastError() << ").\n";
        exit(1);
    }
    return pi;
}
#endif

// 创建子进程 (Linux)
#ifdef __linux__
pid_t createChildProcess(const std::string& commandLine) {
    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "Fork failed.\n";
        exit(1);
    } else if (pid == 0) { // 子进程
        execl("/bin/sh", "sh", "-c", commandLine.c_str(), NULL);
        std::cerr << "Exec failed.\n";
        exit(1);
    }
    return pid;
}
#endif

int main(int argc, char* argv[]) {
    // 解析命令行参数
    std::string commandLine = getCommandLine(argc, argv);

    // 创建子进程
#ifdef _WIN32
    PROCESS_INFORMATION pi = createChildProcess(commandLine);
    
    // 获取子进程基地址
    HMODULE hMods[1024];
    DWORD cbNeeded;
    uintptr_t baseAddress = 0;
    if (EnumProcessModules(pi.hProcess, hMods, sizeof(hMods), &cbNeeded)) {
        baseAddress = reinterpret_cast<uintptr_t>(hMods[0]);
    }
#elif __linux__
    pid_t pid = createChildProcess(commandLine);
#endif

    // 读取配置文件
    std::vector<MemoryConfig> configs = readConfigFile("config.txt");
    std::vector<AsmConfig> asmConfigs = readAsmConfigFile("config_asm.txt");

    // 执行汇编指令 (仅一次)
#ifdef _WIN32
    for (const auto& asmConfig : asmConfigs) {
        executeAsmInstruction(pi.hProcess, asmConfig.address + baseAddress, asmConfig.asmInstruction);
    }
#elif __linux__
    for (const auto& asmConfig : asmConfigs) {
        executeAsmInstruction(pid, asmConfig.address, asmConfig.asmInstruction);
    }
#endif

    // 创建监控线程 (Windows)
#ifdef _WIN32
    struct ThreadData {
        HANDLE hProcess;
        std::vector<MemoryConfig> configs;
        uintptr_t baseAddress;
    } data;
    data.hProcess = pi.hProcess;
    data.configs = configs;
    data.baseAddress = baseAddress;

    HANDLE hThread = CreateThread(NULL, 0, monitorThread, &data, 0, NULL);
    if (hThread == NULL) {
        std::cerr << "CreateThread failed (" << GetLastError() << ").\n";
        exit(1);
    }

    // 等待子进程结束
    WaitForSingleObject(pi.hProcess, INFINITE);

    // 清理
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
#elif __linux__
    struct ThreadData {
        pid_t pid;
        std::vector<MemoryConfig> configs;
    } data;
    data.pid = pid;
    data.configs = configs;

    pthread_t thread;
    if (pthread_create(&thread, NULL, monitorThread, &data) != 0) {
        std::cerr << "pthread_create failed.\n";
        exit(1);
    }

    // 等待子进程结束
    waitpid(pid, NULL, 0);
#endif

    return 0;
}
