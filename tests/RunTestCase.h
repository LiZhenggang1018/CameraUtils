#ifndef RUNTESTCASE_H
#define RUNTESTCASE_H

#include "TestCase.h"
#include "Type2InstMapTest.h"
#include "MagicEnumTest.h"
#include "OpticalFlowTest.h"


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
    // Type2InstMapTest();
    // MagicEnumTest::TestMagicEnum();
    // HarrisTest();
     // OpticalFlowTest();
     //Upsample30To60();
}

#endif
