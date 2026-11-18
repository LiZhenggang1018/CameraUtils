#ifndef COREUTILS_H
#define COREUTILS_H

// 纯 C++ 标准库工具层：不依赖 OpenCV / FFmpeg / exiv2。
#include "AllUsedHeadFiles.h"
#include "CommonEnv.h"

#include "CheckUtils.h"
#include "CircularQueue.h"
#include "CustomLog.h"
#include "DoubleEndedBlockingQueue.h"
#include "EnumStrTraits.h"
#include "EventComponents.h"
#include "EventDispatcher.h"
#include "FilesUtils.h"
#include "FindAndThenOrElse.h"
#include "GeneralFactory.h"
#include "InstanceCounter.h"
#include "JsonUtils.h"
#include "NoCopyableMacro.h"
#include "Overload.h"
#include "ScopedUtils.h"
#include "Singleton.h"
#include "SyncAgency.h"
#include "ThreadPool.h"
#include "TimeoutMonitor.h"
#include "Type2InstMap.h"

#include "ConcatCppFilesInDir.h"
#include "SplitCppFilesInDir.h"

#endif // COREUTILS_H
