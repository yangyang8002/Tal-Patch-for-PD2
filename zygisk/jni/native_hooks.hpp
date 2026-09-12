#pragma once

namespace talpatch {

// 安装 native 层防检测 inline hook（Dobby）：
// 过滤 /proc/self/maps 等文件读取中出现的模块/root 痕迹。
void install_native_hooks();

} // namespace talpatch
