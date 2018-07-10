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
 * @copyright Copyright (C) 2015, 2016, 2017, 2018 DigitalGlobe (http://www.digitalglobe.com/)
 */
#ifndef HIGHWAYFILTER_H
#define HIGHWAYFILTER_H

#include "BaseFilter.h"


namespace hoot
{

/**
 * A filter that will either keep or remove matches.
 *
 * TODO: This is redundant with HighwayCriterion and should be removed.
 */
class HighwayFilter : public BaseFilter
{
public:

  HighwayFilter(FilterType type) : BaseFilter(type) {}

  virtual bool isMatch(const Element& e) const;

  virtual ElementCriterionPtr clone() { return ElementCriterionPtr(new HighwayFilter(_type)); }

  virtual QString getDescription() const { return "Identifies highways"; }
};

}

#endif // HIGHWAYFILTER_H