/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil;  eval: (c-set-offset 'innamespace 0); -*- */
/*
* LLNS Copyright Start
* Copyright (c) 2014, Lawrence Livermore National Security
* This work was performed under the auspices of the U.S. Department
* of Energy by Lawrence Livermore National Laboratory in part under
* Contract W-7405-Eng-48 and in part under Contract DE-AC52-07NA27344.
* Produced at the Lawrence Livermore National Laboratory.
* All rights reserved.
* For details, see the LICENSE file.
* LLNS Copyright End
*/
#ifndef LOAD_RELAY_H_
#define LOAD_RELAY_H_

#include "gridRelay.h"

/** class implementing a protective relay for load objects
the protective systems include underfrequency, undervoltage, and a return time so the load automatically recovers
*/
class loadRelay : public gridRelay
{
public:
  enum loadrelay_flags
  {
    nondirectional_flag = object_flag10,
  };
protected:
  double cutoutVoltage = 0;			//!<[puV] low voltage trigger for load
  double cutoutFrequency = 0;		//!<[puHz] low frequency trigger for load
  double voltageDelay = 0;			//!<[s]  the delay on the voltage trip
  double frequencyDelay = 0;		//!<[s] the delay on the frequency tripping
  double offTime = kBigNum;			//!<[s] the time before the load comes back online if the trip cause has been corrected
public:
  loadRelay (const std::string &objName = "loadRelay_$");
  virtual gridCoreObject * clone (gridCoreObject *obj = nullptr) const override;
  virtual int setFlag (const std::string &flag, bool val = true) override;
  virtual int set (const std::string &param,  const std::string &val) override;

  virtual int set (const std::string &param, double val, gridUnits::units_t unitType = gridUnits::defUnit) override;

  virtual void dynObjectInitializeA (double time0, unsigned long flags) override;
protected:
  virtual void actionTaken (index_t ActionNum, index_t conditionNum, change_code actionReturn, double actionTime) override;
  virtual void conditionTriggered (index_t conditionNum, double triggerTime) override;
  virtual void conditionCleared (index_t conditionNum, double triggerTime) override;

};


#endif