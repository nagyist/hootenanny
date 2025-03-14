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
 * @copyright Copyright (C) 2019, 2020, 2021, 2022 Maxar (http://www.maxar.com/)
 */

// Hoot
#include <hoot/core/TestUtils.h>
#include <hoot/core/io/OsmMapReaderFactory.h>
#include <hoot/core/io/OsmMapWriterFactory.h>
#include <hoot/josm/ops/JosmMapCleaner.h>

namespace hoot
{

/**
 * @brief The JosmMapCleanerTest class tests JosmMapCleaner.
 */
class JosmMapCleanerTest : public HootTestFixture
{
  CPPUNIT_TEST_SUITE(JosmMapCleanerTest);
  CPPUNIT_TEST(runCleanNoErrorsTest);
  CPPUNIT_TEST(runCleanTest);
  CPPUNIT_TEST(runCleanNoDetailTagsTest);
  CPPUNIT_TEST(runCleanFileTest);
  CPPUNIT_TEST_SUITE_END();

public:

  JosmMapCleanerTest()
    : HootTestFixture("test-files/ops/JosmMapCleanerTest", "test-output/ops/JosmMapCleanerTest")
  {
  }

  void runCleanNoErrorsTest()
  {
    const QString testName = "runCleanNoErrorsTest";
    OsmMapPtr map = std::make_shared<OsmMap>();
    OsmMapReaderFactory::read(map, "test-files/ToyTestA.osm");
    LOG_VARD(map->size());

    JosmMapCleaner uut;
    uut.setAddDetailTags(true);
    uut.setJosmValidators(QStringList("UntaggedWay"));
    LOG_INFO(uut.getInitStatusMessage());
    uut.apply(map);
    LOG_INFO(uut.getCompletedStatusMessage());

    CPPUNIT_ASSERT_EQUAL(40, uut.getNumElementsProcessed());
    CPPUNIT_ASSERT_EQUAL(0, uut.getNumValidationErrors());
    CPPUNIT_ASSERT_EQUAL(0, uut.getNumElementsCleaned());
    CPPUNIT_ASSERT_EQUAL(0, uut.getNumElementsDeleted());
    CPPUNIT_ASSERT_EQUAL(0, uut.getNumFailingValidators());
    CPPUNIT_ASSERT_EQUAL(0, uut.getNumFailedCleaningOperations());
    HOOT_STR_EQUALS(
      "Found 0 validation errors in 40 features with JOSM.\n"
      "Total elements cleaned: 0\n"
      "Total elements deleted: 0\n"
      "Total failing JOSM validators: 0\n"
      "Total failing JOSM cleaning operations: 0",
      uut.getSummary());

    const QString outTestFileName = testName + "-out.osm";
    OsmMapWriterFactory::write(map, _outputPath + "/" + outTestFileName, false, false);
    HOOT_FILE_EQUALS(_inputPath + "/" + outTestFileName, _outputPath + "/" + outTestFileName);
  }

  void runCleanTest()
  {
    const QString testName = "runCleanTest";
    OsmMapPtr map = std::make_shared<OsmMap>();
    OsmMapReaderFactory::read(map, _inputPath + "/" + testName + "-in.osm");
    LOG_VARD(map->size());

    JosmMapCleaner uut;
    uut.setAddDetailTags(true);
    uut.setMaxElementsForMapString(INT_MAX);
    QStringList validators;
    validators.append("UntaggedWay");   // triggers "One node way"
    validators.append("UnclosedWays");
    validators.append("DuplicatedWayNodes");
    uut.setJosmValidators(validators);
    uut.apply(map);

    CPPUNIT_ASSERT_EQUAL(45, uut.getNumElementsProcessed());
    CPPUNIT_ASSERT_EQUAL(4, uut.getNumValidationErrors());
    CPPUNIT_ASSERT_EQUAL(1, uut.getNumElementsCleaned());
    CPPUNIT_ASSERT_EQUAL(0, uut.getNumElementsDeleted());
    CPPUNIT_ASSERT_EQUAL(0, uut.getNumFailedCleaningOperations());
    HOOT_STR_EQUALS(
      "Found 4 validation errors in 45 features with JOSM.\n"
      "Total elements cleaned: 1\n"
      "Total elements deleted: 0\n"
      "Total failing JOSM validators: 0\n"
      "Total failing JOSM cleaning operations: 0\n"
      "Duplicated way nodes errors:               1            elements cleaned: 1\n"
      "Unclosed Ways errors:                      2            elements cleaned: 0\n"
      "Untagged, empty and one node ways errors:  1            elements cleaned: 0",
      uut.getSummary());

    const QString outTestFileName = testName + "-out.osm";
    OsmMapWriterFactory::write(map, _outputPath + "/" + outTestFileName, false, false);
    HOOT_FILE_EQUALS(_inputPath + "/" + outTestFileName, _outputPath + "/" + outTestFileName);
  }

  void runCleanNoDetailTagsTest()
  {
    const QString testName = "runCleanNoDetailTagsTest";
    OsmMapPtr map = std::make_shared<OsmMap>();
    OsmMapReaderFactory::read(map, _inputPath + "/runCleanTest-in.osm");
    LOG_VARD(map->size());

    JosmMapCleaner uut;
    uut.setAddDetailTags(false);
    uut.setMaxElementsForMapString(INT_MAX);
    QStringList validators;
    validators.append("UntaggedWay");   // triggers "One node way"
    validators.append("UnclosedWays");
    validators.append("DuplicatedWayNodes");
    uut.setJosmValidators(validators);
    uut.apply(map);

    CPPUNIT_ASSERT_EQUAL(45, uut.getNumElementsProcessed());
    CPPUNIT_ASSERT_EQUAL(4, uut.getNumValidationErrors());
    CPPUNIT_ASSERT_EQUAL(1, uut.getNumElementsCleaned());
    CPPUNIT_ASSERT_EQUAL(0, uut.getNumElementsDeleted());
    CPPUNIT_ASSERT_EQUAL(0, uut.getNumFailedCleaningOperations());
    HOOT_STR_EQUALS(
      "Found 4 validation errors in 45 features with JOSM.\n"
      "Total elements cleaned: 1\n"
      "Total elements deleted: 0\n"
      "Total failing JOSM validators: 0\n"
      "Total failing JOSM cleaning operations: 0\n"
      "Duplicated way nodes errors:               1            elements cleaned: 1\n"
      "Unclosed Ways errors:                      2            elements cleaned: 0\n"
      "Untagged, empty and one node ways errors:  1            elements cleaned: 0",
      uut.getSummary());

    const QString outTestFileName = testName + "-out.osm";
    OsmMapWriterFactory::write(map, _outputPath + "/" + outTestFileName, false, false);
    HOOT_FILE_EQUALS(_inputPath + "/" + outTestFileName, _outputPath + "/" + outTestFileName);
  }

  void runCleanFileTest()
  {
    const QString testName = "runCleanFileTest";
    OsmMapPtr map = std::make_shared<OsmMap>();
    OsmMapReaderFactory::read(map, _inputPath + "/runCleanTest-in.osm");
    LOG_VARD(map->size());

    JosmMapCleaner uut;
    uut.setAddDetailTags(true);
    uut.setMaxElementsForMapString(1); // force temp map file I/O
    QStringList validators;
    validators.append("UntaggedWay");   // triggers "One node way"
    validators.append("UnclosedWays");
    validators.append("DuplicatedWayNodes");
    uut.setJosmValidators(validators);
    uut.apply(map);

    CPPUNIT_ASSERT_EQUAL(45, uut.getNumElementsProcessed());
    CPPUNIT_ASSERT_EQUAL(4, uut.getNumValidationErrors());
    CPPUNIT_ASSERT_EQUAL(1, uut.getNumElementsCleaned());
    CPPUNIT_ASSERT_EQUAL(0, uut.getNumElementsDeleted());
    CPPUNIT_ASSERT_EQUAL(0, uut.getNumFailedCleaningOperations());
    HOOT_STR_EQUALS(
      "Found 4 validation errors in 45 features with JOSM.\n"
      "Total elements cleaned: 1\n"
      "Total elements deleted: 0\n"
      "Total failing JOSM validators: 0\n"
      "Total failing JOSM cleaning operations: 0\n"
      "Duplicated way nodes errors:               1            elements cleaned: 1\n"
      "Unclosed Ways errors:                      2            elements cleaned: 0\n"
      "Untagged, empty and one node ways errors:  1            elements cleaned: 0",
      uut.getSummary());

    const QString outTestFileName = testName + "-out.osm";
    OsmMapWriterFactory::write(map, _outputPath + "/" + outTestFileName, false, false);
    HOOT_FILE_EQUALS(_inputPath + "/runCleanTest-out.osm", _outputPath + "/" + outTestFileName);
  }
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(JosmMapCleanerTest, "slow");

}
