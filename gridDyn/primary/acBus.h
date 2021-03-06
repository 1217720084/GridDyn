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

#ifndef BASICBUS_H_
#define BASICBUS_H_

// headers
#include "gridBus.h"
#include "gridObjects.h"
#include "arrayDataCompact.h"

#include "arrayDataTranslate.h"
#include "primary/busControls.h"
class basicBlock;


/** @brief basic power system bus for a power grid simulation
  The gridBus class provides the basic node in a power systems analysis.  It is a locational basis for voltages and angles.  For the power systems analysis
the equations include a power balance equations
\f[
f(V)=\sum Q_{load}+ \sum Q_{gen}+ \sum Q_{link}
\f]
\f[
f(\theta)=\sum P_{load}+\sum P_{gen}+\sum P_{link}
\f]
Buses can have a number of different configurations SLK buses fix the voltage and angle,  PV buses fix the voltage and real power,  PQ buses have known real and reactive generation/load,
afix buses fix the angle and reactive power.  there are different setting for power flow calculations and dynamic calculations.

Buses act as connection points for links to tie to other buses and gridSecondary components such as generators and loads.

*/
class acBus : public gridBus
{
  friend class busControls;
public:
  /** @brief flags for the buses*/
  enum bus_flags
  {
    use_autogen = object_flag2,              //!< indicator if the bus is using an autogen
    slave_bus = object_flag3,               //!< indicator that the bus is a slave Bus
    master_bus = object_flag4,               //!< indicator that a bus is a master bus
    directconnect = object_flag5,               //!< indicator that a bus is direct connected to another bus
    identical_PQ_control_objects = object_flag6,              //!< indicator that the P and Q control are the same units
    compute_frequency = object_flag7,                  //!< indicator that the bus should compute the frequency value
    ignore_angle = object_flag8,                 //!< indicator that the bus should ignore the angle in update functions
    prev_low_voltage_alert = object_flag9,              //!< indicator that the bus has triggered a low voltage alert
  };
protected:
  count_t oCount = 0;                                                                         //!< counter for updates
  arrayDataCompact<2, 3> partDeriv;                                 //!< structure containing the partial derivatives
  double aTarget = 0.0;                                                                       //!< an angle Target(for SLK and afix bus types)
  double vTarget = 1.0;                                                                       //!< a target voltage
  double participation = 1.0;                                                         //!< overall participation factor in power regulation for an area

  busType prevType = busType::PQ;                                                     //!< previous type container if the type automatically changes
  dynBusType prevDynType = dynBusType::normal;                        //!< previous type container if the type automatically changes
  double refAngle = 0;                                                                        //!< reference Angle


  double Vmin = 0;                                                                                    //!< [pu]    voltage minimum
  double Vmax = kBigNum;                                                              //!< [pu]    voltage maximum
  double tieError = 0.0;       //!<tieLine error
  double prevPower = 0.0;                     //!< previous power level
  double Tw = 0.1;               //!<time constant for the frequency estimator


  gridDyn_time lastSetTime = -kBigNum;                      //!< last set time
  std::shared_ptr<basicBlock> fblock;         //!< pointer to frequency estimator block

  busControls busController;                //!< pointer to the eControls object
  //extra blocks and object for remote controlled buses and bus merging
  arrayDataTranslate<4> od;
  index_t lastSmode = kInvalidLocation;
  
public:
  /** @brief default constructor*/
  explicit acBus (const std::string &objName = "bus_$");
  /** @brief alternate constructor to specify voltage and angle
  @param[in] voltage  the initial voltage
  @param[in] angle the initial angle
  */
  acBus (double voltage, double angle,const std::string &objName = "bus_$");
  /** @brief destructor*/
  virtual ~acBus ();

  virtual gridCoreObject * clone (gridCoreObject *obj = nullptr) const override;
  // add components
  using gridBus::add;
  virtual int add (gridCoreObject *obj) override;
  /** @brief  add a gridBus object for merging buses*/
  virtual int add (acBus *bus);
  // remove components
  using gridBus::remove;
  virtual int remove (gridCoreObject *obj) override;

  virtual int remove (acBus *bus);
  //deal with control alerts
  virtual void alert (gridCoreObject *, int code) override;

  // initializeB
  virtual void setOffsets (const solverOffsets &newOffsets, const solverMode &sMode) override;
  virtual void setOffset (index_t offset, const solverMode &sMode) override;
  virtual void loadSizes (const solverMode &sMode, bool dynOnly) override;

  virtual void setRootOffset (index_t Roffset, const solverMode &sMode) override;
protected:
  virtual void pFlowObjectInitializeA (gridDyn_time time0, unsigned long flags) override;
  virtual void pFlowObjectInitializeB () override;
public:
  virtual change_code powerFlowAdjust (unsigned long flags, check_level_t level) override;           //only applicable in pFlow
  /** @brief  adjust the power levels of the contained adjustable secondary objects
  @param[in] adjustment the amount of the adjustment requested*/
  virtual void powerAdjust (double adjustment) override;
  virtual void pFlowCheck (std::vector<violation> &Violation_vector) override;
  virtual void reset (reset_levels level = reset_levels::minimal) override;
  //initializeB dynamics
protected:
  virtual void dynObjectInitializeA (gridDyn_time time0, unsigned long flags) override;
  virtual void dynObjectInitializeB (IOdata &outputSet) override;
public:
  virtual void disable () override;
  /** @brief  reconnect the bus
  @param[in] mapBus  a bus to pick of startup parameters from*/
  virtual void reconnect (gridBus *mapBus = nullptr) override;
  // parameter set functions
  virtual void getParameterStrings (stringVec &pstr, paramStringType pstype = paramStringType::all) const override;
  virtual int setFlag (const std::string &flag, bool val) override;
  virtual int set (const std::string &param, const std::string &val) override;
  virtual int set (const std::string &param, double val, gridUnits::units_t unitType = gridUnits::defUnit) override;
  // parameter get functions
  virtual double get (const std::string &param, gridUnits::units_t unitType = gridUnits::defUnit) const override;

  // solver functions
  virtual void jacobianElements (const stateData *sD, arrayData<double> *ad, const solverMode &sMode) override;
  virtual void residual (const stateData *sD, double resid[], const solverMode &sMode) override;
  virtual void derivative (const stateData *sD, double deriv[], const solverMode &sMode) override;
  virtual void algebraicUpdate (const stateData *sD, double update[], const solverMode &sMode, double alpha) override;
  virtual void voltageUpdate (const stateData *sD, double update[], const solverMode &sMode, double alpha) override;
  virtual void guess (gridDyn_time ttime, double state[], double dstate_dt[], const solverMode &sMode) override;


  /** @brief  try to shift the states to something more consistent
    called when the current states do not make a consistent condition,  calling converge will attempt to move them to a more valid state
  mode controls how this is done  0- does a single iteration loop
  mode=1 tries to iterate until convergence based on tol
  mode=2  tries harder
  mode=3 does it with voltage only
  @pararm[in] ttime  the time of the corresponding states
  @param[in,out]  state the states of the system at present and shifted to match the updates
  @param[in,out] dstate_dt  the derivatives of the state that get updated
  @param[in] sMode the solvemode matching the states
  @param[in] mode  the mode of the convergence
  @param[in] tol  the convergence tolerance
  */
  virtual void converge (gridDyn_time ttime, double state[], double dstate_dt[], const solverMode &sMode, converge_mode mode= converge_mode::high_error_only, double tol = 0.01) override;
  /** @brief  try to shift the local states to something more valid
    called when the current states do not make a consistent condition,  calling converge will attempt to move them to a more valid state
  mode controls how this is done  0- does a single iteration loop
  mode=1 tries to iterate until convergence based on tol
  mode=2  tries harder
  mode=3 does it with voltage only
  @param[in] sMode the solvemode matching the states
  @param[in] mode  the mode of the convergence
  @param[in] tol  the tolerance to converge to
  */
  virtual void localConverge (const solverMode &sMode, int mode = 0, double tol = 0.01);
  /** @brief  return the last error in the real power*/

  virtual void updateLocalCache () override;
  virtual void updateLocalCache (const stateData *sD, const solverMode &sMode) override;
protected:
  /** @brief  compute adjustments required for the dynamic update*/
  virtual void computePowerAdjustments ();
  /** @brief  compute the partial derivatives based on the given state data
  @param[in] sD  the state Data in question
  @param[in] sMode the solver mode*/
  virtual void computeDerivatives (const stateData *sD, const solverMode &sMode);
public:
  double timestep (double ttime, const solverMode &sMode) override;

  virtual void setState (gridDyn_time ttime, const double state[], const double dstate_dt[], const solverMode &sMode) override;
  /** @brief a faster function to set the voltage and angle of a bus*
  @param[in] Vnew  the new voltage
  @param[in] Anew  the new angle
  */
  virtual void setVoltageAngle (double Vnew, double Anew) override;
  //for identifying which variables are algebraic vs differential
  virtual void getVariableType (double sdata[], const solverMode &sMode) override;            //only applicable in DAE mode
  virtual void getTols (double tols[], const solverMode &sMode) override;
  // dynamic simulation
  virtual void getStateName (stringVec &stNames, const solverMode &sMode, const std::string &prefix) const override;

  /** @brief check if the bus is capable of operating
  */
  virtual bool checkCapable () override;
  // find components
  /** @brief find a link based on the bus desiring to be connected to
  @param[in] bs  the bus we want to check for a connecting link
  @return  a pointer to a Link that connects the current bus to the bus specified by bs or nullptr if none exists
  */
  virtual int propogatePower (bool makeSlack = false) override;


  /** @brief get the maximum real power generation
  * @return the maximum real power generation
  **/
  double getMaxGenReal () const
  {
    return busController.Pmax;
  }
  /** @brief get the maximum reactive power generation
  * @return the maximum reactive power generation
  **/
  double getMaxGenReactive () const
  {
    return busController.Qmax;
  }

  /** @brief get the available controllable upward adjustments within a time period
  @ details this means power production or load reduction
  @param[in] time  the time period within which to do the adjustments
  * @return the reactive link power
  **/
  virtual double getAdjustableCapacityUp (gridDyn_time time = kBigNum) const override;
  /** @brief get the available controllable upward adjustments within a time period
  @ details this means power production or load reduction
  @param[in] time  the time period within which to do the adjustments
  * @return the reactive link power
  **/
  virtual double getAdjustableCapacityDown (gridDyn_time time = kBigNum) const override;
  /** @brief the dPdf partial derivative  (may be deprecated in the future)
  * @return the $\frac{\partial P}{\partial f}$
  **/
  virtual double getdPdf () const override
  {
    return 0;
  }
  /** @brief get the tie error (may be deprecated in the future)
  * @return the tie error
  **/
  virtual double getTieError () const override
  {
    return tieError;
  }
  /** @brief get the frequency response
  * @return the tie error
  **/
  virtual double getFreqResp () const override
  {
    return 0;
  }
  /** @brief get available regulation
  * @return the available regulation
  **/
  virtual double getRegTotal () const override
  {
    return 0;
  }
  /** @brief get the scheduled power
  * @return the scheduled power
  **/
  virtual double getSched () const override
  {
    return 0;
  }

  virtual IOdata getOutputs (const stateData *sD, const solverMode &sMode) override;
  virtual index_t getOutputLoc (const solverMode &sMode, index_t num) const override;

  virtual IOlocs getOutputLocs (const solverMode &sMode) const override;
  /** @brief get the voltage
  * @param[in] state the system state
  @param[in] sMode the corresponding solverMode to the state
  @return the sbus voltage
  **/
  virtual double getVoltage (const double state[], const solverMode &sMode) const override;
  /** @brief get the angle
  * @param[in] state the system state
  @param[in] sMode the corresponding solverMode to the state
  @return the bus angle
  **/
  virtual double getAngle (const double state[], const solverMode &sMode) const override;
  /** @brief get the voltage
  * @param[in] sD the system state data
  @param[in] sMode the corresponding solverMode to the state data
  @return the bus voltage
  **/
  virtual double getVoltage (const stateData *sD, const solverMode &sMode) const override;
  /** @brief get the angle
  * @param[in] sD the system state data
  @param[in] sMode the corresponding solverMode to the state
  @return the bus angle
  **/
  virtual double getAngle (const stateData *sD, const solverMode &sMode) const override;
  /** @brief get the bus frequency
  * @param[in] sD the system state data
  @param[in] sMode the corresponding solverMode to the state
  @return the bus frequency
  **/
  virtual double getFreq (const stateData *sD, const solverMode &sMode) const override;

  virtual change_code rootCheck (const stateData *sD, const solverMode &sMode, check_level_t level) override;
  /** @brief function used for returning the mode of the bus
   depends on the interaction of the solverInterface and the bus type
  @param[in] sMode the corresponding solverMode to the state
  @return the system mode
  **/
  virtual int getMode (const solverMode &sMode) const;
  /** @brief function to determine there is a state representing the angle
  @param[in] sMode the corresponding solverMode to the state
  @return true if there is an angle state false otherwise
  **/
  virtual bool useAngle (const solverMode &sMode) const;
  /** @brief function to determine there is a state representing the voltage
  @param[in] sMode the corresponding solverMode to the state
  @return true if there is an voltage state false otherwise
  **/
  virtual bool useVoltage (const solverMode &sMode) const;

  virtual void updateFlags (bool dynOnly = false) override;
  //for registering and removing power control objects

  /** @brief  register an object for voltage control on a bus*/
  void registerVoltageControl (gridObject *obj) override;
  /** @brief  remove an object from voltage control on a bus*/
  void removeVoltageControl (gridObject *obj) override;
  /** @brief  register an object for power control on a bus*/
  void registerPowerControl (gridObject *obj) override;
  /** @brief  remove an object from power control on a bus*/
  void removePowerControl (gridObject *obj) override;

  //for dealing with buses merged with zero impedance link
  /** @brief  merge a bus with the calling bus*/
  virtual void mergeBus (gridBus *mbus) override;
  /** @brief  unmerge a bus with the calling bus*/
  virtual void unmergeBus (gridBus *mbus) override;
  /** @brief  check if all the buses that are merged should be*/
  virtual void checkMerge () override;

protected:
  /** @brief
  @param[in] sD stateData the statedata to compute the Error for
  @param[in] sMode the solverMode corresponding to the stateData
  @return the error in the power balance equations
  */
  double computeError (const stateData *sD, const solverMode &sMode);
private:
  double getAverageAngle ();
  gridDynGenerator *keyGen = nullptr;
};


#endif

