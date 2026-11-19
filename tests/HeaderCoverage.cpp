/*
 * @Description: 头文件覆盖用翻译单元
 *
 * 本文件不含任何逻辑，唯一目的是把未被任何生产 TU 包含的头文件聚合进来，
 * 使它们参与编译并进入 compile_commands.json，从而被 clang-tidy 与编译器检查。
 *
 * 背景：tidy.yml 逐个翻译单元运行 clang-tidy，头文件的告警只有在被某个 TU
 * 包含时才会产生。聚合头（*All.h）与仅被聚合头包含的头文件此前无人包含，
 * 等于从未接受过任何静态检查。
 *
 * 下列聚合头已覆盖：CoreUtils.h、Exiv2Common.h、CommonHead.h、
 * HdrMerge/HdrMergeAll.h、LivePhotoSmoothPlay/LivePhotoSmoothPlayAll.h、
 * OpticalFlow/OpticalFlowAll.h。
 *
 * inc/Utils.h 已标记 @deprecated 且零消费者，内容全部被上面几个聚合头覆盖，
 * 故不在此包含；它属于待删除的死头，删除后覆盖率即为 100%。
 *
 * 新增聚合头或独立头文件时，请同步补充到下面。
 */

#include "CommonHead.h"
#include "CoreUtils.h"
#include "Exiv2Common.h"
#include "HdrMerge/HdrMergeAll.h"
#include "LivePhotoSmoothPlay/LivePhotoSmoothPlayAll.h"
#include "OpticalFlow/OpticalFlowAll.h"
