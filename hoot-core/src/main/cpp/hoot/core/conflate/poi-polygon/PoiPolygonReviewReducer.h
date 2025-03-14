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
 * @copyright Copyright (C) 2016, 2017, 2018, 2019, 2020, 2021, 2022 Maxar (http://www.maxar.com/)
 */
#ifndef POIPOLYGONREVIEWREDUCER_H
#define POIPOLYGONREVIEWREDUCER_H

// Hoot
#include <hoot/core/conflate/address/AddressParser.h>
#include <hoot/core/conflate/poi-polygon/PoiPolygonInfoCache.h>
#include <hoot/core/elements/OsmMap.h>
#include <hoot/core/util/Configurable.h>

namespace hoot
{

/**
 * This class is intended to reduce the number of unnecessary reviews between POI's and polygons
 * with the goal of never causing a miss where there should be a match.  Any rule that results in an
 * incorrect match found over the course of time testing against different datasets should be
 * removed from this class.
 *
 * This class is the result of disatisfaction with the overall high number of reviews generated by
 * poi/poly conflation and has had quite a bit added to it over time.  It would be nice to get rid
 * of most, if not all, of this class, as it can be difficult to maintain.  That may be possible via
 * modifications to the definition of what poi/poly conflation conflates, as well as modifications
 * to the evidence calculation in PoiPolygonMatch.  Alternatively, making another attempt at a
 * poi/poly random forest model could make it entirely obsolete (#2323).  At the very least,
 * triggersRule could benefit from being refactored into smaller chunks. - BDW
 *
 * This class was created almost entirely using the POI/Polygon regression test datasets C and D.
 * Unfortunately, the data found in those tests to warrant creating the review reduction rules
 * never had conflate case tests generated for them. This could eventually be done but will be
 * very tedious, as data for each separate test case must be tracked down individually from those
 * datasets.
 */
class PoiPolygonReviewReducer : public Configurable
{

public:

  PoiPolygonReviewReducer(const ConstOsmMapPtr& map, const std::set<ElementId>& polyNeighborIds, double distance,
                          double nameScoreThreshold, double nameScore, bool nameMatch, bool exactNameMatch,
                          double typeScoreThreshold, double typeScore, bool typeMatch, double matchDistanceThreshold,
                          double addressScore, bool addressParsingEnabled, PoiPolygonInfoCachePtr infoCache);
  ~PoiPolygonReviewReducer() override = default;

  void setConfiguration(const Settings& conf) override;

  /**
   * Determines whether the input features trigger a rule which precludes them from being matched or
   * reviewed against each other.
   *
   * @param poi the POI feature to be examined
   * @param poly the polygon feature to be examined
   * @return return true if the features trigger a review reduction rule; false otherwise
   * @note this needs to be broken up into more modular pieces
   */
  bool triggersRule(ConstNodePtr poi, ConstElementPtr poly);

  QString getTriggeredRuleDescription() const { return _triggeredRuleDescription; }

private:

  ConstOsmMapPtr _map;

  std::set<ElementId> _polyNeighborIds;

  double _distance;
  double _nameScoreThreshold;
  double _nameScore;
  bool _nameMatch;
  bool _exactNameMatch;
  double _typeScoreThreshold;
  double _typeScore;
  bool _typeMatch;
  double _matchDistanceThreshold;
  double _addressScore;
  bool _addressMatch;

  QStringList _genericLandUseTagVals;

  bool _addressParsingEnabled;
  AddressParser _addressParser;

  PoiPolygonInfoCachePtr _infoCache;

  QString _triggeredRuleDescription;

  bool _nonDistanceSimilaritiesPresent() const;
};

}

#endif // POIPOLYGONREVIEWREDUCER_H
