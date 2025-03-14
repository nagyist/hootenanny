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
 * @copyright Copyright (C) 2017, 2018, 2019, 2020, 2021, 2022 Maxar (http://www.maxar.com/)
 */
#include "ImplicitPoiPolygonTypeTagger.h"

#include <hoot/core/schema/OsmSchema.h>
#include <hoot/core/util/ConfigOptions.h>
#include <hoot/core/util/Factory.h>


namespace hoot
{

HOOT_FACTORY_REGISTER(ElementVisitor, ImplicitPoiPolygonTypeTagger)

ImplicitPoiPolygonTypeTagger::ImplicitPoiPolygonTypeTagger(const QString& databasePath)
  : ImplicitPoiTypeTagger(databasePath)
{
}

bool ImplicitPoiPolygonTypeTagger::_visitElement(const ElementPtr& e)
{
  _inABuildingOrPoiCategory =
    OsmSchema::getInstance().getCategories(e->getTags()).intersects(
      OsmSchemaCategory::building() | OsmSchemaCategory::poi());

  return (ImplicitPoiTypeTagger::_visitElement(e) || _elementIsATaggablePolygon(e));
}

bool ImplicitPoiPolygonTypeTagger::_elementIsATaggablePolygon(const ElementPtr& e)
{
  // see comment in OsmSchema::isPoiPolygonPoly
  if (e->getTags().contains("highway"))
    return false;

  const bool elementIsAWay = e->getElementType() == ElementType::Way;
  LOG_VART(elementIsAWay);
  const bool elementIsARelation = e->getElementType() == ElementType::Relation;
  LOG_VART(elementIsARelation);
  const bool elementIsAPoly = _polyCrit.isSatisfied(e);
  LOG_VART(elementIsAPoly);
  const bool elementIsASpecificPoly =
    _inABuildingOrPoiCategory && e->getTags().get(MetadataTags::Building()) != QLatin1String("yes") &&
     e->getTags().get(MetadataTags::Area()) != QLatin1String("yes") &&
     e->getTags().get("office") != QLatin1String("yes");
  //bit of a hack
  _elementIsASpecificFeature = elementIsASpecificPoly;
  LOG_VART(_elementIsASpecificFeature);
  const bool elementIsAGenericPoly = !elementIsASpecificPoly;
  LOG_VART(elementIsAGenericPoly);
  LOG_VART(_getNames(e->getTags()).size());

  if (elementIsAGenericPoly)  // always allow generic elements
    return true;
  else if (elementIsASpecificPoly && _allowTaggingSpecificFeatures) // allowing specific elements is configurable
    return true;
  else if ((elementIsAWay || elementIsARelation) && !_getNames(e->getTags()).empty())
    return true;

  return false;
}

}
