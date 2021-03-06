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

#include "submodels/gridControlBlocks.h"
#include "vectorOps.hpp"
#include "arrayData.h"
#include "gridCoreTemplates.h"

integralBlock::integralBlock (const std::string &objName) : basicBlock (objName)
{
  opFlags.set (differential_output);
  opFlags.set (use_state);
}

integralBlock::integralBlock (double gain, const std::string &objName) : basicBlock (gain,objName)
{
  opFlags.set (differential_output);
  opFlags.set (use_state);
}

gridCoreObject *integralBlock::clone (gridCoreObject *obj) const
{
  integralBlock *nobj = cloneBase<integralBlock, basicBlock> (this, obj);
  if (nobj == nullptr)
    {
      return obj;
    }
  nobj->iv = iv;
  return nobj;
}


// initial conditions
void integralBlock::objectInitializeB (const IOdata &args, const IOdata &outputSet, IOdata &fieldSet)
{
  index_t loc = limiter_diff;
  if (outputSet.empty ())
    {
      m_state[loc] = iv;
      if (limiter_diff > 0)
        {
          basicBlock::objectInitializeB (args, outputSet,fieldSet);
        }
      m_dstate_dt[loc] = K * (args[0] + bias);
    }
  else
    {
      basicBlock::objectInitializeB (args, outputSet, fieldSet);
    }

}


// residual
void integralBlock::residElements (double input, double didt, const stateData *sD, double resid[], const solverMode &sMode)
{
  if (isAlgebraicOnly (sMode))
    {
      basicBlock::residElements (input,didt, sD, resid, sMode);
      return;
    }
  auto offset = offsets.getDiffOffset (sMode);
  resid[offset] = (K * (input + bias) - sD->dstate_dt[offset]);
  basicBlock::residElements (input, didt, sD, resid, sMode);

}

void integralBlock::derivElements (double input, double didt, const stateData *sD, double deriv[], const solverMode &sMode)
{
  auto offset = offsets.getDiffOffset (sMode);
  deriv[offset + limiter_diff ] = K * (input + bias);
  if (opFlags[use_ramp_limits])
    {
      basicBlock::derivElements (input, didt, sD, deriv, sMode);
    }
}


void integralBlock::jacElements (double input, double didt, const stateData *sD, arrayData<double> *ad, index_t argLoc, const solverMode &sMode)
{
  if (isAlgebraicOnly (sMode))
    {
      basicBlock::jacElements (input, didt, sD, ad, argLoc, sMode);
    }
  auto offset = offsets.getDiffOffset (sMode);
  //use the ad->assign Macro defined in basicDefs
  // ad->assign(arrayIndex, RowIndex, ColIndex, value)
  ad->assignCheck (offset, argLoc, K);
  ad->assign (offset, offset, -sD->cj);
  basicBlock::jacElements (input,didt, sD,ad,argLoc,sMode);
}

double integralBlock::step (double ttime, double inputA)
{

  double dt = ttime - prevTime;
  double out;
  double input = inputA + bias;
  index_t loc = limiter_diff + limiter_alg;
  m_state[loc] = m_state[loc] + K * (input + prevInput) / 2.0 * dt;
  prevInput = input;
  if (loc > 0)
    {
      out = basicBlock::step (ttime, inputA);
    }
  else
    {
      out = m_state[0];
      prevTime = ttime;
      m_output = out;
    }
  return out;
}

// set parameters
int integralBlock::set (const std::string &param,  const std::string &val)
{
  return basicBlock::set (param, val);
}

int integralBlock::set (const std::string &param, double val, gridUnits::units_t unitType)
{
  int out = PARAMETER_FOUND;
  if ((param == "iv") || (param == "initial_value"))
    {
      iv = val;
    }
  else if (param == "t")
    {
      if (val != 0)
        {
          K = 1 / val;
        }
    }
  else
    {
      out = basicBlock::set (param, val, unitType);
    }
  return out;
}

