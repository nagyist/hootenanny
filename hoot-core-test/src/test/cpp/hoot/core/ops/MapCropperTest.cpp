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
 * @copyright Copyright (C) 2013, 2014, 2015, 2016, 2017, 2018, 2019, 2020, 2021, 2022 Maxar (http://www.maxar.com/)
 */

// Hoot
#include <hoot/core/TestUtils.h>
#include <hoot/core/criterion/ElementIdCriterion.h>
#include <hoot/core/elements/MapProjector.h>
#include <hoot/core/elements/OsmMap.h>
#include <hoot/core/geometry/ElementToGeometryConverter.h>
#include <hoot/core/geometry/GeometryUtils.h>
#include <hoot/core/io/OsmMapReaderFactory.h>
#include <hoot/core/io/OsmMapWriterFactory.h>
#include <hoot/core/io/OsmXmlReader.h>
#include <hoot/core/ops/MapCropper.h>
#include <hoot/core/util/Settings.h>

// geos
#include <geos/geom/Point.h>
#include <geos/geom/Polygon.h>
#include <geos/io/WKTReader.h>

// TGS
#include <tgs/Statistics/Random.h>

using namespace geos::geom;
using namespace std;
using namespace Tgs;

namespace hoot
{

class MapCropperTest : public HootTestFixture
{
  CPPUNIT_TEST_SUITE(MapCropperTest);
  CPPUNIT_TEST(runGeometryTest);
  CPPUNIT_TEST(runConfigurationTest);
  CPPUNIT_TEST(runMultiPolygonTest);
  CPPUNIT_TEST(runKeepFeaturesOnlyCompletelyInBoundsTest);
  CPPUNIT_TEST(runDontSplitCrossingFeaturesTest);
  CPPUNIT_TEST(runInvertTest);
  CPPUNIT_TEST(runInclusionTest);
  CPPUNIT_TEST_SUITE_END();

public:

  MapCropperTest()
    : HootTestFixture("test-files/ops/MapCropper", "test-output/ops/MapCropper")
  {
  }

  OsmMapPtr genPoints(int seed)
  {
    Tgs::Random::instance()->seed(seed);
    OsmMapPtr result = std::make_shared<OsmMap>();

    for (int i = 0; i < 1000; i++)
    {
      double x = Random::instance()->generateUniform() * 360 - 180;
      double y = Random::instance()->generateUniform() * 180 - 90;

      result->addNode(std::make_shared<Node>(Status::Invalid, result->createNextNodeId(), x, y, 10));
    }

    return result;
  }

  void runGeometryTest()
  {
    OsmMapPtr map = genPoints(0);

    std::shared_ptr<Geometry> g(geos::io::WKTReader().read("POLYGON ((-50 0, 0 50, 50 0, 0 -50, 0 0, -50 0))"));

    int insideCount = 0;
    const NodeMap& nm = map->getNodes();
    for (auto it = nm.begin(); it != nm.end(); ++it)
    {
      Coordinate c = it->second->toCoordinate();
      std::shared_ptr<Point> p(GeometryFactory::getDefaultInstance()->createPoint(c));
      if (g->intersects(p.get()))
        insideCount++;
    }

    {
      MapCropper uut;
      uut.setBounds(g);
      uut.setInvert(false);
      uut.setRemoveSuperflousFeatures(false);
      uut.apply(map);

      CPPUNIT_ASSERT_EQUAL(insideCount, (int)map->getNodeCount());
    }

    {
      OsmMapPtr map2 = genPoints(0);

      MapCropper uut;
      uut.setBounds(g);
      uut.setInvert(true);
      uut.setRemoveSuperflousFeatures(false);
      uut.apply(map2);
      CPPUNIT_ASSERT_EQUAL(1000 - insideCount, (int)map2->getNodeCount());
    }
  }

  void runConfigurationTest()
  {
    MapCropper cropper;
    Settings settings = conf();

    settings.set(ConfigOptions::getCropBoundsKey(), "12.462,41.891,12.477,41.898");
    cropper.setConfiguration(settings);
    HOOT_STR_EQUALS("Env[12.462:12.477,41.891:41.898]", cropper._bounds->getEnvelopeInternal()->toString());

    settings.clear();
    settings.set(ConfigOptions::getCropBoundsKey(), "-12.462,41.891,12.477,41.898");
    cropper.setConfiguration(settings);
    HOOT_STR_EQUALS("Env[-12.462:12.477,41.891:41.898]", cropper._bounds->getEnvelopeInternal()->toString());

    settings.clear();
    settings.set(ConfigOptions::getCropBoundsKey(), "12,41.891,13,41.898");
    cropper.setConfiguration(settings);
    HOOT_STR_EQUALS("Env[12:13,41.891:41.898]", cropper._bounds->getEnvelopeInternal()->toString());

    settings.clear();
    settings.set(ConfigOptions::getCropBoundsKey(), "12,41.891,13.,42");
    cropper.setConfiguration(settings);
    HOOT_STR_EQUALS("Env[12:13,41.891:42]", cropper._bounds->getEnvelopeInternal()->toString());
  }

  void runMultiPolygonTest()
  {
    OsmMapPtr map = std::make_shared<OsmMap>();
    OsmMapReaderFactory::read(map, _inputPath + "/MultipolygonTest.osm", true);

    Envelope env(0.30127,0.345,0.213,0.28154);

    MapCropper uut;
    uut.setBounds(env);
    uut.apply(map);

    // compare relations
    const RelationMap& relations = map->getRelations();
    HOOT_STR_EQUALS(1, relations.size());
    QString relationStr = "relation(-1592); type: multipolygon; members:"
                          "   Entry: role: outer, eid: Way(-1556);"
                          "   Entry: role: inner, eid: Way(-1552); ;"
                          " tags: landuse = farmland; status: invalid; version: 0; visible: 1; circular error: 15";
    for (auto it = relations.begin(); it != relations.end(); ++it)
      HOOT_STR_EQUALS(relationStr, it->second->toString().replace("\n","; "));

    // compare ways
    int count = 0;
    const WayMap& ways = map->getWays();
    HOOT_STR_EQUALS(2, ways.size());
    for (auto it = ways.begin(); it != ways.end(); ++it)
    {
      const WayPtr& w = it->second;
      std::shared_ptr<Polygon> pl = ElementToGeometryConverter(map).convertToPolygon(w);
      const Envelope& e = *(pl->getEnvelopeInternal());
      double area = pl->getArea();
      if (count == 0)
      {
        HOOT_STR_EQUALS("Env[0.303878:0.336159,0.220255:0.270199]", e.toString());
        CPPUNIT_ASSERT_DOUBLES_EQUAL(0.00150737, area, 0.00001);
      }
      else
      {
        HOOT_STR_EQUALS("Env[0.314996:0.328946,0.231514:0.263127]", e.toString());
        CPPUNIT_ASSERT_DOUBLES_EQUAL(0.000401258, area, 0.00001);
      }
      count++;
    }
  }

  void runKeepFeaturesOnlyCompletelyInBoundsTest()
  {
    const QString testFileNameBase = "runKeepFeaturesOnlyCompletelyInBoundsTest";
    QString testFileName;
    OsmMapPtr map;
    geos::geom::Envelope bounds(-104.9007, -104.8994, 38.8540, 38.8552);
    OsmMapWriterFactory::write(GeometryUtils::createMapFromBounds(bounds), _outputPath + "/" + testFileNameBase + "-bounds.osm");
    MapCropper uut;
    uut.setBounds(bounds);

    // regular crop output
    map = std::make_shared<OsmMap>();
    OsmMapReaderFactory::read(map, "test-files/ToyTestA.osm");
    uut.setInvert(false);
    uut.setKeepEntireFeaturesCrossingBounds(false);
    uut.setKeepOnlyFeaturesInsideBounds(false);
    uut.apply(map);
    MapProjector::projectToWgs84(map);
    testFileName = testFileNameBase + "-1.osm";
    OsmMapWriterFactory::write(map, _outputPath + "/" + testFileName);
    HOOT_FILE_EQUALS(_inputPath + "/" + testFileName, _outputPath + "/" + testFileName);

    // only one way remains since it was the only one completely inside the bounds
    map = std::make_shared<OsmMap>();
    OsmMapReaderFactory::read(map, "test-files/ToyTestA.osm");
    uut.setInvert(false);
    uut.setKeepEntireFeaturesCrossingBounds(false);
    uut.setKeepOnlyFeaturesInsideBounds(true);
    uut.apply(map);
    MapProjector::projectToWgs84(map);
    testFileName = testFileNameBase + "-2.osm";
    OsmMapWriterFactory::write(map, _outputPath + "/" + testFileName);
    HOOT_FILE_EQUALS(_inputPath + "/" + testFileName, _outputPath + "/" + testFileName);

    // illegal configuration
    uut.setInvert(false);
    uut.setKeepEntireFeaturesCrossingBounds(true);
    QString exceptionMsg("");
    try
    {
       uut.setKeepOnlyFeaturesInsideBounds(true);
    }
    catch (const HootException& e)
    {
      exceptionMsg = e.what();
    }
    CPPUNIT_ASSERT(exceptionMsg.contains("Incompatible crop options"));

    // setting invert to true negates the keep only features inside bounds setting;
    // so output looks like regular inverted crop output
    map = std::make_shared<OsmMap>();
    OsmMapReaderFactory::read(map, "test-files/ToyTestA.osm");
    uut.setInvert(true);
    uut.setKeepEntireFeaturesCrossingBounds(false);
    uut.setKeepOnlyFeaturesInsideBounds(true);
    uut.apply(map);
    MapProjector::projectToWgs84(map);
    testFileName = testFileNameBase + "-3.osm";
    OsmMapWriterFactory::write(map, _outputPath + "/" + testFileName);
    HOOT_FILE_EQUALS(_inputPath + "/" + testFileName, _outputPath + "/" + testFileName);
  }

  void runDontSplitCrossingFeaturesTest()
  {
    const QString testFileNameBase = "runDontSplitCrossingFeaturesTest";
    QString testFileName;
    OsmMapPtr map;
    geos::geom::Envelope bounds(-104.9007, -104.8994, 38.8540, 38.8552);
    OsmMapWriterFactory::write(GeometryUtils::createMapFromBounds(bounds), _outputPath + "/" + testFileNameBase + "-bounds.osm");
    MapCropper uut;
    uut.setBounds(bounds);

    // regular crop output
    map = std::make_shared<OsmMap>();
    OsmMapReaderFactory::read(map, "test-files/ToyTestA.osm");
    uut.setInvert(false);
    uut.setKeepOnlyFeaturesInsideBounds(false);
    uut.setKeepEntireFeaturesCrossingBounds(false);
    uut.apply(map);
    MapProjector::projectToWgs84(map);
    testFileName = testFileNameBase + "-1.osm";
    OsmMapWriterFactory::write(map, _outputPath + "/" + testFileName);
    HOOT_FILE_EQUALS(_inputPath + "/" + testFileName, _outputPath + "/" + testFileName);

    // should end up with all features
    map = std::make_shared<OsmMap>();
    OsmMapReaderFactory::read(map, "test-files/ToyTestA.osm");
    uut.setInvert(false);
    uut.setKeepOnlyFeaturesInsideBounds(false);
    uut.setKeepEntireFeaturesCrossingBounds(true);
    uut.apply(map);
    MapProjector::projectToWgs84(map);
    testFileName = testFileNameBase + "-2.osm";
    OsmMapWriterFactory::write(map, _outputPath + "/" + testFileName);
    HOOT_FILE_EQUALS(_inputPath + "/" + testFileName, _outputPath + "/" + testFileName);

    // illegal configuration
    uut.setInvert(false);
    uut.setKeepOnlyFeaturesInsideBounds(true);
    QString exceptionMsg("");
    try
    {
       uut.setKeepEntireFeaturesCrossingBounds(true);
    }
    catch (const HootException& e)
    {
      exceptionMsg = e.what();
    }
    CPPUNIT_ASSERT(exceptionMsg.contains("Incompatible crop options"));

    // setting invert to true negates the keep entire features crossing bounds setting; so output
    // looks like regular inverted crop output
    map = std::make_shared<OsmMap>();
    OsmMapReaderFactory::read(map, "test-files/ToyTestA.osm");
    uut.setInvert(true);
    uut.setKeepOnlyFeaturesInsideBounds(false);
    uut.setKeepEntireFeaturesCrossingBounds(true);
    uut.apply(map);
    MapProjector::projectToWgs84(map);
    testFileName = testFileNameBase + "-4.osm";
    OsmMapWriterFactory::write(map, _outputPath + "/" + testFileName);
    HOOT_FILE_EQUALS(_inputPath + "/" + testFileName, _outputPath + "/" + testFileName);
  }

  void runInvertTest()
  {
    const QString testFileNameBase = "runInvertTest";
    QString testFileName;
    OsmMapPtr map;
    geos::geom::Envelope bounds(-104.9007, -104.8994, 38.8540, 38.8552);
    OsmMapWriterFactory::write(GeometryUtils::createMapFromBounds(bounds), _outputPath + "/" + testFileNameBase + "-bounds.osm");
    MapCropper uut;
    uut.setBounds(bounds);

    // should end up with everything inside of the bounds
    map = std::make_shared<OsmMap>();
    OsmMapReaderFactory::read(map, "test-files/ToyTestA.osm");
    uut.setInvert(false);
    uut.setKeepOnlyFeaturesInsideBounds(false);
    uut.setKeepEntireFeaturesCrossingBounds(false);
    uut.apply(map);
    MapProjector::projectToWgs84(map);
    testFileName = testFileNameBase + "-1.osm";
    OsmMapWriterFactory::write(map, _outputPath + "/" + testFileName);
    HOOT_FILE_EQUALS(_inputPath + "/" + testFileName, _outputPath + "/" + testFileName);

    // should end up with everything outside of the bounds
    map = std::make_shared<OsmMap>();
    OsmMapReaderFactory::read(map, "test-files/ToyTestA.osm");
    uut.setInvert(true);
    uut.setKeepOnlyFeaturesInsideBounds(false);
    uut.setKeepEntireFeaturesCrossingBounds(false);
    uut.apply(map);
    MapProjector::projectToWgs84(map);
    testFileName = testFileNameBase + "-2.osm";
    OsmMapWriterFactory::write(map, _outputPath + "/" + testFileName);
    HOOT_FILE_EQUALS(_inputPath + "/" + testFileName, _outputPath + "/" + testFileName);
  }

  void runInclusionTest()
  {
    const QString testFileNameBase = "runInclusionTest";
    QString testFileName;
    OsmMapPtr map;
    geos::geom::Envelope bounds(38.91362, 38.915478, 15.37365, 15.37506);
    OsmMapWriterFactory::write(GeometryUtils::createMapFromBounds(bounds), _outputPath + "/" + testFileNameBase + "-bounds.osm");

    MapCropper uut;
    uut.setBounds(bounds);
    map = std::make_shared<OsmMap>();
    OsmMapReaderFactory::read(map, "test-files/ops/ImmediatelyConnectedOutOfBoundsWayTagger/in.osm", true);
    uut.setInvert(false);
    uut.setKeepEntireFeaturesCrossingBounds(false);
    uut.setKeepOnlyFeaturesInsideBounds(false);
    // Exclude one way outside of the bounds from being cropped out of the map. The whole way and
    // its nodes should be retained.
    uut.setInclusionCriterion(std::make_shared<ElementIdCriterion>(ElementId(ElementType::Way, 1687)));
    uut.apply(map);

    MapProjector::projectToWgs84(map);
    testFileName = testFileNameBase + ".osm";
    OsmMapWriterFactory::write(map, _outputPath + "/" + testFileName, false, true);
    HOOT_FILE_EQUALS(_inputPath + "/" + testFileName, _outputPath + "/" + testFileName);
  }
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(MapCropperTest, "quick");

}
