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
 * @copyright Copyright (C) 2017, 2018, 2019, 2020, 2021, 2022 Maxar (http://www.maxar.com/)
 */
#ifndef IMPLICITTAGRAWRULESDERIVER_H
#define IMPLICITTAGRAWRULESDERIVER_H

// Hoot
#include <hoot/core/util/Configurable.h>
#include <hoot/core/criterion/ImplicitTagEligibleCriterion.h>
#include <hoot/core/io/ElementInputStream.h>
#include <hoot/core/io/PartialOsmMapReader.h>
#include <hoot/core/algorithms/string/StringTokenizer.h>
#include <hoot/core/language/ToEnglishTranslator.h>

// Qt
#include <QTemporaryFile>

namespace hoot
{

class Tags;

/**
 * Used to derive implicit tag raw rules for elements and writes the output to a flat file
 *
 * The logic in this class is separated from that in ImplicitTagRulesDatabaseDeriver due to the fact
 * that the logic in this class takes a significant amount of time to run against large input
 * datasets (~5.5 hours to run against both global GeoNames and OSM at the time of this writing).
 * The raw rules generated by this class can then be tweaked repeatedly with a number of settings
 * when deriving the rules database with ImplicitTagRulesDatabaseDeriver.  Databases typically take
 * less than a minute to generate.  So from one long raw rules file processing run by this class,
 * many test rules databases can be generated by ImplicitTagRulesDatabaseDeriver.
 */
class ImplicitTagRawRulesDeriver : public Configurable
{

public:

  ImplicitTagRawRulesDeriver();
  ~ImplicitTagRawRulesDeriver() override = default;

  /**
   * Derives implicit tag rules for POIs given input data and writes the rules to output
   *
   * @param inputs a list of hoot supported feature input formats to derive rules from
   * @param translationScripts list of OSM translation scripts corresponding to the datasets
   * specified by the inputs parameter
   * @param output the file to write the rules to
   */
  void deriveRawRules(const QStringList& inputs, const QStringList& translationScripts, const QString& output);

  void setConfiguration(const Settings& conf) override;

  void setSortParallelCount(int count) { _sortParallelCount = count; }
  void setSkipFiltering(bool skip) { _skipFiltering = skip; }
  void setKeepTempFiles(bool keep) { _keepTempFiles = keep; }
  void setTempFileDir(const QString& dir) { _tempFileDir = dir; }
  void setTranslateNamesToEnglish(bool translate) { _translateNamesToEnglish = translate; }
  void setElementCriterion(const QString& criterionName);

private:

  //for testing
  friend class ImplicitTagRawRulesDeriverTest;

  long _statusUpdateInterval;
  long _countFileLineCtr;
  //number of threads to use when calling Unix sort command; too high of a value in some VM
  //environments can cause memory issues due to the VM OS not releasing the memory used by the
  //command after it finishes
  int _sortParallelCount;
  //completely skip filtering out ineligible elements (those which don't satisfy _elementCriterion);
  //to be used only when the input data has been pre-filtered
  bool _skipFiltering;
  //will keep all temp files; very useful for debugging sort work done by the Unix commands
  bool _keepTempFiles;
  QString _tempFileDir;
  //if true; all element names are first translated to english before a raw rule is derived
  bool _translateNamesToEnglish;

  //contains the word/tag occurrence counts at various stages; line format:
  //<count>\t<word>\t<key=value>
  std::shared_ptr<QTemporaryFile> _countFile;
  std::shared_ptr<QTemporaryFile> _sortedCountFile;
  std::shared_ptr<QTemporaryFile> _dedupedCountFile;
  std::shared_ptr<QTemporaryFile> _tieResolvedCountFile;

  //final output file
  std::shared_ptr<QFile> _output;

  //maps the a name token concatenated with a tag key to a tag value
  QHash<QString, QString> _wordKeysToCountsValues;
  //same as above but used to combine duplicated count file lines into single lines
  QHash<QString, QStringList> _duplicatedWordTagKeyCountsToValues;

  StringTokenizer _tokenizer;

  std::shared_ptr<PartialOsmMapReader> _inputReader;

  //controls which elements have tags harvested from them
  std::shared_ptr<ImplicitTagEligibleCriterion> _elementCriterion;

  //translates names to English
  std::shared_ptr<ToEnglishTranslator> _translator;

  void _init();
  void _validateInputs(const QStringList& inputs, const QStringList& translationScripts,
                       const QString& output);
  std::shared_ptr<ElementInputStream> _getInputStream(const QString& input,
                                                      const QString& translationScript);

  /*
   * Examine each word token to determine if a raw implicit tag rule should be created for it
   */
  void _updateForNewWord(const QString& word, const QString& kvp);
  /*
   * Gets tags to generate raw implicit tag rules from
   */
  QStringList _getPoiKvps(const Tags& tags) const;

  void _parseNames(const QStringList& names, const QStringList& kvps);
  void _parseNameToken(QString& nameToken, const QStringList& kvps);

  /*
   * Sorts word/tag occurrence count lines by descending occurrence count
   */
  void _sortByTagOccurrence();
  /*
   * Ensure that no two tag keys have the same word and occurrence count line
   */
  void _removeDuplicatedKeyTypes();
  /*
   * In cases where these is a word/tag key occurrence count tie, this resolves the tie.
   */
  void _resolveCountTies();
  void _sortByWord(const std::shared_ptr<QTemporaryFile>& input) const;
};

}

#endif // IMPLICITTAGRAWRULESDERIVER_H
