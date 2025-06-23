#ifndef PROGTEST_SOLVER_H_23452908476417340123
#define PROGTEST_SOLVER_H_23452908476417340123

#include <vector>
#include <memory>
#include "common.h"

//=============================================================================================================================================================
/**
 * Problem solver provided by the progtest environment. Solves a batch of problems. The solvers is technically
 * a subclasses of the base CProgtestSolver class, the factory functions below are used to create the instances.
 *
 * Each instance of the solver has a certain capacity - the solver solves a batch of problems at most of that size.
 * You are expected to fill the solver with problem instances, you may add any number of instances smaller than or
 * equal to the capacity. Subsequently, you are expected to call the solve() method to start the computation.
 * Each solver needs one or more threads to call the solve () method simultaneously. The computation starts once
 * the required number of threads calls method solve (). The solver then processes all stored problems and fills
 * the corresponding field in the COrder instances.
 *
 * If you need to solve further problems, you are free to create further CProgtestSolver instances. Moreover, you may
 * call solve () at any moment before the capacity of a CProgtestSolver is exceeded. That is, you may even add just
 * one COrder instance and call solve ().
 *
 * There are, however, some pitfalls. The progtest solver is a bit moody. It is aware of the test data and it knows
 * there will be N problems in the test. Moreover, it decides to create only k useful instances (k is a certain integer).
 * The created instances set their capacities to m1, m2, m3, ..., mk. The total sum is M = m1+m2+m3+ ... + mk and the
 * solver guarantees that M is equal to N or a bit greater than N.
 *
 * In other words, the solver is able to solve all problems in the test, however, you may not waste the capacity of
 * the solver instances. If you do so, you may run out of the total solver capacity.
 *
 * As stated above, the solver only creates k useful instances. If you want to create more than k instances, the
 * subsequent calls to createProgtestSolver() will return an empty smart pointer, a solved that fills-in invalid
 * results, or a solver with zero capacity (as stated above, the solver is moody/playful). Either way, the extra solver
 * instances are not very useful.
 */
class CProgtestSolver {
public:
    virtual                            ~CProgtestSolver() noexcept = default;

    /**
     * Check the used capacity of the solver. Returns true if the already used capacity is smaller than the limit.
     * @return true = there is a free space (capacity not fully used yet), false = no free capacity
     */
    virtual bool hasFreeCapacity() const = 0;

    /**
     * Add a new problem instance to be solved.
     * @param[in] priceList            the prices to use for the computation
     * @param[in] order                the problem to slve, the result is filled into COrder::m_Cost
     * @return true = problem added, false = problem not added (already at the capacity)
     */
    virtual bool addProblem(APriceList priceList,
                            COrder &order) = 0;

    /**
     * Get the number of threads that need to call the solve () method before the computaiton starts.
     * @return the nuber of threads needed
     */
    virtual size_t totalThreads() const = 0;

    /**
     * Solve the problems previously stored into the solver. The calling threads are used to compute the
     * previously stored problems. The computation starts when the solve () method is called by sufficient number of
     * threads - use totalThreads () method to get the number. The threads are suspended in the solve () method until
     * there is just enough threads ready. Once there is the correct number of threads, the stored problem instances
     * are solved. Once there are no further problems to solve, the threads return from solve () method. The last
     * returning thread indicates that the stored problems were solved.
     * @return the true (all problems solved) / false (error, e.g., solve called on an instance with zero capacity).
     */
    virtual bool solve() = 0;
};

using AProgtestSolver = std::shared_ptr<CProgtestSolver>;
//=============================================================================================================================================================
/**
 * Factory function to create a new instance of progtest solver.
 *
 * The delivered library creates solver instances such that the total number of solved problems is 100 (M=100) and the
 * solver requires 2 threads to start. These limits apply to the solver in the attached library only. The solvers in the
 * Progtest testing environment use different limits, the limits are set with respect to the size of input data used for
 * the tests.
 * @return an instance of the solver
 */
AProgtestSolver createProgtestSolver();

/**
 * Factory function to create a new instance of progtest solver - debug.
 *
 * This function is an extension of the factory function above. The function takes two parameters - the capacity of the
 * solver and the nuber of threads required by the solver. The extra parameters are present to provide an easy way
 * to debug your program. However, the debug function only exists in the provided library. There IS NOT such debug
 * function available in the testing environment. The program will not compile if the function is used in the submitted
 * program.
 *
 * @param[in] capacity       the desired capacity of the solver
 * @param[in] thr            the desired number of threads required by the solver
 * @return an instance of the solver
 */
AProgtestSolver createProgtestSolverDebug(size_t capacity,size_t thr);

#endif /* PROGTEST_SOLVER_H_23452908476417340123 */
