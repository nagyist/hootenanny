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
 * @copyright Copyright (C) 2016, 2017, 2018, 2019, 2021, 2022 Maxar (http://www.maxar.com/)
 */

#include "LongBox.h"

//  std
#include <cmath>

using namespace std;

namespace hoot
{

LongBox::LongBox(const std::vector<long>& min, const std::vector<long>& max)
{
  setMin(min);
  setMax(max);
}

std::shared_ptr<LongBox> LongBox::copy() const
{
  return std::make_shared<LongBox>(getMin(), getMax());
}

LongBox::~LongBox()
{
  _min.clear();
  _max.clear();
}

long int LongBox::calculateVolume() const
{
  long result = 1;
  for (uint i = 0; i < getMin().size(); i++)
    result *= getWidth(i);
  return result;
}

bool LongBox::edgeOverlaps(const LongBox& b) const
{
  bool result = false;
  for (uint i = 0; i < getMin().size(); i++)
  {
    result =
        result ||
        (getMin()[i] == b.getMin()[i]) ||
        (getMin()[i] == b.getMax()[i]) ||
        (getMax()[i] == b.getMin()[i]) ||
        (getMax()[i] == b.getMax()[i]);
  }
  return result;
}

LongBox LongBox::expand(int size) const
{
  vector<long int> min = getMin();
  vector<long int> max = getMax();
  for (uint i = 0; i < getMin().size(); i++)
  {
    min[i] -= size;
    max[i] += size;
  }

  return LongBox(min, max);
}

bool LongBox::in(const vector<long int>& p) const
{
  if (p.size() < _min.size() || p.size() < _max.size())
    throw HootException("Input vector size is less than min or max size.");
  bool result = true;
  for (uint i = 0; i < getMin().size(); i++)
    result = result && (p[i] >= getMin()[i]) && (p[i] <= getMax()[i]);
  return result;
}

QString LongBox::toString() const
{
  QString result = "{ ";
  for (uint i = 0; i < _min.size(); i++)
    result += "( " + QString::number(getMin()[i]) + " : " + QString::number(getMax()[i]) + ") ";
  result += "}";
  return result;
}

long LongBox::getWidth(int d) const
{
  if (d > (int)getMin().size() || d > (int)getMax().size())
    throw HootException("Index is greater than min or max size.");
  return getMax()[d] - getMin()[d] + 1;
}

}
