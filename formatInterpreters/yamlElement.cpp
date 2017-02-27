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

#include "yamlElement.h"

static const std::string nullStr = std::string("");



yamlElement::yamlElement(const YAML::Node &vElement, std::string newName) : name(newName),element(vElement)
{
	elementIndex = 0;

	if (element.IsSequence())
	{
		arraytype = true;
		arrayIndex = 0;
		while ((arrayIndex < element.size()) && (element[arrayIndex].IsNull()))
		{
			++arrayIndex;
		}
	}
}

yamlElement::yamlElement(const yamlElement &ye) = default;
yamlElement::yamlElement(yamlElement &&ye) = default;


yamlElement & yamlElement::operator=(const yamlElement& ye) = default;


yamlElement &yamlElement::operator=(yamlElement &&ye) = default;

void yamlElement::clear()
{
	element.reset();
	elementIndex = 0;
	arrayIndex = 0;
	arraytype = false;
	name = nullStr;
}


const YAML::Node yamlElement::getElement() const
{
	return (arraytype) ? (element[arrayIndex]) : element;
}
