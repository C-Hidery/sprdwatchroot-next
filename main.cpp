/*
 * SPDX-License-Identifier: GPL-3.0-or-later
 * sprdwatchroot-next Copyright (C) 2026 Ryan Crepa
 */
#include <iostream>
#include <string>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>  // 添加 _chdir 所需的头文件
#endif
#include <thread>
#include <new>
#include <cstdlib>
#include <signal.h>
#include <cstdarg>
#include <vector>    // 添加 vector 头文件
#include <limits>    // 添加 limits 头文件
#include "core/file_io.h"
#include "third_party/nlohmann/json.hpp"

using json = nlohmann::json;

#define VERSION "0.2.0"

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
const std::string FDL1_SC9832("FDL\\SC9832\\fdl1.bin");
const std::string FDL2_SC9832("FDL\\SC9832\\fdl2.bin");

enum LogLevel : int {
    I = 0, // Info
    W = 1, // Warning
    E = 2, // Error
    DE = 3, // Debug
    UN = 4,  // Undefined
    OP = 5  // Operation
};

std::string custom_FDL1;
std::string custom_FDL2;
std::string custom_FDL1_ADDR;
std::string custom_FDL2_ADDR;

// 前向声明 copy_file 函数
bool copy_file(const char* src, const char* dst);
// 前向声明 DEG_LOG 函数
void DEG_LOG(LogLevel level, const char* __format, ...);

bool exec_cmd(std::string cmd)
{
    DEG_LOG(I, u8"执行命令: %s\n", cmd.c_str());
    int ret = system(cmd.c_str());
    if (ret != 0)
    {
        DEG_LOG(E, u8"执行命令失败，错误码: %d\n", ret);
        return false;
    }
    return true;
}

// UTF8ToANSI 函数实现保持不变...
#ifdef _WIN32
std::string UTF8ToANSI(const std::string& utf8Str) {
    if (utf8Str.empty()) return "";
    
    int wideLen = MultiByteToWideChar(
        CP_UTF8, 0, utf8Str.c_str(), -1, nullptr, 0
    );
    
    if (wideLen == 0) {
        return utf8Str;
    }
    
    std::vector<wchar_t> wideBuf(wideLen);
    MultiByteToWideChar(
        CP_UTF8, 0, utf8Str.c_str(), -1, wideBuf.data(), wideLen
    );
    
    int ansiLen = WideCharToMultiByte(
        CP_ACP, 0, wideBuf.data(), -1, nullptr, 0, nullptr, nullptr
    );
    
    if (ansiLen == 0) {
        return utf8Str;
    }
    
    std::vector<char> ansiBuf(ansiLen);
    WideCharToMultiByte(
        CP_ACP, 0, wideBuf.data(), -1, ansiBuf.data(), ansiLen, nullptr, nullptr
    );
    
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
    va_list args_copy;
    va_copy(args_copy, args);
    
    int len = vsnprintf(nullptr, 0, __format, args_copy);
    va_end(args_copy);
    
    if (len > 0) {
        std::vector<char> utf8Buffer(len + 1);
        vsnprintf(utf8Buffer.data(), utf8Buffer.size(), __format, args);
        
        std::string gbkStr = UTF8ToANSI(std::string(utf8Buffer.data()));
        printf("%s", gbkStr.c_str());
    }
#else
    vprintf(__format, args);
#endif
    va_end(args);
}

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
    const size_t bufferSize = 8192;
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

bool magisk_patch(const char* fn)
{
    DEG_LOG(I, u8"Start to patch Magisk 27000 to boot\n");

    EnhancedFile f = oxfopen_enhanced(MAGISKBOOT.c_str(), "rb");
    if (!f)
    {
        DEG_LOG(E, u8"找不到%s，无法继续进行操作\n", MAGISKBOOT.c_str());
        return false;
    }
    f.close();

    std::string cmd = MAGISKBOOT + " unpack " + fn;
    DEG_LOG(I, u8"执行命令: %s\n", cmd.c_str());
    int ret = system(cmd.c_str());
    if (ret != 0)
    {
        DEG_LOG(E, u8"执行命令失败，错误码: %d\n", ret);
        return false;
    }
    EnhancedFile f2 = oxfopen_enhanced("ramdisk.cpio", "rb");
    if (f2)
    {
        f2.close();
        std::string cmdf = MAGISKBOOT + " cpio ramdisk.cpio test";
        DEG_LOG(I, u8"执行命令: %s\n", cmdf.c_str());
        int retf = system(cmdf.c_str());
        if (retf == 1)
        {
            // 已被修补，还原
            std::string cmdf2 = MAGISKBOOT + " cpio ramdisk.cpio \"extract .backup/.magisk config.orig\" restore";
            DEG_LOG(I, u8"执行命令: %s\n", cmdf2.c_str());
            int retf2 = system(cmdf2.c_str());
            if (retf2 != 0)
            {
                DEG_LOG(E, u8"执行命令失败，错误码: %d\n", retf2);
                return false;
            }
        }
        else if (retf != 0)
        {
            DEG_LOG(E, u8"执行命令失败，错误码: %d\n", retf);
            return false;
        }
        if (!copy_file("ramdisk.cpio", "ramdisk.cpio.orig"))
        {
            DEG_LOG(E, u8"复制ramdisk.cpio失败，请检查文件是否存在\n");
            return false;
        }
    }
    f2.close();
    EnhancedFile f3 = oxfopen_enhanced("config", "r");
    if (!f3)
    {
        DEG_LOG(E, u8"无法打开config文件\n");
        return false;
    }
    std::string configContent = "RECOVERYMODE=true";
    f3 << configContent;
    f3.close();

    std::string cmdo = MAGISKBOOT + " cpio ramdisk.cpio";

    std::string cmd2 = cmdo + " \"add 0750 init magiskinit\"";
    DEG_LOG(I, u8"执行命令: %s\n", cmd2.c_str());
    int ret2 = system(cmd2.c_str());
    if (ret2 != 0)
    {
        DEG_LOG(E, u8"执行命令失败，错误码: %d\n", ret2);
        return false;
    }
    std::string cmd3 = cmdo + " \"mkdir 0750 overlay.d\"";
    DEG_LOG(I, u8"执行命令: %s\n", cmd3.c_str());
    int ret3 = system(cmd3.c_str());
    if (ret3 != 0)
    {
        DEG_LOG(E, u8"执行命令失败，错误码: %d\n", ret3);
        return false;
    }
    std::string cmd4 = cmdo + " \"mkdir 0750 overlay.d/sbin\"";
    DEG_LOG(I, u8"执行命令: %s\n", cmd4.c_str());
    int ret4 = system(cmd4.c_str());
    if (ret4 != 0)
    {
        DEG_LOG(E, u8"执行命令失败，错误码: %d\n", ret4);
        return false;
    }
    std::string cmd5 = MAGISKBOOT + " compress=xz magisk magisk.xz";
    DEG_LOG(I, u8"执行命令: %s\n", cmd5.c_str());
    int ret5 = system(cmd5.c_str());
    if (ret5 != 0)
    {
        DEG_LOG(E, u8"执行命令失败，错误码: %d\n", ret5);
        return false;
    }
    std::string cmd6 = cmdo + " \"add 0644 overlay.d/sbin/magisk.xz magisk.xz\"";
    DEG_LOG(I, u8"执行命令: %s\n", cmd6.c_str());
    int ret6 = system(cmd6.c_str());
    if (ret6 != 0)
    {
        DEG_LOG(E, u8"执行命令失败，错误码: %d\n", ret6);
        return false;
    }
    EnhancedFile f4 = oxfopen_enhanced("stub.apk", "rb");
    if (f4)
    {
        f4.close();
        std::string cmdi = MAGISKBOOT + " compress=xz stub.apk stub.xz";
        DEG_LOG(I, u8"执行命令: %s\n", cmd6.c_str());
        int reti = system(cmdi.c_str());
        if (reti != 0)
        {
            DEG_LOG(E, u8"执行命令失败，错误码: %d\n", reti);
            return false;
        }
    }
    f4.close();
    std::string cmd7 = cmdo + " \"add 0644 overlay.d/sbin/stub.xz stub.xz\"";
    if (!exec_cmd(cmd7)) return false;

    EnhancedFile f5 = oxfopen_enhanced("init-ld", "rb");

    if (f5)
    {
        f5.close();
        std::string cmdp = MAGISKBOOT + " compress=xz init-ld init-ld.xz";
        if (!exec_cmd(cmdp)) return false;
    }
    f5.close();
    
    EnhancedFile fp = oxfopen_enhanced("init-ld.xz", "rb");
    if (fp)
    {
        fp.close();
        if (!exec_cmd(std::string(cmdo + " \"add 0644 overlay.d/sbin/init-ld.xz init-ld.xz\""))) return false;
    }
    fp.close();

    if (!exec_cmd(std::string(cmdo + " \"patch\""))) return false;
    if (!exec_cmd(std::string(cmdo + " \"backup ramdisk.cpio.orig\""))) return false;
    if (!exec_cmd(std::string(cmdo + " \"mkdir 000 .backup\""))) return false;
    if (!exec_cmd(std::string(cmdo + " \"add 000 .backup/.magisk config\""))) return false;

    EnhancedFile fex = oxfopen_enhanced("extra", "rb");
    if (fex)
    {
        fex.close();
        if (!exec_cmd(std::string(MAGISKBOOT + "dtb extra patch"))) return false;
    }
    fex.close();

    if (!exec_cmd(std::string(MAGISKBOOT + " repack " + fn + " new-boot.img"))) return false;

    return true;
}

// w527_patch 函数保持不变，但修复 std::cin.ignore 的问题
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
    
    // 清除输入缓冲区的换行符
    std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');

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
        std::cin.get();  // 等待用户按键
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

void sc9832_patch()
{
    chdir("Res");

    DEG_LOG(I, u8"------------------------------------------\n");
    DEG_LOG(I, u8"Spreadtrum SL8541E (SC9832E) ROOT 处理程序\n");
    DEG_LOG(I, u8"[1] 执行AVB修补(可选)\n");
    DEG_LOG(I, u8"[2] 执行ROOT修补\n");
    DEG_LOG(I, u8"[3] 救砖\n");
    DEG_LOG(I, u8"[4] 退出程序\n");
    DEG_LOG(I, u8"------------------------------------------\n");

    int selection;
    std::cin >> selection;
    
    // 清除输入缓冲区的换行符
    std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');

    if (selection == 1)
    {
        std::string cmd = SFD_TOOL + " --no-gui --wait 30000 fdl " + FDL1_SC9832 + " 0x5000 fdl " + FDL2_SC9832 + " 0x9efffe00 exec dis_avb_tos reset";
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
        DEG_LOG(I, u8"请在关机状态下按住侧键并连接四点线，工具有反应之后松手\n");
        DEG_LOG(I, u8"开始读取Boot/Vbmeta分区\n");
        if (!exec_cmd(std::string(SFD_TOOL + " --no-gui --wait 30000 fdl " + FDL1_SC9832 + " 0x5000 fdl " + FDL2_SC9832 + " 0x9efffe00 exec path Boot r boot r vbmeta reset"))) return;
        DEG_LOG(I, u8"手表已重启\n");

        if(!copy_file("Boot/boot.img", "boot.img"))
        {
            DEG_LOG(E, u8"复制boot.img失败\n");
            return;
        }
        if(!copy_file("Boot/vbmeta.img", "vbmeta.img"))
        {
            DEG_LOG(E, u8"复制vbmeta.img失败\n");
            return;
        }

        DEG_LOG(I, u8"开始修补Boot分区\n");
        if (!magisk_patch("boot.img"))
        {
            DEG_LOG(E, u8"修补失败\n");
            return;
        }

        EnhancedFile fnew = oxfopen_enhanced("new-boot.img", "rb");
        if (fnew)
        {
            fnew.close();
            DEG_LOG(I, u8"开始签名\n");
            if (!exec_cmd(std::string("python sign_image.py -i new-boot.img"))) return;
            DEG_LOG(I, u8"开始刷入Boot分区\n");
            DEG_LOG(I, u8"请在关机状态下按住侧键并连接四点线，工具有反应之后松手\n");
            if (!exec_cmd(std::string(SFD_TOOL + " --no-gui --wait 30000 fdl " + FDL1_SC9832 + " 0x5000 fdl " + FDL2_SC9832 + " 0x9efffe00 exec w vbmeta vbmeta-sign-custom.img w boot new-boot.img reset"))) return;
            DEG_LOG(I, u8"刷写完毕！请检查是否有报错\n");
            return;
        }
        else
        {
            DEG_LOG(E, u8"找不到new-boot.img\n");
            return;
        }
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
        std::string cmd = SFD_TOOL + " --no-gui --wait 30000 fdl " + FDL1_SC9832 + " 0x5000 fdl " + FDL2_SC9832 + " 0x9efffe00 exec w vbmeta Boot/vbmeta.img w boot Boot/boot.img reset";
        DEG_LOG(I, u8"请在关机状态下按住侧键并连接四点线，工具有反应之后松手\n");
        DEG_LOG(I, u8"执行命令: %s\n", cmd.c_str());
        int ret = system(cmd.c_str());
        if (ret != 0)
        {
            DEG_LOG(E, u8"执行命令失败，错误码: %d\n", ret);
        }
        DEG_LOG(I, u8"刷写完毕！请检查是否有报错\n");
        DEG_LOG(I, u8"请按任意键退出程序...\n");
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

void custom_patch()
{
    chdir("Res");

    DEG_LOG(I, u8"输入FDL1文件地址\n");
    std::cin >> custom_FDL1;
    DEG_LOG(I, u8"输入FDL2文件地址\n");
    std::cin >> custom_FDL2;
    DEG_LOG(I, u8"输入FDL1发送地址\n");
    std::cin >> custom_FDL1_ADDR;
    DEG_LOG(I, u8"输入FDL2发送地址\n");
    std::cin >> custom_FDL2_ADDR;

    bool isAVB = false;
    bool isBSP = false;

    DEG_LOG(I, u8"签名方式？\n");

    DEG_LOG(I, u8"[1] AVB\n");
    DEG_LOG(I, u8"[2] BSP\n");
    int n;
    std::cin >> n;
    
    if (n == 1) isAVB = true;
    else if (n == 2) isBSP = true;
    else 
    {
        DEG_LOG(I, u8"无效输入\n");
        return;
    }


    DEG_LOG(I, u8"------------------------------------------\n");
    DEG_LOG(I, u8"自定义 ROOT 处理程序\n");
    DEG_LOG(I, u8"[1] 执行AVB修补(可选)\n");
    DEG_LOG(I, u8"[2] 执行ROOT修补\n");
    DEG_LOG(I, u8"[3] 救砖\n");
    DEG_LOG(I, u8"[4] 退出程序\n");
    DEG_LOG(I, u8"------------------------------------------\n");

    int selection;
    std::cin >> selection;
    
    // 清除输入缓冲区的换行符
    std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');

    if (selection == 1)
    {
        std::string cmd = SFD_TOOL + " --no-gui --wait 30000 fdl " + custom_FDL1 + " " + custom_FDL1_ADDR + " fdl " + custom_FDL2 + " " + custom_FDL2_ADDR + " exec dis_avb_tos reset";
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
        DEG_LOG(I, u8"请在关机状态下按住侧键并连接四点线，工具有反应之后松手\n");
        DEG_LOG(I, u8"开始读取Boot/Vbmeta分区\n");
        if (!exec_cmd(std::string(SFD_TOOL + " --no-gui --wait 30000 fdl " + custom_FDL1 + " " + custom_FDL1_ADDR + " fdl " + custom_FDL2 + " " + custom_FDL2_ADDR + " exec path Boot r boot r vbmeta reset"))) return;
        DEG_LOG(I, u8"手表已重启\n");

        if(!copy_file("Boot/boot.img", "boot.img"))
        {
            DEG_LOG(E, u8"复制boot.img失败\n");
            return;
        }
        if(!copy_file("Boot/vbmeta.img", "vbmeta.img"))
        {
            DEG_LOG(E, u8"复制vbmeta.img失败\n");
            return;
        }

        DEG_LOG(I, u8"开始修补Boot分区\n");
        if (!magisk_patch("boot.img"))
        {
            DEG_LOG(E, u8"修补失败\n");
            return;
        }

        EnhancedFile fnew = oxfopen_enhanced("new-boot.img", "rb");
        if (fnew)
        {
            fnew.close();
            DEG_LOG(I, u8"开始签名\n");
            if (isAVB)
                if (!exec_cmd(std::string("python sign_image.py -i new-boot.img"))) return;
            else if (isBSP)
                if (!exec_cmd(std::string("python avbtool.py add_hash_footer --image new-boot.img --partition_size " + BOOT_SIZE + " --algorithm " + ALGORITHM + " --key " + KEY_BOOT))) return;
            DEG_LOG(I, u8"开始刷入Boot分区\n");
            DEG_LOG(I, u8"请在关机状态下按住侧键并连接四点线，工具有反应之后松手\n");
            if (!exec_cmd(std::string(SFD_TOOL + " --no-gui --wait 30000 fdl " + custom_FDL1 + " " + custom_FDL1_ADDR + " fdl " + custom_FDL2 + " " + custom_FDL2_ADDR + " exec w vbmeta vbmeta-sign-custom.img w boot new-boot.img reset"))) return;
            DEG_LOG(I, u8"刷写完毕！请检查是否有报错\n");
            return;
        }
        else
        {
            DEG_LOG(E, u8"找不到new-boot.img\n");
            return;
        }
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
        std::string cmd = SFD_TOOL + " --no-gui --wait 30000 fdl " + custom_FDL1 + " " + custom_FDL1_ADDR + " fdl " + custom_FDL2 + " " + custom_FDL2_ADDR + " exec w vbmeta Boot/vbmeta.img w boot Boot/boot.img reset";
        DEG_LOG(I, u8"请在关机状态下按住侧键并连接四点线，工具有反应之后松手\n");
        DEG_LOG(I, u8"执行命令: %s\n", cmd.c_str());
        int ret = system(cmd.c_str());
        if (ret != 0)
        {
            DEG_LOG(E, u8"执行命令失败，错误码: %d\n", ret);
        }
        DEG_LOG(I, u8"刷写完毕！请检查是否有报错\n");
        DEG_LOG(I, u8"请按任意键退出程序...\n");
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
    std::cout << "\t\tResources By TWDXT" << std::endl;
    std::cout << "-------------------------------------------" << std::endl;

    DEG_LOG(I, u8"请选择您的处理器：\n");
    DEG_LOG(I, u8"[1] Spreadtrum SL8541E (SC9832E)\n");
    DEG_LOG(I, u8"[2] Spreadtrum W527\n");
    DEG_LOG(I, u8"[3] 自定义FDL文件\n");

    int selection;
    std::cin >> selection;
    std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');

    switch (selection)
    {
    case 1:
        sc9832_patch();
        break;
    case 2:
        w527_patch();
        break;
    case 3:
        custom_patch();
        break;
    default:
        break;
    }

    DEG_LOG(I, u8"按任意键继续\n");
    std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
    std::cin.get();

    return 0;
}