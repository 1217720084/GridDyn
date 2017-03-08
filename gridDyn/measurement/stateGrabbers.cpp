/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil;  eval: (c-set-offset 'innamespace 0); -*- */
/*
   * LLNS Copyright Start
 * Copyright (c) 2017, Lawrence Livermore National Security
 * This work was performed under the auspices of the U.S. Department
 * of Energy by Lawrence Livermore National Laboratory in part under
 * Contract W-7405-Eng-48 and in part under Contract DE-AC52-07NA27344.
 * Produced at the Lawrence Livermore National Laboratory.
 * All rights reserved.
 * For details, see the LICENSE file.
 * LLNS Copyright End
*/

#include "stateGrabber.h"
#include "gridBus.h"
#include "linkModels/gridLink.h"
#include "relays/gridRelay.h"
#include "relays/sensor.h"
#include "gridArea.h"
#include "generators/gridDynGenerator.h"
#include "grabberInterpreter.hpp"
#include "gridCondition.h"
#include "utilities/matrixDataScale.h"
#include "utilities/matrixDataTranslate.h"
#include "core/helperTemplates.h"
#include <algorithm>
#include "utilities/mapOps.h"

static grabberInterpreter<stateGrabber, stateOpGrabber, stateFunctionGrabber> sgInterpret ([](const std::string &fld, coreObject *obj){
  return std::make_unique<stateGrabber> (fld, obj);
});

static const std::string specialChars(":(+-/*\\^?");
static const std::string sepChars(",;");

std::vector < std::unique_ptr < stateGrabber >> makeStateGrabbers (const std::string & command, coreObject * obj)
{
  std::vector < std::unique_ptr < stateGrabber >> v;
  auto gsplit = stringOps::splitlineBracket(command,sepChars);
  stringOps::trim(gsplit);
  for (auto &cmd:gsplit)
    {
      if (cmd.find_first_of (specialChars) != std::string::npos)
        {
          auto sgb = sgInterpret.interpretGrabberBlock (cmd, obj);
          if ((sgb)&&(sgb->loaded))
            {
              v.push_back (std::move(sgb));
            }
        }
      else
        {
          auto sgb = std::make_unique<stateGrabber> (cmd, dynamic_cast<gridObject *>(obj));
          if ((sgb) && (sgb->loaded))
            {
              v.push_back (std::move(sgb));
            }
        }
    }
  return v;

}

stateGrabber::stateGrabber()
{}

stateGrabber::stateGrabber( coreObject* obj):cobj(dynamic_cast<gridObject *>(obj))
{

}

stateGrabber::stateGrabber (const std::string &fld, coreObject* obj):stateGrabber(obj)
{
	stateGrabber::updateField(fld);
}

stateGrabber::stateGrabber(index_t noffset, coreObject *obj) : offset(noffset), cobj(dynamic_cast<gridObject *>(obj))
{
	
}

std::shared_ptr<stateGrabber> stateGrabber::clone (std::shared_ptr<stateGrabber> ggb) const
{
  if (ggb == nullptr)
    {
      ggb = std::make_shared<stateGrabber> ();
    }
  ggb->field = field;
  ggb->fptr = fptr;
  ggb->jacIfptr = jacIfptr;
  ggb->gain = gain;
  ggb->bias = bias;
  ggb->inputUnits = inputUnits;
  ggb->outputUnits = outputUnits;
  ggb->loaded = loaded;
  ggb->cacheUpdateRequired = cacheUpdateRequired;
  ggb->offset = offset;
  ggb->jacMode = jacMode;
  ggb->prevIndex = prevIndex;
  ggb->cobj = cobj;
  
  return ggb;
}

void stateGrabber::updateField (const std::string &fld)
{
  field = fld;
  auto fd=convertToLowerCase (fld);
  loaded = true;
  if (dynamic_cast<gridBus *> (cobj))
    {
      busLoadInfo (fd);
    }
  else if (dynamic_cast<gridLink *> (cobj))
    {
      linkLoadInfo (fd);
    }
  else if (dynamic_cast<gridSecondary *> (cobj))
    {
      secondaryLoadInfo (fd);
    }
  else if (dynamic_cast<gridRelay *> (cobj))
    {
      relayLoadInfo (fd);
    }
  else
    {
      loaded = false;
    }
  

}

using namespace gridUnits;

/** map of all the alternate strings that can be used*/
/* *INDENT-OFF* */
static const std::map<std::string, std::string> stringTranslate
{
	{ "voltage","v" },
	{ "vol","v" },
	{ "link","linkreal" },
	{ "linkp","linkreal" },
	{ "loadq","loadreactive" },
	{ "loadreactivepower","loadreactive" },
	{ "load","loadreal" },
	{ "loadp","loadreal" },
	{ "reactivegen","genreactive" },
	{ "genq","genreactive" },
	{ "gen","genreal" },
	{ "generation","genreal" },
	{ "genp","genreal" },
	{ "realgen","genreal" },
	{ "f","freq" },
	{ "frequency","freq" },
	{ "omega","freq" },
	{ "a","angle" },
	{"ang","angle"},
	{ "phase","angle" },
	{ "busgenerationreal","busgenreal" },
	{ "busp","busgenreal" },
	{ "busgen","busgenreal" },
	{ "busgenerationreactive","busgenreactive" },
	{ "busq","busgenreactive" },
	{ "linkrealpower","linkreal" },
	{ "linkp1","linkreal" },
	{ "linkq","linkreactive" },
	{ "linkreactivepower","linkreactive" },
	{ "linkrealpower1","linkreal" },
	{ "linkq1","linkreactive" },
	{ "linkreactivepower1","linkreactive" },
	{ "linkreal1","linkreal" },
	{ "linkreactive1","linkreactive" },
	{ "linkrealpower2","linkreal2" },
	{ "linkq2","linkreactive2" },
	{ "linkreactivepower2","linkreactive2" },
	{ "linkp2","linkreal2" },
	{ "p","real" },
	{ "q","reactive" },
	{ "impedance","z" },
	{ "admittance","y" },
	{ "impedance1","z" },
	{ "admittance1","y" },
	{ "z1","z" },
	{ "y1","y" },
	{ "impedance2","z2" },
	{ "admittance2","y2" },
	{ "status","connected" },
	{ "breaker","switch" },
	{ "breaker1","switch" },
	{ "switch1","switch" },
	{ "breaker2","switch2" },
	{ "i","current" },
	{ "i1","current" },
	{ "current1","current" },
	{ "i2","current2" },
	{ "imagcurrent1","imagcurrent" },
	{ "realcurrent1","realcurrent" },
	{ "lossreal","loss" },
	{ "angle1","angle" },
	{ "absangle1","absangle" },
	{ "voltage1","voltage" },
	{ "v1","voltage" },
	{ "v2","voltage2" },
	{ "output0","output" },
	{ "cv","output" },
	{ "o0","output" },
	{ "currentvalue","output" },
	{ "deriv0","deriv" },
	{ "dodt","deriv" },{ "dodt0","deriv" },
	{ "doutdt","deriv" },{ "doutdt0","deriv" }
};

#define FUNCTION_SIGNATURE [](gridObject *obj, const stateData &sD, const solverMode &sMode)
#define FUNCTION_SIGNATURE_OBJ_ONLY [](gridObject *obj, const stateData &, const solverMode &)

#define JAC_FUNCTION_SIGNATURE [](gridObject *obj, const stateData &sD, matrixData<double> &ad, const solverMode &sMode)
#define JAC_FUNCTION_SIGNATURE_NO_STATE [](gridObject *obj, const stateData &, matrixData<double> &ad, const solverMode &sMode)

static const std::map<std::string, fstateobjectPair> objectFunctions
{
	{ "connected",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<double>(obj->isConnected()); },defUnit } },
	{ "enabled",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<double>(obj->isEnabled()); },defUnit } },
	{ "armed",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<double>(obj->isArmed()); },defUnit } },
	{ "output",{ FUNCTION_SIGNATURE{return obj->getOutput(noInputs,sD,sMode,0); },defUnit } },
	{ "deriv",{ FUNCTION_SIGNATURE{return obj->getDoutdt(noInputs,sD,sMode,0); },defUnit } }
};

static const std::map<std::string, fstateobjectPair> busFunctions
{
	{ "v",{ FUNCTION_SIGNATURE{return static_cast<gridBus *> (obj)->getVoltage(sD, sMode); }, puV } },
	{ "angle",{ FUNCTION_SIGNATURE{return static_cast<gridBus *>(obj)->getAngle(sD, sMode); }, rad } },
	{ "freq",{ FUNCTION_SIGNATURE{return static_cast<gridBus *>(obj)->getFreq(sD, sMode); },puHz } },
	{ "genreal",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridBus *>(obj)->getGenerationReal(); }, puMW } },
	{ "genreactive",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridBus *>(obj)->getGenerationReactive(); }, puMW } },
	{ "loadreal",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridBus *>(obj)->getLoadReal(); }, puMW } },
	{ "loadreactive",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridBus *>(obj)->getLoadReactive(); }, puMW } },
	{ "linkreal",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridBus *>(obj)->getLinkReal(); }, puMW } },
	{ "linkreactive",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridBus *>(obj)->getLinkReactive(); }, puMW } },
};

static const std::map<std::string, objJacFunction> busJacFunctions
{
	{ "v",JAC_FUNCTION_SIGNATURE_NO_STATE{ ad.assignCheckCol(0, static_cast<gridBus *> (obj)->getOutputLoc(sMode,voltageInLocation), 1.0); }  },
	{ "angle", JAC_FUNCTION_SIGNATURE_NO_STATE{ ad.assignCheckCol(0, static_cast<gridBus *> (obj)->getOutputLoc(sMode,angleInLocation), 1.0); } },
};

static const std::map<std::string, fstateobjectPair> areaFunctions
{
	{ "avgfreq",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridArea *>(obj)->getAvgFreq(); },puHz } },
	{ "genreal",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridArea *>(obj)->getGenerationReal(); }, puMW } },
	{ "genreactive",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridArea *>(obj)->getGenerationReactive(); }, puMW } },
	{ "loadreal",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridArea *>(obj)->getLoadReal(); }, puMW } },
	{ "loadreactive",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridArea *>(obj)->getLoadReactive(); }, puMW } },
	{ "loss",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridArea *>(obj)->getLoss(); }, puMW } },
	{ "tieflow",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridArea *>(obj)->getTieFlowReal(); }, puMW } },
};


static const  objStateGrabberFunction rpower = FUNCTION_SIGNATURE{ return static_cast<gridSecondary *> (obj)->getRealPower(noInputs, sD, sMode); };


static const  objStateGrabberFunction qpower = FUNCTION_SIGNATURE{ return static_cast<gridSecondary *> (obj)->getReactivePower(noInputs, sD, sMode); };

static const std::map<std::string, fstateobjectPair> loadFunctions
{
	{ "real",{rpower, puMW } },
	{ "reactive",{qpower, puMW } },
	{ "loadreal",{ rpower,puMW } },
	{ "loadreactive",{qpower, puMW } }
};

static const std::map<std::string, fstateobjectPair> genFunctions
{
	{ "real",{ rpower, puMW } },
	{ "reactive",{ qpower, puMW } },
	{ "genreal",{ rpower,puMW } },
	{ "genreactive",{qpower, puMW } },
	{ "pset",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridDynGenerator *>(obj)->getPset(); },puMW } },
	{ "adjup",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridDynGenerator *>(obj)->getAdjustableCapacityUp(); },puMW } },
	{ "adjdown",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridDynGenerator *>(obj)->getAdjustableCapacityDown(); },puMW } },
	{ "pmax",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridDynGenerator *>(obj)->getPmax(); },puMW } },
	{ "pmin",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridDynGenerator *>(obj)->getPmin(); },puMW } },
	{ "qmax",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridDynGenerator *>(obj)->getQmax(); },puMW } },
	{ "qmin",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridDynGenerator *>(obj)->getQmin(); },puMW } },
	{ "freq",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridDynGenerator *>(obj)->getFreq(emptyStateData,cLocalSolverMode); },puHz } },
	{ "angle",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridDynGenerator *>(obj)->getAngle(emptyStateData,cLocalSolverMode); },rad } },
};

static const std::map<std::string, fstateobjectPair> linkFunctions
{
	{ "real",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridLink *>(obj)->getRealPower(1); }, puMW } },
	{ "reactive",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridLink *>(obj)->getReactivePower(1); }, puMW } },
	{ "linkreal",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridLink *>(obj)->getRealPower(1); },puMW } },
	{ "linkreactive",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridLink *>(obj)->getReactivePower(1); }, puMW } },
	{ "z",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridLink *>(obj)->getTotalImpedance(1); },puOhm } },
	{ "y",{ FUNCTION_SIGNATURE_OBJ_ONLY{return 1.0 / (static_cast<gridLink *>(obj)->getTotalImpedance(1)); }, puOhm } },
	{ "r",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridLink *>(obj)->getRealImpedance(1); },puOhm } },
	{ "x",{ FUNCTION_SIGNATURE_OBJ_ONLY{return 1.0 / (static_cast<gridLink *>(obj)->getImagImpedance(1)); }, puOhm } },
	{ "current",{ FUNCTION_SIGNATURE_OBJ_ONLY{return (static_cast<gridLink *>(obj)->getCurrent(1)); }, puA } },
	{ "realcurrent",{ FUNCTION_SIGNATURE_OBJ_ONLY{return (static_cast<gridLink *>(obj)->getRealCurrent(1)); }, puA } },
	{ "imagcurrent",{ FUNCTION_SIGNATURE_OBJ_ONLY{return (static_cast<gridLink *>(obj)->getImagCurrent(1)); }, puA } },
	{ "voltage",{ FUNCTION_SIGNATURE_OBJ_ONLY{return (static_cast<gridLink *>(obj)->getVoltage(1)); }, puV } },
	{ "absangle",{ FUNCTION_SIGNATURE_OBJ_ONLY{return (static_cast<gridLink *>(obj)->getAbsAngle(1)); }, rad } },
	//functions for to side
	{ "real2",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridLink *>(obj)->getRealPower(2); }, puMW } },
	{ "reactive2",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridLink *>(obj)->getReactivePower(2); }, puMW } },
	{ "linkreal2",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridLink *>(obj)->getRealPower(2); },puMW } },
	{ "linkreactive2",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridLink *>(obj)->getReactivePower(2); }, puMW } },
	{ "z2",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridLink *>(obj)->getTotalImpedance(2); },puOhm } },
	{ "y2",{ FUNCTION_SIGNATURE_OBJ_ONLY{return 1.0 / (static_cast<gridLink *>(obj)->getTotalImpedance(2)); }, puOhm } },
	{ "r2",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<gridLink *>(obj)->getRealImpedance(2); },puOhm } },
	{ "x2",{ FUNCTION_SIGNATURE_OBJ_ONLY{return 1.0 / (static_cast<gridLink *>(obj)->getImagImpedance(2)); }, puOhm } },
	{ "current2",{ FUNCTION_SIGNATURE_OBJ_ONLY{return (static_cast<gridLink *>(obj)->getCurrent(2)); }, puA } },
	{ "realcurrent2",{ FUNCTION_SIGNATURE_OBJ_ONLY{return (static_cast<gridLink *>(obj)->getRealCurrent(2)); }, puA } },
	{ "imagcurrent2",{ FUNCTION_SIGNATURE_OBJ_ONLY{return (static_cast<gridLink *>(obj)->getImagCurrent(2)); }, puA } },
	{ "voltage2",{ FUNCTION_SIGNATURE_OBJ_ONLY{return (static_cast<gridLink *>(obj)->getVoltage(2)); }, puV } },
	{ "absangle2",{ FUNCTION_SIGNATURE_OBJ_ONLY{return (static_cast<gridLink *>(obj)->getAbsAngle(2)); }, rad } },

	//non numbered fields
	{ "angle",{ FUNCTION_SIGNATURE{return static_cast<gridLink *> (obj)->getAngle(sD.state, sMode);}, rad } },
	{ "loss",{ FUNCTION_SIGNATURE_OBJ_ONLY{return (static_cast<gridLink *>(obj)->getLoss()); }, puMW } },
	{ "lossreactive",{ FUNCTION_SIGNATURE_OBJ_ONLY{return (static_cast<gridLink *>(obj)->getReactiveLoss()); }, puMW } },
	{ "attached",{ FUNCTION_SIGNATURE_OBJ_ONLY{return static_cast<double> (((!static_cast<gridLink *>(obj)->checkFlag(gridLink::switch1_open_flag)) || (!static_cast<gridLink *>(obj)->checkFlag(gridLink::switch2_open_flag))) && (static_cast<gridLink *>(obj)->isEnabled())); }, defUnit } },
};

/* *INDENT-ON* */

void stateGrabber::objectLoadInfo(const std::string &fld)
{
	auto funcfind = objectFunctions.find(fld);
	if (funcfind != objectFunctions.end())
	{
		fptr = funcfind->second.first;
	}
	else 
	{
		std::string fieldStr;
		int num = stringOps::trailingStringInt(fld, fieldStr, 0);
		if ((fieldStr == "value") || (fieldStr == "output") || (fieldStr == "o"))
		{
			fptr = [num](gridObject *obj, const stateData &sD, const solverMode &sMode) {
				return obj->getOutput(noInputs, sD, sMode, static_cast<index_t> (num));
			};
		}
		if ((fieldStr == "deriv") || (fieldStr == "doutdt") || (fieldStr == "derivative"))
		{
			fptr = [num](gridObject *obj, const stateData &sD, const solverMode &sMode) {
				return obj->getDoutdt(noInputs, sD, sMode, static_cast<index_t> (num));
			};
		}
		else
		{
			fptr = nullptr;
			loaded = false;
		}
		
	}
}

void stateGrabber::busLoadInfo (const std::string &fld)
{
	std::string nfstr = mapFind(stringTranslate, fld, fld);

	auto funcfind = busFunctions.find(nfstr);
	if (funcfind != busFunctions.end())
	{
		fptr=funcfind->second.first;
		inputUnits = funcfind->second.second;
		loaded = true;
		auto jacfind = busJacFunctions.find(nfstr);
		if (jacfind != busJacFunctions.end())
		{
			jacIfptr = jacfind->second;
			jacMode = jacobian_mode::computed;
		}
	}
	else
	{
		objectLoadInfo(nfstr);
	}
  
}

void stateGrabber::linkLoadInfo (const std::string &fld)
{
	std::string nfstr = mapFind(stringTranslate, fld, fld);

	auto funcfind = linkFunctions.find(nfstr);
	if (funcfind != linkFunctions.end())
	{
		fptr = funcfind->second.first;
		inputUnits = funcfind->second.second;
		loaded = true;
		cacheUpdateRequired = true;
		/*auto jacfind = linkJacFunctions.find(nfstr);
		if (jacfind != busJacFunctions.end())
		{
			jacIfptr = jacfind->second;
			jacMode = jacobian_mode::computed;
		}
		*/
	}
	else
	{
		objectLoadInfo(nfstr);
	}
}
void stateGrabber::relayLoadInfo (const std::string &fld)
{
  std::string fieldStr;
  int num = stringOps::trailingStringInt (fld, fieldStr, 0);
  if ((fieldStr == "block") || (fieldStr == "b"))
    {
      if (dynamic_cast<sensor *> (cobj))
        {
          fptr = [ num ](gridObject *obj, const stateData &sD, const solverMode &sMode) {
              return static_cast<sensor *> (obj)->getBlockOutput (sD, sMode, num);
            };
        }
      else
        {
          loaded = false;
        }
    }
  else if ((fieldStr == "input") || (fieldStr == "i"))
    {
      if (dynamic_cast<sensor *> (cobj))
        {
          fptr = [ num ](gridObject *obj, const stateData &sD, const solverMode &sMode) {
              return static_cast<sensor *> (obj)->getInput (sD, sMode, num);
            };
        }
      else
        {
          loaded = false;
        }
    }
  else if ((fieldStr == "condition") || (fieldStr == "c"))
    {
      //dgptr = &gridLink::getAngle;
      fptr = [ num ](gridObject *obj, const stateData &sD, const solverMode &sMode) {
          return (static_cast<gridRelay *> (obj))->getCondition (num)->getVal (1, sD, sMode);
        };
    }
  else
    {
      if (dynamic_cast<sensor *> (cobj))
        {
          //try to lookup named output for sensors
          index_t outIndex = static_cast<sensor *> (cobj)->lookupOutput (fld);
          if (outIndex != kNullLocation)
            {
              fptr = [ outIndex ](gridObject *obj, const stateData &sD, const solverMode &sMode) {
                  return static_cast<sensor *> (obj)->getOutput (noInputs,sD, sMode, outIndex);
                };
            }
			else
			{
				objectLoadInfo(fld);
			}
        }
     
      else
        {
		  objectLoadInfo(fld);
        }

    }
}


void stateGrabber::secondaryLoadInfo (const std::string &fld)
{
  if ((fld == "realpower") || (fld == "power") || (fld == "p"))
    {
	  cacheUpdateRequired = true;
      fptr = [ ](gridObject *obj, const stateData &sD, const solverMode &sMode) {
          return static_cast<gridSecondary *> (obj)->getRealPower (noInputs, sD, sMode);
        };
	  jacMode = jacobian_mode::computed;
      jacIfptr = [ ](gridObject *obj, const stateData &sD, matrixData<double> &ad, const solverMode &sMode) {
          matrixDataTranslate<1,double> b(ad);
		  b.setTranslation(PoutLocation, 0);
          static_cast<gridSecondary *> (obj)->outputPartialDerivatives (noInputs, sD, b, sMode);
        };
    }
  else if ((fld == "reactivepower") || (fld == "reactive") || (fld == "q"))
    {
	  cacheUpdateRequired = true;
      fptr = [ ](gridObject *obj, const stateData &sD, const solverMode &sMode) {
          return static_cast<gridSecondary *> (obj)->getReactivePower (noInputs, sD, sMode);
        };
	  jacMode = jacobian_mode::computed;
      jacIfptr = [  ](gridObject *obj, const stateData &sD, matrixData<double> &ad, const solverMode &sMode) {
          matrixDataTranslate<1,double> b(ad);
		  b.setTranslation(QoutLocation, 0);
          static_cast<gridSecondary *> (obj)->outputPartialDerivatives (noInputs, sD, b, sMode);
        };
    }
  else
    {
      offset = static_cast<gridSecondary *> (cobj)->findIndex (fld, cLocalbSolverMode);
      if (offset != kInvalidLocation)
        {
          prevIndex = 1;
          fptr = [ &](gridObject *obj, const stateData &sD, const solverMode &sMode) {
              if (sMode.offsetIndex != prevIndex)
                {
                  offset = static_cast<gridSecondary *> (obj)->findIndex (field, sMode);
                  prevIndex = sMode.offsetIndex;
                }
              return (offset != kNullLocation) ? sD.state[offset] : kNullVal;
            };
		  jacMode = jacobian_mode::computed;
          jacIfptr = [ =](gridObject *,const stateData &, matrixData<double> &ad, const solverMode &) {
              ad.assignCheckCol (0, offset, 1.0);
            };
        }
      else
        {
          loaded = false;
        }
    }
}

void stateGrabber::areaLoadInfo (const std::string & /*fld*/)
{

}

double stateGrabber::grabData (const stateData &sD, const solverMode &sMode)
{
  if (loaded)
    {
	  if (cacheUpdateRequired)
	  {
		  cobj->updateLocalCache(noInputs, sD, sMode);
	  }
      double val = fptr (cobj,sD, sMode);
      val = std::fma (val, gain, bias);
      return val;
    }
  else
    {
      return kNullVal;
    }

}

void stateGrabber::updateObject (coreObject *obj, object_update_mode mode)
{
	if (mode == object_update_mode::direct)
	{
		cobj = dynamic_cast<gridObject *>(obj);
	}
	else if (mode == object_update_mode::match)
	{
		cobj=dynamic_cast<gridObject *>(findMatchingObject(cobj, obj));
	}
}

coreObject * stateGrabber::getObject() const
{
	return cobj;
}

void stateGrabber::getObjects(std::vector<coreObject *> &objects) const
{
	objects.push_back(getObject());
}

void stateGrabber::outputPartialDerivatives (const stateData &sD, matrixData<double> &ad, const solverMode &sMode)
{
  if (jacMode==jacobian_mode::none)
    {
      return;
    }
  if (gain != 1.0)
    {
      matrixDataScale<double> bd(ad,gain); 
      jacIfptr (cobj,sD, bd, sMode);
    }
  else
    {
      jacIfptr (cobj,sD, ad, sMode);
    }

}

customStateGrabber::customStateGrabber(gridObject *obj) :stateGrabber(obj)
{

}

void customStateGrabber::setGrabberFunction (objStateGrabberFunction nfptr)
{
  fptr = nfptr;
  loaded = true;
}


void customStateGrabber::setGrabberJacFunction(objJacFunction nJfptr)
{
	jacIfptr = nJfptr;
	jacMode = (jacIfptr)?jacobian_mode::computed:jacobian_mode::none;
}

std::shared_ptr<stateGrabber> customStateGrabber::clone (std::shared_ptr<stateGrabber > ggb) const
{
	auto cgb = cloneBase<customStateGrabber, stateGrabber>(this, ggb);
  if (cgb == nullptr)
    {
	  return ggb;
    }
  return cgb;
}

stateFunctionGrabber::stateFunctionGrabber (std::shared_ptr<stateGrabber> ggb, std::string func): function_name(func)
{
  if (ggb)
    {
      bgrabber = ggb;
    }
  opptr = get1ArgFunction (function_name);
  jacMode = (bgrabber->getJacobianMode());
  loaded = bgrabber->loaded;
}


void stateFunctionGrabber::updateField (const std::string &fld)
{
	if (!fld.empty())
	{
		if (isFunctionName(fld, function_type::arg))
		{
			function_name = fld;
			opptr = get1ArgFunction(function_name);
		}
		else
		{
			loaded = false;
		}
	}
	
	loaded = true;
}



std::shared_ptr<stateGrabber> stateFunctionGrabber::clone (std::shared_ptr<stateGrabber> ggb) const
{
	auto fgb = cloneBase<stateFunctionGrabber, stateGrabber>(this, ggb);
  if (fgb == nullptr)
    {
	  if (bgrabber)
	  {
		  return bgrabber->clone(ggb);
	  }
	  else
	  {
		  return ggb;
	  }
    }
  if (bgrabber)
  {
	  fgb->bgrabber = bgrabber->clone(fgb->bgrabber);
  }
  fgb->function_name = function_name;
  fgb->opptr = opptr;
  return fgb;
}

double stateFunctionGrabber::grabData (const stateData &sD, const solverMode &sMode)
{
  double val = opptr (bgrabber->grabData(sD, sMode));
  val = std::fma (val, gain, bias);
  return val;
}


void stateFunctionGrabber::updateObject (coreObject *obj, object_update_mode mode)
{
  if (bgrabber)
    {
      bgrabber->updateObject (obj,mode);
    }

}

coreObject *stateFunctionGrabber::getObject () const
{
  if (bgrabber)
    {
      return bgrabber->getObject ();
    }
  else
    {
      return nullptr;
    }
}

void stateFunctionGrabber::outputPartialDerivatives (const stateData &sD, matrixData<double> &ad, const solverMode &sMode)
{

	if (jacMode == jacobian_mode::none)
    {
      return;
    }

  double temp = bgrabber->grabData (sD, sMode);
  double t1 = opptr (temp);
  double t2 = opptr (temp + 1e-7);
  double dodI = (t2 - t1) / 1e-7;

  matrixDataScale<double> d1(ad, dodI * gain);
  bgrabber->outputPartialDerivatives(sD, d1, sMode);
}

stateOpGrabber::stateOpGrabber (std::shared_ptr<stateGrabber> ggb1, std::shared_ptr<stateGrabber> ggb2, std::string op): op_name(op)
{
  if (ggb1)
    {
      bgrabber1 = ggb1;
    }
  if (ggb2)
    {
      bgrabber2 = ggb2;
    }
  opptr = get2ArgFunction (op);
  jacMode = std::min(bgrabber1->getJacobianMode(), bgrabber2->getJacobianMode());
  loaded = ((ggb1->loaded) && (ggb2->loaded));
}


void stateOpGrabber::updateField (const std::string &opName)
{
	if (!opName.empty())
	{
		if (isFunctionName(opName, function_type::arg2))
		{
			op_name = opName;
			opptr = get2ArgFunction(op_name);
		}
		else
		{
			loaded = false;
		}
	}

	loaded = true;
}

std::shared_ptr<stateGrabber> stateOpGrabber::clone (std::shared_ptr<stateGrabber> ggb) const
{
	auto ogb = cloneBase<stateOpGrabber, stateGrabber>(this, ggb);
  if (ogb == nullptr)
    {
	  if (bgrabber1)
	  {
		  bgrabber1->clone(ggb); 
	  }
	  return ggb;
    }
  if (bgrabber1)
  {
	  ogb->bgrabber1 = bgrabber1->clone(ogb->bgrabber1);
 }
  if (bgrabber2)
  {
	  ogb->bgrabber2 = bgrabber2->clone(ogb->bgrabber2);
  }
  
  ogb->op_name = op_name;
  ogb->opptr = opptr;
  return ogb;
}

double stateOpGrabber::grabData (const stateData &sD, const solverMode &sMode)
{
  double grabber1Data = bgrabber1->grabData (sD, sMode);
  double grabber2Data = bgrabber2->grabData (sD, sMode);
  double val = opptr (grabber1Data, grabber2Data);
  val = std::fma (val, gain, bias);
  return val;
}



void stateOpGrabber::updateObject (coreObject *obj, object_update_mode mode)
{
  if (bgrabber1)
    {
      bgrabber1->updateObject (obj,mode);
    }
  if (bgrabber2)
    {
      bgrabber2->updateObject (obj,mode);
    }

}

void stateOpGrabber::updateObject (coreObject *obj, int num)
{
  if (num == 1)
    {
      if (bgrabber1)
        {
          bgrabber1->updateObject (obj);
        }
    }
  else if (num == 2)
    {
      if (bgrabber2)
        {
          bgrabber2->updateObject (obj);
        }
    }

}

coreObject *stateOpGrabber::getObject () const
{
  if (bgrabber1)
    {
      return bgrabber1->getObject ();
    }
  else if (bgrabber2)
  {
	  return bgrabber2->getObject();
  }
  else
    {
      return nullptr;
    }
}

void stateOpGrabber::outputPartialDerivatives (const stateData &sD, matrixData<double> &ad, const solverMode &sMode)
{

  if (jacMode == jacobian_mode::none)
    {
      return;
    }
 

  double grabber1Data = bgrabber1->grabData (sD, sMode);
  double grabber2Data = bgrabber2->grabData (sD, sMode);

  double t1 = opptr (grabber1Data, grabber2Data);
  double t2 = opptr (grabber1Data + 1e-7, grabber2Data);
  
  double dodI = (t2 - t1) / 1e-7;

  matrixDataScale<double> d1(ad, dodI * gain);
  bgrabber1->outputPartialDerivatives(sD, d1, sMode);

  double t3 = opptr(grabber1Data, grabber2Data + 1e-7);
  dodI = (t3 - t1) / 1e-7;
  d1.setScale(dodI * gain);
  bgrabber2->outputPartialDerivatives(sD, d1, sMode);

}
