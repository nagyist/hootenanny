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
 * @copyright Copyright (C) 2015-2023 Maxar (http://www.maxar.com/)
 */

#include "PhoneNumberNormalizer.h"

// Hoot
#include <hoot/core/elements/Element.h>
#include <hoot/core/util/ConfigOptions.h>
#include <hoot/core/util/StringUtils.h>

// libphonenumber
#include <phonenumbers/phonenumbermatcher.h>
#include <phonenumbers/phonenumbermatch.h>

using namespace i18n::phonenumbers;

namespace hoot
{

PhoneNumberNormalizer::PhoneNumberNormalizer()
  : _regionCode("US"),
    _searchInText(false),
    _format(PhoneNumberUtil::PhoneNumberFormat::NATIONAL),
    _numNormalized(0)
{
}

void PhoneNumberNormalizer::setSearchInText(bool search)
{
  _searchInText = search;
}

void PhoneNumberNormalizer::setRegionCode(QString code)
{
  code = code.trimmed().toUpper();
  if (!code.isEmpty())
  {
    std::set<std::string> regions;
    std::lock_guard<std::mutex> libphonenumber_lock(getPhoneNumberMutex());
    PhoneNumberUtil::GetInstance()->GetSupportedRegions(&regions);
    if (regions.find(code.toStdString()) == regions.end())
      throw IllegalArgumentException("Invalid phone number region code: " + code);
    _regionCode = code;
  }
  else
    throw IllegalArgumentException("Empty phone number region code.");
}

void PhoneNumberNormalizer::setFormat(QString format)
{
  if (format.toUpper() == "E164")
    _format = PhoneNumberUtil::PhoneNumberFormat::E164;
  else if (format.toUpper() == "INTERNATIONAL")
    _format = PhoneNumberUtil::PhoneNumberFormat::INTERNATIONAL;
  else if (format.toUpper() == "NATIONAL")
    _format = PhoneNumberUtil::PhoneNumberFormat::NATIONAL;
  else if (format.toUpper() == "RFC3966")
    _format = PhoneNumberUtil::PhoneNumberFormat::RFC3966;
  else
    throw HootException("Invalid phone number format: " + format);
}

void PhoneNumberNormalizer::setConfiguration(const Settings& conf)
{
  ConfigOptions config = ConfigOptions(conf);
  setRegionCode(config.getPhoneNumberRegionCode());
  setAdditionalTagKeys(config.getPhoneNumberAdditionalTagKeys());
  setSearchInText(config.getPhoneNumberSearchInText());
  setFormat(config.getPhoneNumberNormalizationFormat());
}

void PhoneNumberNormalizer::normalizePhoneNumbers(const ElementPtr& element)
{
  if (_regionCode.isEmpty())
    throw HootException("Phone number normalization requires a region code.");

  // This method has a lot of similarity with PhoneNumberParser::parsePhoneNumber, so look there
  // for explanation detail.
  const Tags& tags = element->getTags();
  for (auto it = tags.constBegin(); it != tags.constEnd(); ++it)
  {
    const QString tagKey = it.key();
    LOG_VART(tagKey);
    if (_additionalTagKeys.contains(tagKey, Qt::CaseInsensitive) || tagKey.contains("phone", Qt::CaseInsensitive))
    {
      const QString tagValue = it.value().toUtf8().trimmed().simplified();
      LOG_VART(tagValue);

      std::lock_guard<std::mutex> libphonenumber_lock(getPhoneNumberMutex());
      if (!_searchInText)
      {
        if (PhoneNumberUtil::GetInstance()->IsPossibleNumberForString(tagValue.toStdString(), _regionCode.toStdString()))
        {
          PhoneNumber parsedPhoneNumber;
          PhoneNumberUtil::ErrorType error =
            PhoneNumberUtil::GetInstance()->Parse(tagValue.toStdString(), _regionCode.toStdString(), &parsedPhoneNumber);
          if (error == PhoneNumberUtil::ErrorType::NO_PARSING_ERROR)
          {
            std::string formattedPhoneNumber;
            PhoneNumberUtil::GetInstance()->Format(parsedPhoneNumber, _format, &formattedPhoneNumber);
            element->getTags().set(tagKey, QString::fromStdString(formattedPhoneNumber));
            LOG_TRACE("Normalized phone number from: " << tagValue << " to: " << formattedPhoneNumber);
            _numNormalized++;
          }
        }
      }
      else
      {
        PhoneNumberMatcher numberFinder(
          *PhoneNumberUtil::GetInstance(), tagValue.toStdString(), _regionCode.toStdString(),
          PhoneNumberMatcher::Leniency::POSSIBLE, 1);
        // arbitrarily populate the original phone number field with the first phone number found,
        // and then concatenate any other phone numbers found together and put in an alt field
        int phoneNumberCount = 0;
        QString phoneNumber;
        QString altPhoneNumbers;
        while (numberFinder.HasNext())
        {
          PhoneNumberMatch match;
          numberFinder.Next(&match);
          phoneNumberCount++;
          // Is normalization here necessary?  Did PhoneNumberMatcher already do it?
          std::string formattedPhoneNumber;
          PhoneNumberUtil::GetInstance()->Format(match.number(), _format, &formattedPhoneNumber);
          // appending all found phone numbers into a single tag value
          if (phoneNumberCount == 1)
            phoneNumber = QString::fromStdString(formattedPhoneNumber);
          else
            altPhoneNumbers += QString::fromStdString(formattedPhoneNumber) + ";";
          _numNormalized++;
        }

        if (phoneNumberCount > 0)
        {
          element->getTags().set(tagKey, phoneNumber);
          LOG_TRACE("Normalized phone number from: " << tagValue << " to: " << phoneNumber);
          if (!altPhoneNumbers.isEmpty())
          {
            altPhoneNumbers.chop(1);
            element->getTags().set("alt_phone", altPhoneNumbers);
            LOG_TRACE(
              "Normalized phone numbers alternates from: " << tagValue << " to: " <<
              altPhoneNumbers);
          }
        }
      }
    }
  }
}

}
