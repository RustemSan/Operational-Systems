#include <cstdio>
#include <cmath>
#include "sample_tester.h"

//=============================================================================================================================================================
void CProducerSync::sendPriceList(unsigned materialID) {
    if (materialID == 1) {
        APriceList l = std::make_shared<CPriceList>(1);
        for (const auto &x: c_Prod)
            l->add(x);
        m_Receiver(std::static_pointer_cast<CProducerSync>(shared_from_this()), l);
    }
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------
std::vector<CProd>                     CProducerSync::c_Prod =
        {
                CProd(1, 1, 10),
                CProd(2, 7, 120),
                CProd(11, 8, 950)
        };

//=============================================================================================================================================================
void CProducerAsync::start() {
    m_Thr = std::move(std::thread(&CProducerAsync::prodThr, this));
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------
void CProducerAsync::stop() {
    std::unique_lock locker(m_Mtx);
    m_Stop = true;
    m_Cond.notify_one();
    locker.unlock();
    m_Thr.join();
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------
void CProducerAsync::sendPriceList(unsigned materialID) {
    if (materialID == 1) {
        std::lock_guard locker(m_Mtx);
        m_Req++;
        m_Cond.notify_one();
    }
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------
void CProducerAsync::prodThr() {
    while (true) {
        std::unique_lock locker(m_Mtx);
        m_Cond.wait(locker, [this](void) {
            return m_Stop || m_Req > 0;
        });

        if (m_Stop)
            break;
        if (m_Req > 0) {
            m_Req--;
            locker.unlock();
            APriceList l = std::make_shared<CPriceList>(1);
            for (const auto &x: c_Prod)
                l->add(x);
            m_Receiver(shared_from_this(), l);
        }
    }
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------
std::vector<CProd>                     CProducerAsync::c_Prod =
        {
                CProd(2, 7, 125),
                CProd(3, 5, 150),
                CProd(7, 3, 240),
                CProd(4, 4, 155),
        };

//=============================================================================================================================================================
AOrderList CCustomerTest::waitForDemand() {
    if (!m_Count)
        return AOrderList();
    m_Count--;
    AOrderList req = std::make_shared<COrderList>(1);
    for (const auto &x: c_Orders)
        req->add(x.first);
    return req;
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------
void CCustomerTest::completed(AOrderList x) {
    bool mismatch = false;

    for (size_t i = 0; i < c_Orders.size(); i++)
        if (fabs(c_Orders[i].second - x->m_List[i].m_Cost) > 1e-5 * c_Orders[i].second) {
            mismatch = true;
            break;
        }
    printf("CCustomerTest::completed, status = %s\n", mismatch ? "fail" : "OK");
}

//-------------------------------------------------------------------------------------------------------------------------------------------------------------
std::vector<std::pair<COrder, double> > CCustomerTest::c_Orders =
        {
                std::make_pair(COrder(2, 2, 0.0), 40.0),
                std::make_pair(COrder(7, 12, 1.0), 755.0),
                std::make_pair(COrder(8, 4, 10.0), 350.0),
                std::make_pair(COrder(25, 11, 0.1), 2399.8)
        };
//=============================================================================================================================================================
