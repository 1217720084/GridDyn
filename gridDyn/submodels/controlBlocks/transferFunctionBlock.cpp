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

#include "submodels/otherBlocks.h"
#include "vectorOps.hpp"
#include "arrayData.h"
#include "stringOps.h"

transferFunctionBlock::transferFunctionBlock (int order) : a (order + 1,1),b (order + 1,0)
{
  b[0] = 1;
  opFlags.set (use_state);
}

transferFunctionBlock::transferFunctionBlock (std::vector<double> Acoef) : a (Acoef),b (Acoef.size (),0)
{
  b[0] = 1;
  opFlags.set (use_state);
}

transferFunctionBlock::transferFunctionBlock (std::vector<double> Acoef, std::vector<double> Bcoef) : a (Acoef),b (Bcoef)
{
  b.resize (Acoef.size (),0);
  opFlags.set (use_state);
}

gridCoreObject *transferFunctionBlock::clone (gridCoreObject *obj) const
{
  transferFunctionBlock *nobj;
  if (obj == nullptr)
    {
      nobj = new transferFunctionBlock ();
    }
  else
    {
      nobj = dynamic_cast<transferFunctionBlock *> (obj);
      if (nobj == nullptr)
        {
          basicBlock::clone (obj);
          return obj;
        }
    }
  basicBlock::clone (nobj);

  nobj->a = a;
  nobj->b = b;
  return nobj;
}
//set up the number of states
void transferFunctionBlock::objectInitializeA (double time0, unsigned long flags)
{
  if (b.back () == 0)
    {
      opFlags[differential_output] = true;
      extraOutputState = false;
    }
  else
    {
      extraOutputState = true;
    }
  basicBlock::objectInitializeA (time0, flags);
  offsets.local->local.jacSize += static_cast<count_t> (3 * (a.size () - 2) + 1);
  offsets.local->local.diffSize += static_cast<count_t> (a.size ()) - 2;
  if (extraOutputState)
    {
      offsets.local->local.diffSize += 1;
      offsets.local->local.jacSize += 3;
    }

}
// initial conditions
void transferFunctionBlock::objectInitializeB (const IOdata &args, const IOdata &outputSet, IOdata &fieldSet)
{
  if (outputSet.empty ())
    {
      //	m_state[2] = (1.0 - m_T2 / m_T1) * (args[0] + bias);
      m_state[1] = (args[0] + bias);
      m_state[0] = m_state[1] * K;
      if (opFlags[has_limits])
        {
          basicBlock::rootCheck (args, nullptr, cLocalSolverMode, check_level_t::reversable_only);
          m_state[0] = valLimit (m_state[0], Omin, Omax);
        }
      fieldSet[0] = m_state[0];
      prevInput = args[0] + bias;
    }
  else
    {
      m_state[0] = outputSet[0];
      //	m_state[1] = (1.0 - m_T2 / m_T1) * outputSet[0] / K;
      fieldSet[0] = outputSet[0] - bias;
      prevInput = outputSet[0] / K;
    }
}


// residual
void transferFunctionBlock::residElements (double input, double didt, const stateData *sD, double resid[], const solverMode &sMode)
{
  Lp Loc = offsets.getLocations (sD, resid, sMode, this);
  if (extraOutputState)
    {

    }
  else
    {
      for (size_t kk = 0; kk < a.size () - 1; ++kk)
        {
          Loc.destLoc[limiter_alg + kk] = -a[kk] * Loc.diffStateLoc[kk] + Loc.diffStateLoc[kk + 1] + b[kk];
        }
    }

  //Loc.destLoc[limiter_alg] = Loc.diffStateLoc[limiter_diff] + m_T2 / m_T1 * (input + bias) - Loc.algStateLoc[limiter_alg];
  basicBlock::residElements (input, didt, sD, resid, sMode);

}

void transferFunctionBlock::derivElements (double input, double didt, const stateData *sD, double deriv[], const solverMode &sMode)
{
//  auto offset = offsets.getDiffOffset (sMode);
// auto Aoffset = offsets.getAlgOffset (sMode);
//deriv[offset + limiter_diff] = K*(input + bias - sD->state[Aoffset + limiter_alg]) / m_T1;
  if (opFlags[use_ramp_limits])
    {
      basicBlock::derivElements (input, didt, sD, deriv, sMode);
    }
}


void transferFunctionBlock::jacElements (double input, double didt, const stateData *sD, arrayData<double> *ad, index_t argLoc, const solverMode &sMode)
{
  Lp Loc = offsets.getLocations  (sD, sMode, this);
  ad->assign (Loc.algOffset + 1, Loc.algOffset + 1, -1);

  //ad->assignCheck(Loc.algOffset + 1, argLoc, m_T2 / m_T1);

  basicBlock::jacElements (input, didt, sD, ad, argLoc, sMode);
  if (isAlgebraicOnly (sMode))
    {
      return;
    }
  ad->assign (Loc.algOffset + 1, Loc.diffOffset, 1);
  //use the ad->assign Macro defined in basicDefs
  // ad->assign(arrayIndex, RowIndex, ColIndex, value)

  //ad->assignCheck(Loc.diffOffset, argLoc, 1 / m_T1);
//	ad->assign(Loc.diffOffset, Loc.algOffset + 1, -1 / m_T1);
  ad->assign (Loc.diffOffset, Loc.diffOffset, -sD->cj);
}

double transferFunctionBlock::step (double ttime, double inputA)
{

  double dt = ttime - prevTime;
  double out;
  double input = inputA + bias;
  // double ival, ival2;
  if (dt >= fabs (5.0 ))
    {
      m_state[2] = input;
    }
  else if (dt <= fabs (0.05 ))
    {
      //m_state[2] = m_state[2] + 1.0 / m_T1 * ((input + prevInput) / 2.0 - m_state[1]) * dt;
    }
  else
    {
      double tstep = 0.05;
      double ct = prevTime + tstep;
      double in = prevInput;
      //double pin = prevInput;
      //   ival = m_state[2];
      //    ival2 = m_state[1];
      while (ct < ttime)
        {
          in = in + (input - prevInput) / dt * tstep;
          //ival = ival + 1.0 / m_T1 * ((pin + in) / 2.0 - ival2) * tstep;
          //	ival2 = ival + m_T2 / m_T1 * (input);
          ct += tstep;
          //  pin = in;
        }
      //m_state[2] = ival + 1.0 / m_T1 * ((pin + input) / 2.0 - ival2) * (ttime - ct + tstep);
    }
  //m_state[1] = m_state[2] + m_T2 / m_T1 * (input);

  prevInput = input;
  if (opFlags[has_limits])
    {
      out = basicBlock::step (ttime, input);
    }
  else
    {
      out = K * m_state[1];
      m_state[0] = out;
      prevTime = ttime;
      m_output = out;
    }
  return out;
}

index_t transferFunctionBlock::findIndex (const std::string &field, const solverMode &sMode) const
{
  index_t ret = kInvalidLocation;
  if (field == "m1")
    {
      ret = offsets.getDiffOffset (sMode);
    }
  else
    {
      ret = basicBlock::findIndex (field, sMode);
    }
  return ret;
}

// set parameters
int transferFunctionBlock::set (const std::string &param, const std::string &val)
{
  int out = PARAMETER_FOUND;
  if (param == "a")
    {
      a = str2vector (val,0);
    }
  else if (param == "b")
    {
      b = str2vector (val, 0);
    }
  else
    {
      out = basicBlock::set (param, val);
    }
  return out;
}

int transferFunctionBlock::set (const std::string &param, double val, gridUnits::units_t unitType)
{
  int out = PARAMETER_FOUND;

  //param   = gridDynSimulation::toLower(param);
  std::string pstr;
  int num = trailingStringInt (param, pstr, -1);
  if (pstr.length () == 1)
    {
      switch (pstr[0])
        {
        case '#':
          break;
        case 'a':
        case 't':
          if (num >= 0)
            {
              if (num > static_cast<int> (a.size ()))
                {
                  a.resize (num + 1, 0);
                  b.resize (num + 1, 0);
                }
              a[num] = val;
            }
          else
            {
              out = PARAMETER_NOT_FOUND;
            }
          break;
        case 'b':
          if (num >= 0)
            {
              if (num > static_cast<int> (a.size ()))
                {
                  a.resize (num + 1, 0);
                  b.resize (num + 1, 0);
                }
              b[num] = val;
            }
          else
            {
              out = PARAMETER_NOT_FOUND;
            }
          break;
        case 'k':
          K = val;
          break;
        default:
          out = PARAMETER_NOT_FOUND;
        }
      return out;
    }

  if (param == "#")
    {
      //m_T1 = val;
    }
  else
    {
      out = basicBlock::set (param, val, unitType);
    }

  return out;
}

static stringVec stNames {
  "output", "Intermediate1","intermediate2"
};

stringVec transferFunctionBlock::localStateNames () const
{
  return stNames;
}

