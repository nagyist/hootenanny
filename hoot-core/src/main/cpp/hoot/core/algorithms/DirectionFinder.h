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
 * @copyright Copyright (C) 2015 DigitalGlobe (http://www.digitalglobe.com/)
 */
#ifndef DIRECTIONFINDER_H
#define DIRECTIONFINDER_H

// Boost
#include <boost/shared_ptr.hpp>

#include <hoot/core/OsmMap.h>

namespace hoot {
  class Way;
}

namespace hoot {

class DirectionFinder
{
public:
  /**
   * Returns true if these lines are generally pointed in a similar direction.
   */
  static bool isSimilarDirection(const hoot::ConstOsmMapPtr &map, hoot::ConstWayPtr w1,
    hoot::ConstWayPtr w2);
};

}

#endif // DIRECTIONFINDER_H
