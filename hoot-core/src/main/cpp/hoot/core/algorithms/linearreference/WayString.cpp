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
 * @copyright Copyright (C) 2016, 2017, 2018, 2019, 2021, 2022 Maxar (http://www.maxar.com/)
 */
#include "WayString.h"

// hoot
#include <hoot/core/algorithms/linearreference/WayLocation.h>
#include <hoot/core/schema/TagMergerFactory.h>

using namespace std;

namespace hoot
{

int WayString::logWarnCount = 0;

// If the difference is smaller than this, we consider it to be equivalent.
Meters WayString::_epsilon = 1e-9;

Meters WayString::_aggregateCircularError() const
{
  // I considered averaging the circular errors, but I think that could cause undesireable behaviour
  // if it is then used for comparison. This too could cause issues, but I think they'll be a little
  // more acceptable. At some point it would be good to only assign circular errors to nodes, but
  // that is a problem for the distant future.
  Meters result = 0.0;

  for (const auto& subline : qAsConst(_sublines))
    result = max(result, subline.getWay()->getCircularError());

  return result;
}

void WayString::append(const WaySubline& subline)
{
  if (!_sublines.empty())
  {
    if (back().getWay() == subline.getWay() &&
        back().getEnd() != subline.getStart())
    {
      LOG_VART(back());
      LOG_VART(subline);
      throw IllegalArgumentException("All consecutive ways must connect end to start in a "
        "WayString.");
    }
    else
    {
      if (subline.getStart().isExtreme(WayLocation::SLOPPY_EPSILON) == false ||
          back().getEnd().isExtreme(WayLocation::SLOPPY_EPSILON) == false)
      {
        throw IllegalArgumentException("If ways are different they must connect at an extreme "
          "node.");
      }
      if (back().getEnd().getNode(WayLocation::SLOPPY_EPSILON) !=
          subline.getStart().getNode(WayLocation::SLOPPY_EPSILON))
      {
        // TODO: The intent of this class is being violated. So either change this back to an
        // exception as part of the work to be done in #1312, or create a new class for the new
        // behavior.

        // throw IllegalArgumentException("Ways must connect at a node in the WayString.");

        if (logWarnCount < Log::getWarnMessageLimit())
        {
          // Decided to not make this a warning for the time being, since it pops up quite a bit.
          LOG_TRACE("Ways must connect at a node in the WayString.");
          LOG_VART(back());
          LOG_VART(back().getWay());
          LOG_VART(subline);
          LOG_VART(subline.getWay());
          LOG_TRACE("Nodes don't match: "
            << back().getEnd().getNode(WayLocation::SLOPPY_EPSILON)->getElementId()
            << " vs. " << subline.getStart().getNode(WayLocation::SLOPPY_EPSILON)->getElementId());
        }
        else if (logWarnCount == Log::getWarnMessageLimit())
        {
          LOG_TRACE(className() << ": " << Log::LOG_WARN_LIMIT_REACHED_MESSAGE);
        }
        logWarnCount++;
      }
    }
  }
  _sublines.append(subline);
}

WayLocation WayString::calculateLocationFromStart(Meters distance, ElementId preferredEid) const
{
  if (distance <= 0.0)
    return _sublines[0].getStart();

  Meters soFar = 0.0;
  for (int i = 0; i < _sublines.size(); ++i)
  {
    const WaySubline& subline = _sublines[i];
    Meters ls = subline.calculateLength();
    if (distance <= soFar + ls)
    {
      ConstOsmMapPtr map = subline.getStart().getMap();
      ConstWayPtr w = subline.getStart().getWay();
      Meters offset = subline.getStart().calculateDistanceOnWay();
      if (subline.isBackwards())
        offset -= distance - soFar;
      else
        offset += distance - soFar;
      // if the offset isn't expected (while allowing for floating point rounding)
      if (offset < subline.getFormer().calculateDistanceOnWay() - _epsilon ||
        offset > subline.getLatter().calculateDistanceOnWay() + _epsilon)
      {
        throw InternalErrorException("Expected offset to be in the bounds of the subline. "
          "Logic error?");
      }
      WayLocation result(map, w, offset);
      return _changeToPreferred(i, result, preferredEid);
    }
    soFar += ls;
  }

  return _sublines.back().getEnd();
}

Meters WayString::calculateDistanceOnString(const WayLocation& l) const
{
  Meters d = 0.0;

  for (const auto& subline : qAsConst(_sublines))
  {
    if (subline.contains(l))
      return d + fabs(l.calculateDistanceOnWay() - subline.getStart().calculateDistanceOnWay());
    else
      d += subline.calculateLength();
  }

  throw IllegalArgumentException("Way location was not found in this way string.");
}

Meters WayString::calculateLength() const
{
  Meters result = 0.0;
  for (const auto& subline : _sublines)
    result += subline.getLength();

  return result;
}

WayLocation WayString::_changeToPreferred(int index, const WayLocation& wl, ElementId preferredEid) const
{
  WayLocation result = wl;

  if (preferredEid.isNull() || wl.getWay()->getElementId() == preferredEid)
    result = wl;
  else if (index >= 1 &&
           _sublines[index - 1].getWay()->getElementId() == preferredEid &&
           fabs(calculateDistanceOnString(wl) - calculateDistanceOnString(_sublines[index - 1].getEnd())) <
           _epsilon)
  {
    result = _sublines[index - 1].getEnd();
  }
  else if (index < _sublines.size() - 1 &&
           _sublines[index + 1].getWay()->getElementId() == preferredEid &&
           fabs(calculateDistanceOnString(wl) - calculateDistanceOnString(_sublines[index + 1].getStart())) <
           _epsilon)
  {
    result = _sublines[index + 1].getStart();
  }

  return result;
}

WayPtr WayString::copySimplifiedWayIntoMap(const ElementProvider& map, OsmMapPtr destination)
{
  ConstWayPtr w = _sublines.front().getWay();
  Meters ce = _aggregateCircularError();
  WayPtr newWay = std::make_shared<Way>(w->getStatus(), destination->createNextWayId(), ce);
  newWay->setPid(w->getPid());

  Tags newTags;

  // go through each subline.
  for (const auto& subline : qAsConst(_sublines))
  {
    // first, create a vector that contains all the node IDs in ascending order.
    ConstWayPtr oldWay = subline.getWay();
    newWay->setPid(Way::getPid(newWay, oldWay));

    newTags = TagMergerFactory::mergeTags(newTags, oldWay->getTags(), ElementType::Way);

    // Figure out which node is the first node. If we're between nodes, then create a new node to
    // add.
    size_t formeri;
    vector<long> newNids;
    if (subline.getFormer().isNode() == false)
    {
      NodePtr n =
        std::make_shared<Node>(
          w->getStatus(), destination->createNextNodeId(), subline.getFormer().getCoordinate(), ce);
      destination->addNode(n);
      newNids.push_back(n->getId());
      formeri = subline.getFormer().getSegmentIndex() + 1;
    }
    else
      formeri = subline.getFormer().getSegmentIndex();

    // which is the last node that we can directly add.
    size_t latteri = subline.getLatter().getSegmentIndex();

    // add all the pre-existing nodes that we can.
    for (size_t i = formeri; i <= latteri; ++i)
    {
      long nid = oldWay->getNodeId(static_cast<int>(i));
      newNids.push_back(nid);
      destination->addNode(std::make_shared<Node>(*map.getNode(nid)));
    }

    // if the last location isn't on a node, create a new node for it
    if (subline.getLatter().isNode() == false)
    {
      NodePtr n =
        std::make_shared<Node>(
          w->getStatus(), destination->createNextNodeId(), subline.getLatter().getCoordinate(), ce);
      destination->addNode(n);
      newNids.push_back(n->getId());
    }

    // if the subline is backwards reverse the nodes so we can add them in the correct order.
    if (subline.isBackwards())
      std::reverse(newNids.begin(), newNids.end());

    // add each node to the new way. If the node is a duplicate (could happen with adjoining
    // sublines), then just don't add it.
    for (auto nid : newNids)
    {
      if (newWay->getNodeCount() == 0 || newWay->getLastNodeId() != nid)
        newWay->addNode(nid);
    }
  }

  newWay->setTags(newTags);
  destination->addWay(newWay);

  return newWay;
}

Meters WayString::getMaxCircularError() const
{
  Meters result = 0.0;
  for (const auto& ws : qAsConst(_sublines))
    result = max(ws.getWay()->getCircularError(), result);

  return result;
}

QString WayString::toString() const
{
  return hoot::toString(_sublines);
}

void WayString::visitRo(const ElementProvider &map, ConstElementVisitor& v) const
{
  // go through each subline and call visitRw on the subline. The sublines should only visit the
  // nodes that intersect the subline.
  for (const auto& subline : qAsConst(_sublines))
    subline.visitRo(map, v);
}

}
