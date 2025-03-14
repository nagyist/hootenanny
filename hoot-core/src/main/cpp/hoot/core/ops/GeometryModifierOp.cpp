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
 * @copyright Copyright (C) 2019, 2020, 2021, 2022 Maxar (http://www.maxar.com/)
 */

#include "GeometryModifierOp.h"

// Hoot
#include <hoot/core/elements/MapProjector.h>
#include <hoot/core/util/ConfigOptions.h>
#include <hoot/core/util/Factory.h>
#include <hoot/core/visitors/geometrymodifiers/GeometryModifierAction.h>

namespace bpt = boost::property_tree;

namespace hoot
{

HOOT_FACTORY_REGISTER(OsmMapOperation, GeometryModifierOp)

const std::string GeometryModifierAction::FILTER_TAG = "filter";
const std::string GeometryModifierAction::ARGUMENT_TAG = "arguments";

GeometryModifierOp::GeometryModifierOp()
  : _pConf(&conf())
{
  // get and instantiate available actions
  std::vector<QString> availableActionTypes =
    Factory::getInstance().getObjectNamesByBase(GeometryModifierAction::className());
  LOG_DEBUG( "Available Geometry Modifiers:")
  for (const auto& availType : availableActionTypes)
  {
    std::shared_ptr<GeometryModifierAction> pAction =
      Factory::getInstance().constructObject<GeometryModifierAction>(availType);
    _actions.append(pAction);
    LOG_DEBUG( "class: " << availType << " command: " << pAction->getCommandName());
  }
}

void GeometryModifierOp::setConfiguration(const Settings& conf)
{
  _pConf = &conf;
}

void GeometryModifierOp::apply(std::shared_ptr<OsmMap>& map)
{
  QList<GeometryModifierActionDesc> actionDescs = _readJsonRules();

  // re-project and set visitor map
  MapProjector::projectToPlanar(map);
  _geometryModifierVisitor.setOsmMap(map.get());

  // process
  for (const auto& actionDesc : qAsConst(actionDescs))
  {
    // visit with specific action, using proper arguments
    LOG_DEBUG("Processing geometry modifier " + actionDesc.command + "...");
    _geometryModifierVisitor.setActionDesc(actionDesc);
    actionDesc.pAction->parseArguments( actionDesc.arguments );

    // start processing
    actionDesc.pAction->processStart(map);

    // process elements with the visitor
    map->visitRw(_geometryModifierVisitor);

    // finalize processing
    actionDesc.pAction->processFinalize(map);
  }

  // update operation status info
  _numAffected = _geometryModifierVisitor._numAffected;
  _numProcessed = _geometryModifierVisitor._numProcessed;
}

QList<GeometryModifierActionDesc> GeometryModifierOp::_readJsonRules()
{
  // read the json rules
  ConfigOptions opts = ConfigOptions(*_pConf);
  _rulesFileName = opts.getGeometryModifierRulesFile();
  bpt::ptree propPtree;
  bpt::read_json(_rulesFileName.toLatin1().constData(), propPtree );

  QList<GeometryModifierActionDesc> actionDescs;

  for (const auto& commandLevelValue : qAsConst(propPtree))
  {
    // read command
    GeometryModifierActionDesc actionDesc;
    actionDesc.command = QString::fromStdString(commandLevelValue.first);

    // check command availability
    for (const auto& pAction : qAsConst(_actions))
    {
      if (pAction->getCommandName() == actionDesc.command)
      {
        actionDesc.pAction = pAction;
        break;
      }
    }

    if (!actionDesc.pAction)
      throw HootException(QString("Invalid geometry modifier action '%1' in %2").arg(actionDesc.command, _rulesFileName));

    if (!commandLevelValue.second.empty())
    {
      for (const auto& dataLevelValue : commandLevelValue.second)
      {
        // read filter
        if (dataLevelValue.first == GeometryModifierAction::FILTER_TAG)
          _parseFilter(actionDesc, dataLevelValue.second);
        else if (dataLevelValue.first == GeometryModifierAction::ARGUMENT_TAG)  // read arguments
          _parseArguments(actionDesc, dataLevelValue.second);
        else
          throw HootException(
            QString("Invalid geometry modifier tag '%1' for action '%2' in %3")
              .arg(QString::fromStdString(dataLevelValue.first), actionDesc.command, _rulesFileName));
      }

      actionDescs.append(actionDesc);
    }
  }

  return actionDescs;
}

void GeometryModifierOp::_parseFilter(GeometryModifierActionDesc& actionDesc, bpt::ptree ptree) const
{
  // turn the filter part of the rules file entry back to a json string
  std::stringstream stringStream;
  bpt::json_parser::write_json(stringStream, ptree);
  QString jsonFilter = QString::fromStdString(stringStream.str());

  // create a TagAdvancedCriterion for filtering from the json string
  try
  {
    actionDesc.filter = TagAdvancedCriterion(jsonFilter);
  }
  catch (const HootException& e)
  {
    QString exceptionMsg = QString(e.what());
    throw HootException(QString("Invalid filter for action '%1' in %2: '%3'").arg(actionDesc.command, _rulesFileName, exceptionMsg));
  }
}

void GeometryModifierOp::_parseArguments(GeometryModifierActionDesc& actionDesc, boost::property_tree::ptree ptree) const
{
  QList<QString> availableParameters = actionDesc.pAction->getParameterNames();

  for (const auto& data : ptree)
  {
    QString arg = QString::fromStdString(data.first);
    if (availableParameters.contains(arg))
      actionDesc.arguments[arg] = QString::fromStdString(data.second.data());
    else
      throw HootException(QString("Invalid geometry modifier argument '%1' for action '%2' in %3").arg(arg, actionDesc.command, _rulesFileName));
  }
}

}
