#ifndef __PROGTEST__

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <climits>
#include <cfloat>
#include <cmath>
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <numeric>
#include <vector>
#include <set>
#include <list>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <stack>
#include <deque>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <semaphore>
#include <atomic>
#include <condition_variable>
#include "progtest_solver.h"
#include "sample_tester.h"

#endif /* __PROGTEST__ */

struct trackMaterialID {
    unsigned int totalProducers;
    unsigned int prodRemain;
    bool isAnswered;
    std::unordered_set<AProducer> respondedCust;
};

using orderItem = std::pair<AOrderList, ACustomer>;

// ------------------- MySolver -------------------
class Mysolver {
public:
    static double calculatePrice(const APriceList &priceList, int w, int h,
                                 double weldStrength, unsigned int threadCount = 1);

private:
    struct MemoSolver {
        MemoSolver(const std::map<std::pair<unsigned, unsigned>, double> & c,
                   int maxW, int maxH, double ws)
                : cost(c), W(maxW), H(maxH), weld(ws)
        {
            dp.resize((W+1) * (H+1), DBL_MAX);
            used.resize((W+1) * (H+1), false);
        }

        double solve(int w, int h) {
            if (w == 0 || h == 0)
                return DBL_MAX;

            int idx = w*(H+1) + h;
            if (used[idx]) {
                return dp[idx];
            }
            used[idx] = true;

            double best = DBL_MAX;
            auto it = cost.find({w, h});
            if (it != cost.end()) {
                best = it->second;
            }

            for (int x = 1; x < w; x++) {
                double c1 = solve(x, h);
                double c2 = solve(w - x, h);
                if (c1 < DBL_MAX && c2 < DBL_MAX) {
                    double candidate = c1 + c2 + weld * h;
                    if (candidate < best) {
                        best = candidate;
                    }
                }
            }
            for (int y = 1; y < h; y++) {
                double c1 = solve(w, y);
                double c2 = solve(w, h - y);
                if (c1 < DBL_MAX && c2 < DBL_MAX) {
                    double candidate = c1 + c2 + weld * w;
                    if (candidate < best) {
                        best = candidate;
                    }
                }
            }

            dp[idx] = best;
            return best;
        }

        const std::map<std::pair<unsigned, unsigned>, double> & cost;
        int W, H;
        double weld;

        std::vector<double> dp;
        std::vector<bool> used;
    };
};

double Mysolver::calculatePrice(const APriceList &priceList,
                                int w, int h,
                                double weldStrength,
                                unsigned int threadCount)
{
    if (!priceList || w <= 0 || h <= 0)
        return DBL_MAX;

    std::map<std::pair<unsigned, unsigned>, double> currentCost;
    for (auto &prod : priceList->m_List) {
        unsigned w1 = prod.m_W;
        unsigned h1 = prod.m_H;
        double c = prod.m_Cost;
        auto it = currentCost.find({w1, h1});
        if (it == currentCost.end()) {
            currentCost[{w1, h1}] = c;
        } else {
            it->second = std::min(it->second, c);
        }
        auto it2 = currentCost.find({h1, w1});
        if (it2 == currentCost.end()) {
            currentCost[{h1, w1}] = c;
        } else {
            it2->second = std::min(it2->second, c);
        }
    }

    MemoSolver solver(currentCost, w, h, weldStrength);

    double result = solver.solve(w, h);
    return result;
}


// ------------------- CWeldingCompany -------------------
class CWeldingCompany {
public:
    static bool usingProgtestSolver() { return false; }
    static void seqSolve(APriceList priceList, COrder &order) {
        double cost = Mysolver::calculatePrice(priceList, order.m_W, order.m_H, order.m_WeldingStrength);
        order.m_Cost = (cost < DBL_MAX) ? cost : DBL_MAX;
    }
    void addProducer(AProducer prod);
    void addCustomer(ACustomer cust);
    void addPriceList(AProducer prod, APriceList priceList);
    void receiverThreadMethod(ACustomer customer);
    void workingThreadMethod();
    void senderThreadMethod();
    void start(unsigned thrCount);
    void stop();
private:
    std::vector<AProducer> prodList;
    std::vector<ACustomer> custList;
    std::queue<orderItem> orderQueue;
    std::unordered_map<int, APriceList> priceLists;
    std::queue<orderItem> completedOrders;
    std::mutex completedOrdersMutex;
    std::condition_variable completedOrdersCV;
    std::mutex priceListMutex;
    std::vector<std::thread> workingThreads;
    std::vector<std::thread> customerThreads;
    std::thread senderThread;
    std::mutex queueMutex;
    std::condition_variable queueCV;
    std::atomic_bool stopQueue = false;
    std::condition_variable priceListCV;
    std::unordered_map<int, trackMaterialID> requestId;
    std::unordered_set<int> requestedId;
    std::mutex requestedIDMutex;
    std::atomic_bool allProducersDone{false};
    std::atomic_bool runningWorkerThread{false};
    std::unordered_map<int, std::vector<orderItem>> waitingOrderQueue;
    std::mutex waitingQueueMutex;
};

void CWeldingCompany::addProducer(AProducer prod) {
    if (prod)
        prodList.push_back(prod);
}

void CWeldingCompany::addCustomer(ACustomer cust) {
    if (cust)
        custList.push_back(cust);
}

struct PairHash {
    std::size_t operator()(const std::pair<unsigned, unsigned>& p) const {
        return std::hash<unsigned>()(p.first) ^ (std::hash<unsigned>()(p.second) << 1);
    }
};


void CWeldingCompany::addPriceList(AProducer prod, APriceList newList) {
    if (!newList || newList->m_MaterialID == 0 || !prod)
        return;

    {
        std::lock_guard<std::mutex> lock(priceListMutex);
        auto &existing = priceLists[newList->m_MaterialID];
        if (!existing)
            existing = std::make_shared<CPriceList>(newList->m_MaterialID);

        std::unordered_map<std::pair<unsigned, unsigned>, double, PairHash> merged;
        merged.reserve(existing->m_List.size() + newList->m_List.size());

        auto updateMerged = [&merged](const CProd &item) {
            unsigned w = std::min(item.m_W, item.m_H);
            unsigned h = std::max(item.m_W, item.m_H);
            std::pair<unsigned, unsigned> key = {w, h};
            auto it = merged.find(key);
            if (it == merged.end())
                merged[key] = item.m_Cost;
            else
                it->second = std::min(it->second, item.m_Cost);
        };

        for (auto &oldItem : existing->m_List)
            updateMerged(oldItem);
        for (auto &newItem : newList->m_List)
            updateMerged(newItem);

        std::vector<CProd> newListVec;
        newListVec.reserve(merged.size());
        for (auto &entry : merged) {
            unsigned W = entry.first.first;
            unsigned H = entry.first.second;
            double cost = entry.second;
            newListVec.push_back(CProd(W, H, cost));
        }
        existing->m_List = std::move(newListVec);
    }
    {
        std::lock_guard<std::mutex> lock(priceListMutex);
        int mid = newList->m_MaterialID;
        auto it = requestId.find(mid);
        if (it == requestId.end()) {
            trackMaterialID tmp;
            tmp.totalProducers = (unsigned)prodList.size();
            tmp.prodRemain = tmp.totalProducers;
            if (tmp.prodRemain > 0)
                tmp.prodRemain--;
            if (tmp.prodRemain == 0) {
                tmp.isAnswered = true;
                priceListCV.notify_all();
                {
                    std::lock_guard<std::mutex> plock(waitingQueueMutex);
                    if (waitingOrderQueue.count(mid) > 0) {
                        std::lock_guard<std::mutex> qlock(queueMutex);
                        for (auto &ord : waitingOrderQueue[mid])
                            orderQueue.push(ord);
                        waitingOrderQueue.erase(mid);
                        queueCV.notify_all();
                    }
                }
            }
            requestId[mid] = tmp;
        } else {
            trackMaterialID &info = it->second;
            if (info.respondedCust.count(prod) == 0) {
                info.respondedCust.insert(prod);
                if (info.prodRemain > 0)
                    info.prodRemain--;
                if (info.prodRemain == 0) {
                    info.isAnswered = true;
                    priceListCV.notify_all();
                    {
                        std::lock_guard<std::mutex> plock(waitingQueueMutex);
                        if (waitingOrderQueue.count(mid) > 0) {
                            std::lock_guard<std::mutex> qlock(queueMutex);
                            for (auto &ord : waitingOrderQueue[mid])
                                orderQueue.push(ord);
                            waitingOrderQueue.erase(mid);
                            queueCV.notify_all();
                        }
                    }
                }
            }
        }
    }
}



void CWeldingCompany::start(unsigned int thrCount) {
    stopQueue = false;
    workingThreads.resize(thrCount);
    for (unsigned int i = 0; i < thrCount; ++i)
        workingThreads[i] = std::thread(&CWeldingCompany::workingThreadMethod, this);
    senderThread = std::thread(&CWeldingCompany::senderThreadMethod, this);
    for (auto &cust : custList) {
        std::thread tmpThread(&CWeldingCompany::receiverThreadMethod, this, cust);
        customerThreads.push_back(std::move(tmpThread));
    }
}

void CWeldingCompany::receiverThreadMethod(ACustomer customer) {
    while (true) {
        AOrderList tmpOrder = customer->waitForDemand();
        if (!tmpOrder)
            break;
        orderItem orderGroup = {tmpOrder, customer};
        int matID = (int)tmpOrder->m_MaterialID;
        {
            std::lock_guard<std::mutex> lock(requestedIDMutex);
            if (!requestedId.count(matID)) { // todo chto za hujnya
                requestedId.insert(matID);
                for (auto &p : prodList)
                    p->sendPriceList(matID);
            }
        }
        bool ready = false;
        {
            std::lock_guard<std::mutex> lock(priceListMutex);
            auto it = requestId.find(matID);
            if (it != requestId.end() && it->second.prodRemain == 0)
                ready = true;
        }
        if (ready) {
            std::lock_guard<std::mutex> lock(queueMutex);
            orderQueue.push(orderGroup);
            queueCV.notify_one();
        } else {
            std::lock_guard<std::mutex> lock(waitingQueueMutex);
            waitingOrderQueue[matID].push_back(orderGroup);
        } // todo pryam stranno
    }
}

void CWeldingCompany::workingThreadMethod() {
    while (true) {
        orderItem order;
        {
            std::unique_lock<std::mutex> lkQ(queueMutex);
            queueCV.wait(lkQ, [this]() {
                return !orderQueue.empty() || (stopQueue.load() && allProducersDone);
            });
            if (stopQueue.load() && orderQueue.empty() && allProducersDone.load())
                return;
            if (orderQueue.empty())
                continue;

            order = orderQueue.front();
            orderQueue.pop();
        }

        int matID = (int)order.first->m_MaterialID;
        APriceList tmpPriceList;
        {
            std::lock_guard<std::mutex> gl(priceListMutex);
            auto itPL = priceLists.find(matID);
            if (itPL != priceLists.end())
                tmpPriceList = itPL->second;
        }

        if (!tmpPriceList) {
            for (auto &ord : order.first->m_List)
                ord.m_Cost = DBL_MAX;
        } else {
            for (auto &ord : order.first->m_List){
                double c = Mysolver::calculatePrice(tmpPriceList,
                                                    ord.m_W,
                                                    ord.m_H,
                                                    ord.m_WeldingStrength);
                ord.m_Cost = (c < DBL_MAX) ? c : DBL_MAX;
            }
        } // \todo strannaya xuina
        {
            std::lock_guard<std::mutex> lkC(completedOrdersMutex);
            completedOrders.push(order);
        }
        completedOrdersCV.notify_one();
    }
}

void CWeldingCompany::senderThreadMethod() {
    while (true) {
        orderItem orderList;
        {
            std::unique_lock<std::mutex> lock(completedOrdersMutex);
            completedOrdersCV.wait(lock, [this]() {
                return !completedOrders.empty() || stopQueue.load();
            });
            if (stopQueue.load() && completedOrders.empty() && runningWorkerThread.load())
                return;
            if (!completedOrders.empty()) {
                orderList = completedOrders.front();
                completedOrders.pop();
            } else
                continue;
        }
        if (orderList.first)
            orderList.second->completed(orderList.first);
    }
}

void CWeldingCompany::stop() {
    for (auto &tmp : customerThreads) {
        if (tmp.joinable())
            tmp.join();
    }
    customerThreads.clear();
    while (true) {
        allProducersDone = true;
        {
            std::lock_guard<std::mutex> lock(priceListMutex);
            for (const auto &entry : requestId) {
                if (entry.second.prodRemain > 0) {
                    allProducersDone = false;
                    break;
                }
            }
        }
        if (allProducersDone)
            break;
    }
    allProducersDone.store(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    {
        std::unique_lock<std::mutex> lockQueue(queueMutex);
        stopQueue.store(true);
    }
    queueCV.notify_all();
    priceListCV.notify_all();
    for (auto &tmp : workingThreads) {
        if (tmp.joinable())
            tmp.join();
    }
    workingThreads.clear();
    runningWorkerThread.store(true);
    completedOrdersCV.notify_all();
    if (senderThread.joinable())
        senderThread.join();
}

//-------------------------------------------------------------------------------------------------
#ifndef __PROGTEST__

int main() {
    using namespace std::placeholders;
    CWeldingCompany test;
    AProducer p1 = std::make_shared<CProducerSync>(
            std::bind(&CWeldingCompany::addPriceList, &test, _1, _2));
    AProducerAsync p2 = std::make_shared<CProducerAsync>(
            std::bind(&CWeldingCompany::addPriceList, &test, _1, _2));

    test.addProducer(p1);
    test.addProducer(p2);
    test.addCustomer(std::make_shared<CCustomerTest>(2));
    p2->start();
    test.start(3);
    test.stop();
    p2->stop();
    return EXIT_SUCCESS;
}

#endif /* __PROGTEST__ */
