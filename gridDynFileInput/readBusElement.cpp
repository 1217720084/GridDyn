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

#include "readElement.h"
#include "readerElement.h"
#include "readerHelper.h"
#include "gridDynFileInput.h"
#include "elementReaderTemplates.hpp"

#include "gridArea.h"
#include "gridBus.h"
#include "stringOps.h"
#include "objectInterpreter.h"

#include "objectFactory.h"
#include <cstdio>

using namespace readerConfig;

static const IgnoreListType busIgnore {
  "area"
};
static const std::string busComponentName = "bus";
// "aP" is the XML element passed from the reader
gridBus * readBusElement (std::shared_ptr<readerElement> &element, readerInfo *ri, gridCoreObject *searchObject)
{

  gridParameter param;
  auto riScope = ri->newScope ();

  //boiler plate code to setup the object from references or new object
  //check for the area field

  gridBus *bus = ElementReaderSetup (element, (gridBus *)nullptr, busComponentName, ri, searchObject);

  std::string valType = getElementField (element, "type", defMatchType);
  if (!valType.empty ())
    {
      valType = ri->checkDefines (valType);
      auto cloc = valType.find_first_of (',');
      if (cloc != std::string::npos)
        {
          std::string A = valType.substr (0, cloc);
          std::string B = valType.substr (cloc + 1);
          trimString (A);
          trimString (B);
          int ret = bus->set ("type", A);
          if (ret != PARAMETER_FOUND)
            {
              WARNPRINT (READER_WARN_IMPORTANT, "Bus type parameter not found " << A);
            }
          ret = bus->set ("type", B);
          if (ret != PARAMETER_FOUND)
            {
              WARNPRINT (READER_WARN_IMPORTANT, "Bus type parameter not found " << B);
            }
        }
      else
        {
          int ret = bus->set ("type", valType);
          if (ret != PARAMETER_FOUND)
            {
              if (!(coreObjectFactory::instance ()->isValidType (busComponentName, valType)))
                {
                  WARNPRINT (READER_WARN_IMPORTANT, "Bus type parameter not found " << valType);
                }
            }
        }
    }
  loadElementInformation (bus, element, busComponentName, ri, busIgnore);

  LEVELPRINT (READER_NORMAL_PRINT, "loaded Bus " << bus->getName ());

  ri->closeScope (riScope);
  return bus;
}
