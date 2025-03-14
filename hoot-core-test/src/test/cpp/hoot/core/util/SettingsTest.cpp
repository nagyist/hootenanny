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
 * @copyright Copyright (C) 2012, 2013, 2015, 2016, 2018, 2019, 2020, 2021, 2022, 2023 Maxar (http://www.maxar.com/)
 */

// Hoot
#include <hoot/core/TestUtils.h>
#include <hoot/core/criterion/BuildingCriterion.h>
#include <hoot/core/elements/Node.h>
#include <hoot/core/ops/ReplaceElementOp.h>
#include <hoot/core/util/Log.h>
#include <hoot/core/util/Settings.h>
#include <hoot/core/visitors/RemoveElementsVisitor.h>

namespace hoot
{

class SettingsTest : public HootTestFixture
{
  CPPUNIT_TEST_SUITE(SettingsTest);
  CPPUNIT_TEST(envTest);
  CPPUNIT_TEST(replaceTest);
  CPPUNIT_TEST(storeTest);
  CPPUNIT_TEST(baseSettingsTest);
  CPPUNIT_TEST(invalidOptionNameTest);
  CPPUNIT_TEST(invalidOperatorsTest);
  CPPUNIT_TEST(prependTest);
  CPPUNIT_TEST_SUITE_END();

public:

  SettingsTest()
    : HootTestFixture(UNUSED_PATH, "test-output/utils/")
  {
    setResetType(ResetConfigs);
  }

  void envTest()
  {
    Settings uut;
    uut.loadEnvironment();
    char* path = getenv("PATH");
    HOOT_STR_EQUALS(QString(path), uut.getString("PATH"));

    uut.set("mypath", "my path: ${PATH}");
    HOOT_STR_EQUALS(QString("my path: ") + path, uut.getString("mypath"));
  }

  void replaceTest()
  {
    Settings uut;
    uut.loadDefaults();
    uut.set("perty.csm.D", "2");
    uut.set("perty.test.num.runs", 2);
    uut.set("map.factory.writer", "1");
    uut.set("map.factory.reader", "${perty.csm.D}");
    HOOT_STR_EQUALS(QString("2"), uut.getString("map.factory.reader"));

    uut.set("map.factory.reader", "${perty.csm.D} ${map.factory.writer}");
    HOOT_STR_EQUALS(QString("2 1"), uut.getString("map.factory.reader"));

    uut.set("map.factory.reader", "${map.factory.writer} ${map.factory.writer}");
    HOOT_STR_EQUALS(QString("1 1"), uut.getString("map.factory.reader"));

    uut.set("perty.csm.D", "${doesnt.exist}");
    HOOT_STR_EQUALS(QString(""), uut.getString("perty.csm.D"));

    HOOT_STR_EQUALS("", uut.getValue("${doesnt.exist}"));
    HOOT_STR_EQUALS("1", uut.getValue("${map.factory.writer}"));
    HOOT_STR_EQUALS(1, uut.getDoubleValue("${map.factory.writer}"));
    HOOT_STR_EQUALS(2, uut.getDoubleValue("${perty.test.num.runs}"));
  }

  void storeTest()
  {
    Settings uut;
    uut.loadDefaults();
    uut.set("perty.csm.D", "2");
    uut.set("map.factory.writer", "1");
    uut.set("map.factory.reader", "${perty.csm.D}");

    uut.storeJson(_outputPath + "SettingsTest.json");

    Settings uut2;
    uut2.loadDefaults();
    uut2.loadJson(_outputPath + "SettingsTest.json");
    HOOT_STR_EQUALS(uut.getString("perty.csm.D"), "2");
    HOOT_STR_EQUALS(uut.getString("map.factory.writer"), "1");
    HOOT_STR_EQUALS(uut.getString("map.factory.reader"), "2");

    Settings uut3;
    uut3.loadDefaults();
    uut3.loadFromString(uut.toString());
    HOOT_STR_EQUALS(uut.getString("perty.csm.D"), "2");
    HOOT_STR_EQUALS(uut.getString("map.factory.writer"), "1");
    HOOT_STR_EQUALS(uut.getString("map.factory.reader"), "2");
  }

  void baseSettingsTest()
  {
    Settings uut;
    uut.loadDefaults();

    //  The following
    //  Default value before change in JSON
    CPPUNIT_ASSERT_EQUAL(false, uut.getBool("uuid.helper.repeatable"));
    //  Default value before change in AttributeConflation.conf
    HOOT_STR_EQUALS("OverwriteTag2Merger", uut.getString("tag.merger.default"));
    HOOT_STR_EQUALS("LinearSnapMerger", uut.getString("geometry.linear.merger.default"));
    //  Default value before change in NetworkAlgorithm.conf
    HOOT_STR_EQUALS("HighwayRfClassifier", uut.getString("conflate.match.highway.classifier"));
    HOOT_STR_EQUALS("MaximalNearestSublineMatcher", uut.getString("way.subline.matcher"));

    uut.loadFromString("{ \"base.config\": \"AttributeConflation.conf,NetworkAlgorithm.conf\", \"uuid.helper.repeatable\": \"true\" }");
    //  From the JSON
    CPPUNIT_ASSERT_EQUAL(true, uut.getBool("uuid.helper.repeatable"));
    //  From AttributeConflation.conf
    HOOT_STR_EQUALS("OverwriteTag1Merger", uut.getString("tag.merger.default"));
    HOOT_STR_EQUALS("LinearTagOnlyMerger", uut.getString("geometry.linear.merger.default"));
    //  From NetworkAlgorithm.conf
    HOOT_STR_EQUALS("HighwayExpertClassifier", uut.getString("conflate.match.highway.classifier"));
    HOOT_STR_EQUALS("MaximalSublineMatcher", uut.getString("highway.subline.matcher"));
  }

  void invalidOptionNameTest()
  {
    QString log;
    QStringList args;
    args.append("-D");
    args.append("blah=true");
    //  Capture the log output in a nested scope
    {
      CaptureLog capture;
      Settings::parseCommonArguments(args);
      log = capture.getLogsStripped();
    }
    //  Compare the captured log output
    HOOT_STR_EQUALS("Skipping unknown settings option: (blah)", log);
  }

  void invalidOperatorsTest()
  {
    QStringList args;
    QString exceptionMsg;
    QString expectedErrorMessage;

    args.append("-D");
    args.append(
      ConfigOptions::getConvertOpsKey() + "=" +
      ReplaceElementOp::className() + ";" +
      RemoveElementsVisitor::className() + ";" +
      BuildingCriterion::className() + ";" +
      // fails; only ops, vis, and crits are valid
      Node::className());
    exceptionMsg = "";
    try
    {
      Settings::parseCommonArguments(args);
    }
    catch (const HootException& e)
    {
      exceptionMsg = e.what();
    }
    expectedErrorMessage = "Invalid option operator class name: " + Node::className();
    CPPUNIT_ASSERT_EQUAL(expectedErrorMessage.toStdString(), exceptionMsg.toStdString());

    args.clear();
    args.append("-D");
    args.append(
      ConfigOptions::getConvertOpsKey() + "=" +
      ReplaceElementOp::className() + ";" +
      RemoveElementsVisitor::className() + ";" +
      "blah");
    exceptionMsg = "";
    try
    {
      Settings::parseCommonArguments(args);
    }
    catch (const HootException& e)
    {
      exceptionMsg = e.what();
    }
    expectedErrorMessage = "Invalid option operator class name: blah";
    CPPUNIT_ASSERT_EQUAL(expectedErrorMessage.toStdString(), exceptionMsg.toStdString());
  }

  void prependTest()
  {
    Settings uut;
    uut.loadDefaults();
    //  Check the original value
    HOOT_STR_EQUALS(QString("error:circular;accuracy"), uut.getString("circular.error.tag.keys"));
    //  Prepend a few keys and check the new value
    uut.prepend("circular.error.tag.keys", QStringList({"first", "second", "third"}));
    HOOT_STR_EQUALS(QString("first;second;third;error:circular;accuracy"), uut.getString("circular.error.tag.keys"));
  }
};

CPPUNIT_TEST_SUITE_NAMED_REGISTRATION(SettingsTest, "quick");

}

