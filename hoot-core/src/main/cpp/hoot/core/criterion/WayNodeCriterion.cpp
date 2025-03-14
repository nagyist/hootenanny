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
 * @copyright Copyright (C) 2019-2023 Maxar (http://www.maxar.com/)
 */
#include "WayNodeCriterion.h"

// hoot
#include <hoot/core/elements/Node.h>
#include <hoot/core/elements/NodeToWayMap.h>
#include <hoot/core/index/OsmMapIndex.h>
#include <hoot/core/util/Factory.h>

namespace hoot
{

HOOT_FACTORY_REGISTER(ElementCriterion, WayNodeCriterion)

bool WayNodeCriterion::isSatisfied(const ConstElementPtr& e) const
{
  if (e->getElementType() != ElementType::Node)
    return false;
  if (!_map)
    throw HootException("You must set a map before calling: " + toString());
  const std::set<long>& containingWays = _map->getIndex().getNodeToWayMap()->getWaysByNode(e->getId());
  if (containingWays.empty())
    return false;

  for (auto containingWayId : containingWays)
  {
    if (!_map->containsElement(ElementId(ElementType::Way, containingWayId)))
      return false;
    if (_parentCriterion && !_parentCriterion->isSatisfied(_map->getWay(containingWayId)))
      return false;
  }
  return true;
}

long WayNodeCriterion::getFirstOwningWayId(const ConstNodePtr& node) const
{
  long result = 0;

  if (!_map)
    throw HootException("You must set a map before calling WayNodeCriterion");

  const std::set<long> waysContainingNode = _map->getIndex().getNodeToWayMap()->getWaysByNode(node->getId());
  for (auto containingWayId : waysContainingNode)
  {
    if (_map->containsElement(ElementId(ElementType::Way, containingWayId)) &&
        (!_parentCriterion || _parentCriterion->isSatisfied(_map->getWay(containingWayId))))
    {
      return containingWayId;
      break;
    }
  }
  return result;
}

}
