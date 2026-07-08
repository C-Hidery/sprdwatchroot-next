/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * sprdwatchroot-next Copyright (C) 2026 Ryan Crepa
 */
#include <iostream>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif
#include <thread>
#include <new>
#include <cstdlib>
#include <signal.h>
#include "core/file_io.h"
#include "third_party/nlohmann/json.hpp"
#include <cstdarg>

using json = nlohmann::json;

#define VERSION "0.1.0"

#ifdef _WIN32
#define chdir _chdir
#endif

// 外部工具链/文件硬编码
const std::string KP_IMG_PATCHER("kptools.exe");
const std::string KP_IMG("kpimg-android.kpimg-android");
const std::string KEY_DIR("keys");
const std::string ALGORITHM("SHA256_RSA4096");
const std::string KEY_BOOT("keys\\rsa4096_boot.pem");
const std::string MAGISKBOOT("magiskboot.exe");
const std::string BOOT_SIZE("36700160"); // 35MB
const std::string SFD_TOOL("sfd_tool");
const std::string FDL1_W527("FDL\\W527\\fdl1.bin");
const std::string FDL2_W527("FDL\\W527\\fdl2.bin");
const std::string SPL_W527("FDL\\W527\\spl.bin");

enum LogLevel : int {
    I = 0, // Info
    W = 1, // Warning
    E = 2, // Error
    DE = 3, // Debug
    UN = 4,  // Undefined
    OP = 5  // Operation
};

bool copy_file(const char* src, const char* dst)
{
    EnhancedFile fi = oxfopen_enhanced(src, "rb");
    if (!fi)
    {
        DEG_LOG(E, u8"无法打开源文件: %s\n", src);
        return false;
    }
    EnhancedFile fo = oxfopen_enhanced(dst, "wb");
    if (!fo)
    {
        DEG_LOG(E, u8"无法创建目标文件: %s\n", dst);
        return false;
    }
    const size_t bufferSize = 8192; // 8KB 缓冲区
    char buffer[bufferSize];
    size_t bytesRead;
    while ((bytesRead = fi.read(buffer, 1, bufferSize)) > 0)
    {
        size_t bytesWritten = fo.write(buffer, 1, bytesRead);
        if (bytesWritten != bytesRead)
        {
            DEG_LOG(E, u8"写入目标文件失败: %s\n", dst);
            return false;
        }
    }
    fi.close();
    fo.close();

    return true;
}

#ifdef _WIN32
std::string UTF8ToANSI(const std::string& utf8Str) {
    if (utf8Str.empty()) return "";
    
    // 步骤1: UTF-8 -> UTF-16 (宽字符)
    int wideLen = MultiByteToWideChar(
        CP_UTF8,               // 源编码是 UTF-8
        0,                     // 标志位，0 即可
        utf8Str.c_str(),       // 输入字符串
        -1,                    // 自动计算长度（包含结尾 '\0'）
        nullptr,               // 第一次调用，只获取长度
        0
    );
    
    if (wideLen == 0) {
        // 转换失败，返回原始字符串（降级处理）
        return utf8Str;
    }
    
    std::vector<wchar_t> wideBuf(wideLen);
    MultiByteToWideChar(
        CP_UTF8,
        0,
        utf8Str.c_str(),
        -1,
        wideBuf.data(),
        wideLen
    );
    
    // 步骤2: UTF-16 -> ANSI (GBK, 即 CP_ACP)
    int ansiLen = WideCharToMultiByte(
        CP_ACP,                // 目标编码：系统默认 ANSI (Windows 中文下就是 GBK)
        0,
        wideBuf.data(),        // 输入宽字符
        -1,                    // 自动计算长度
        nullptr,
        0,
        nullptr,
        nullptr
    );
    
    if (ansiLen == 0) {
        return utf8Str;
    }
    
    std::vector<char> ansiBuf(ansiLen);
    WideCharToMultiByte(
        CP_ACP,
        0,
        wideBuf.data(),
        -1,
        ansiBuf.data(),
        ansiLen,
        nullptr,
        nullptr
    );
    
    // 注意：ansiBuf 包含结尾的 '\0'，但 string 构造时会忽略它
    return std::string(ansiBuf.data(), ansiLen - 1);
}
#endif

// Print log messages with a specific log level
void DEG_LOG(LogLevel level, const char* __format, ...)
{
    printf("[%s] ", (level == LogLevel::I) ? "INFO" :
                    (level == LogLevel::W) ? "WARN" :
                    (level == LogLevel::E) ? "ERROR" :
                    (level == LogLevel::DE) ? "DEBUG" : "UNDEF");
    va_list args;
    va_start(args, __format);
#ifdef _WIN32
    // ===== Windows 平台：需要进行 UTF-8 -> GBK 转码 =====
    
    // 2.1 先用 vprintf 获取格式化后的完整字符串（UTF-8 编码）
    va_list args_copy;
    va_copy(args_copy, args);
    
    // 计算格式化后字符串的长度
    int len = vsnprintf(nullptr, 0, __format, args_copy);
    va_end(args_copy);
    
    if (len > 0) {
        // 分配缓冲区
        std::vector<char> utf8Buffer(len + 1);
        vsnprintf(utf8Buffer.data(), utf8Buffer.size(), __format, args);
        
        // 2.2 将 UTF-8 字符串转换为 GBK
        std::string gbkStr = UTF8ToANSI(std::string(utf8Buffer.data()));
        
        // 2.3 输出 GBK 编码的中文
        printf("%s", gbkStr.c_str());
    }
    
#else
    // ===== Linux / macOS 平台：直接输出 UTF-8 =====
    vprintf(__format, args);
#endif
    va_end(args);
}

// 处理器特定处理函数

void w527_patch()
{
    chdir("Res");

    DEG_LOG(I, u8"------------------------------------------\n");
    DEG_LOG(I, u8"Spreadtrum W527 ROOT 处理程序\n");
    DEG_LOG(I, u8"[1] 执行AVB修补（必须执行）\n");
    DEG_LOG(I, u8"[2] 执行ROOT修补\n");
    DEG_LOG(I, u8"[3] 救砖\n");
    DEG_LOG(I, u8"[4] 退出程序\n");
    DEG_LOG(I, u8"------------------------------------------\n");

    int selection;
    std::cin >> selection;

    if (selection == 1)
    {
        std::string cmd = SFD_TOOL + " --no-gui --wait 30000 fdl " + FDL1_W527 + " 0x5500 fdl " + FDL2_W527 + " 0x9efffe00 exec w splloader " + SPL_W527 + " dis_avb_tos reset";
        DEG_LOG(I, u8"请在关机状态下按住侧键并连接四点线，工具有反应之后松手\n");
        DEG_LOG(I, u8"执行命令: %s\n", cmd.c_str());
        int ret = system(cmd.c_str());
        if (ret != 0)
        {
            DEG_LOG(E, u8"执行命令失败，错误码: %d\n", ret);
        }
        DEG_LOG(I, u8"AVB修补完成，请重新运行程序以继续ROOT\n");
        return;
    }
    else if (selection == 2)
    {
        DEG_LOG(I, u8"请确保已执行AVB修补，否则ROOT可能失败\n");
        DEG_LOG(I, u8"开始读取Boot分区\n");
        std::string cmd = SFD_TOOL + " --no-gui --wait 30000 fdl " + FDL1_W527 + " 0x5500 fdl " + FDL2_W527 + " 0x9efffe00 exec w splloader " + SPL_W527 + " path Boot r boot reset";
        DEG_LOG(I, u8"请在关机状态下按住侧键并连接四点线，工具有反应之后松手\n");
        DEG_LOG(I, u8"执行命令: %s\n", cmd.c_str());
        int ret = system(cmd.c_str());
        if (ret != 0)
        {
            DEG_LOG(E, u8"执行命令失败，错误码: %d\n", ret);
        }
        DEG_LOG(I, u8"手表已重启\n");
        DEG_LOG(I, u8"开始修补Boot镜像\n");
        if (!copy_file("Boot/boot.img", "boot.img"))
        {
            DEG_LOG(E, u8"复制Boot分区失败，请检查文件是否存在\n");
            return;
        }   
        std::string cmd3 =  MAGISKBOOT + " unpack boot.img";
        DEG_LOG(I, u8"执行命令: %s\n", cmd3.c_str());
        int ret3 = system(cmd3.c_str());
        if (ret3 != 0)
        {
            DEG_LOG(E, u8"执行命令失败，错误码: %d\n", ret3);
        }
        if (!copy_file("kernel", "kernel-b"))
        {
            DEG_LOG(E, u8"复制Kernel失败，请检查文件是否存在\n");
            return;
        }
        std::string cmd4 = KP_IMG_PATCHER + " -p --image kernel-b --skey \"w527root741852\" --kpimg " + KP_IMG + " --out kernel";
        DEG_LOG(I, u8"执行命令: %s\n", cmd4.c_str());
        int ret4 = system(cmd4.c_str());
        if (ret4 != 0)
        {
            DEG_LOG(E, u8"执行命令失败，错误码: %d\n", ret4);
        }
        std::string cmd5 = MAGISKBOOT + " repack boot.img";
        DEG_LOG(I, u8"执行命令: %s\n", cmd5.c_str());
        int ret5 = system(cmd5.c_str());
        if (ret5 != 0)
        {
            DEG_LOG(E, u8"执行命令失败，错误码: %d\n", ret5);
        }
        DEG_LOG(I, u8"完成，已修补的Boot镜像为\"new-boot.img\"\n");
        DEG_LOG(I, u8"开始签名\n");
        std::string cmd6 = "python avbtool.py add_hash_footer --image new-boot.img --partition_size " + BOOT_SIZE + " --algorithm " + ALGORITHM + " --key " + KEY_BOOT;
        DEG_LOG(I, u8"执行命令: %s\n", cmd6.c_str());
        int ret6 = system(cmd6.c_str());
        if (ret6 != 0)
        {
            DEG_LOG(E, u8"执行命令失败，错误码: %d\n", ret6);
        }
        DEG_LOG(I, u8"签名完成\n");
        DEG_LOG(I, u8"请按任意键继续...\n");
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cin.get();
        DEG_LOG(I, u8"开始刷入Boot分区\n");
        std::string cmd7 = SFD_TOOL + " --no-gui --wait 30000 fdl " + FDL1_W527 + " 0x5500 fdl " + FDL2_W527 + " 0x9efffe00 exec w splloader " + SPL_W527 + " w boot new-boot.img reset";
        DEG_LOG(I, u8"请在关机状态下按住侧键并连接四点线，工具有反应之后松手\n");
        DEG_LOG(I, u8"执行命令: %s\n", cmd7.c_str());
        int ret7 = system(cmd7.c_str());
        if (ret7 != 0)
        {
            DEG_LOG(E, u8"执行命令失败，错误码: %d\n", ret7);
        }
        DEG_LOG(I, u8"超级用户密钥:w527root741852\n");
        DEG_LOG(I, u8"刷写完毕！请检查是否有报错\n");
        DEG_LOG(I, u8"请按任意键退出程序...\n");
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cin.get();
        return;
    }
    else if (selection == 3)
    {
        EnhancedFile fi = oxfopen_enhanced("Boot/boot.img", "rb");
        if (!fi)
        {
            DEG_LOG(E, u8"无法找到备份文件，请检查文件是否存在\n");
            return;
        }
        fi.close();
        std::string cmd = SFD_TOOL + " --no-gui --wait 30000 fdl " + FDL1_W527 + " 0x5500 fdl " + FDL2_W527 + " 0x9efffe00 exec w splloader " + SPL_W527 + " w boot Boot/boot.img reset";
        DEG_LOG(I, u8"请在关机状态下按住侧键并连接四点线，工具有反应之后松手\n");
        DEG_LOG(I, u8"执行命令: %s\n", cmd.c_str());
        int ret = system(cmd.c_str());
        if (ret != 0)
        {
            DEG_LOG(E, u8"执行命令失败，错误码: %d\n", ret);
        }
        DEG_LOG(I, u8"刷写完毕！请检查是否有报错\n");
        DEG_LOG(I, u8"请按任意键退出程序...\n");
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cin.get();
        return;
    }
    else if (selection == 4)
    {
        DEG_LOG(I, u8"退出程序\n");
    }
    else
    {
        DEG_LOG(W, u8"无效的选择，请重新运行程序\n");
    }
    return;
}
int main(int argc, char* argv[])
{
    setlocale(LC_ALL, "zh_CN");
    if (argc > 1 && (!strcmp(argv[1], "--version") || !strcmp(argv[1], "-v")))
    {
        std::cout << "sprdwatchroot-next version " << VERSION << std::endl;
        return 0;
    }
    
    DEG_LOG(I, u8"Starting sprdwatchroot-next %s...\n", VERSION);

    std::cout << std::endl;
    std::cout << "-------------------------------------------" << std::endl;
    std::cout << "Sprd Watch Rooter" << std::endl;
    std::cout << "\t\tCopyright (C) 2026 Ryan Crepa" << std::endl;
    std::cout << "-------------------------------------------" << std::endl;

    DEG_LOG(I, u8"请选择您的处理器：\n");
    DEG_LOG(I, u8"[1] Spreadtrum SL8541E (SC9832E)\n");
    DEG_LOG(I, u8"[2] Spreadtrum W377E\n");
    DEG_LOG(I, u8"[3] Spreadtrum W527\n");
    DEG_LOG(I, u8"[4] 自定义FDL文件\n");

    int selection;
    std::cin >> selection;

    switch (selection)
    {
    case 1:
        break;
    case 2:
        break;
    case 3:
        w527_patch();
        break;
    case 4:
        break;
    default:
        break;
    }



    return 0;
}