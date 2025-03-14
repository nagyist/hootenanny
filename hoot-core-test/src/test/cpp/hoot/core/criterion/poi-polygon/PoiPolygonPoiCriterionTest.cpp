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
#include <hoot/core/criterion/poi-polygon/PoiPolygonPoiCriterion.h>

using namespace geos::geom;

namespace hoot
{

class PoiPolygonPoiCriterionTest : public HootTestFixture
{
  CPPUNIT_TEST_SUITE(PoiPolygonPoiCriterionTest);
  CPPUNIT_TEST(runBasicTest);
  CPPUNIT_TEST_SUITE_END();

public:

  void runBasicTest()
  {
    PoiPolygonPoiCriterion uut;

    //nodes only
    WayPtr way1 = std::make_shared<Way>(Status::Unknown1, -1, 15.0);
    CPPUNIT_ASSERT(!uut.isSatisfied(way1));

    //type specifically excluded outside of schema
    NodePtr node1 = std::make_shared<Node>(Status::Unknown1, -1, Coordinate(0.0, 0.0), 15.0);
    node1->getTags().set("amenity", "drinking_water");
    CPPUNIT_ASSERT(!uut.isSatisfied(node1));

    //has name
    NodePtr node2 = std::make_shared<Node>(Status::Unknown1, -1, Coordinate(0.0, 0.0), 15.0);
    node2->getTags().set("name", "blah");
    CPPUNIT_ASSERT(uut.isSatisfied(node2));

    //is poi or building type
    NodePtr node3 = std::make_shared<Node>(Status::Unknown1, -1, Coordinate(0.0, 0.0), 15.0);
    node3->getTags().set("amenity", "school");
    CPPUNIT_ASSERT(uut.isSatisfied(node3));

    //is not poi or building type
    NodePtr node4 = std::make_shared<Node>(Status::Unknown1, -1, Coordinate(0.0, 0.0), 15.0);
    node4->getTags().set("email", "blah");
    CPPUNIT_ASSERT(!uut.isSatisfied(node4));
  }
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(PoiPolygonPoiCriterionTest, "quick");

}
