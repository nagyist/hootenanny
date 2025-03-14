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
#ifndef SCRIPTMATCHCREATOR_H
#define SCRIPTMATCHCREATOR_H

// Hoot
#include <hoot/core/conflate/SearchRadiusProvider.h>
#include <hoot/core/conflate/matching/MatchCreator.h>
#include <hoot/core/criterion/ElementCriterion.h>
#include <hoot/core/util/Configurable.h>

#include <hoot/js/PluginContext.h>

namespace hoot
{

class ScriptMatchVisitor;

/**
 * Match creator for all generic conflation scripts
 *
 * @sa ScriptMatch
 */
class ScriptMatchCreator : public MatchCreator, public SearchRadiusProvider, public Configurable
{
public:

  static QString className() { return "ScriptMatchCreator"; }

  static const QString POINT_POLYGON_SCRIPT_NAME;

  ScriptMatchCreator();
  ~ScriptMatchCreator() override = default;

  /**
   * @see SearchRadiusProvider
   */
  void init(const ConstOsmMapPtr& map) override;
  /**
   * @see SearchRadiusProvider
   */
  Meters calculateSearchRadius(const ConstOsmMapPtr& map, const ConstElementPtr& e) override;

  /**
   * @see Configurable
   */
  void setConfiguration(const Settings& conf) override;

  /**
   * @see MatchCreator
   */
  MatchPtr createMatch(const ConstOsmMapPtr&, ElementId, ElementId) override;
  /**
   * Search the provided map for POI matches and add the matches to the matches vector.
   */
  void createMatches(const ConstOsmMapPtr& map, std::vector<ConstMatchPtr>& matches,
                     ConstMatchThresholdPtr threshold) override;
  /**
   * Determines whether an element is a candidate for matching for this match creator
   *
   * @param element element to determine the match candidate status of
   * @param map the map the element whose candidacy is being determined belongs to
   * @return true if the element is a match candidate; false otherwise
   */
  bool isMatchCandidate(ConstElementPtr element, const ConstOsmMapPtr& map) override;
  /**
   * @see MatchCreator
   */
  std::shared_ptr<MatchThreshold> getMatchThreshold() override;

  /**
   * @see MatchCreator
   */
  std::vector<CreatorDescription> getAllCreators() const override;

  /**
   * @see MatchCreator
   */
  void setArguments(const QStringList& args) override;

  /**
   * @see MatchCreator
   */
  QString getName() const override;

  /**
   * @see FilteredByGeometryTypeCriteria
   */
  QStringList getCriteria() const override;

private:

  friend class ScriptMatchCreatorTest;

  std::shared_ptr<PluginContext> _script;
  QString _scriptPath;
  CreatorDescription _scriptInfo;

  // options vals to validate; Its sometimes easier to perform validation on these types of options
  // with core C++ as opposed to in the script JS files. As this list grows, consider creating a new
  // options class for each feature type.

  // railway
  bool _railwayOneToManyMatchEnabled;
  QStringList _railwayOneToManyIdentifyingKeys;
  QStringList _railwayOneToManyTransferKeys;
  bool _railwayOneToManyTransferAllKeys;

  std::shared_ptr<ScriptMatchVisitor> _cachedScriptVisitor;
  std::shared_ptr<MatchThreshold> _matchThreshold;
  QMap<QString, Meters> _cachedCustomSearchRadii;
  QMap<QString, double> _candidateDistanceSigmaCache;
  QMap<QString, CreatorDescription> _descriptionCache;

  ElementCriterionPtr _pointPolyPolyCrit;
  ElementCriterionPtr _pointPolyPointCrit;

  CreatorDescription _getScriptDescription(QString path) const;
  /**
   * @brief _validateConfigOptions validates configuration options for the specified feature type
   * being conflated
   * @param baseFeatureType the type of feature to conflate
   */
  void _validateConfig(const CreatorDescription::BaseFeatureType& baseFeatureType);
  /**
   * @brief _validatePluginConfig validates configuration options for the specified feature type
   * being conflated when a call into the conflate script must be made to retrieve information
   * @param baseFeatureType the type of feature to conflate
   * @param plugin conflate script being executed
   * @todo This needs to be moved into _validateConfig but haven't figured out a good way to do it
   * due to needing the plugin object. Also, the type match threshold range checking within this
   * methods being done for rails needs to be extended to all scripts that use it.
   */
  void _validatePluginConfig(const CreatorDescription::BaseFeatureType& baseFeatureType,
                             v8::Local<v8::Object> plugin) const;

  std::shared_ptr<ScriptMatchVisitor> _getCachedVisitor(const ConstOsmMapPtr& map);
};

}

#endif // SCRIPTMATCHCREATOR_H
