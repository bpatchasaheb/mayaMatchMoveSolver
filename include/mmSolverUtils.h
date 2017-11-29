/*
 * Uses Non-Linear Least Squares algorithm from levmar library to calculate attribute values based on 2D-to-3D error measurements through a pinhole camera.
 */


#ifndef MAYA_MM_SOLVER_UTILS_H
#define MAYA_MM_SOLVER_UTILS_H


#ifndef HAVE_SPLM
#error "HAVE_SPLM were not defined."
#endif


#ifdef HAVE_SPLM
#if HAVE_SPLM == 0
#warning "HAVE_SPLM was not given."
#endif
#endif


// Lev-Mar
#include <levmar.h>  // dlevmar_dif


// Sparse Lev-Mar
#if HAVE_SPLM == 1
#include <splm.h>    // sparselm_difccs
#endif


// STL
#include <ctime>     // time
#include <cmath>     // exp
#include <iostream>  // cout, cerr, endl
#include <string>    // string
#include <vector>    // vector
#include <cassert>   // assert
#include <math.h>

// Utils
#include <utilities/debugUtils.h>

// Maya
#include <maya/MPoint.h>
#include <maya/MVector.h>
#include <maya/MString.h>
#include <maya/MStringArray.h>
#include <maya/MObject.h>
#include <maya/MFnAnimCurve.h>
#include <maya/MAnimCurveChange.h>
#include <maya/MSelectionList.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MMatrix.h>
#include <maya/MFloatMatrix.h>
#include <maya/MFnCamera.h>
#include <maya/MComputation.h>
#include <maya/MProfiler.h>

// GL Math
#include <glm/glm.hpp>

#define FABS(x)     (((x)>=0)? (x) : -(x))
#define CNST(x) (x)
#define EXTRA_GET_TRIGGER 0
#define SWITCH_TIME_TRIGGER 0
#define BENCHMARK_TYPE debug::TimestampBenchmark

#define SOLVER_TYPE_LEVMAR 0
#define SOLVER_TYPE_SPARSE_LEVMAR 1


// Sparse LM or Lev-Mar Termination Reasons:
const std::string reasons[8] = {
        // reason 0
        "No reason, should not get here!",

        // reason 1
        "Stopped by small gradient J^T e",

        // reason 2
        "Stopped by small Dp",

        // reason 3
        "Stopped by reaching maximum iterations",

        // reason 4
        "Singular matrix. Restart from current parameters with increased \'Tau Factor\'",

        // reason 5
        "Too many failed attempts to increase damping. Restart with increased \'Tau Factor\'",

        // reason 6
        "Stopped by small error",

        // reason 7
        // "stopped by invalid (i.e. NaN or Inf) \"func\" refPoints (user error)",
        "User canceled",
};


// The user data given to levmar.
struct SolverData {
    // Solver Objects.
    CameraPtrList cameraList;
    MarkerPtrList markerList;
    BundlePtrList bundleList;
    AttrPtrList attrList;
    MTimeArray frameList;  // Times to solve

    // Relational mapping indexes.
    std::vector<std::pair<int, int> > paramToAttrList;
    std::vector<std::pair<int, int> > errorToMarkerList;

    // Internal Solver Data.
    std::vector<double> errorList;
    int iterNum;
    int jacIterNum;
    int iterMax;
    int solverType;
    bool isJacobianCalculation;

    // Error Thresholds.
    double tau;
    double eps1;
    double eps2;
    double eps3;
    double delta;

    BENCHMARK_TYPE *jacBench;
    BENCHMARK_TYPE *funcBench;
    BENCHMARK_TYPE *errorBench;
    BENCHMARK_TYPE *paramBench;

    // Storing changes for undo/redo.
    MDGModifier *dgmod;
    MAnimCurveChange *curveChange;
    MComputation *computation;

    // Verbosity.
    bool verbose;
};


// Function run by lev-mar algorithm to test the input parameters, p, and compute the output errors, x.
inline
void levmar_solveFunc(double *p, double *x, int m, int n, void *data) {
    register int i = 0;
    SolverData *ud = static_cast<SolverData *>(data);
    ud->funcBench->start();
    ud->computation->setProgress(ud->iterNum);
    if (ud->isJacobianCalculation == false){
        INFO("Solve " << ++ud->iterNum);
    } else {
        INFO("Solve Jacobian " << ++ud->jacIterNum);
    }

    if (ud->computation->isInterruptRequested()) {
        WRN("User wants to cancel the solve!");
        // This is an ugly hack to force levmar to stop computing. We give
        // a NaN value which causes levmar detect it and quit the loop.
        for (i = 0; i < n; ++i) {
            x[i] = NAN;
        }
        return;
    }

    ud->paramBench->start();
    MStatus status;
    MTime time;

    MTime currentFrame = MAnimControl::currentTime();
    for (i = 0; i < m; ++i) {
        std::pair<int, int> attrPair = ud->paramToAttrList[i];
        AttrPtr attr = ud->attrList[attrPair.first];

        // Get frame time
        MTime frame = currentFrame;
        if (attrPair.second != -1) {
            frame = ud->frameList[attrPair.second];
        }

        attr->setValue(p[i], frame, *ud->dgmod, *ud->curveChange);
    }

    // Commit changed data into Maya
    ud->dgmod->doIt();

    // Invalidate the Camera Matrix cache.
    // In future we might be able to auto-detect if the camera will change based on
    // the current solve and not invalidate the cache but for now we cannot take the
    // risk of an incorrect solve; we clear the cache.
    for (i = 0; i < (int) ud->cameraList.size(); ++i) {
        ud->cameraList[i]->clearWorldProjMatrixCache();
    }
    ud->paramBench->stop();

    // Calculate Errors
    ud->errorBench->start();
    MMatrix cameraWorldProjectionMatrix;
    MPoint mkr_mpos;
    MPoint bnd_mpos;
    for (i = 0; i < (n / 3); ++i) {
        std::pair<int, int> markerPair = ud->errorToMarkerList[i];
        MarkerPtr marker = ud->markerList[markerPair.first];
        MTime frame = ud->frameList[markerPair.second];

        CameraPtr camera = marker->getCamera();
        status = camera->getWorldProjMatrix(cameraWorldProjectionMatrix, frame);
        CHECK_MSTATUS(status);

#if SWITCH_TIME_TRIGGER == 1
        MAnimControl::setCurrentTime(frame+1);
        status = marker->getPos(mkr_mpos, frame + 1);
        MAnimControl::setCurrentTime(frame);
#endif

#if EXTRA_GET_TRIGGER == 1
        status = marker->getPos(mkr_mpos, frame + 1);
        CHECK_MSTATUS(status);
#endif
        status = marker->getPos(mkr_mpos, frame);
        CHECK_MSTATUS(status);
        mkr_mpos = mkr_mpos * cameraWorldProjectionMatrix;
        mkr_mpos.cartesianize();
        glm::vec2 mkr_pos2d(mkr_mpos.x, mkr_mpos.y);

        BundlePtr bnd = marker->getBundle();
#if EXTRA_GET_TRIGGER == 1
        status = bnd->getPos(bnd_mpos, frame + 1);
        CHECK_MSTATUS(status);
#endif
        status = bnd->getPos(bnd_mpos, frame);
        CHECK_MSTATUS(status);
        bnd_mpos = bnd_mpos * cameraWorldProjectionMatrix;
        bnd_mpos.cartesianize();
        glm::vec2 bnd_pos2d(bnd_mpos.x, bnd_mpos.y);

        // NOTE: Interestingly, using an x, y and distance error measurement
        // seems to allow at least some scenes to converge much faster;
        // ~20 iterations compared to ~160 iterations.
        // TODO: dx, dy and d are all in world units. We should shift them
        // into 'image space', so that we can refer to the error in
        // terms of pixels.
        double dx = fabs(mkr_mpos.x - bnd_mpos.x);
        double dy = fabs(mkr_mpos.y - bnd_mpos.y);
        double d = fabs(glm::distance(bnd_pos2d, mkr_pos2d));
        x[(i * 3) + 0] = dx;  // X error
        x[(i * 3) + 1] = dy;  // Y error
        x[(i * 3) + 2] = d;   // Distance error

        ud->errorList[(i * 3) + 0] = dx;
        ud->errorList[(i * 3) + 1] = dy;
        ud->errorList[(i * 3) + 2] = dy;
    }
    ud->errorBench->stop();
    ud->funcBench->stop();
    return;
}

// From 'splm.c'.
//
// Attempt to guess the Jacobian's non-zero pattern
// The idea is to add a small value to each parameter in turn
// and identify the observations that are influenced.
//
// This function should be used with caution as it cannot guarantee
// that the true non-zero pattern will be found. Furthermore, it can
// give rise to domain errors.
//
// Returns the number of nonzero elements found
//
inline
int jacobianZeroPatternGuess(void (*func)(double *p, double *hx, int nvars, int nobs, void *adata),
                             double *p,
                             struct splm_ccsm *jac,
                             int nvars,
                             int nobs,
                             void *adata,
                             double *hx,
                             double delta) {
    const double delta_scale = 1E+02;
    register int i, j, k;
    register double d, tmp;
    double *hxx;
    int *colptr, *rowidx;

    colptr = jac->colptr;
    rowidx = jac->rowidx;

    // Solve once to get the base-line, these errors and parameters
    // will be compared against to build the jacobian.
    (*func)(p, hx, nvars, nobs, adata); // hx=f(p)
    hxx = (double *) malloc(nobs * sizeof(double));

    // Loop over parameters.
    for (j = k = 0; j < nvars; ++j) {
        colptr[j] = k;

        // determine d=max(DELTA_SCALE*|p[j]|, delta), see HZ
        d = delta_scale * p[j]; // force evaluation
        d = FABS(d);
        if (d < delta) {
            d = delta;
        }

        // TODO: Sometimes, a small or large delta may not produce a measurable
        // change in error. Therefore, it may help to compute an accurate jacobian.

        // Set the parameter, solve with the adjustment and then reset the parameter.
        tmp = p[j];
        p[j] += d;
        (*func)(p, hxx, nvars, nobs, adata); // hxx=f(p+d)
        p[j] = tmp; // restore

        // Loop over errors.
        for (i = 0; i < nobs; ++i) {
            tmp = FABS(hxx[i] - hx[i]);
            if (tmp > 0.0) { // tmp>1E-07*d
                // element (i, j) of jac != 0
                if (k >= jac->nnz) { // more memory needed, double current size
                    splm_ccsm_realloc_novalues(jac, nobs, nvars, jac->nnz << 1); // 2*jac->nnz
                    rowidx = jac->rowidx; // re-init
                }
                rowidx[k++] = i;
            }
        }
    }
    colptr[nvars] = k;
    splm_ccsm_realloc_novalues(jac, nobs, nvars, k); // adjust to actual size...

    free(hxx);
    return k;
}

//// From levmar project, used internally to create a jacobian.
//// Forward finite difference approximation to the Jacobian of func
//inline
//void finiteDiffForwJacGuess(
//
//        // function to differentiate
//        void (*func)(double *p, double *hx, int m, int n, void *adata),
//
//        // I: current parameter estimate, mx1
//        double *p,
//
//        // I: func evaluated at p, i.e. hx=func(p), nx1
//        double *hx,
//
//        // W/O: work array for evaluating func(p+delta), nx1
//        double *hxx,
//
//        // increment for computing the Jacobian
//        double delta,
//
//        // O: array for storing approximated Jacobian, nxm
//        double *jac,
//
//        int m,
//        int n,
//        void *adata)
//{
//    register int i, j;
//    double tmp;
//    register double d;
//
//    for(j=0; j<m; ++j){
//        /* determine d=max(1E-04*|p[j]|, delta), see HZ */
//        d=CNST(1E-04)*p[j]; // force evaluation
//        d=FABS(d);
//        if(d<delta)
//            d=delta;
//
//        tmp=p[j];
//        p[j]+=d;
//
//        (*func)(p, hxx, m, n, adata);
//
//        p[j]=tmp; /* restore */
//
//        d=CNST(1.0)/d; /* invert so that divisions can be carried out faster as multiplications */
//        for(i=0; i<n; ++i){
//            jac[i*m+j]=(hxx[i]-hx[i])*d;
//        }
//    }
//}

//// From levmar project, used internally to create a jacobian.
//// Central finite difference approximation to the Jacobian of func
//inline
//void finiteDiffCentJacGuess(
//
//        // function to differentiate
//        void (*func)(double *p, double *hx, int m, int n, void *adata),
//
//        // I: current parameter estimate, mx1 */
//        double *p,
//
//        // W/O: work array for evaluating func(p-delta), nx1
//        double *hxm,
//
//        // W/O: work array for evaluating func(p+delta), nx1
//        double *hxp,
//
//        // increment for computing the Jacobian
//        double delta,
//
//        // O: array for storing approximated Jacobian, nxm
//        double *jac,
//
//        int m,
//        int n,
//        void *adata)
//{
//    register int i, j;
//    double tmp;
//    register double d;
//
//    for(j=0; j<m; ++j){
//        /* determine d=max(1E-04*|p[j]|, delta), see HZ */
//        d=CNST(1E-04)*p[j]; // force evaluation
//        d=FABS(d);
//        if(d<delta)
//            d=delta;
//
//        tmp=p[j];
//        p[j]-=d;
//        (*func)(p, hxm, m, n, adata);
//
//        p[j]=tmp+d;
//        (*func)(p, hxp, m, n, adata);
//        p[j]=tmp; /* restore */
//
//        d=CNST(0.5)/d; /* invert so that divisions can be carried out faster as multiplications */
//        for(i=0; i<n; ++i){
//            jac[i*m+j]=(hxp[i]-hxm[i])*d;
//        }
//    }
//}


//void solveJacobianFunc(double *p, struct splm_ccsm *jac, int m, int n, void *adata)
//{
//    SolverData *ud = static_cast<SolverData *>(adata);
//    double *hx = &ud->errorList[0];
//    double delta = ud->delta;
//    ud->isJacobianCalculation = true;
//    jacobianZeroPatternGuess(levmar_solveFunc, p, jac, m, n, adata, hx, delta);
//    ud->isJacobianCalculation = false;
//}


inline
bool solve(int iterMax,
           double tau,
           double eps1,
           double eps2,
           double eps3,
           double delta,
           int solverType,
           std::vector<std::shared_ptr<Camera> > cameraList,
           std::vector<std::shared_ptr<Marker> > markerList,
           std::vector<std::shared_ptr<Bundle> > bundleList,
           std::vector<std::shared_ptr<Attr> > attrList,
           MTimeArray frameList,
           MDGModifier &dgmod,
           MAnimCurveChange &curveChange,
           MComputation &computation,
           double &outError) {
    register int i=0;
    register int j=0;
    MStatus status;
    int ret = true;
    int profileCategory = MProfiler::getCategoryIndex("mmSolverCategory");
    MProfilingScope profilingScope(profileCategory,
                                   MProfiler::kColorC_L3,
                                   "mmSolverName",
                                   "mmSolverDesc");
    struct SolverData userData;

    // Indexing maps
    std::vector<std::pair<int, int> > paramToAttrList;
    std::vector<std::pair<int, int> > errorToMarkerList;

    // Errors and parameters as used by the solver.
    std::vector<double> errorList;
    std::vector<double> paramList;

    // Number of unknown parameters.
    int m = 0;

    // Number of measurement errors. (Must be less than or equal to number of unknown parameters).
    int n = 0;

    // Count up number of unknown parameters
    i = 0;
    j = 0;
    for (AttrPtrListIt ait = attrList.begin(); ait != attrList.end(); ++ait) {
        AttrPtr attr = *ait;
        if (attr->getDynamic()) {
            m += frameList.length();
            for (j = 0; j < (int) frameList.length(); ++j) {
                // first index is into 'attrList'
                // second index is into 'frameList'
                std::pair<int, int> attrPair(i, j);
                paramToAttrList.push_back(attrPair);
            }
        } else {
            ++m;
            // first index is into 'attrList'
            // second index is into 'frameList', '-1' means a static value.
            std::pair<int, int> attrPair(i, -1);
            paramToAttrList.push_back(attrPair);
        }
        i++;
    }

    // Count up number of errors
    // For each marker on each frame that it is valid, we add 3 errors.
    i = 0;
    j = 0;
    for (MarkerPtrListIt mit = markerList.begin(); mit != markerList.end(); ++mit) {
        MarkerPtr marker = *mit;
        for (j = 0; j < (int) frameList.length(); ++j) {
            MTime frame = frameList[j];
            bool valid;
            status = marker->getValid(valid, frame);
            CHECK_MSTATUS_AND_RETURN_IT(status);

            if (valid) {
                std::pair<int, int> markerPair(i, j);
                errorToMarkerList.push_back(markerPair);
                n += 3;
            }
        }
        i++;
    }

    INFO("params m=" << m);
    INFO("errors n=" << n);
    assert(m <= n);
    paramList.resize((unsigned long) m, 0);
    errorList.resize((unsigned long) n, 0);

    // Debug timers
    BENCHMARK_TYPE errorBench = BENCHMARK_TYPE();
    BENCHMARK_TYPE paramBench = BENCHMARK_TYPE();
    BENCHMARK_TYPE solveBench = BENCHMARK_TYPE();
    BENCHMARK_TYPE funcBench = BENCHMARK_TYPE();
    BENCHMARK_TYPE jacBench = BENCHMARK_TYPE();

    // Solving Objects.
    userData.cameraList = cameraList;
    userData.markerList = markerList;
    userData.bundleList = bundleList;
    userData.attrList = attrList;
    userData.frameList = frameList;

    // Indexing maps
    userData.paramToAttrList = paramToAttrList;
    userData.errorToMarkerList = errorToMarkerList;

    // Solver Aux data
    userData.errorList = errorList;
    userData.iterNum = 0;
    userData.jacIterNum = 0;
    userData.iterMax = iterMax;
    userData.isJacobianCalculation = false;

    // Solver Errors Thresholds
    userData.tau = tau;
    userData.eps1 = eps1;
    userData.eps2 = eps2;
    userData.eps3 = eps3;
    userData.delta = delta;
    userData.solverType = solverType;

    // Timers
    userData.funcBench = &funcBench;
    userData.jacBench = &jacBench;
    userData.errorBench = &errorBench;
    userData.paramBench = &paramBench;

    // Undo/Redo
    userData.dgmod = &dgmod;
    userData.curveChange = &curveChange;
    userData.computation = &computation;

    // Set Initial parameters
    INFO("Set Initial parameters");
    MTime currentFrame = MAnimControl::currentTime();
    i = 0;
    for (i = 0; i < m; ++i) {
        std::pair<int, int> attrPair = paramToAttrList[i];
        AttrPtr attr = attrList[attrPair.first];

        // Get frame time
        MTime frame = currentFrame;
        if (attrPair.second != -1) {
            frame = frameList[attrPair.second];
        }

        double value;
        status = attr->getValue(value, frame);
        CHECK_MSTATUS_AND_RETURN(status, false);

        paramList[i] = value;
    }

    // // Initial Parameters
    // INFO("Initial Parameters: ");
    // for (i = 0; i < m; ++i) {
    //     INFO("-> " << params[i]);
    // }
    // INFO("");

    // Options and Info
    unsigned int optsSize = LM_OPTS_SZ;
    unsigned int infoSize = LM_INFO_SZ;
    if (solverType == SOLVER_TYPE_SPARSE_LEVMAR) {
        optsSize = SPLM_OPTS_SZ;
        infoSize = SPLM_INFO_SZ;
    }
    double opts[optsSize];
    double info[infoSize];

    // Options
    opts[0] = tau;
    opts[1] = eps1;
    opts[2] = eps2;
    opts[3] = eps3;
    opts[4] = delta;

    // no Jacobian, caller allocates work memory, covariance estimated
    INFO("Solving...");
    INFO("Solver Type=" << solverType);
    INFO("Maximum Iterations=" << iterMax);
    INFO("Tau=" << tau);
    INFO("Epsilon1=" << eps1);
    INFO("Epsilon2=" << eps2);
    INFO("Epsilon3=" << eps3);
    INFO("Delta=" << delta);
    computation.setProgressRange(0, iterMax);
    computation.beginComputation();

    // Determine the solver type, levmar or sparse levmar.
    if (solverType != SOLVER_TYPE_LEVMAR) {
        if (solverType == SOLVER_TYPE_SPARSE_LEVMAR) {
#if HAVE_SPLM == 0
             WRN("Selected solver type \'SparseLM\' is not available, switching to \'Lev-Mar\' instead.");
             solverType = SOLVER_TYPE_LEVMAR;
#endif
        } else {
            WRN("Selected Solver Type \'" << solverType << "\' is unknown, switching to \'Lev-Mar\' instead.");
            solverType = SOLVER_TYPE_LEVMAR;
        }
    }

    solveBench.start();
    if (solverType == SOLVER_TYPE_LEVMAR) {

        // Allocate a memory block for both 'work' and 'covar', so that
        // the block is close together in physical memory.
        double *work, *covar;
        work = (double *) malloc((LM_DIF_WORKSZ(m, n) + m * m) * sizeof(double));
        if (!work) {
            ERR("Memory allocation request failed.");
            return false;
        }
        covar = work + LM_DIF_WORKSZ(m, n);

        ret = dlevmar_dif(

                // Function to call (input only)
                // Function must be of the structure:
                //   func(double *params, double *x, int m, int n, void *data)
                levmar_solveFunc,

                // Parameters (input and output)
                // Should be filled with initial estimate, will be filled
                // with output parameters
                &paramList[0],

                // Measurement Vector (input only)
                // NULL implies a zero vector
                // NULL,
                &errorList[0],

                // Parameter Vector Dimension (input only)
                // (i.e. #unknowns)
                m,

                // Measurement Vector Dimension (input only)
                n,

                // Maximum Number of Iterations (input only)
                iterMax,

                // Minimisation options (input only)
                // opts[0] = tau      (scale factor for initialTransform mu)
                // opts[1] = epsilon1 (stopping threshold for ||J^T e||_inf)
                // opts[2] = epsilon2 (stopping threshold for ||Dp||_2)
                // opts[3] = epsilon3 (stopping threshold for ||e||_2)
                // opts[4] = delta    (step used in difference approximation to the Jacobian)
                //
                // If \delta<0, the Jacobian is approximated with central differences
                // which are more accurate (but slower!) compared to the forward
                // differences employed by default.
                // Set to NULL for defaults to be used.
                opts,

                // Output Information (output only)
                // information regarding the minimization.
                // info[0] = ||e||_2 at initialTransform params.
                // info[1-4] = (all computed at estimated params)
                //  [
                //   ||e||_2,
                //   ||J^T e||_inf,
                //   ||Dp||_2,
                //   \mu/max[J^T J]_ii
                //  ]
                // info[5] = number of iterations,
                // info[6] = reason for terminating:
                //   1 - stopped by small gradient J^T e
                //   2 - stopped by small Dp
                //   3 - stopped by iterMax
                //   4 - singular matrix. Restart from current params with increased \mu
                //   5 - no further error reduction is possible. Restart with increased mu
                //   6 - stopped by small ||e||_2
                //   7 - stopped by invalid (i.e. NaN or Inf) "func" refPoints; a user error
                // info[7] = number of function evaluations
                // info[8] = number of Jacobian evaluations
                // info[9] = number linear systems solved (number of attempts for reducing error)
                //
                // Set to NULL if don't care
                info,

                // Working Data (input only)
                // working memory, allocated internally if NULL. If !=NULL, it is assumed to
                // point to a memory chunk at least LM_DIF_WORKSZ(m, n)*sizeof(double) bytes
                // long
                work,

                // Covariance matrix (output only)
                // Covariance matrix corresponding to LS solution; Assumed to point to a mxm matrix.
                // Set to NULL if not needed.
                covar,

                // Custom Data for 'func' (input only)
                // pointer to possibly needed additional data, passed uninterpreted to func.
                // Set to NULL if not needed
                (void *) &userData);

        free(work);
    }
    else if (solverType == SOLVER_TYPE_SPARSE_LEVMAR) {
#if (HAVE_SPLM == 1)

        // TODO: We could calculate an (approximate) non-zero value. We can do this by assuming that all dynamic attributes solve on on all single frames and are independant of static attributes.

        // Calculate number of non-zeros.
        int nonzeros = 0;  // Estimated non-zeros

        // nonzeros = (n * m) / 2;  // Estimated non-zeros
        // int Jnnz = 128; // TODO: How can we estimate this value better?
        // Jnnz = n * m;
        // struct splm_ccsm jac;
        // splm_ccsm_alloc_novalues(&jac, m, n, Jnnz); // no better estimate of Jnnz yet...
        //
        // // Do calculation
        // userData.isJacobianCalculation = true;
        // nonzeros = jacobianZeroPatternGuess(solveFunc, params, &jac, m, n, (void *) &userData,
        //                                 &errorList[0], delta);
        // userData.isJacobianCalculation = false;
        // INFO("non-zeros=" << nonzeros);
        // for (i=0; i<n; ++i) {
        //     INFO("hx[" << i << "]=" << errorList[i]);
        // }

        // Options
        opts[5] = SPLM_CHOLMOD;

        //
        // Similar to sparselm_dercrs() except that fjac supplies only the non-zero pattern of the Jacobian
        // and its non-zero elements are then appoximated internally with the aid of finite differences.
        // If the analytic Jacobian is available, it is advised to use sparselm_dercrs() above.
        //
        // Returns the number of iterations (>=0) if successful, SPLM_ERROR if failed
        //
        ret = sparselm_difccs(

                levmar_solveFunc,
                // functional relation describing measurements. Given a parameter vector p,
                // computes a prediction of the measurements \hat{x}. p is nvarsx1,
                // \hat{x} is nobsx1, maximum
                //
                // void (*func)(double *p, double *hx, int nvars, int nobs, void *adata),

                NULL,
                // function to initialize the preallocated jac structure with the Jacobian's
                // non-zero pattern. Non-zero elements will be computed with the aid of finite differencing.
                // if NULL, the non-zero pattern of the Jacobian is detected automatically via an
                // exhaustive search procedure that is time-consuming and thus not recommended.
                //
                // void (*fjac)(double *p, struct splm_ccsm *jac, int nvars, int nobs, void *adata),

                // I/O: initial parameter estimates. On output has the estimated solution. size nvars
                &paramList[0],

                // I: measurement vector. size nobs. NULL implies a zero vector
                NULL,

                // I: parameter vector dimension (i.e. #unknowns) [m]
                m,

                // I: number of parameters (starting from the 1st) whose values should not be modified. >=0
                0,

                // I: measurement vector dimension [n]
                n,

                // I: number of nonzeros for the Jacobian J
                nonzeros,

                // I: number of nonzeros for the product J^t*J, -1 if unknown
                -1,

                // I: maximum number of iterations
                iterMax,

                // I: minim. options [\mu, \epsilon1, \epsilon2, \epsilon3, delta, spsolver]. Respectively the scale factor
                // for initial \mu, stopping thresholds for ||J^T e||_inf, ||dp||_2 and ||e||_2, the step used in difference
                // approximation to the Jacobian and the sparse direct solver to employ. Set to NULL for defaults to be used.
                // If \delta<0, the Jacobian is approximated with central differences which are more accurate (but more
                // expensive to compute!) compared to the forward differences employed by default.
                //
                opts,

                // O: information regarding the minimization. Set to NULL if don't care
                // info[0]=||e||_2 at initial p.
                // info[1-4]=[ ||e||_2, ||J^T e||_inf,  ||dp||_2, mu/max[J^T J]_ii ], all computed at estimated p.
                // info[5]= # iterations,
                // info[6]=reason for terminating: 1 - stopped by small gradient J^T e
                //                                 2 - stopped by small dp
                //                                 3 - stopped by itmax
                //                                 4 - singular matrix. Restart from current p with increased mu
                //                                 5 - too many failed attempts to increase damping. Restart with increased mu
                //                                 6 - stopped by small ||e||_2
                //                                 7 - stopped by invalid (i.e. NaN or Inf) "func" values. User error
                // info[7]= # function evaluations
                // info[8]= # Jacobian evaluations
                // info[9]= # linear systems solved, i.e. # attempts for reducing error
                //
                info,

                // pointer to possibly additional data, passed uninterpreted to func, fjac
                (void *) &userData);
#endif
    }
    solveBench.stop();
    computation.endComputation();

    INFO("Results:");
    INFO("Solver returned " << ret << " in " << (int) info[5]
                            << " iterations");

    int reasonNum = (int) info[6];
    INFO("Reason: " << reasons[reasonNum]);
    INFO("Reason number: " << info[6]);
    INFO("");

    INFO("Solved Parameters:");
    for (i = 0; i < m; ++i) {
        INFO("-> " << paramList[i]);
    }
    INFO("");

    // Compute the average error based on the error values
    // the solve function last computed.
    // TODO: Create a list of frames and produce an error
    // per-frame. This information will eventually be given
    // to the user to diagnose problems.
    double avgError = 0;
    for (i = 0; i < n; ++i) {
        avgError += userData.errorList[i];
    }
    avgError /= (double) n;

    INFO(std::endl << std::endl << "Solve Information:");
    INFO("Initial Error: " << info[0]);
    INFO("Final Error: " << info[1]);
    INFO("Average Error: " << avgError);
    INFO("J^T Error: " << info[2]);
    INFO("Dp Error: " << info[3]);
    INFO("Max Error: " << info[4]);

    INFO("Iterations: " << info[5]);
    INFO("Termination Reason: " << reasons[reasonNum]);
    INFO("Function Evaluations: " << info[7]);
    INFO("Jacobian Evaluations: " << info[8]);
    INFO("Attempts for reducing error: " << info[9]);

    solveBench.print("Solve", 1);
    funcBench.print("Func", 1);
    jacBench.print("Jacobian", 1);
    paramBench.print("Param", (uint) userData.iterNum);
    errorBench.print("Error", (uint) userData.iterNum);
    funcBench.print("Func", (uint) userData.iterNum);

    // TODO: Compute the errors of all markers so we can add it to a vector
    // and return it to the user. This vector should be resized so we can
    // return frame-based information. The UI could then graph this information.
    outError = info[1];
    return ret != -1;
}


#endif // MAYA_MM_SOLVER_UTILS_H
