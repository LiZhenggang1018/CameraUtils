#ifndef RUNTESTCASE_H
#define RUNTESTCASE_H

#include "TestCase.h"

inline void RunTest()
{
    // TestInBlockingMode();
    // TestInNonBlockingMode();
    // TestFindAndThenOrElse();
    // TestOverloadRef();
    // TimeoutMonitorTest();
    // MiscTest();
    // MsgQueueTest();
    // TestCircularQueue();
    // TestOverwriteFunction();
    // TestEvent();
    // FactoryJsonTest();
    // TestSyncAgency();
    // #if VAS_HAS_STDEXEC
    //     VasConcurrencyTest();
    // #endif
    // ThreadPoolTest();

    // OpenCVTest();
    // TestFFMpeg();
    // SplitCppFilesInDirTest();
    // ConcatCppFilesInDirTest();
    RunLivePhotoSmoothPlayTest();
    return;
}

#endif
