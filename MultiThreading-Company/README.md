### Welding Company Simulator 

A multithreaded pricing engine that simulates a steel‑plate welding company.  
The program receives **customer orders** for rectangular plates, fetches **price lists** from multiple suppliers, and computes the cheapest way to assemble each plate by welding prefabricated sheets.

Key objectives of the assignment:

* design a scalable thread model (customer‑handler threads + worker pool)
* coordinate asynchronous supplier responses (`addPriceList`)
* load‑balance jobs across CPU cores without busy‑waiting
* optionally integrate the provided `CProgtestSolver` or plug in a custom O(n³) solver

---

## High‑level workflow

```
Customers ──▶ handler threads ──▶ job queue ──▶ worker threads ──▶ CProgtestSolver ──▶ completed()
            ▲                                             │
            │                      ▲                      │
            └────── waitForDemand()│                      │
                                   │                      │
Suppliers ◀─── sendPriceList() ◀───┘
```

* **Handler threads** pull orders via `CCustomer::waitForDemand()` and push them to a concurrent queue.  
* **Worker pool** consumes jobs, merges supplier price lists, and calls the solver.  
* Results are returned to the customer with `CCustomer::completed()`.  
* `CWeldingCompany::stop()` blocks until all queues are empty and every thread joins.

---

## Build & run

```bash
cd MultiThreading-Company
make              # or cmake -B build && cmake --build build -j

# run sample driver (single thread)
./demo_single

# benchmark with 8 worker threads
./demo_multi 8
```

> Tested with **g++ 12.2** (C++20, pthreads).

---

## Core classes

| Class / struct | Role |
| -------------- | ---- |
| `CWeldingCompany` | Manages producer/consumer queues, spawns threads, owns solver instances |
| `CProducer`    | Supplier interface (`sendPriceList`) |
| `CCustomer`    | Customer interface (`waitForDemand`, `completed`) |
| `CProgtestSolver` | Reference solver (capacity‑limited, requires `totalThreads()` simultaneous calls) |
| `CPriceList`, `COrderList` | Data containers for price tables and orders |

---

## Thread safety highlights

* Concurrent job queue guarded by `std::mutex` + `std::condition_variable` (no busy waiting).  
* Atomic counter tracks outstanding orders to gracefully exit workers on `stop()`.  
* Per‑material price‑list cache is `std::unordered_map<materialID, std::vector<CPriceList>>` with RW‑lock.

---

Feel free to open issues or discussions if you have suggestions!
