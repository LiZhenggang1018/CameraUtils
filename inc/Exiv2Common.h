/*
 * Copyright (c) 2026 by Vivo Technologies Co., Ltd, All Rights Reserved.
 * @Description:
 * @Author: lizhenggang
 * @Date: 2026-11-13 16:19:43
 * @LastEditTime: 2026-11-16 11:28:06
 * @FilePath: \CameraUtils\inc\Exiv2Common.h
 */
#ifndef EXIV2COMMON_H
#define EXIV2COMMON_H

#include <string>
#include <unordered_map>
#include <exiv2/exiv2.hpp>
#include "CommonEnv.h"

// ============================================================
// 路径常量：全部改为"函数内 static"返回，避免跨文件静态初始化顺序问题
// 使用方式与原全局变量完全一致（通过 #define 宏别名），引用点无需修改
// ============================================================

// 根路径（末尾带斜杠，与原 LP_SMOOTH_PLAY_DATA_PATH 语义一致）
inline const std::string& Exif2Path() {
    static const std::string s = EnvRootPath() + "/data/HdrMerge/normal.jpg";
    return s;
}