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
#ifndef RANDOM_TAG_MODIFIER_H
#define RANDOM_TAG_MODIFIER_H

// hoot
#include <hoot/core/util/Configurable.h>
#include <hoot/core/util/RngConsumer.h>
#include <hoot/core/visitors/ElementVisitor.h>

namespace hoot
{

/**
 * A simple random tag modifier
 */
class RandomTagModifier : public ElementVisitor, public RngConsumer, public Configurable
{
public:

  static QString className() { return "RandomTagModifier"; }

  RandomTagModifier();
  ~RandomTagModifier() override = default;

  QString permuteName(const QString& s);

  /**
   * @see Configurable
   */
  void setConfiguration(const Settings& conf) override;

  /**
   * @see RngConsumer
   */
  void setRng(boost::minstd_rand& rng) override { _rng = &rng; }

  /**
   * @see ElementVisitor
   */
  void visit(const std::shared_ptr<Element>& e) override;

  QString getDescription() const override { return "Randomly modifies feature tags"; }
  QString getName() const override { return className(); }
  QString getClassName() const override { return className(); }

  void setExemptTagKeys(const QStringList& keys) { _exemptTagKeys = keys; }
  void setReplacementTagKeys(const QStringList& keys) { _replacementTagKeys = keys; }
  void setReplacementTagValues(const QStringList& values) { _replacementTagValues = values; }
  /**
   * Set the probability that a tag will be removed.
   */
  void setProbability(double p) { _p = p; }

private:

  double _p;
  boost::minstd_rand* _rng;
  std::shared_ptr<boost::minstd_rand> _localRng;

  QStringList _exemptTagKeys;
  QStringList _replacementTagKeys;
  QStringList _replacementTagValues;
};

}

#endif // RANDOM_TAG_MODIFIER_H
