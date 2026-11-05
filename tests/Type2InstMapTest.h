#ifndef TESTS_TYPE2INSTMAPTEST_H_
#define TESTS_TYPE2INSTMAPTEST_H_

#include <cassert>
#include <iostream>
#include <type_traits>
#include "Type2InstMap.h"

enum class IVEReqType { CAP, VID, AUD };

// ---------- 场景1：Inst 由 Type 构造，同时继承单例 ----------
class RequestManager : public Singleton<RequestManager>
{
public:
    RequestManager() : m_type(IVEReqType::CAP) {}
    explicit RequestManager(IVEReqType type) : m_type(type) {}
    void RemoveRequestData() const
    {
        std::cout << "RemoveRequestData type=" << static_cast<int>(m_type) << '\n';
    }
private:
    IVEReqType m_type;
};

class DefaultMgr : public Singleton<DefaultMgr>
{
public:
    DefaultMgr() = default;
    void DoSomething() const { std::cout << "DefaultMgr::DoSomething\n"; }
};

using ReqMgrMap     = Type2InstMap<IVEReqType, RequestManager>;
using DefaultMgrMap = Type2InstMap<IVEReqType, DefaultMgr>;

inline void TestRequestManager()
{
    std::cout << "===== TestRequestManager: RequestManager (Singleton), Inst from Type =====\n";

    // 池：注册两个实例
    ReqMgrMap::Register({IVEReqType::CAP, IVEReqType::VID});

    // 池内实例可直接调用
    ReqMgrMap::GetMgr(IVEReqType::CAP).RemoveRequestData();
    ReqMgrMap::GetMgr(IVEReqType::VID).RemoveRequestData();

    // 遍历池
    ReqMgrMap::ForEachMgr(
        [](RequestManager& mgr) { mgr.RemoveRequestData(); });

    // 重复注册：应打印 "map insert error"
    ReqMgrMap::Register({IVEReqType::CAP});
}

inline void TestDefaultMgr()
{
    std::cout << "===== TestDefaultMgr: DefaultMgr (Singleton), Inst default constructed =====\n";

    // 池：默认构造分支
    DefaultMgrMap::Register({IVEReqType::CAP, IVEReqType::VID, IVEReqType::AUD});

    DefaultMgrMap::GetMgr(IVEReqType::CAP).DoSomething();
    DefaultMgrMap::ForEachMgr([](DefaultMgr& mgr) { mgr.DoSomething(); });
}

inline void Type2InstMapTest()
{
    TestRequestManager();
    TestDefaultMgr();
    std::cout << "All tests passed.\n";
}

#endif // TESTS_TYPE2INSTMAPTEST_H_
