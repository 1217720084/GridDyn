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

//test case for gridCoreObject object

#include "testHelper.h"
#include "gridDynTypes.h"
#include "readerInfo.h"

#include <boost/test/unit_test.hpp>
#include <boost/test/floating_point_comparison.hpp>


static const std::string xmlTestDirectory(GRIDDYN_TEST_DIRECTORY "/xml_tests/");

BOOST_AUTO_TEST_SUITE(readerInfo_tests)

BOOST_AUTO_TEST_CASE(readerInfo_test_defines)
{
	readerInfo r1;
	r1.addDefinition("bob", "rt");
	r1.addDefinition("bb2", "rt2");
	auto text1 = r1.checkDefines("bob");
	BOOST_CHECK(text1 == "rt");
	auto text2 = r1.checkDefines("bb2");
	BOOST_CHECK(text2 == "rt2");
	//test the substring replacement
	auto text3 = r1.checkDefines("$bob$_and_$bb2$");
	BOOST_CHECK(text3 == "rt_and_rt2");
}

BOOST_AUTO_TEST_CASE(readerInfo_test_iterated_define)
{
	readerInfo r1;
	r1.addDefinition("bob", "rt");
	r1.addDefinition("rt", "rtb");
	auto text1 = r1.checkDefines("bob");
	BOOST_CHECK(text1 == "rtb");
	
	r1.addDefinition("rtbrtb", "rtb^2");
	auto text2 = r1.checkDefines("$rt$$rt$");
	BOOST_CHECK(text2 == "rtb^2");
}

BOOST_AUTO_TEST_CASE(readerInfo_test_definition_scope)
{
	readerInfo r1;
	r1.addDefinition("bob", "rt");
	r1.addDefinition("bb2", "rt2");
	auto p = r1.newScope();
	r1.addDefinition("bob2", "rt3");
	auto text1 = r1.checkDefines("bob2");
	BOOST_CHECK(text1 == "rt3");
	r1.closeScope(p);
	text1 = r1.checkDefines("bob2");
	BOOST_CHECK(text1 == "bob2");
	//check overwritten definitions in different scopes
	r1.addDefinition("scope", "scope0");
	r1.newScope();
	r1.addDefinition("scope", "scope1");
	r1.newScope();
	r1.addDefinition("scope", "scope2");
	text1 = r1.checkDefines("scope");
	BOOST_CHECK(text1 == "scope2");
	r1.closeScope();
	text1 = r1.checkDefines("scope");
	BOOST_CHECK(text1 == "scope1");
	r1.closeScope();
	text1 = r1.checkDefines("scope");
	BOOST_CHECK(text1 == "scope0");
	r1.closeScope();
	text1 = r1.checkDefines("scope");
	BOOST_CHECK(text1 == "scope0");
	
}

BOOST_AUTO_TEST_CASE(readerInfo_test_directories)
{
	readerInfo r1;
	r1.addDirectory(xmlTestDirectory);

	std::string test1 = "test_xmltest1.xml";

	auto res = r1.checkFileParam(test1, false);

	BOOST_CHECK(res);
	BOOST_CHECK(test1 == xmlTestDirectory + "test_xmltest1.xml");
	
	readerInfo r2;
	r2.addDirectory(GRIDDYN_TEST_DIRECTORY);
	std::string testfile = "location_testFile.txt";

	res = r2.checkFileParam(testfile, false);
	BOOST_CHECK((testfile == (std::string(GRIDDYN_TEST_DIRECTORY) + "\\location_testFile.txt") )||
	(testfile == (std::string(GRIDDYN_TEST_DIRECTORY) + "/location_testFile.txt")));
	r2.newScope();
	r2.addDirectory(xmlTestDirectory);
	//this file is in 2 locations to ensure the recent directory takes precedence
	testfile = "location_testFile.txt";

	res = r2.checkFileParam(testfile, false);
	BOOST_CHECK(testfile == (xmlTestDirectory + "location_testFile.txt"));
	r2.closeScope();
	testfile = "location_testFile.txt";

	res = r2.checkFileParam(testfile, false);
	BOOST_CHECK((testfile == (std::string(GRIDDYN_TEST_DIRECTORY) + "\\location_testFile.txt")) ||
	(testfile == (std::string(GRIDDYN_TEST_DIRECTORY) + "/location_testFile.txt")));
	//now check if we close and extra scope;
	r2.closeScope();
	testfile = "location_testFile.txt";

	res = r2.checkFileParam(testfile, false);
	BOOST_CHECK((testfile == (std::string(GRIDDYN_TEST_DIRECTORY) + "\\location_testFile.txt") )||
	(testfile == (std::string(GRIDDYN_TEST_DIRECTORY) + "/location_testFile.txt")));
}

BOOST_AUTO_TEST_SUITE_END()
