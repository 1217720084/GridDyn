/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil;  eval: (c-set-offset 'innamespace 0); -*- */
/*
* LLNS Copyright Start
* Copyright (c) 2016, Lawrence Livermore National Security
* This work was performed under the auspices of the U.S. Department
* of Energy by Lawrence Livermore National Laboratory in part under
* Contract W-7405-Eng-48 and in part under Contract DE-AC52-07NA27344.
* Produced at the Lawrence Livermore National Laboratory.
* All rights reserved.
* For details, see the LICENSE file.
* LLNS Copyright End
*/

#include "sundialsInterface.h"

#include "gridDyn.h"
#include "vectorOps.hpp"
#include "stringOps.h"
#include "core/helperTemplates.h"

#include <arkode/arkode.h>
#include <arkode/arkode_dense.h>
#include <sundials/sundials_math.h>


#ifdef KLU_ENABLE
#include <arkode/arkode_klu.h>
#include <arkode/arkode_sparse.h>
#endif

#include <cstdio>
#include <algorithm>
#include <string>
#include <map>
#include <cassert>


int arkodeFunc (realtype ttime, N_Vector state, N_Vector dstate_dt, void *user_data);
int arkodeJacDense (long int Neq, realtype ttime, N_Vector state, N_Vector dstate_dt, DlsMat J, void *user_data, N_Vector tmp1, N_Vector tmp2, N_Vector tmp3);
#ifdef KLU_ENABLE
int arkodeJacSparse (realtype ttime, N_Vector state, N_Vector dstate_dt, SlsMat J, void *user_data, N_Vector tmp1, N_Vector tmp2, N_Vector tmp3);
#endif
int arkodeRootFunc (realtype ttime, N_Vector state, realtype *gout, void *user_data);


arkodeInterface::arkodeInterface ()
{

}


arkodeInterface::arkodeInterface (gridDynSimulation *gds, const solverMode& sMode) : sundialsInterface (gds, sMode)
{
}

arkodeInterface::~arkodeInterface ()
{
  // clear variables for CVode to use
  if (initialized)
    {
      ARKodeFree (&solverMem);
    }
}

std::shared_ptr<solverInterface> arkodeInterface::clone(std::shared_ptr<solverInterface> si, bool fullCopy) const
{
	auto rp = cloneBaseStack<arkodeInterface, sundialsInterface, solverInterface>(this, si, fullCopy);
	if (!rp)
	{
		return si;
	}

	return rp;
}

int arkodeInterface::allocate (count_t stateCount, count_t numRoots)
{
  // load the vectors
  if (stateCount == svsize)
    {
      return 0;
    }
  initialized = false;
  
  a1.setRowLimit (stateCount);
  a1.setColLimit (stateCount);

  //update the rootCount
  rootCount = numRoots;
  rootsfound.resize (numRoots);

  //allocate the solverMemory
  if (solverMem)
    {
      ARKodeFree (&(solverMem));
    }
  solverMem = ARKodeCreate ();
  if (check_flag (solverMem, "ARKodeCVodeCreate", 0))
    {
      return(1);
    }



  sundialsInterface::allocate(stateCount, numRoots);

  return 0;
}


void arkodeInterface::setMaxNonZeros (count_t nonZeroCount)
{
	maxNNZ = nonZeroCount;
  a1.reserve (nonZeroCount);
  a1.clear ();
}


int arkodeInterface::set (const std::string &param, const std::string &val)
{
  int out = PARAMETER_FOUND;
  if (param == "mode")
    {
      auto v2 = splitlineTrim (convertToLowerCase (val));
      for (const auto &str : v2)
        {
          if (str == "bdf")
            {
              use_bdf = true;
            }
          else if (str == "adams")
            {
              use_bdf = false;
            }
          else if (str == "functional")
            {
              use_newton = false;
            }
          else if (str == "newton")
            {
              use_newton = true;
            }
          else
            {
              out = INVALID_PARAMETER_VALUE;
            }
        }
    }
  else
    {
      out = sundialsInterface::set (param, val);
    }
  return out;
}


int arkodeInterface::set (const std::string &param, double val)
{
  int out = PARAMETER_FOUND;
  if (param[0] == '#')
    {

    }
  else
    {
      out = sundialsInterface::set (param, val);
    }
  return out;
}

double arkodeInterface::get (const std::string &param) const
{
  long int val = -1;
  if ((param == "resevals") || (param == "iterationcount"))
    {
      //	CVodeGetNumResEvals(solverMem, &val);
    }
  else if (param == "iccount")
    {
      val = icCount;
    }
  else if (param == "jac calls")
    {
#ifdef KLU_ENABLE
      //	CVodeCVodeSlsGetNumJacEvals(solverMem, &val);
#else
      CVodeDlsGetNumJacEvals (solverMem, &val);
#endif
    }
  else
    {
	  return sundialsInterface::get(param);
    }

  return static_cast<double> (val);
}


// output solver stats
void arkodeInterface::logSolverStats (int logLevel, bool /*iconly*/) const
{
  if (!initialized)
    {
      return;
    }
  long int nni = 0;
  long int nst, nre, nfi, netf, ncfn, nge;
  realtype tolsfac, hlast, hcur;

  std::string logstr = "";


  int retval = ARKodeGetNumRhsEvals (solverMem, &nre,&nfi);
  check_flag (&retval, "ARKodeGetNumResEvals", 1);

  retval = ARKodeGetNumNonlinSolvIters (solverMem, &nni);
  check_flag (&retval, "ARKodeGetNumNonlinSolvIters", 1);
  retval = ARKodeGetNumNonlinSolvConvFails (solverMem, &ncfn);
  check_flag (&retval, "ARKodeGetNumNonlinSolvConvFails", 1);

  retval = ARKodeGetNumSteps (solverMem, &nst);
  check_flag (&retval, "ARKodeGetNumSteps", 1);
  retval = ARKodeGetNumErrTestFails (solverMem, &netf);
  check_flag (&retval, "ARKodeGetNumErrTestFails", 1);

  retval = ARKodeGetNumGEvals (solverMem, &nge);
  check_flag (&retval, "ARKodeGetNumGEvals", 1);
  ARKodeGetCurrentStep (solverMem, &hcur);

  ARKodeGetLastStep (solverMem, &hlast);
  ARKodeGetTolScaleFactor (solverMem, &tolsfac);

  logstr = "Arkode Run Statistics: \n";

  logstr += "Number of steps                    = " + std::to_string (nst) + '\n';
  logstr += "Number of rhs evaluations     = " + std::to_string (nre) + std::to_string (nfi) + '\n';
  logstr += "Number of Jacobian evaluations     = " + std::to_string (jacCallCount) + '\n';
  logstr += "Number of nonlinear iterations     = " + std::to_string (nni) + '\n';
  logstr += "Number of error test failures      = " + std::to_string (netf) + '\n';
  logstr += "Number of nonlinear conv. failures = " + std::to_string (ncfn) + '\n';
  logstr += "Number of root fn. evaluations     = " + std::to_string (nge) + '\n';

  logstr += "Current step                       = " + std::to_string (hcur) + '\n';
  logstr += "Last step                          = " + std::to_string (hlast) + '\n';
  logstr += "Tolerance scale factor             = " + std::to_string (tolsfac) + '\n';


  if (m_gds)
    {
      m_gds->log (m_gds, logLevel, logstr);
    }
  else
    {
      printf ("\n%s", logstr.c_str ());
    }
}


void arkodeInterface::logErrorWeights (int logLevel) const
{

  N_Vector eweight = NVECTOR_NEW(use_omp, svsize);
  N_Vector ele = NVECTOR_NEW(use_omp, svsize);

  realtype *eldata = NVECTOR_DATA (use_omp, ele);
  realtype *ewdata = NVECTOR_DATA (use_omp, eweight);
  int retval = ARKodeGetErrWeights (solverMem, eweight);
  check_flag (&retval, "ARKodeGetErrWeights", 1);
  retval = ARKodeGetEstLocalErrors (solverMem, ele);
  check_flag (&retval, "ARKodeGetEstLocalErrors ", 1);
  std::string logstr = "Error Weight\tEstimated Local Errors\n";
  for (size_t kk = 0; kk < svsize; ++kk)
    {
      logstr += std::to_string (kk) + ':' + std::to_string (ewdata[kk]) + '\t' + std::to_string (eldata[kk]) + '\n';
    }

  if (m_gds)
    {
      m_gds->log (m_gds, logLevel, logstr);
    }
  else
    {
      printf ("\n%s", logstr.c_str ());
    }
  NVECTOR_DESTROY(use_omp, eweight);
  NVECTOR_DESTROY(use_omp, ele);
}


static const std::map<int, std::string> arkodeRetCodes {
  {
    ARK_MEM_NULL, "The solver memory argument was NULL"
  },
  {
    ARK_ILL_INPUT, "One of the function inputs is illegal"
  },
  {
    ARK_NO_MALLOC, "The solver memory was not allocated by a call to CVodeMalloc"
  },
  {
    ARK_TOO_MUCH_WORK, "The solver took mxstep internal steps but could not reach tout"
  },
  {
    ARK_TOO_MUCH_ACC, "The solver could not satisfy the accuracy demanded by the user for some internal step"
  },
  {
    ARK_TOO_CLOSE, "t0 and tout are too close and user didn't specify a step size"
  },
  {
    ARK_LINIT_FAIL, "The linear solver's initialization function failed"
  },
  {
    ARK_LSETUP_FAIL, "The linear solver's setup function failed in an unrecoverable manner"
  },
  {
    ARK_LSOLVE_FAIL, "The linear solver's solve function failed in an unrecoverable manner"
  },
  {
    ARK_ERR_FAILURE, "The error test occured too many times"
  },
  {
    ARK_MEM_FAIL, "A memory allocation failed"
  },
  {
    ARK_CONV_FAILURE, "convergence test failed too many times"
  },
  {
    ARK_BAD_T, "The time t is outside the last step taken"
  },
  {
    ARK_FIRST_RHSFUNC_ERR, "The user - provided rhs function failed recoverably on the first call"
  },
  {
    ARK_REPTD_RHSFUNC_ERR, "convergence test failed with repeated recoverable erros in the rhs function"
  },

  {
    ARK_RTFUNC_FAIL, "The rootfinding function failed in an unrecoverable manner"
  },
  {
    ARK_UNREC_RHSFUNC_ERR, "The user-provided right hand side function repeatedly returned a recoverable error flag, but the solver was unable to recover"
  },
  {
    ARK_BAD_K, "Bad K"
  },
  {
    ARK_BAD_DKY, "Bad DKY"
  },
  {
    ARK_BAD_DKY, "Bad DKY"
  },
};

int arkodeInterface::initialize (double t0)
{
  if (!allocated)
    {
      printf ("ERROR,  arkode data not allocated\n");
      return -2;
    }
  auto jsize = m_gds->jacSize (mode);

  // initializeB CVode - Sundials

  int retval = ARKodeSetUserData (solverMem, (void *)this);
  if (check_flag (&retval, "ARKodeSetUserData", 1))
    {
      return(1);
    }

  //guess an initial condition
  m_gds->guess (t0, state_data(), deriv_data(), mode);

  retval = ARKodeInit (solverMem, arkodeFunc, arkodeFunc,t0, state);
  if (check_flag (&retval, "ARKodeInit", 1))
    {
      return(1);
    }

  if (rootCount > 0)
    {
      rootsfound.resize (rootCount);
      retval = ARKodeRootInit (solverMem, rootCount, arkodeRootFunc);
      if (check_flag (&retval, "ARKodeRootInit", 1))
        {
          return(1);
        }
    }

  N_VConst (tolerance, abstols);

  retval = ARKodeSVtolerances (solverMem, tolerance / 100, abstols);
  if (check_flag (&retval, "ARKodeSVtolerances", 1))
    {
      return(1);
    }

  retval = ARKodeSetMaxNumSteps (solverMem, 1500);
  if (check_flag (&retval, "ARKodeSetMaxNumSteps", 1))
    {
      return(1);
    }
#ifdef KLU_ENABLE
  if (dense)
    {
      retval = ARKDense (solverMem, svsize);
      if (check_flag (&retval, "ARKDense", 1))
        {
          return(1);
        }

      retval = ARKDlsSetDenseJacFn (solverMem, arkodeJacDense);
      if (check_flag (&retval, "ARKDlsSetDenseJacFn", 1))
        {
          return(1);
        }
    }
  else
    {

      retval = ARKKLU (solverMem, svsize, jsize);
      if (check_flag (&retval, "ARKodeKLU", 1))
        {
          return(1);
        }

      retval = ARKSlsSetSparseJacFn (solverMem, arkodeJacSparse);
      if (check_flag (&retval, "ARKSlsSetSparseJacFn", 1))
        {
          return(1);
        }
    }
#else
  retval = ARKDense (solverMem, svsize);
  if (check_flag (&retval, "ARKDense", 1))
    {
      return(1);
    }

  retval = ARKDlsSetDenseJacFn (solverMem, arkodeJacDense);
  if (check_flag (&retval, "ARKDlsSetDenseJacFn", 1))
    {
      return(1);
    }
#endif





  retval = ARKodeSetMaxNonlinIters (solverMem, 20);
  if (check_flag (&retval, "ARKodeSetMaxNonlinIters", 1))
    {
      return(1);
    }


  retval = ARKodeSetErrHandlerFn (solverMem, sundialsErrorHandlerFunc, (void *)this);
  if (check_flag (&retval, "ARKodeSetErrHandlerFn", 1))
    {
      return(1);
    }

  setConstraints ();

  initialized = true;
  return FUNCTION_EXECUTION_SUCCESS;

}

int arkodeInterface::sparseReInit (sparse_reinit_modes sparseReinitMode)
{
#ifdef KLU_ENABLE
  int kinmode = (sparseReinitMode == sparse_reinit_modes::refactor) ? 1 : 2;
  int retval = ARKKLUReInit (solverMem, static_cast<int> (svsize), static_cast<int> (a1.capacity ()), kinmode);
  if (check_flag (&retval, "ARKKLUReInit", 1))
    {
      return(FUNCTION_EXECUTION_FAILURE);
    }
#endif
  return FUNCTION_EXECUTION_SUCCESS;
}


int arkodeInterface::setRootFinding (count_t numRoots)
{
  if (numRoots != rootsfound.size ())
    {
      rootsfound.resize (numRoots);
    }
  rootCount = numRoots;
  int retval = ARKodeRootInit (solverMem, numRoots, arkodeRootFunc);
  if (check_flag (&retval, "ARKodeRootInit", 1))
    {
      return(retval);
    }
  return FUNCTION_EXECUTION_SUCCESS;
}

int arkodeInterface::getCurrentData ()
{
  /*
  int retval = CVodeGetConsistentIC(solverMem, state, deriv);
  if (check_flag(&retval, "CVodeGetConsistentIC", 1))
  {
  return(retval);
  }
  */
  return FUNCTION_EXECUTION_SUCCESS;
}

int arkodeInterface::solve (double tStop, double &tReturn, step_mode stepMode)
{
  assert (rootCount == m_gds->rootSize (mode));
  ++solverCallCount;
  icCount = 0;
  int retval = ARKode (solverMem, tStop, state, &tReturn, (stepMode == step_mode::normal) ? ARK_NORMAL : ARK_ONE_STEP);
  check_flag (&retval, "ARKodeSolve", 1, false);

  if (retval == ARK_ROOT_RETURN)
    {
      retval = SOLVER_ROOT_FOUND;
    }
  return retval;
}

int arkodeInterface::getRoots ()
{
  int ret = ARKodeGetRootInfo (solverMem, rootsfound.data ());
  if (!check_flag (&ret, "ARKodeGetRootInfo", 1))
    {
      return ret;
    }
  return ret;
}


void arkodeInterface::loadMaskElements ()
{
  std::vector<double> mStates (svsize, 0.0);
  m_gds->getVoltageStates (mStates.data (), mode);
  m_gds->getAngleStates (mStates.data (), mode);
  maskElements = vecFindgt<double, index_t> (mStates, 0.5);
  tempState.resize (svsize);
  double *lstate = NV_DATA_S (state);
  for (auto &v : maskElements)
    {
      tempState[v] = lstate[v];
    }
}


//#define CAPTURE_STATE_FILE

#ifdef CAPTURE_STATE_FILE
void saveStateFile (double time, count_t size, double *state, double *dstate, double *resid, std::string fname, bool append)
{
  std::ofstream  bFile;

  if (append)
    {
      bFile.open (fname.c_str (), std::ios::out | std::ios::binary | std::ios::app);
    }
  else
    {
      bFile.open (fname.c_str (), std::ios::out | std::ios::binary);
    }
  bFile.write ((char *)(&time), sizeof(double));
  bFile.write ((char *)(&size), sizeof(count_t));

  bFile.write ((char *)(state), sizeof(double) * size);
  bFile.write ((char *)(dstate), sizeof(double) * size);
  bFile.write ((char *)(resid), sizeof(double) * size);

  bFile.close ();
}
#endif

// CVode C Functions
int arkodeFunc (realtype ttime, N_Vector state, N_Vector dstate_dt, void *user_data)
{
  arkodeInterface *sd = reinterpret_cast<arkodeInterface *> (user_data);
  //printf("time=%f\n", ttime);
  int ret = sd->m_gds->derivativeFunction (ttime, NVECTOR_DATA(sd->use_omp, state), NVECTOR_DATA(sd->use_omp, dstate_dt), sd->mode);

#ifdef CAPTURE_STATE_FILE
  saveStateFile (ttime, sd->svsize, NVECTOR_DATA (state), NVECTOR_DATA (dstate_dt), NVECTOR_DATA (resid), "jac_new_state.dat", true);
#endif
  return ret;
}

int arkodeRootFunc (realtype ttime, N_Vector state, realtype *gout, void *user_data)
{

	arkodeInterface *sd = reinterpret_cast<arkodeInterface *> (user_data);
	sd->m_gds->rootFindingFunction(ttime, NVECTOR_DATA(sd->use_omp, state), sd->deriv_data(), gout, sd->mode);

	return FUNCTION_EXECUTION_SUCCESS;

}

#define CHECK_JACOBIAN 1
int arkodeJacDense (long int Neq, realtype ttime, N_Vector state, N_Vector dstate_dt, DlsMat J, void *user_data, N_Vector /*tmp1*/, N_Vector /*tmp2*/, N_Vector /*tmp3*/)
{
  arkodeInterface *sd = reinterpret_cast<arkodeInterface *> (user_data);

  assert(Neq == static_cast<int> (sd->svsize));
  _unused(Neq);

  arrayDataSparse *a1 = &(sd->a1);
  sd->m_gds->jacobianFunction (ttime, NVECTOR_DATA(sd->use_omp, state), NVECTOR_DATA(sd->use_omp, dstate_dt), a1,0, sd->mode);


  if (sd->useMask)
    {
      for (auto &v : sd->maskElements)
        {
          a1->translateRow (v, kNullLocation);
          a1->assign (v, v, 100);
        }
      a1->filter ();
    }

  //assign the elements
  for (index_t kk = 0; kk < a1->size (); ++kk)
    {
      DENSE_ELEM (J, a1->rowIndex (kk), a1->colIndex (kk)) = DENSE_ELEM (J, a1->rowIndex (kk), a1->colIndex (kk)) + a1->val (kk);
    }

#if (CHECK_JACOBIAN > 0)
  auto mv = findMissing (a1);
  for (auto &me : mv)
    {
      printf ("no entries for element %d\n", me);
    }
#endif
  return FUNCTION_EXECUTION_SUCCESS;
}

//#define CAPTURE_JAC_FILE

#ifdef KLU_ENABLE
int arkodeJacSparse (realtype ttime, N_Vector state, N_Vector dstate_dt, SlsMat J, void *user_data, N_Vector, N_Vector, N_Vector)
{

  arkodeInterface *sd = reinterpret_cast<arkodeInterface *> (user_data);

  arrayDataSparse *a1 = &(sd->a1);

  sd->m_gds->jacobianFunction (ttime, NVECTOR_DATA(sd->use_omp, state), NVECTOR_DATA(sd->use_omp, dstate_dt), a1,0, sd->mode);
  a1->sortIndexCol ();
  if (sd->useMask)
    {
      for (auto &v : sd->maskElements)
        {
          a1->translateRow (v, kNullLocation);
          a1->assign (v, v, 1);
        }
      a1->filter ();
      a1->sortIndexCol ();
    }
  a1->compact ();

  SlsSetToZero (J);

  count_t colval = 0;
  J->colptrs[0] = colval;
  for (index_t kk = 0; kk < a1->size (); ++kk)
    {
      //	  printf("kk: %d  dataval: %f  rowind: %d   colind: %d \n ", kk, a1->val(kk), a1->rowIndex(kk), a1->colIndex(kk));
      if (a1->colIndex (kk) > colval)
        {
          colval++;
          J->colptrs[colval] = static_cast<int> (kk);
        }
      J->data[kk] = a1->val (kk);
      J->rowvals[kk] = a1->rowIndex (kk);
    }
  J->colptrs[colval + 1] = static_cast<int> (a1->size ());

#ifdef CAPTURE_JAC_FILE
  a1->saveFile (ttime, "jac_new.dat", true);
#endif
#if (CHECK_JACOBIAN > 0)
  auto mv = findMissing (a1);
  for (auto &me : mv)
    {
      printf ("no entries for element %d\n", me);
    }
#endif
  return 0;
}
#endif


