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
#include "MergerFactory.h"

// hoot
#include <hoot/core/conflate/matching/Match.h>
#include <hoot/core/conflate/matching/MatchType.h>
#include <hoot/core/elements/OsmMapConsumer.h>
#include <hoot/core/util/ConfigOptions.h>
#include <hoot/core/util/Factory.h>
#include <hoot/core/util/StringUtils.h>

using namespace std;

namespace hoot
{

int MergerFactory::logWarnCount = 0;

std::shared_ptr<MergerFactory> MergerFactory::_theInstance;

MergerFactory::~MergerFactory()
{
  reset();
}

void MergerFactory::reset()
{
  _creators.clear();
}

void MergerFactory::createMergers(const OsmMapPtr& map, const MatchSet& matches, vector<MergerPtr>& result) const
{
  LOG_TRACE("Creating merger group for " << StringUtils::formatLargeNumber(matches.size()) << " matches...");
  for (size_t i = 0; i < _creators.size(); i++)
  {
    PROGRESS_DEBUG(
      "Creating merger group " << i + 1 << " of " << _creators.size() << " for " <<
      StringUtils::formatLargeNumber(matches.size()) << " match(es)...");

    OsmMapConsumer* omc = dynamic_cast<OsmMapConsumer*>(_creators[i].get());
    if (omc)
      omc->setOsmMap(map.get());
    if (_creators[i]->createMergers(matches, result))
      return;
    // We don't want the creators to hold onto a map pointer that will go out of scope.
    if (omc)
      omc->setOsmMap(nullptr);
  }

  // In #2069, a ScriptMatch and a NetworkMatch are being grouped together, which ultimately causes
  // the exception below to be thrown. Now, instead of an error we're only logging a warning. This
  // also required additional error handling in ScriptMerger (see ScriptMerger::_applyMergePair).
  if (logWarnCount < Log::getWarnMessageLimit())
  {
    // Changing this to info since its leaked into the case tests, although its not causing them to
    // fail. Opened #5082 to deal with.
    LOG_INFO("Unable to create merger for the provided set of matches: " << matches);
  }
  else if (logWarnCount == Log::getWarnMessageLimit())
  {
    LOG_INFO(className() << ": " << Log::LOG_WARN_LIMIT_REACHED_MESSAGE);
  }
  logWarnCount++;
}

vector<CreatorDescription> MergerFactory::getAllAvailableCreators() const
{
  vector<CreatorDescription> result;

  // get all match creators from the factory
  vector<QString> names = Factory::getInstance().getObjectNamesByBase(MergerCreator::className());
  for (const auto& name : names)
  {
    // get all names known by this creator.
    std::shared_ptr<MergerCreator> mc = Factory::getInstance().constructObject<MergerCreator>(name);

    vector<CreatorDescription> d = mc->getAllCreators();
    result.insert(result.end(), d.begin(), d.end());
  }

  return result;
}

MergerFactory& MergerFactory::getInstance()
{
  if (!_theInstance.get())
    _theInstance.reset(new MergerFactory());

  if (_theInstance->_creators.empty())
    _theInstance->registerDefaultCreators();

  return *_theInstance;
}

bool MergerFactory::isConflicting(const ConstOsmMapPtr& map, const ConstMatchPtr& m1, const ConstMatchPtr& m2,
                                  const QHash<QString, ConstMatchPtr>& matches) const
{
  // if any creator considers a match conflicting then it is a conflict
  for (const auto& creator : _creators)
  {
    if (creator->isConflicting(map, m1, m2, matches))
    {
      LOG_TRACE("Conflicting matches: " << m1 << ", " << m2);
      return true;
    }
  }
  return false;
}

void MergerFactory::registerDefaultCreators()
{  
  const QStringList mergerCreators = ConfigOptions().getMergerCreators();
  LOG_VARD(mergerCreators);
  for (const auto& c : qAsConst(mergerCreators))
  {
    QStringList args = c.split(",");
    QString className = args[0];
    if (className.length() > 0)
    {
      args.removeFirst();
      MergerCreatorPtr mc(Factory::getInstance().constructObject<MergerCreator>(className));
      registerCreator(mc);
      if (!args.empty())
        mc->setArguments(args);
    }
  }
}

QString MergerFactory::toString() const
{
  QStringList creatorList;
  for (const auto& creator : _creators)
  {
    vector<CreatorDescription> desc = creator->getAllCreators();
    for (const auto& description : desc)
      creatorList << description.toString();
  }
  return QString("{ %1 }").arg(creatorList.join(",\n"));
}

}

