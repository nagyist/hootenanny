/*
 * This file is part of Hootenanny.
 *
 * Hootenanny is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --------------------------------------------------------------------
 *
 * The following copyright notices are generated automatically. If you
 * have a new notice to add, please use the format:
 * " * @copyright Copyright ..."
 * This will properly maintain the copyright information. Maxar
 * copyrights will be updated automatically.
 *
 * @copyright Copyright (C) 2015, 2016, 2017, 2018, 2019, 2021 Maxar (http://www.maxar.com/)
 */

// Hoot
#include <hoot/core/TestUtils.h>
#include <hoot/core/conflate/network/OsmNetworkExtractor.h>
#include <hoot/core/criterion/HighwayCriterion.h>
#include <hoot/core/io/OsmMapReaderFactory.h>

namespace hoot
{

class OsmNetworkExtractorTest : public HootTestFixture
{
  CPPUNIT_TEST_SUITE(OsmNetworkExtractorTest);
  CPPUNIT_TEST(toyTest);
  CPPUNIT_TEST_SUITE_END();

public:

  OsmNetworkExtractorTest() : HootTestFixture("test-files/conflate/network/", UNUSED_PATH)
  {
  }

  /**
   * Extract a toy network and verify that the result is as expected.
   */
  void toyTest()
  {
    OsmMapPtr map = std::make_shared<OsmMap>();
    OsmMapReaderFactory::read(map, _inputPath + "ToyInput.osm");

    OsmNetworkExtractor uut;
    uut.setCriterion(std::make_shared<HighwayCriterion>(map));
    OsmNetworkPtr network = uut.extractNetwork(map);

    // Note 7/23/21: As a result of #4895, now believe that the untagged road belonging to the
    // relation in the input is invalid and shouldn't be picked up by the network extractor.
    HOOT_STR_EQUALS("(0) Node(-169) -- Way(-247) -- (1) Node(-221)\n"
      "(2) Node(-175) -- Way(-245) -- (3) Node(-217)\n"
      "(4) Node(-165) -- Way(-243) -- (0) Node(-169)\n"
      "(5) Node(-229) -- Way(-241) -- (1) Node(-221)\n"
      "(6) Node(-171) -- Way(-239) -- (2) Node(-175)\n"
      "(1) Node(-221) -- Way(-237) -- (6) Node(-171)\n"
      "(0) Node(-169) -- Way(-235) -- (2) Node(-175)",
      network->toString());
  }
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(OsmNetworkExtractorTest, "quick");

}
