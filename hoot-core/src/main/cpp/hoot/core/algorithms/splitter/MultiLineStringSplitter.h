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
#ifndef MULTILINESTRINGSPLITTER_H
#define MULTILINESTRINGSPLITTER_H

#include <hoot/core/elements/OsmMap.h>
#include <hoot/core/geometry/GeometryToElementConverter.h>

namespace hoot
{

class MultiLineStringLocation;
class WaySublineCollection;
class FindNodesInWayFactory;

class MultiLineStringSplitter
{
public:

  static QString className() { return "MultiLineStringSplitter"; }

  MultiLineStringSplitter(const bool markAddedMultilineStringRelations = false);

  /**
   * @brief createSublines given a WaySublineCollection, extract all the matching sub-elements.
   * @param map Map that contains the data. The resulting match will be added to map if it isn't
   *  already in map.
   * @param string The string of sublines to extract.
   * @param match The match will be placed here. If there are multiple matching sublines they'll
   *  be placed into a multilinestring relation. The relations tags will _not_ be populated with
   *  the subline way tags. Status and CE will be taken from the first matching subline.
   */
  ElementPtr createSublines(const OsmMapPtr& map, const WaySublineCollection& string, const std::vector<bool>& reverse,
                            std::shared_ptr<GeometryToElementConverter::NodeFactory> nf =
                              std::shared_ptr<GeometryToElementConverter::NodeFactory>()) const;

  /**
   * @brief split given a subline string, cut out all the bits that match the subline and put them
   * into @a match and put anything that doesn't match into @a scraps.
   *
   * The elements referenced in @a string are not removed from the map. @a match and @a scraps are
   * added to the map along with all their children.
   * @param match Contains all the bits that match the subline string. This element will be created
   * and added to the map. If nothing matches or the line length is zero then @a match will be
   * reset.
   * @param scraps Contains all the bits that don't match the subline string. This element will be
   *  created and added to the map. If nothing matches or the line length is zero then @a scraps
   */
  void split(const OsmMapPtr& map, const WaySublineCollection& string, const std::vector<bool>& reverse,
             ElementPtr& match, ElementPtr& scraps,
             std::shared_ptr<GeometryToElementConverter::NodeFactory> nf =
               std::shared_ptr<GeometryToElementConverter::NodeFactory>()) const;
  /**
   * @brief split splits a multi-line string at a given location, puts the matching subline into
   * @a match, and removes the pieces that do not match from the map.
   * @param map The map containing the multi-line string to split
   * @param splitPoint The point on a given way in the multi-line string to split on
   * @param match contains all the bits that match the subline string. This element will be created
   * and added to the map. If nothing matches or the line length is zero then @a match will be
   * reset.
   */
  void split(const OsmMapPtr& map, const MultiLineStringLocation& splitPoint, ElementPtr& match) const;

private:

  bool _markAddedMultilineStringRelations;

  std::shared_ptr<FindNodesInWayFactory> _createNodeFactory(const WaySublineCollection& string) const;
};

}

#endif // MULTILINESTRINGSPLITTER_H
