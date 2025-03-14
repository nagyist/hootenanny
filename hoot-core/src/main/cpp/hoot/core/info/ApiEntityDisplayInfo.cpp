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
 * @copyright Copyright (C) 2018, 2019, 2020, 2021, 2022 Maxar (http://www.maxar.com/)
 */

#include "ApiEntityDisplayInfo.h"

// Hoot
#include <hoot/core/algorithms/WayJoiner.h>
#include <hoot/core/algorithms/aggregator/ValueAggregator.h>
#include <hoot/core/algorithms/extractors/FeatureExtractor.h>
#include <hoot/core/algorithms/string/StringDistance.h>
#include <hoot/core/algorithms/subline-matching/SublineMatcher.h>
#include <hoot/core/algorithms/subline-matching/SublineStringMatcher.h>
#include <hoot/core/conflate/SuperfluousConflateOpRemover.h>
#include <hoot/core/conflate/matching/Match.h>
#include <hoot/core/conflate/matching/MatchCreator.h>
#include <hoot/core/conflate/merging/Merger.h>
#include <hoot/core/conflate/merging/MergerCreator.h>
#include <hoot/core/criterion/ConflatableElementCriterion.h>
#include <hoot/core/criterion/ElementCriterion.h>
#include <hoot/core/criterion/ElementCriterionConsumer.h>
#include <hoot/core/info/ApiEntityInfo.h>
#include <hoot/core/info/NumericStatistic.h>
#include <hoot/core/info/SingleStatistic.h>
#include <hoot/core/ops/OsmMapOperation.h>
#include <hoot/core/schema/TagMerger.h>
#include <hoot/core/util/ConfigOptions.h>
#include <hoot/core/util/Factory.h>
#include <hoot/core/validation/Validator.h>
#include <hoot/core/visitors/ElementVisitor.h>

//  Qt
#include <QTextStream>

// std
#include <sstream>

// Boost
#include <boost/property_tree/json_parser.hpp>

namespace hoot
{

const int ApiEntityDisplayInfo::MAX_NAME_SIZE = 48;
const int ApiEntityDisplayInfo::MAX_TYPE_SIZE = 18;

template<typename ApiEntity>
class ApiEntityNameComparator
{
public:

  ApiEntityNameComparator() = default;

  bool operator()(const QString& name1, const QString& name2) const
  {
    return name1 < name2;
  }
};

ApiEntityDisplayInfo::ApiEntityDisplayInfo()
  : _asJson(false)
{
}

QString ApiEntityDisplayInfo::getDisplayInfoOps(const QString& optName) const
{
  if (_asJson)
    throw IllegalArgumentException("JSON format not supported for operations information.");

  LOG_TRACE("getDisplayInfoOps: " << optName);

  const QString errorMsg = "Invalid config option name: " + optName;
  if (!conf().hasKey(optName))
    throw IllegalArgumentException(errorMsg);

  const QStringList listOpt = conf().get(optName).toStringList();
  LOG_VART(listOpt.size());
  if (listOpt.isEmpty())
    throw IllegalArgumentException(errorMsg);

  const QStringList operations = listOpt[0].split(";");

  QString buffer;
  QTextStream ts(&buffer);
  for (auto className : qAsConst(operations))
  {
    LOG_VARD(className);

    // There's a lot of duplicated code in here when compared with printApiEntities.  Haven't
    // figured out a good way to combine the two yet.

    std::shared_ptr<ApiEntityInfo> apiEntityInfo;
    const QString apiEntityType = _apiEntityTypeForBaseClass(className);
    std::shared_ptr<SingleStatistic> singleStat;
    // :-( this is messy...
    if (Factory::getInstance().hasBase<OsmMapOperation>(className))
    {
      std::shared_ptr<OsmMapOperation> apiEntity = Factory::getInstance().constructObject<OsmMapOperation>(className);
      apiEntityInfo = std::dynamic_pointer_cast<ApiEntityInfo>(apiEntity);
      singleStat = std::dynamic_pointer_cast<SingleStatistic>(apiEntity);
    }
    else if (Factory::getInstance().hasBase<ElementVisitor>(className))
    {
      std::shared_ptr<ElementVisitor> apiEntity = Factory::getInstance().constructObject<ElementVisitor>(className);
      apiEntityInfo = std::dynamic_pointer_cast<ApiEntityInfo>(apiEntity);
      singleStat = std::dynamic_pointer_cast<SingleStatistic>(apiEntity);
    }

    if (!apiEntityInfo.get())
      throw HootException("Calls to getDisplayInfoOps must return a list of classes, all that implement ApiEntityInfo.");
    const bool supportsSingleStat = singleStat.get();

    QString name = className.remove(MetadataTags::HootNamespacePrefix());
    //append '*' to the names of visitors that support the SingleStatistic interface
    if (supportsSingleStat)
      name += "*";
    const int indentAfterName = MAX_NAME_SIZE - name.size();
    const int indentAfterType = MAX_TYPE_SIZE - apiEntityType.size();
    QString line = "  " + name + QString(indentAfterName, ' ');
    line += apiEntityType + QString(indentAfterType, ' ');
    line += apiEntityInfo->getDescription();
    ts << line << endl;
  }
  return ts.readAll();
}

QString ApiEntityDisplayInfo::getDisplayInfo(const QString& apiEntityType) const
{
  // The log must be disabled for this to display things correctly. Disable it for debugging only.
  DisableLog dl;
  QString msg = "";
  QString buffer;
  QTextStream ts(&buffer);

  if (apiEntityType == "operators")
  {
    if (_asJson)
      throw IllegalArgumentException("JSON format not supported for complete operators list information.");

    msg += "; * = implements SingleStatistic, ** = NumericStatistic):";
    msg.prepend("Operators");
    ts << msg << endl;
    ts << _getApiEntities<ElementCriterion, ElementCriterion>(ElementCriterion::className(), "criterion", true, MAX_NAME_SIZE);
    ts << _getApiEntities<OsmMapOperation, OsmMapOperation>(OsmMapOperation::className(), "operation", true, MAX_NAME_SIZE);
    ts << _getApiEntities<ElementVisitor, ElementVisitor>(ElementVisitor::className(), "visitor", true, MAX_NAME_SIZE);
  }
  // All of this from here on down is pretty repetitive :-( Maybe we can make it cleaner.
  else if (apiEntityType == "filters")
  {
    // This is the criterion portion of --operators only.
    if (!_asJson)
    {
      msg += ":";
      msg.prepend("Filters");
      ts << msg << endl;
    }
    ts << _getApiEntities<ElementCriterion, ElementCriterion>(ElementCriterion::className(), "criterion", true, MAX_NAME_SIZE);
  }
  else if (apiEntityType == "feature-extractors")
  {
    if (!_asJson)
    {
      msg += ":";
      msg.prepend("Feature Extractors");
      ts << msg << endl;
    }
    ts << _getApiEntities<FeatureExtractor, FeatureExtractor>(FeatureExtractor::className(), "feature extractor", false, MAX_NAME_SIZE);
  }
  else if (apiEntityType == "matchers")
  {
    if (!_asJson)
    {
      msg += ":";
      msg.prepend("Matchers");
      ts << msg << endl;
    }
    ts << _getApiEntities<Match, Match>(Match::className(), "matcher", false, MAX_NAME_SIZE);
  }
  else if (apiEntityType == "mergers")
  {
    if (!_asJson)
    {
      msg += ":";
      msg.prepend("Mergers");
      ts << msg << endl;
    }
    ts << _getApiEntities<Merger, Merger>(Merger::className(), "merger", false, MAX_NAME_SIZE);
  }
  else if (apiEntityType == "match-creators")
  {
    msg += ":";
    msg.prepend("Conflate Match Creators");
    ts << msg << endl;
    ts << _getApiEntitiesForMatchMergerCreators<MatchCreator>(MatchCreator::className());
  }
  else if (apiEntityType == "merger-creators")
  {
    msg += ":";
    msg.prepend("Conflate Merger Creators");
    ts << msg << endl;
    ts << _getApiEntitiesForMatchMergerCreators<MergerCreator>(MergerCreator::className());
  }
  else if (apiEntityType == "tag-mergers")
  {
    // TODO
    if (!_asJson)
    {
      msg += ":";
      msg.prepend("Tag Mergers");
      ts << msg << endl;
    }
    ts << _getApiEntities<TagMerger, TagMerger>(TagMerger::className(), "tag merger", false, MAX_NAME_SIZE - 10);
  }
  else if (apiEntityType == "string-comparators")
  {
    if (!_asJson)
    {
      msg += ":";
      msg.prepend("String Comparators");
      ts << msg << endl;
    }
    ts << _getApiEntities<StringDistance, StringDistance>(StringDistance::className(), "string comparator", false, MAX_NAME_SIZE - 15);
  }
  else if (apiEntityType == "subline-matchers")
  {
    if (!_asJson)
    {
      msg += ":";
      msg.prepend("Subline Matchers");
      ts << msg << endl;
    }
    ts << _getApiEntities<SublineMatcher, SublineMatcher>(SublineMatcher::className(), "subline matcher", false, MAX_NAME_SIZE - 15);
  }
  else if (apiEntityType == "subline-string-matchers")
  {
    if (!_asJson)
    {
      msg += ":";
      msg.prepend("Subline Matchers");
      ts << msg << endl;
    }
    ts << _getApiEntities<SublineStringMatcher, SublineStringMatcher>(SublineStringMatcher::className(), "subline string matcher", false, MAX_NAME_SIZE - 15);
  }
  else if (apiEntityType == "value-aggregators")
  {
    if (!_asJson)
    {
      msg += ":";
      msg.prepend("Value Aggregators");
      ts << msg << endl;
    }
    ts << _getApiEntities<ValueAggregator, ValueAggregator>(ValueAggregator::className(), "value aggregator", false, MAX_NAME_SIZE - 10);
  }
  else if (apiEntityType == "way-joiners")
  {
    if (!_asJson)
    {
      msg += ":";
      msg.prepend("Way Joiners");
      ts << msg << endl;
    }
    ts << _getApiEntities<WayJoiner, WayJoiner>(WayJoiner::className(), "way joiner", false, MAX_NAME_SIZE - 10);
  }
  else if (apiEntityType == "way-snap-criteria")
  {
    // For this one we're just print a list of class names, rather than the descriptions as well, as
    // that's all that's needed right now.
    ts << _getWaySnapCriteria() << endl;
  }
  else if (apiEntityType == "conflatable-criteria")
  {
    if (!_asJson)
    {
      msg += ":";
      msg.prepend("Conflatable Criteria");
      ts << msg << endl;
    }
    ts << _getApiEntities<ElementCriterion, ConflatableElementCriterion>(ElementCriterion::className(), "conflatable criteria", false, MAX_NAME_SIZE - 10);
  }
  else if (apiEntityType == "criterion-consumers")
  {
    if (_asJson)
      throw IllegalArgumentException("JSON format not supported for criterion consumers.");

    msg += ":";
    msg.prepend("Criterion Consumers");
    ts << msg << endl;
    ts << _getApiEntities<OsmMapOperation, ElementCriterionConsumer>(OsmMapOperation::className(), "criterion consumer", false, MAX_NAME_SIZE - 10);
    ts << _getApiEntities<ElementVisitor, ElementCriterionConsumer>(ElementVisitor::className(), "criterion consumer", false, MAX_NAME_SIZE - 10);
    ts << _getApiEntities<ElementCriterion, ElementCriterionConsumer>(ElementCriterion::className(), "criterion consumer", false, MAX_NAME_SIZE - 10);
  }
  else if (apiEntityType == "geometry-type-criteria")
  {
    if (!_asJson)
    {
      msg += ":";
      msg.prepend("Geometry Type Criteria");
      ts << msg << endl;
    }
    ts << _getApiEntities<ElementCriterion, GeometryTypeCriterion>(ElementCriterion::className(), "geometry type criteria", false, MAX_NAME_SIZE - 10);
  }
  else if (apiEntityType == "validators")
  {
    // We know that all validators must be map ops b/c validators need to look at entire maps.
    if (!_asJson)
    {
      msg += ":";
      msg.prepend("Validators");
      ts << msg << endl;
    }
    ts << _getApiEntities<OsmMapOperation, Validator>(OsmMapOperation::className(), "validator", false, MAX_NAME_SIZE - 10);
  }
  return ts.readAll();
}

template<typename ApiEntity, typename ApiEntityChild>
QString ApiEntityDisplayInfo::_getApiEntities(const QString& apiEntityBaseClassName, const QString& apiEntityType,
                                              const bool displayType, const int maxNameSize) const
{
  LOG_VARD(apiEntityBaseClassName);
  std::vector<QString> classNames = Factory::getInstance().getObjectNamesByBase(apiEntityBaseClassName);
  LOG_VARD(classNames);
  ApiEntityNameComparator<ApiEntity> apiEntityNameComparator;
  std::sort(classNames.begin(), classNames.end(), apiEntityNameComparator);
  QString buffer;
  QTextStream ts(&buffer);
  boost::property_tree::ptree jsonChildren;

  for (auto className : qAsConst(classNames))
  {
    LOG_VARD(className);

    std::shared_ptr<ApiEntity> apiEntity = Factory::getInstance().constructObject<ApiEntity>(className);
    std::shared_ptr<ApiEntityInfo> apiEntityInfo = std::dynamic_pointer_cast<ApiEntityInfo>(apiEntity);
    if (!apiEntityInfo.get())
      throw IllegalArgumentException("Calls to printApiEntities must be made with classes that implement ApiEntityInfo.");

    if (ApiEntity::className() != ApiEntityChild::className())
    {
      std::shared_ptr<ApiEntityChild> child = std::dynamic_pointer_cast<ApiEntityChild>(apiEntity);
      if (!child)
        continue;
    }

    LOG_VARD(apiEntityInfo->getDescription());
    if (!apiEntityInfo->getDescription().isEmpty())
    {
      QString lineBuffer;
      QTextStream line(&lineBuffer);
      QString name = className.remove(MetadataTags::HootNamespacePrefix());
      const QString description = apiEntityInfo->getDescription();
      if (!_asJson)
      {
        bool supportsSingleStat = false;
        std::shared_ptr<SingleStatistic> singleStat = std::dynamic_pointer_cast<SingleStatistic>(apiEntity);
        if (singleStat.get())
          supportsSingleStat = true;

        bool supportsNumericStat = false;
        std::shared_ptr<NumericStatistic> numericStat = std::dynamic_pointer_cast<NumericStatistic>(apiEntity);
        if (numericStat.get())
          supportsNumericStat = true;

        // append '*' to the names of visitors that support the SingleStatistic interface
        if (supportsNumericStat)
          name += "**";
        else if (supportsSingleStat)
          name += "*";
        const int indentAfterName = maxNameSize - name.size();
        const int indentAfterType = MAX_TYPE_SIZE - apiEntityType.size();
        line << "  " << name << QString(indentAfterName, ' ');
        if (displayType)
          line << apiEntityType << QString(indentAfterType, ' ');
        line << description;

        ts << line.readAll() << endl;
      }
      else
      {
        boost::property_tree::ptree jsonChild;
        jsonChild.put("name", name.toStdString());
        jsonChild.put("description", description.toStdString());
        jsonChildren.push_back(std::make_pair("", jsonChild));
      }
    }
  }
  if (!_asJson)
    return ts.readAll();
  else
  {
    boost::property_tree::ptree json;
    json.add_child(apiEntityBaseClassName.toStdString(), jsonChildren);

    std::stringstream strStrm;
    boost::property_tree::json_parser::write_json(strStrm, json);
    return QString::fromStdString(strStrm.str());
  }
}

// match/merger creators have a more roundabout way to get at the description, so we'll create a new
// display method for them
template<typename ApiEntity>
QString ApiEntityDisplayInfo::_getApiEntitiesForMatchMergerCreators(const QString& apiEntityClassName) const
{
  if (_asJson)
    throw IllegalArgumentException("JSON format not supported for match/merger creators.");

  // the size of the longest names plus a 3 space buffer
  const int maxNameSize = 48;

  std::vector<QString> names = Factory::getInstance().getObjectNamesByBase(apiEntityClassName);
  ApiEntityNameComparator<ApiEntity> apiEntityNameComparator;
  std::sort(names.begin(), names.end(), apiEntityNameComparator);
  LOG_VARD(names);
  QStringList output;
  for (const auto& obj_name : names)
  {
    // get all names known by this creator
    std::shared_ptr<ApiEntity> mc = Factory::getInstance().constructObject<ApiEntity>(obj_name);
    std::vector<CreatorDescription> creators = mc->getAllCreators();
    LOG_VARD(creators.size());

    for (const auto& description : creators)
    {
      LOG_VARD(description);
      const QString name = description.getClassName().remove(MetadataTags::HootNamespacePrefix());
      LOG_VARD(name);
      //this suppresses test and auxiliary rules files
      if (!name.endsWith("Test.js") && !name.endsWith("Rules.js"))
      {
        const int indentAfterName = maxNameSize - name.size();
        QString line = "  " + name + QString(indentAfterName, ' ');
        line += description.getDescription();
        if (description.getExperimental())
          line += " (experimental)";
        LOG_VARD(line);
        output.append(line);
      }
    }
  }
  output.sort();

  QString buffer;
  QTextStream ts(&buffer);
  for (const auto& out : qAsConst(output))
    ts << out << endl;

  return ts.readAll();
}

QString ApiEntityDisplayInfo::_apiEntityTypeForBaseClass(const QString& className) const
{
  LOG_VARD(className);
  if (className == OsmMapOperation::className() || Factory::getInstance().hasBase<OsmMapOperation>(className))
    return "operation";
  else if (className == ElementVisitor::className() || Factory::getInstance().hasBase<ElementVisitor>(className))
    return "visitor";
  return "";
}

QString ApiEntityDisplayInfo::_getWaySnapCriteria() const
{
  if (_asJson)
    throw IllegalArgumentException("JSON format not supported for way snap criteria.");

  QStringList filteredCritClassNames;
  const QStringList linearCritClassNames = ConflatableElementCriterion::getCriterionClassNamesByGeometryType(GeometryTypeCriterion::GeometryType::Line);
  LOG_VARD(linearCritClassNames);
  const QSet<QString> matchCreatorCritClassNames = SuperfluousConflateOpRemover::getMatchCreatorGeometryTypeCrits();
  LOG_VARD(matchCreatorCritClassNames);
  for (const auto& name : qAsConst(matchCreatorCritClassNames))
  {
    if (linearCritClassNames.contains(name))
      filteredCritClassNames.append(name);
  }
  qSort(filteredCritClassNames);
  return filteredCritClassNames.join(";");
}

}
