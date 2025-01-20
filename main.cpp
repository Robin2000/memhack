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

// 内存配置结构体
struct MemoryConfig {
    unsigned long address;
    size_t typeSize;
    unsigned long value;
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

// 读取配置文件
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

#ifdef _WIN32
// 创建子进程 (Windows)
PROCESS_INFORMATION createChildProcess(const std::string& commandLine) {
    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    if (!CreateProcess(NULL, const_cast<char*>(commandLine.c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        std::cerr << "CreateProcess failed (" << GetLastError() << ").\n";
        exit(1);
    }
    return pi;
}

// 监控线程 (Windows)
DWORD WINAPI monitorThread(LPVOID lpParam) {
    struct ThreadData {
        HANDLE hProcess;
        std::vector<MemoryConfig> configs;
        unsigned long baseAddress;
    };
    ThreadData* data = static_cast<ThreadData*>(lpParam);
    HANDLE hProcess = data->hProcess;
    std::vector<MemoryConfig> configs = data->configs;
    unsigned long baseAddress = data->baseAddress;

    while (true) {
        for (const auto& config : configs) {
            BYTE buffer[sizeof(unsigned int)]; // 使用最大可能的类型大小
            SIZE_T bytesRead;
            if (!ReadProcessMemory(hProcess, (LPCVOID)(config.address + baseAddress), buffer, config.typeSize, &bytesRead)) {
                std::cerr << "ReadProcessMemory failed (" << GetLastError() << ").\n";
                continue;
            }
            unsigned long actualValue;
            memcpy(&actualValue, buffer, config.typeSize);

            if (actualValue != config.value) {
                SIZE_T bytesWritten;
                if (!WriteProcessMemory(hProcess, (LPVOID)(config.address + baseAddress), &config.value, config.typeSize, &bytesWritten)) {
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

#elif __linux__

// 创建子进程 (Linux)
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

// 监控线程 (Linux)
void* monitorThread(void* arg) {
    struct ThreadData {
        pid_t pid;
        std::vector<MemoryConfig> configs;
    };
    ThreadData* data = static_cast<ThreadData*>(arg);
    pid_t pid = data->pid;
    std::vector<MemoryConfig> configs = data->configs;

    if (ptrace(PTRACE_ATTACH, pid, NULL, NULL) < 0) {
        perror("ptrace attach");
        return NULL;
    }
    waitpid(pid, NULL, 0);

    while (true) {
        for (const auto& config : configs) {
            unsigned long data_read;
            errno = 0;
            data_read = ptrace(PTRACE_PEEKDATA, pid, (void*)config.address, NULL);
            if(errno != 0) {
                perror("ptrace peekdata");
                continue;
            }
            if (data_read != config.value) {
                if (ptrace(PTRACE_POKEDATA, pid, (void*)config.address, config.value) < 0) {
                    perror("ptrace pokedata");
                } else {
                    std::cout << "Address: 0x" << std::hex << config.address << " changed to: " << std::dec << config.value << std::endl;
                }
            }
        }
        usleep(100000); // 100ms
    }
    return NULL;
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
    unsigned long baseAddress = 0;
    if (EnumProcessModules(pi.hProcess, hMods, sizeof(hMods), &cbNeeded)) {
        baseAddress = (unsigned long)hMods[0];
    }
#elif __linux__
    pid_t pid = createChildProcess(commandLine);
#endif

    // 读取配置文件
    std::vector<MemoryConfig> configs = readConfigFile("config.txt");

    // 创建监控线程
#ifdef _WIN32
    struct ThreadData {
        HANDLE hProcess;
        std::vector<MemoryConfig> configs;
        unsigned long baseAddress;
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
