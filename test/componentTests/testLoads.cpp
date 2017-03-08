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

#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>
#include "gridDyn.h"
#include "gridBus.h"
#include "loadModels/zipLoad.h"
#include "loadModels/gridLabDLoad.h"
#include "loadModels/motorLoad.h"
#include "loadModels/gridLoad3Phase.h"
#include "gridDynFileInput.h"
#include "simulation/diagnostics.h"
#include "testHelper.h"
#include <cmath>

static const std::string load_test_directory(GRIDDYN_TEST_DIRECTORY "/load_tests/");
static const std::string gridlabd_test_directory(GRIDDYN_TEST_DIRECTORY "/gridlabD_tests/");

BOOST_FIXTURE_TEST_SUITE(load_tests, gridLoadTestFixture)

BOOST_AUTO_TEST_CASE(basic_load_test)
{
	ld1 = new zipLoad(1.1, -0.3);
  ld1->setFlag("no_pqvoltage_limit");
	double val;
	//check basic return values
	val = ld1->getRealPower(1.0);
	BOOST_CHECK_CLOSE(val, 1.1, 0.000001);
	val = ld1->getReactivePower(1.0);
	BOOST_CHECK_CLOSE(val, -0.3, 0.000001);
	//check for no voltage dependence
	val = ld1->getRealPower(1.5);
	BOOST_CHECK_CLOSE(val, 1.1, 0.000001);
	val = ld1->getReactivePower(1.5);
	BOOST_CHECK_CLOSE(val, -0.3, 0.000001);

	//check basic setting function
	ld1->set("p", 1.2);
	ld1->set("q", 0.234);
	val = ld1->getRealPower(1.0);
	BOOST_CHECK_CLOSE(val, 1.2, 0.000001);
	val = ld1->getReactivePower(1.0);
	BOOST_CHECK_CLOSE(val, 0.234, 0.000001);

	//check basic setting function
	ld1->set("p", 0.0);
	ld1->set("q", 0.0);
	ld1->set("r", 2.0);
	val = ld1->getRealPower(1.0);
	BOOST_CHECK_CLOSE(val, 0.5, 0.000001);
	val = ld1->getRealPower(1.2);
	BOOST_CHECK_CLOSE(val, 1.2*1.2/2.0, 0.000001);

	//check basic setting function for x
	ld1->set("r", 0.0);
	ld1->set("x", 2.0);
	val = ld1->getRealPower(1.2);
	BOOST_CHECK_SMALL(val, 0.000001);
	val = ld1->getReactivePower(1.0);
	BOOST_CHECK_CLOSE(val, 0.5, 0.000001);
	val = ld1->getReactivePower(1.2);
	BOOST_CHECK_CLOSE(val, 1.2*1.2 / 2.0, 0.000001);

	//check basic setting function for x
	ld1->set("r", kBigNum);
	ld1->set("x", 0.0);
	ld1->set("ip", 1.0);
	ld1->set("iq", 2.0);
	val = ld1->getRealPower(1.2);
	BOOST_CHECK_CLOSE(val,1.2, 0.000001);
	val = ld1->getReactivePower(1.2);
	BOOST_CHECK_CLOSE(val, 2.4, 0.000001);
	val = ld1->getRealPower(0.99);
	BOOST_CHECK_CLOSE(val, 0.99, 0.000001);
	val = ld1->getReactivePower(0.99);
	BOOST_CHECK_CLOSE(val, 1.98, 0.000001);

	ld1->set("r", kBigNum);
	ld1->set("x", 0.0);
	ld1->set("ip", 0.0);
	ld1->set("iq", 0.0);
	ld1->set("p", 1.0);
	ld1->set("pf", 0.9);
	val = ld1->getRealPower(1.2);
	BOOST_CHECK_CLOSE(val, 1.0, 0.0001);
	val = ld1->getReactivePower(1.2);
	BOOST_CHECK_SMALL(val-0.4843, 0.0003);
	ld1->set("p", 1.4);
	val = ld1->getReactivePower();
	BOOST_CHECK_SMALL(val-0.6781, 0.0003);

}

BOOST_AUTO_TEST_CASE(load_voltage_sweep)
{
  ld1 = new zipLoad(1.0,0.0);
  std::vector<double> v;
  std::vector<double> out;
  ld1->set("vpqmin",0.75);
  ld1->set("vpqmax",1.25);
 
  for (double vt=0.0;vt<=1.5;vt+=0.001)
  {
	  v.push_back(vt);
    out.push_back(ld1->getRealPower(vt));
  }
  v.push_back(1.5);
  BOOST_CHECK_SMALL(std::abs(out[400]-v[400]*v[400]/(0.75*0.75)),0.001);
  BOOST_CHECK_SMALL(std::abs(out[1350] - v[1350] * v[1350] / (1.25*1.25)), 0.001);
  BOOST_CHECK_SMALL(std::abs(out[800] - 1.0), 0.001);
  BOOST_CHECK_SMALL(std::abs(out[1249] - 1.0), 0.001);
}

BOOST_AUTO_TEST_CASE(ramp_load_test)
{
	ld1 = new gridRampLoad();
	gridRampLoad *ldT = dynamic_cast<gridRampLoad *>(ld1);
	BOOST_REQUIRE((ldT));
	double val;
	//test P ramp
	ld1->set("p", 0.5);
	ld1->pFlowInitializeA(timeZero, 0);
	ldT->set("dpdt",0.01);
	val = ld1->getRealPower(1.0);
	BOOST_CHECK_CLOSE(val, 0.5, 0.000001);
	ldT->setState(4.0, nullptr, nullptr, cLocalSolverMode);
	val = ld1->getRealPower(1.0);
	BOOST_CHECK_CLOSE(val, 0.54, 0.000001);
	ld1->set("dpdt", 0.0);
	//test Q ramp
	ld1->pFlowInitializeA(timeZero,0);
	ldT->set("p", 0.0);
	ldT->set("q", -0.3);
	ldT->set("dqdt",-0.01);
	val = ld1->getReactivePower(1.0);
	BOOST_CHECK_CLOSE(val, -0.3, 0.000001);
	ldT->setState(6.0, nullptr, nullptr, cLocalSolverMode);
	val = ld1->getReactivePower(1.0);
	BOOST_CHECK_CLOSE(val, -0.36, 0.000001);
	ld1->set("dqdt", 0.0);
	ld1->set("q", 0.0);
	//test r ramp
	ld1->pFlowInitializeA(timeZero, 0);
	ldT->set("r", 1.0);
  ldT->set("drdt", 0.05); 
	val = ld1->getRealPower(1.0);
	BOOST_CHECK_CLOSE(val, 1.0, 0.000001);
	ldT->setState(2.0, nullptr, nullptr, cLocalSolverMode);
	val = ld1->getRealPower(1.2);
	BOOST_CHECK_CLOSE(val, 1.44/1.1, 0.000001);
	ld1->set("drdt", 0.0);
	ld1->set("r", 0.0);
	//test x ramp
	ld1->pFlowInitializeA(timeZero, 0);
	ldT->set("x", 0.5);
  ldT->set("dxdt", -0.05); 
	val = ld1->getReactivePower(1.0);
	BOOST_CHECK_CLOSE(val, 2.0, 0.000001);
	ldT->setState(4.0, nullptr, nullptr, cLocalSolverMode);
	val = ld1->getReactivePower(1.2);
	BOOST_CHECK_CLOSE(val, 1.2*1.2 / 0.3, 0.000001);
	ld1->set("dxdt", 0.0);
	ld1->set("x", 0.0);
	ld1->set("r", kBigNum);
	//test Ir ramp
	ld1->pFlowInitializeA(timeZero, 0);
	ldT->set("ip", 0.5);
	ldT->set("dipdt",-0.01);
	val = ld1->getRealPower(1.0);
	BOOST_CHECK_CLOSE(val, 0.5, 0.000001);
	ldT->setState(10.0, nullptr, nullptr, cLocalSolverMode);
	val = ld1->getRealPower(1.2);
	BOOST_CHECK_CLOSE(val, 1.2*0.4 , 0.000001);
	ld1->set("dipdt", 0.0);
	ld1->set("ip", 0.0);
	//test Iq ramp
	ld1->pFlowInitializeA(timeZero, 0);
	ldT->set("iq", 1.0);
	ldT->set("diqdt",0.05);
	val = ld1->getReactivePower(1.0);
	BOOST_CHECK_CLOSE(val, 1.0, 0.000001);
	ldT->setState(1.0, nullptr, nullptr, cLocalSolverMode);
	val = ld1->getReactivePower(1.2);
	BOOST_CHECK_CLOSE(val, 1.2*1.05, 0.000001);
	ld1->set("diqdt", 0.0);
	ld1->set("iq", 0.0);

	//final check
	val = ld1->getReactivePower(1.2);
	BOOST_CHECK_SMALL(val, 0.00000001);
	val = ld1->getRealPower(1.2);
	BOOST_CHECK_SMALL(val, 0.000001);

}


BOOST_AUTO_TEST_CASE(random_load_test)
{
	ld1 = new sourceLoad(sourceLoad::sourceType::random);
	sourceLoad *ldT = static_cast<sourceLoad *>(ld1);
	BOOST_REQUIRE(ldT != nullptr);
	//test P random
	ld1->set("p:trigger_dist", "constant");
	ldT->set("p:mean_t",5.0);
	ld1->set("p:size_dist", "constant");
	ldT->set("p:mean_l" ,0.3);
	ld1->set("p", 0.5);
	ld1->pFlowInitializeA(timeZero, 0);
	double val = ld1->getRealPower(1.0);
	BOOST_CHECK_CLOSE(val, 0.5, 0.000001);
	ld1->timestep(5.0,noInputs,cLocalSolverMode);
	val = ld1->getRealPower(1.1);
	BOOST_CHECK_CLOSE(val, 0.8, 0.000001);
	//check the uniform distribution
	ldT->reset();
	ld1->set("p:trigger_dist", "uniform");
	ldT->set("p:min_t",2.0);
	ldT->set("p:max_t", 5.0);
	ld1->pFlowInitializeA(6.0,0);
	auto src = ld1->find("p");
	BOOST_REQUIRE(src != nullptr);
	coreTime otime = src->getNextUpdateTime();
	BOOST_CHECK_GE(otime, 8.0);
	BOOST_CHECK_LE(otime,11.0);
	ld1->timestep(otime-0.2, noInputs, cLocalSolverMode);
	val = ld1->getRealPower(1.1);
	BOOST_CHECK_CLOSE(val, 0.8, 0.000001);
	ld1->timestep(otime + 0.2, noInputs, cLocalSolverMode);
	val = ld1->getRealPower(1.1);
	BOOST_CHECK_CLOSE(val, 1.1, 0.000001);
	//check to make sure they are not the same number
	ld1->pFlowInitializeA(6.0, 0);
	double ntime = src->getNextUpdateTime();
	BOOST_CHECK_NE(otime,ntime);
	//check if the set seed works
	ldT->set("p:seed",0);
	ld1->pFlowInitializeA(6.0, 0);
	ntime = src->getNextUpdateTime();
	ldT->set("p:seed", 0);
	ld1->pFlowInitializeA(6.0, 0);
	otime = src->getNextUpdateTime();
	BOOST_CHECK_EQUAL(otime,ntime);
	
	
}

BOOST_AUTO_TEST_CASE(random_load_test2)
{
	ld1 = new sourceLoad(sourceLoad::sourceType::random);
	sourceLoad *ldT = static_cast<sourceLoad *>(ld1);
	BOOST_REQUIRE(ldT != nullptr);
	double val;
	//test P ramp
	ld1->set("p:trigger_dist", "constant");
	ldT->set("p:mean_t",5.0);
	ld1->set("p:size_dist", "constant");
	ldT->set("p:mean_l",0.5);
	ld1->set("p", 0.5);
	ldT->setFlag("p:interpolate");
	ld1->pFlowInitializeA(timeZero,0);
	val = ld1->getRealPower(1.0);
	BOOST_CHECK_CLOSE(val, 0.5, 0.000001);
	//check interpolate
	ldT->setState(2.0,nullptr,nullptr,cLocalSolverMode);
	val = ld1->getRealPower(1.0);
	BOOST_CHECK_CLOSE(val, 0.7, 0.000001);
	//check uniform load change
	ldT->reset();
	ldT->setFlag("p:interpolate",false);
	ld1->set("p:size_dist", "uniform");
	ldT->set("p:min_l", 0.2);
	ldT->set("p:max_l",0.5);
	ldT->set("p:seed", "");
	ld1->set("p", 0.5);
	ld1->pFlowInitializeA(6.0,0);
	ld1->timestep(12.0, noInputs, cLocalSolverMode);
	val = ld1->getRealPower(1.0);
	BOOST_CHECK_GE(val, 0.7);
	BOOST_CHECK_LE(val,1.0);
	

}

BOOST_AUTO_TEST_CASE(pulse_load_test2)
{
	ld1 = new sourceLoad(sourceLoad::sourceType::pulse);
	sourceLoad *ldT = static_cast<sourceLoad *>(ld1);
	BOOST_REQUIRE(ldT != nullptr);

	//test P ramp
	ld1->set("p:type", "square");
	ldT->set("p:amplitude", 1.3);
	ld1->set("p:period", 5);
	ld1->pFlowInitializeA(timeZero,0);
	double val = ld1->getRealPower(1.0);
	BOOST_CHECK_CLOSE(val, 0, 0.000001);
	//check the pulse
	ldT->timestep(1.0, noInputs, cLocalSolverMode);
	val = ld1->getRealPower(1.0);
	BOOST_CHECK_CLOSE(val, 0.0, 0.000001);
	ld1->timestep(2.0, noInputs, cLocalSolverMode);
	val = ld1->getRealPower(1.0);
	BOOST_CHECK_CLOSE(val, 1.3, 0.000001);
	ld1->timestep(4.0, noInputs, cLocalSolverMode);
	val = ldT->getRealPower(1.0);
	BOOST_CHECK_CLOSE(val, 0, 0.000001);
	
}

BOOST_AUTO_TEST_CASE(file_load_test1)
{
	ld1 = new gridFileLoad();
	gridFileLoad *ldT = static_cast<gridFileLoad *>(ld1);
	BOOST_REQUIRE(ldT != nullptr);
	std::string fname = load_test_directory+ "FileLoadInfo.bin";
	//test P ramp
	ld1->set("file", fname);
	ldT->setFlag("step");
	
	ld1->pFlowInitializeA(timeZero,0);
	double val = ld1->getRealPower(1.0);
	BOOST_CHECK_CLOSE(val, 0.5, 0.000001);
	//check interpolate

  IOdata input{ 0,0 };
	ldT->timestep(12.0,input, cPflowSolverMode);
	val = ld1->getRealPower(1.0);
	BOOST_CHECK_CLOSE(val, 0.8, 0.000001);
	delete ld1;

	ld1 = new gridFileLoad();
	ldT = static_cast<gridFileLoad *>(ld1);
	BOOST_REQUIRE(ldT != nullptr);
	
	//test P ramp
	ld1->set("file", fname);
	ldT->set("mode", "interpolate");
	ld1->pFlowInitializeA(timeZero, 0);
	
  ldT->timestep(1.0, input, cPflowSolverMode);
	val = ld1->getRealPower(1.0);
	BOOST_CHECK_CLOSE(val, 0.53, 0.000001);
	//check interpolate
  ldT->timestep(10.0, input, cPflowSolverMode);
	val = ld1->getRealPower(1.0);
	BOOST_CHECK_CLOSE(val, 0.8, 0.000001);

  ldT->timestep(12.0, input, cPflowSolverMode);
	val = ld1->getRealPower(1.0);
	BOOST_CHECK_CLOSE(val, 0.78, 0.000001);


  ldT->timestep(50.0, input, cPflowSolverMode);
	val = ld1->getRealPower(1.0);
	BOOST_CHECK_CLOSE(val, 0.6, 0.000001);
}

BOOST_AUTO_TEST_CASE(gridDynLoad_test1)
{
	std::string fname = gridlabd_test_directory+"IEEE_13_mod.xml";

	auto gds = readSimXMLFile(fname);

	gridBus *bus = gds->getBus(1);
	gridLabDLoad *gld = dynamic_cast<gridLabDLoad *>(bus->getLoad());

	BOOST_REQUIRE(gld != nullptr);
	
	gds->run();
	BOOST_REQUIRE_EQUAL (gds->currentProcessState (),gridDynSimulation::gridState_t::DYNAMIC_COMPLETE);
	
	
}

BOOST_AUTO_TEST_CASE(motor_test1)
{
  std::string fname = load_test_directory+ "motorload_test1.xml";

  auto gds = readSimXMLFile(fname);


  gridBus *bus = gds->getBus(1);
  motorLoad *mtld = dynamic_cast<motorLoad *>(bus->getLoad());
  
  BOOST_REQUIRE(mtld!=nullptr);
 
 gds->dynInitialize();
 BOOST_REQUIRE_EQUAL (gds->currentProcessState (),gridDynSimulation::gridState_t::DYNAMIC_INITIALIZED);
 int mmatch=runResidualCheck(gds,cDaeSolverMode);

 BOOST_REQUIRE_EQUAL(mmatch, 0);
  mmatch=runJacobianCheck(gds,cDaeSolverMode);
  BOOST_REQUIRE_EQUAL(mmatch, 0);
  gds->run();
  BOOST_REQUIRE_EQUAL (gds->currentProcessState (), gridDynSimulation::gridState_t::DYNAMIC_COMPLETE);
 
}

BOOST_AUTO_TEST_CASE(motor_test3)
{
  std::string fname = load_test_directory+"motorload_test3.xml";

  auto gds = readSimXMLFile(fname);


  gridBus *bus = gds->getBus(1);
  motorLoad3 *mtld = dynamic_cast<motorLoad3 *>(bus->getLoad());

  BOOST_REQUIRE(mtld != nullptr);
  gds->pFlowInitialize();
  
  runJacobianCheck(gds,cPflowSolverMode);
 
  gds->dynInitialize();
  runResidualCheck(gds,cDaeSolverMode);

  
  runJacobianCheck(gds,cDaeSolverMode,1e-8);
  
  BOOST_REQUIRE_EQUAL (gds->currentProcessState (), gridDynSimulation::gridState_t::DYNAMIC_INITIALIZED);
  gds->run();
  BOOST_REQUIRE_EQUAL (gds->currentProcessState (), gridDynSimulation::gridState_t::DYNAMIC_COMPLETE);

}

/** test case runs a 3rd order motor load to stall and unstall conditions*/
BOOST_AUTO_TEST_CASE(motor_test3_stall)
{
  std::string fname = load_test_directory+"motorload_test3_stall.xml";

  auto gds = readSimXMLFile(fname);


  gridBus *bus = gds->getBus(1);
  motorLoad3 *mtld = dynamic_cast<motorLoad3 *>(bus->getLoad());

  BOOST_REQUIRE(mtld != nullptr);
  gds->pFlowInitialize();

  runJacobianCheck(gds,cPflowSolverMode);
 
  gds->dynInitialize();
  runResidualCheck(gds,cDaeSolverMode);

 runJacobianCheck(gds,cDaeSolverMode, 1e-8);
  
  BOOST_REQUIRE_EQUAL (gds->currentProcessState (), gridDynSimulation::gridState_t::DYNAMIC_INITIALIZED);
  gds->run(2.5);
  BOOST_REQUIRE_EQUAL (gds->currentProcessState (), gridDynSimulation::gridState_t::DYNAMIC_COMPLETE);
  BOOST_CHECK(mtld->checkFlag(motorLoad::stalled));
  gds->run();
  BOOST_CHECK(!mtld->checkFlag(motorLoad::stalled));

}


#ifdef ENABLE_IN_DEVELOPMENT_CASES
#ifdef ENABLE_EXPERIMENTAL_TEST_CASES
BOOST_AUTO_TEST_CASE(motor_test5)
{
  std::string fname = std::string(LOAD_TEST_DIRECTORY "motorload_test5.xml");
  readerConfig::setPrintMode(0);
  auto gds = readSimXMLFile(fname);


  gridBus *bus = gds->getBus(1);
  motorLoad5 *mtld = dynamic_cast<motorLoad5 *>(bus->getLoad());

  BOOST_REQUIRE(mtld != nullptr);
  gds->pFlowInitialize();
  BOOST_REQUIRE_EQUAL (gds->currentProcessState (), gridDynSimulation::gridState_t::INITIALIZED);
  runJacobianCheck(gds,cPflowSolverMode);
 
  gds->dynInitialize();
  runResidualCheck(gds,cDaeSolverMode);
 
  runJacobianCheck(gds,cDaeSolverMode);
 
  gds->run();
  BOOST_REQUIRE_EQUAL (gds->currentProcessState (), gridDynSimulation::gridState_t::DYNAMIC_COMPLETE);
 
}

#endif
#endif

BOOST_AUTO_TEST_CASE(fdep_test)
{
  std::string fname = load_test_directory+"fdepLoad.xml";
  readerConfig::setPrintMode(1);
  auto gds = readSimXMLFile(fname);


  gridBus *bus = gds->getBus(2);
  fDepLoad *mtld = dynamic_cast<fDepLoad *>(bus->getLoad());

  BOOST_REQUIRE(mtld != nullptr);
  gds->pFlowInitialize();
  runJacobianCheck(gds,cPflowSolverMode);
  
  gds->dynInitialize();
  runResidualCheck(gds,cDaeSolverMode);
  
  runJacobianCheck(gds,cDaeSolverMode);
  
  
  gds->run();
  BOOST_REQUIRE_EQUAL (gds->currentProcessState (), gridDynSimulation::gridState_t::DYNAMIC_COMPLETE);

}

BOOST_AUTO_TEST_CASE(simple_3phase_load_test)
{
	auto ld3 = std::make_unique<gridLoad3Phase>();
	ld3->setPa(1.1);
	ld3->setPb(1.2);
	ld3->setPc(1.3);

	auto P = ld3->getRealPower();
	BOOST_CHECK_CLOSE(P, 3.6, 0.00001);

	ld3->setQa(0.1);
	ld3->setQb(0.3);
	ld3->setQc(0.62);

	auto Q = ld3->getReactivePower();
	BOOST_CHECK_CLOSE(Q, 1.02, 0.00001);

	ld3->setLoad(3.0);
	BOOST_CHECK_CLOSE(ld3->get("pa"), 1.0, 0.0000001);
	

	ld3->set("pb", 0.5);
	ld3->set("pc", 0.4);

	P = ld3->getRealPower();
	BOOST_CHECK_CLOSE(P, 1.9, 0.00001);

	auto res = ld3->getRealPower3Phase();
	BOOST_CHECK_EQUAL(res.size(), 3u);
	BOOST_CHECK_CLOSE(res[0], 1.0, 0.0000001);
	BOOST_CHECK_CLOSE(res[1], 0.5, 0.0000001);
	BOOST_CHECK_CLOSE(res[2], 0.4, 0.0000001);

	auto res2 = ld3->getRealPower3Phase(phase_type_t::pnz);
	BOOST_CHECK_EQUAL(res2.size(), 3u);
	BOOST_CHECK_CLOSE(res2[0], 1.9, 0.00001);

	ld3->setLoad(2.7,0.3);
	res = ld3->getRealPower3Phase();
	BOOST_CHECK_CLOSE(res[0], 0.9, 0.0000001);
	BOOST_CHECK_CLOSE(res[1], 0.9, 0.0000001);
	BOOST_CHECK_CLOSE(res[2], 0.9, 0.0000001);

	res = ld3->getRealPower3Phase(phase_type_t::pnz);
	BOOST_CHECK_CLOSE(res[0], 2.7, 0.0000001);
	BOOST_CHECK_SMALL(res[1], 0.0000001);
	BOOST_CHECK_SMALL(res[2], 0.0000001);

	res = ld3->getReactivePower3Phase(phase_type_t::pnz);
	BOOST_CHECK_CLOSE(res[0], 0.3, 0.0000001);
	BOOST_CHECK_SMALL(res[1], 0.0000001);
	BOOST_CHECK_SMALL(res[2], 0.0000001);
}


BOOST_AUTO_TEST_CASE(secondary_3phase_load_test)
{
	auto ld3 = std::make_unique<gridLoad3Phase>();

	ld3->setLoad(5.0, 1.0);
	
	
	ld3->set("imaga", 3.0);
	auto Pa = ld3->get("pa");
	ld3->set("imaga", 6.0);
	auto Pa2 = ld3->get("pa");
	BOOST_CHECK_CLOSE(Pa*2.0, Pa2, 0.00001);


}
BOOST_AUTO_TEST_SUITE_END()
