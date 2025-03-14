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

#include "GeometryToElementConverter.h"

// GEOS
#include <geos/geom/CoordinateSequenceFactory.h>
#include <geos/geom/GeometryFactory.h>
#include <geos/geom/LinearRing.h>
#include <geos/geom/MultiLineString.h>
#include <geos/geom/MultiPolygon.h>
#include <geos/geom/Point.h>
#include <geos/geom/Polygon.h>

// hoot
#include <hoot/core/schema/OsmSchema.h>

// Standard
#include <stdint.h>

using namespace geos::geom;

namespace hoot
{

int GeometryToElementConverter::logWarnCount = 0;

GeometryToElementConverter::GeometryToElementConverter(const OsmMapPtr& map)
  : _constMap(map),
    _map(map)
{
}

std::shared_ptr<Element> GeometryToElementConverter::convertGeometryCollection(const GeometryCollection* gc, Status s, double circularError)
{
  LOG_TRACE("Converting geometry collection...");

  if (gc->getNumGeometries() > 1)
  {
    LOG_DEBUG("Creating relation. convertGeometryCollection");
    RelationPtr r = std::make_shared<Relation>(s, _map->createNextRelationId(), circularError);

    for (size_t i = 0; i < gc->getNumGeometries(); i++)
      r->addElement("", convertGeometryToElement(gc->getGeometryN(i), s, circularError));
    return r;
  }
  else if (gc->getNumGeometries() == 1)
    return convertGeometryToElement(gc->getGeometryN(0), s, circularError);
  else
    return std::shared_ptr<Element>();
}

std::shared_ptr<Element> GeometryToElementConverter::convertGeometryToElement(const Geometry* g, Status s, double circularError)
{
  LOG_VART(g->getGeometryTypeId());
  switch (g->getGeometryTypeId())
  {
  case GEOS_POINT:
    return convertPointToNode(dynamic_cast<const Point*>(g), _map, s, circularError);
  case GEOS_LINESTRING:
  case GEOS_LINEARRING:
    return convertLineStringToWay(dynamic_cast<const LineString*>(g), _map, s, circularError);
  case GEOS_POLYGON:
    return convertPolygonToElement(dynamic_cast<const Polygon*>(g), _map, s, circularError);
  case GEOS_MULTILINESTRING:
    return convertMultiLineStringToElement(dynamic_cast<const MultiLineString*>(g), _map, s, circularError);
  case GEOS_MULTIPOLYGON:
    return convertMultiPolygonToRelation(dynamic_cast<const MultiPolygon*>(g), _map, s, circularError);
  case GEOS_GEOMETRYCOLLECTION:
    return convertGeometryCollection(dynamic_cast<const GeometryCollection*>(g), s, circularError);
  default:
    if (logWarnCount < Log::getWarnMessageLimit())
    {
      LOG_WARN("Unsupported geometry type. Element will be removed from the map. " + g->toString());
    }
    else if (logWarnCount == Log::getWarnMessageLimit())
    {
      LOG_WARN(className() << ": " << Log::LOG_WARN_LIMIT_REACHED_MESSAGE);
    }
    logWarnCount++;
    return std::shared_ptr<Element>();
  }
}

NodePtr GeometryToElementConverter::convertPointToNode(const geos::geom::Point* point, const OsmMapPtr& map,
                                                       Status s, double circularError) const
{
  LOG_TRACE("Converting point to node...");
  return _createNode(map, Coordinate(point->getX(), point->getY()), s, circularError);
}

WayPtr GeometryToElementConverter::convertLineStringToWay(const LineString* ls, const OsmMapPtr& map,
                                                          Status s, double circularError) const
{
  LOG_TRACE("Converting line string to way...");

  WayPtr way;
  if (ls->getNumPoints() > 0)
  {
    way = std::make_shared<Way>(s, map->createNextWayId(), circularError);
    for (size_t i = 0; i < ls->getNumPoints(); i++)
      way->addNode(_createNode(map, ls->getCoordinateN(i), s, circularError)->getId());
    map->addWay(way);
  }

  return way;
}

std::shared_ptr<Element> GeometryToElementConverter::convertMultiLineStringToElement(const MultiLineString* mls, const OsmMapPtr& map,
                                                                                     Status s, double circularError) const
{
  LOG_TRACE("Converting multiline string to element...");

  if (mls->getNumGeometries() > 1)
  {
    RelationPtr r = std::make_shared<Relation>(s, map->createNextRelationId(), circularError, MetadataTags::RelationMultilineString());
    for (size_t i = 0; i < mls->getNumGeometries(); i++)
      r->addElement("", convertLineStringToWay(mls->getGeometryN(i), map, s, circularError));
    map->addRelation(r);
    return r;
  }
  else
    return convertLineStringToWay(mls->getGeometryN(0), map, s, circularError);
}

RelationPtr GeometryToElementConverter::convertMultiPolygonToRelation(const MultiPolygon* mp, const OsmMapPtr& map,
                                                                      Status s, double circularError) const
{
  LOG_TRACE("Converting multipolygon to relation...");

  RelationPtr r = std::make_shared<Relation>(s, map->createNextRelationId(), circularError, MetadataTags::RelationMultiPolygon());
  for (size_t i = 0; i < mp->getNumGeometries(); i++)
    convertPolygonToRelation(mp->getGeometryN(i), map, r, s, circularError);
  map->addRelation(r);
  return r;
}

std::shared_ptr<Element> GeometryToElementConverter::convertPolygonToElement(const Polygon* polygon, const OsmMapPtr& map,
                                                                             Status s, double circularError) const
{
  LOG_TRACE("Converting polygon to element...");

  if (polygon->isEmpty())
    return std::shared_ptr<Element>();
  else if (polygon->getNumInteriorRing() == 0)
  {
    WayPtr result = convertLineStringToWay(polygon->getExteriorRing(), map, s, circularError);
    result->getTags()[MetadataTags::Area()] = "yes";
    return result;
  }
  else
    return convertPolygonToRelation(polygon, map, s, circularError);
}

RelationPtr GeometryToElementConverter::convertPolygonToRelation(const Polygon* polygon, const OsmMapPtr& map,
                                                                 Status s, double circularError) const
{
  RelationPtr r = std::make_shared<Relation>(s, map->createNextRelationId(), circularError, MetadataTags::RelationMultiPolygon());
  convertPolygonToRelation(polygon, map, r, s, circularError);
  map->addRelation(r);
  return r;
}

void GeometryToElementConverter::convertPolygonToRelation(const Polygon* polygon, const OsmMapPtr& map, const RelationPtr& r,
                                                          Status s, double circularError) const
{
  WayPtr outer = convertLineStringToWay(polygon->getExteriorRing(), map, s, circularError);
  if (outer != nullptr)
  {
    r->addElement(MetadataTags::RoleOuter(), outer);
    for (size_t i = 0; i < polygon->getNumInteriorRing(); i++)
      r->addElement(MetadataTags::RoleInner(), convertLineStringToWay(polygon->getInteriorRingN(i), map, s, circularError));
  }
}

NodePtr GeometryToElementConverter::_createNode(const OsmMapPtr& map, const Coordinate& c,
                                                Status s, double circularError) const
{
  if (_nf == nullptr)
  {
    NodePtr n = std::make_shared<Node>(s, map->createNextNodeId(), c, circularError);
    map->addNode(n);
    return n;
  }
  else
    return _nf->createNode(map, c, s, circularError);
}

}
