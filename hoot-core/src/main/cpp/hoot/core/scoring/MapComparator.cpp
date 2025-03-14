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
 * @copyright Copyright (C) 2015, 2016, 2017, 2018, 2019, 2020, 2021, 2022 Maxar (http://www.maxar.com/)
 */
#include "MapComparator.h"

// hoot
#include <hoot/core/elements/OsmMap.h>
#include <hoot/core/geometry/GeometryUtils.h>
#include <hoot/core/util/ConfigOptions.h>
#include <hoot/core/visitors/CountUniqueReviewsVisitor.h>
#include <hoot/core/visitors/ElementConstOsmMapVisitor.h>

// Standard
#include <iomanip>

#define CHECK(con) \
  if (!(con)) \
  { \
    _matches = false; \
    _errorCount++; \
    if (_errorCount <= _errorLimit) \
      LOG_WARN("Check failed."); \
    if (_errorCount == _errorLimit) \
      LOG_WARN("More than " << _errorLimit << " errors, suppressing errors."); \
    return; \
  }

#define CHECK_MSG(con, msg) \
  if (!(con)) \
  { \
    _matches = false; \
    _errorCount++; \
    if (_errorCount <= _errorLimit) \
      LOG_WARN(msg); \
    if (_errorCount == _errorLimit) \
      LOG_WARN("More than " << _errorLimit << " errors, suppressing errors."); \
    return; \
  }

#define CHECK_DOUBLE(v1, v2, t) \
  if (fabs((v1) - (v2)) > t) \
  { \
    _matches = false; \
    _errorCount++; \
    if (_errorCount <= _errorLimit) \
      LOG_WARN("Check Double failed. " << v1 << " vs. " << v2); \
    if (_errorCount == _errorLimit) \
      LOG_WARN("More than " << _errorLimit << " errors, suppressing errors."); \
    return; \
  }

namespace hoot
{

class CompareVisitor : public ElementConstOsmMapVisitor
{
public:

  /**
   * Defaults to 5cm threshold
   */
  CompareVisitor(std::shared_ptr<OsmMap> refMap, bool ignoreUUID, bool useDateTime,
                 const QStringList& ignoreTagKeys, int errorLimit = 5, Meters threshold = 0.05)
    : _refMap(refMap),
      _threshold(threshold),
      _matches(true),
      _ignoreUUID(ignoreUUID),
      _useDateTime(useDateTime),
      _errorCount(0),
      _errorLimit(errorLimit),
      _ignoreTagKeys(ignoreTagKeys)
  {
  }
  ~CompareVisitor() override = default;

  bool isMatch() const { return _matches; }

  QString getDescription() const override { return ""; }
  QString getName() const override { return ""; }
  QString getClassName() const override { return ""; }

  void visit(const std::shared_ptr<const Element>& e) override
  {
    // e is the test element

    CHECK_MSG(_refMap->containsElement(e->getElementId()), "Did not find element: " << e->getElementId());
    const std::shared_ptr<const Element>& refElement = _refMap->getElement(e->getElementId());
    //  Copy the tags so that they can be modified and compared
    Tags refTags = refElement->getTags();
    Tags testTags = e->getTags();

    if (_ignoreUUID)
    {
      refTags.set("uuid", "None");  // Wipe out the UUID's
      testTags.set("uuid", "None");
    }

    // By default, hoot will usually set these tags when ingesting a file this can cause problems
    // when comparing files during testing, so we have the option to ignore it here.
    if (!_useDateTime)
    {
      refTags.set(MetadataTags::SourceIngestDateTime(), "None");  // Wipe out the ingest datetime
      testTags.set(MetadataTags::SourceIngestDateTime(), "None");

      refTags.set(MetadataTags::SourceDateTime(), "None");  // Wipe out the ingest datetime
      testTags.set(MetadataTags::SourceDateTime(), "None");
    }

    for (const auto& key : qAsConst(_ignoreTagKeys))
    {
      refTags.set(key, "None");  // Wipe out the tag
      testTags.set(key, "None");
    }

    if (refTags != testTags)
    {
      _matches = false;
      _errorCount++;
      if (_errorCount <= _errorLimit)
      {
        LOG_WARN("Tags do not match (ref: " << refElement->getElementId() << ", test: " << e->getElementId() << ":");

        QStringList keys = refTags.keys();
        keys.append(testTags.keys());
        keys.removeDuplicates();
        keys.sort();

        if (_errorCount < _errorLimit)
        {
          for (const auto& k : qAsConst(keys))
          {
            if (refTags[k] != testTags[k])
            {
              LOG_WARN("< " + k + " = " + refTags[k]);
              LOG_WARN("> " + k + " = " + testTags[k]);
            }
          }
        }
      }
      return;
    }
    //CHECK_MSG(
    // refTags == testTags, "Tags do not match: " << refTags.toString() << " vs. " <<
    // testTags.toString());

    CHECK_DOUBLE(refElement->getCircularError(), e->getCircularError(), _threshold);
    CHECK_MSG(refElement->getStatus() == e->getStatus(),
          "Status does not match: " << refElement->getStatusString() << " vs. " <<
          e->getStatusString());
    switch (e->getElementType().getEnum())
    {
    case ElementType::Unknown:
      _matches = false;
      LOG_WARN("Encountered an unexpected element type.");
      break;
    case ElementType::Node:
      compareNode(refElement, e);
      break;
    case ElementType::Way:
      compareWay(refElement, e);
      break;
    case ElementType::Relation:
      compareRelation(refElement, e);
      break;
    default:
      _matches = false;
      LOG_WARN("Encountered an unexpected element type.");
      break;
    }
  }

  void compareNode(const std::shared_ptr<const Element>& refElement, const std::shared_ptr<const Element>& testElement)
  {
    ConstNodePtr refNode = std::dynamic_pointer_cast<const Node>(refElement);
    ConstNodePtr testNode = std::dynamic_pointer_cast<const Node>(testElement);

    if (GeometryUtils::haversine(refNode->toCoordinate(), testNode->toCoordinate()) > _threshold)
    {
      if (_errorCount <= _errorLimit)
      {
        LOG_WARN(
          "refNode: " << std::fixed << std::setprecision(15) << refNode->getX() << ", " <<
          refNode->getY() << "; testNode: " << testNode->getX() << ", " << testNode->getY());
      }
      _matches = false;
      _errorCount++;
    }
  }

  void compareWay(const std::shared_ptr<const Element>& refElement, const std::shared_ptr<const Element>& testElement)
  {
    ConstWayPtr refWay = std::dynamic_pointer_cast<const Way>(refElement);
    ConstWayPtr testWay = std::dynamic_pointer_cast<const Way>(testElement);

    CHECK_MSG(
      refWay->getNodeCount() == testWay->getNodeCount(),
      "Node count does not match. " << refWay->getElementId() << ": " <<
      refWay->getNodeCount() << ", " << testWay->getElementId() << ": " <<
      testWay->getNodeCount());
    for (size_t i = 0; i < refWay->getNodeCount(); ++i)
    {
      CHECK_MSG(refWay->getNodeIds()[i] == testWay->getNodeIds()[i],
        QString("Node IDs don't match. (%1 vs. %2)").arg(hoot::toString(refWay), hoot::toString(testWay)));
    }
  }

  void compareRelation(const std::shared_ptr<const Element>& refElement,
                       const std::shared_ptr<const Element>& testElement)
  {
    ConstRelationPtr refRelation = std::dynamic_pointer_cast<const Relation>(refElement);
    ConstRelationPtr testRelation = std::dynamic_pointer_cast<const Relation>(testElement);

    QString relationStr = QString("%1 vs. %2").arg(hoot::toString(refRelation), hoot::toString(testRelation));

    CHECK_MSG(refRelation->getType() == testRelation->getType(), "Types do not match. " + relationStr);
    CHECK_MSG(refRelation->getMemberCount() == testRelation->getMemberCount(), "Member count does not match. " + relationStr);
    for (size_t i = 0; i < refRelation->getMemberCount(); i++)
    {
      CHECK_MSG(
        refRelation->getMembers()[i].getRole() == testRelation->getMembers()[i].getRole(),
        "Member role does not match. " + relationStr);
      CHECK_MSG(
        refRelation->getMembers()[i].getElementId() == testRelation->getMembers()[i].getElementId(),
        "Member element ID does not match. " + relationStr);
    }
  }

private:

  std::shared_ptr<OsmMap> _refMap;
  Meters _threshold;
  bool _matches;
  bool _ignoreUUID;
  bool _useDateTime;
  int _errorCount;
  int _errorLimit;
  QStringList _ignoreTagKeys;
};

MapComparator::MapComparator()
  : _ignoreUUID(false),
    _useDateTime(false),
    _errorLimit(ConfigOptions().getLogWarnMessageLimit())
{
}

void MapComparator::_printIdDiff(const std::shared_ptr<OsmMap>& map1, const std::shared_ptr<OsmMap>& map2,
                                 const ElementType& elementType, const int limit) const
{
  QSet<long> ids1;
  QSet<long> ids2;
  LOG_VARD(limit);

  switch (elementType.getEnum())
  {
  case ElementType::Node:
    ids1 = map1->getNodeIds();
    ids2 = map2->getNodeIds();
    break;
  case ElementType::Way:
    ids1 = map1->getWayIds();
    ids2 = map2->getWayIds();
    break;
  case ElementType::Relation:
    ids1 = map1->getRelationIds();
    ids2 = map2->getRelationIds();
    break;
  default:
    throw HootException(QString("Unexpected element type: %1").arg(elementType.toString()));
  }

  QSet<long> ids1Copy = ids1;
  const QSet<long> idsIn1AndNotIn2 = ids1Copy.subtract(ids2);
  QSet<long> idsIn1AndNotIn2Limited;
  if (limit < idsIn1AndNotIn2.size())
  {
     int ctr = 0;
     for (auto id : qAsConst(idsIn1AndNotIn2))
     {
       idsIn1AndNotIn2Limited.insert(id);
       ctr++;
       if (ctr == limit)
         break;
     }
  }
  else
    idsIn1AndNotIn2Limited = idsIn1AndNotIn2;

  QSet<long> ids2Copy = ids2;
  const QSet<long> idsIn2AndNotIn1 = ids2Copy.subtract(ids1);
  QSet<long> idsIn2AndNotIn1Limited;
  if (limit < idsIn2AndNotIn1.size())
  {
     int ctr = 0;
     for (auto id : qAsConst(idsIn2AndNotIn1))
     {
       idsIn2AndNotIn1Limited.insert(id);
       ctr++;
       if (ctr == limit)
         break;
     }
  }
  else
    idsIn2AndNotIn1Limited = idsIn2AndNotIn1;

  const bool printFullElements = ConfigOptions().getMapComparatorPrintFullMismatchElementsOnMapSizeDiff();
  if (!idsIn1AndNotIn2Limited.empty())
  {
    LOG_WARN(
      "\t" << elementType.toString() << "s in map 1 and not in map 2 (limit " << limit << "): " <<
      idsIn1AndNotIn2Limited);
    if (printFullElements)
    {
      for (auto id : qAsConst(idsIn1AndNotIn2Limited))
      {
        LOG_WARN(map1->getElement(ElementId(elementType, id)));
      }
    }
  }
  if (!idsIn2AndNotIn1Limited.empty())
  {
    LOG_WARN(
      "\t" << elementType.toString() << "s in map 2 and not in map 1 (limit " << limit << "): " <<
      idsIn2AndNotIn1Limited);
    if (printFullElements)
    {
      for (auto id : qAsConst(idsIn2AndNotIn1Limited))
      {
        LOG_WARN(map2->getElement(ElementId(elementType, id)));
      }
    }
  }
}

bool MapComparator::isMatch(const std::shared_ptr<OsmMap>& refMap,
                            const std::shared_ptr<OsmMap>& testMap) const
{
  bool mismatch = false;
  if (refMap->getNodeCount() != testMap->getNodeCount())
  {
    LOG_WARN(
      "Number of nodes does not match (1: " << refMap->getNodeCount() << "; 2: " <<
      testMap->getNodeCount() << ")");
    // Yes, the two map could have the same number of the same type of elements and they still
    // might not completely match up, but we'll let CompareVisitor educate us on that. This gives
    // us a quick rundown of element ID diffs if count discrepancy is detected.
    _printIdDiff(refMap, testMap, ElementType::Node);
    mismatch = true;
  }
  else if (refMap->getWayCount() != testMap->getWayCount())
  {
    LOG_WARN(
      "Number of ways does not match (1: " << refMap->getWayCount() << "; 2: " <<
      testMap->getWayCount() << ")");
    _printIdDiff(refMap, testMap, ElementType::Way);
    mismatch = true;
  }
  else if (refMap->getRelationCount() != testMap->getRelationCount())
  {
    LOG_WARN(
      "Number of relations does not match (1: " << refMap->getRelationCount() << "; 2: " <<
      testMap->getRelationCount() << ")");
    _printIdDiff(refMap, testMap, ElementType::Relation);
    mismatch = true;
  }

  CountUniqueReviewsVisitor countReviewsVis;
  refMap->visitRo(countReviewsVis);
  const int refReviews = (int)countReviewsVis.getStat();
  countReviewsVis.clear();
  testMap->visitRo(countReviewsVis);
  const int testReviews = (int)countReviewsVis.getStat();
  if (refReviews != testReviews)
  {
    LOG_WARN(
      "Number of reviews does not match (1: " << refReviews << "; 2: " << testReviews << ")");
    mismatch = true;
  }

  if (mismatch)
    return false;

  CompareVisitor compareVis(refMap, _ignoreUUID, _useDateTime, _ignoreTagKeys, _errorLimit);
  testMap->visitRo(compareVis);
  return compareVis.isMatch();
}

}
