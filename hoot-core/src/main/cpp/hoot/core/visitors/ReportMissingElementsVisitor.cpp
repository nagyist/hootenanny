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
 * @copyright Copyright (C) 2015-2023 Maxar (http://www.maxar.com/)
 */
#include "ReportMissingElementsVisitor.h"

// hoot
#include <hoot/core/elements/OsmMap.h>
#include <hoot/core/elements/Relation.h>
#include <hoot/core/util/Factory.h>

using namespace std;

namespace hoot
{

HOOT_FACTORY_REGISTER(ElementVisitor, ReportMissingElementsVisitor)

ReportMissingElementsVisitor::ReportMissingElementsVisitor(const bool removeMissing, const Log::WarningLevel& logLevel,
                                                           const int maxReport)
  : _logLevel(logLevel),
    _maxReport(maxReport),
    _missingCount(0),
    _removeMissing(removeMissing),
    _markWaysForReview(false),
    _numWaysMarkedForReview(0),
    _markRelationsForReview(false),
    _numRelationsMarkedForReview(0),
    _numWaysTagged(0),
    _numRelationsTagged(0)
{
}

void ReportMissingElementsVisitor::setConfiguration(const Settings& conf)
{
  setMaxReport(ConfigOptions(conf).getLogWarnMessageLimit());
}

void ReportMissingElementsVisitor::_reportMissing(ElementId referer, ElementId missing)
{
  QString msg;
  if (_removeMissing)
    msg = QString("Removing missing %1 in %2.").arg(missing.toString(), referer.toString());
  else
    msg = QString("Missing %1 in %2.").arg(missing.toString(), referer.toString());

  if (_missingCount < _maxReport)
  {
    LOG_LEVEL(_logLevel, msg);
  }
  else
  {
    LOG_TRACE(msg);
  }
  _missingCount++;
  if (_missingCount == _maxReport)
  {
    LOG_LEVEL(_logLevel, "Reached maximum number of missing element reports (" << _maxReport << "). No longer reporting.");
  }
}

void ReportMissingElementsVisitor::visit(const ConstElementPtr& e)
{    
  if (_removeMissing)
    _visitAndRemove(e->getElementType(), e->getId());
  else
    _visitAndReport(e->getElementType(), e->getId());
}

void ReportMissingElementsVisitor::_updateWay(const WayPtr& way, const QStringList& missingChildIds)
{
  if (!missingChildIds.empty())
  {
    if (_markWaysForReview && !ReviewMarker::isNeedsReview(_map, way))
    {
      _reviewMarker.mark(
        _map, way,
        way->getElementId().toString() + ", name: " + way->getTags().getName() +
          "; Missing way node(s): " + missingChildIds.join(","), getName(), 1.0);
      _numWaysMarkedForReview++;
    }

    if (!_wayKvp.trimmed().isEmpty())
    {
      way->getTags().appendValue(_wayKvp);
      _numWaysTagged++;
    }
  }
}

void ReportMissingElementsVisitor::_updateRelation(const RelationPtr& relation, const QStringList& missingChildIds)
{
  if (!missingChildIds.empty())
  {
    if (_markRelationsForReview && !ReviewMarker::isNeedsReview(_map, relation))
    {
      _reviewMarker.mark(
        _map, relation,
        relation->getElementId().toString() + ", name: " + relation->getTags().getName() +
          ", type: " + relation->getType() +
          ", Missing relation member(s): " + missingChildIds.join(","), getName(), 1.0);
       _numRelationsMarkedForReview++;
       LOG_TRACE("Marked " << relation->getElementId() << " for review.");
    }

    if (!_relationKvp.trimmed().isEmpty())
    {
      relation->getTags().appendValue(_relationKvp);
      _numRelationsTagged++;
    }
  }
}

void ReportMissingElementsVisitor::_visitAndReport(ElementType type, long id)
{ 
  QStringList missingChildIds;

  if (type == ElementType::Way)
  {
    const WayPtr& way = _map->getWay(id);
    for (auto node_id : way->getNodeIds())
    {
      if (_map->containsNode(node_id) == false)
      {
        const ElementId missingChildId = ElementId::node(node_id);
        _reportMissing(ElementId(type, id), missingChildId);
        missingChildIds.append(missingChildId.toString());
      }
    }

    _updateWay(way, missingChildIds);
  }
  else if (type == ElementType::Relation)
  {
    const RelationPtr& relation = _map->getRelation(id);
    for (const auto& e : relation->getMembers())
    {
      if (_map->containsElement(e.getElementId()) == false)
      {
        const ElementId missingChildId = e.getElementId();
        _reportMissing(ElementId(type, id), missingChildId);
        missingChildIds.append(missingChildId.toString());
      }
    }

    _updateRelation(relation, missingChildIds);
  }
}

void ReportMissingElementsVisitor::_visitAndRemove(ElementType type, long id)
{
  QStringList missingChildIds;

  if (type == ElementType::Way)
  {
    const WayPtr& way = _map->getWay(id);
    vector<long> newNids;
    newNids.reserve(way->getNodeCount());
    for (auto node_id : way->getNodeIds())
    {
      if (_map->containsNode(node_id) == false)
      {
        const ElementId missingChildId = ElementId::node(node_id);
        _reportMissing(ElementId(type, id), missingChildId);
        missingChildIds.append(missingChildId.toString());
      }
      else
        newNids.push_back(node_id);
    }
    if (newNids.size() != way->getNodeCount())
    {
      LOG_TRACE("Way nodes size before: " << way->getNodeCount());
      way->setNodes(newNids);
      LOG_TRACE("Way nodes size after: " << way->getNodeCount());
    } 

    _updateWay(way, missingChildIds);
  }
  else if (type == ElementType::Relation)
  {
    const RelationPtr& relation = _map->getRelation(id);
    vector<RelationData::Entry> newEntries;
    newEntries.reserve(relation->getMemberCount());
    for (const auto& member : relation->getMembers())
    {
      if (_map->containsElement(member.getElementId()) == false)
      {
        const ElementId missingChildId = member.getElementId();
        _reportMissing(ElementId(type, id), missingChildId);
        missingChildIds.append(missingChildId.toString());
      }
      else
        newEntries.push_back(member);
    }
    if (newEntries.size() != relation->getMemberCount())
    {
      LOG_TRACE("Relation members size before: " << relation->getMemberCount());
      relation->setMembers(newEntries);
      LOG_TRACE("Relation members size after: " << relation->getMemberCount());
    }

    _updateRelation(relation, missingChildIds);
  }
}

}
