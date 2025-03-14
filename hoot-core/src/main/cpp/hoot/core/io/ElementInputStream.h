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
#ifndef ELEMENTINPUTSTREAM_H
#define ELEMENTINPUTSTREAM_H

#include <ogr_spatialref.h>

#include <hoot/core/elements/Element.h>

namespace hoot
{

class ElementInputStream
{

public:

  ElementInputStream() = default;

  virtual std::shared_ptr<OGRSpatialReference> getProjection() const = 0;

  /**
   * @brief ~ElementInputStream
   *
   * If the stream is open when the destructor is called, close must be called in the destructor
   */
  virtual ~ElementInputStream() = default;

  /**
   * @brief closeStream
   *
   * Releases all resources associated with the stream, if any
   */
  virtual void close() = 0;

  virtual bool hasMoreElements() = 0;

  virtual ElementPtr readNextElement() = 0;

  /**
   * @brief canStreamBounded Most stream objects cannot stream correctly when bounded, some input streams
   *  apply the bounds at the API level (for instance) and are applied outside of Hootenanny and therefore all
   *  elements can be streamed.
   * @return false The default is bounded inputs cannot stream.
   */
  virtual bool canStreamBounded() const { return false; }
};

using ElementInputStreamPtr = std::shared_ptr<ElementInputStream>;

}

#endif // ELEMENTINPUTSTREAM_H
