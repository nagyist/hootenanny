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
 * @copyright Copyright (C) 2015, 2017, 2018, 2019, 2020, 2021, 2022 Maxar (http://www.maxar.com/)
 */
#include "AddGeometryTypeVisitor.h"

// GEOS
#include <geos/geom/Geometry.h>

// hoot
#include <hoot/core/geometry/ElementToGeometryConverter.h>
#include <hoot/core/util/Factory.h>

namespace hoot
{

HOOT_FACTORY_REGISTER(ElementVisitor, AddGeometryTypeVisitor)

void AddGeometryTypeVisitor::visit(const std::shared_ptr<Element>& e)
{
  if (e->getTags().getNonDebugCount() > 0)
  {
    if (e->getElementType() == ElementType::Node)
      e->getTags()["geometry_type"] = "Point";
    else
    {
      std::shared_ptr<geos::geom::Geometry> geometry =
        ElementToGeometryConverter(_map->shared_from_this()).convertToGeometry(e);
      LOG_VART(geometry->isEmpty());
      if (geometry && !geometry->isEmpty())
        e->getTags()["geometry_type"] = QString::fromStdString(geometry->getGeometryType());
    }
  }
}

}
