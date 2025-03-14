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
 * @copyright Copyright (C) 2015, 2017, 2019, 2021, 2022 Maxar (http://www.maxar.com/)
 */

#ifndef LONGESTCOMMONNODESTRING_H
#define LONGESTCOMMONNODESTRING_H

namespace hoot
{
  class Way;

/**
 * @brief The LongestCommonNodeString class given two ways calculates the longest string of nodes
 * that are identical within the ways.
 */
class LongestCommonNodeString
{
public:

  LongestCommonNodeString(const std::shared_ptr<Way>& w1, const std::shared_ptr<Way>& w2);

  /**
   * @brief apply Returns the length of the common substring. Zero if no common substring was found.
   */
  int apply();

  /**
   * @brief getW1Index returns the start index of the match in w1.
   */
  int getW1Index() const{ return static_cast<int>(_i1); }

  /**
   * @brief getW2Index returns the start index of the match in w2.
   */
  int getW2Index() const{ return static_cast<int>(_i2); }

private:

  std::shared_ptr<Way> _w1;
  std::shared_ptr<Way> _w2;
  size_t _i1;
  size_t _i2;
};

}

#endif // LONGESTCOMMONNODESTRING_H
