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
 * This will properly maintain the copyright information. DigitalGlobe
 * copyrights will be updated automatically.
 *
 * @copyright Copyright (C) 2015, 2016 DigitalGlobe (http://www.digitalglobe.com/)
 */
#ifndef NETWORKMERGERCREATOR_H
#define NETWORKMERGERCREATOR_H

// hoot
#include <hoot/core/ConstOsmMapConsumer.h>
#include <hoot/core/algorithms/SublineStringMatcher.h>
#include <hoot/core/conflate/MergerCreator.h>

namespace hoot
{

class NetworkMatch;

class NetworkMergerCreator : public MergerCreator, public ConstOsmMapConsumer
{
public:

  static string className() { return "hoot::NetworkMergerCreator"; }

  NetworkMergerCreator();

  /**
   *
   */
  virtual bool createMergers(const MatchSet& matches, vector<Merger*>& mergers) const;

  virtual vector<Description> getAllCreators() const;

  virtual bool isConflicting(const ConstOsmMapPtr& map, const Match* m1, const Match* m2) const;

  virtual void setOsmMap(const OsmMap* map) { _map = map; }

private:
  const OsmMap* _map;
  Meters _minSplitSize;
  shared_ptr<SublineStringMatcher> _sublineMatcher;

  /**
   * If there are exactly 2 matches
   * and one match contains the other.
   * return the matches that contains the other.
   * Otherwise, return 0.
   */
  const NetworkMatch* _getLargestContainer(const MatchSet& matches) const;

  /**
   * Returns true if one or more matches are conflicting matches.
   */
  bool _isConflictingSet(const MatchSet& matches) const;
};

}

#endif // NETWORKMERGERCREATOR_H
