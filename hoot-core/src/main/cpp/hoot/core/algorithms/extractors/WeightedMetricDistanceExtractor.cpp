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

#include "WeightedMetricDistanceExtractor.h"

// geos
#include <geos/geom/GeometryFactory.h>
#include <geos/geom/LineString.h>
#include <geos/geom/Point.h>

// hoot
#include <hoot/core/algorithms/aggregator/MeanAggregator.h>
#include <hoot/core/algorithms/WayDiscretizer.h>
#include <hoot/core/geometry/ElementToGeometryConverter.h>
#include <hoot/core/util/ConfigOptions.h>
#include <hoot/core/util/Factory.h>

using namespace geos::geom;
using namespace std;

namespace hoot
{

HOOT_FACTORY_REGISTER(FeatureExtractor, WeightedMetricDistanceExtractor)

WeightedMetricDistanceExtractor::WeightedMetricDistanceExtractor(ValueAggregatorPtr wayAgg,
                                                                 ValueAggregatorPtr pointAgg,
                                                                 Meters searchRadius)
  : WayFeatureExtractor(wayAgg),
    _pointAgg(pointAgg),
    _searchRadius(searchRadius)
{
  if (searchRadius == -1)
    setSearchRadius(ConfigOptions().getWeightedMetricDistanceExtractorSearchRadius());
}

WeightedMetricDistanceExtractor::WeightedMetricDistanceExtractor(Meters searchRadius)
  : WayFeatureExtractor(),
    _pointAgg(),
    _searchRadius(searchRadius)
{
  setPointAggregator(ConfigOptions().getWeightedMetricDistanceExtractorPointAggregator());
  if (searchRadius == -1)
    setSearchRadius(ConfigOptions().getWeightedMetricDistanceExtractorSearchRadius());
}

double WeightedMetricDistanceExtractor::_extract(const OsmMap& map, const ConstWayPtr& w1,
                                                 const ConstWayPtr& w2) const
{
  vector<double> distances;

  vector<Coordinate> v;
  WayDiscretizer wd(map.shared_from_this(), w1);
  wd.discretize(2.0, v);

  std::shared_ptr<LineString> ls2 = ElementToGeometryConverter(map.shared_from_this()).convertToLineString(w2);

  double sigma = _searchRadius;

  if (_searchRadius == -1)
  {
    double s1 = w1->getCircularError() / 2.0;
    double s2 = w2->getCircularError() / 2.0;
    sigma = sqrt(s1 * s1 + s2 * s2);
  }

  distances.reserve(v.size());
  for (const auto& coord : v)
  {
    std::shared_ptr<Point> point(GeometryFactory::getDefaultInstance()->createPoint(coord));
    double d = ls2->distance(point.get()) / sigma;
    distances.push_back(d);
  }

  return _pointAgg->aggregate(distances);
}

QString WeightedMetricDistanceExtractor::getName() const
{
  return getClassName() + " way agg: " + _agg->toString() + " point agg: " + _pointAgg->toString();
}

void WeightedMetricDistanceExtractor::setPointAggregator(const QString& aggregator)
{
  _pointAgg = Factory::getInstance().constructObject<ValueAggregator>(aggregator);
}

void WeightedMetricDistanceExtractor::setSearchRadius(const double radius)
{
  _searchRadius = radius;
  LOG_VART(_searchRadius);
}

void WeightedMetricDistanceExtractor::setConfiguration(const Settings& conf)
{
  ConfigOptions configOptions(conf);
  setPointAggregator(configOptions.getWeightedMetricDistanceExtractorPointAggregator());
  setSearchRadius(configOptions.getWeightedMetricDistanceExtractorSearchRadius());
}

}
