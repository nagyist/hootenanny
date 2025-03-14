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
 * @copyright Copyright (C) 2014 Maxar (http://www.maxar.com/)
 */

//
// Convert NFDD English back to OSM+
//

hoot.require('etds61_osm');


function initialize()
{
  // Turn off the TDS structure so we just get the raw feature
  hoot.Settings.set({'writer.thematic.structure':'false'});
}


// IMPORT
// translateToOsm - Normally takes 'attrs' and returns OSM 'tags'.  This version
//    converts OSM+ tags to NFDD "English" Attributes
//
// This can be called via the following for testing:
// hoot convert -D "convert.ops=SchemaTranslationVisitor"  \
//      -D schema.translation.script=$HOOT_HOME/translations/script.js <input>.osm <output>.osm
//
function translateToOsm(attrs, layerName, geometryType)
{

  // We use the temp var because nfdd_e.toEnglish returns "attrs" and "tableName"
  var output = etds61_osm.toOSM(attrs, layerName, geometryType);

  // Make sure the returned value isn't NULL. This does occur
  if (output)
  {
    return output.attrs;
  }
  else
  {
    return null;
  }

} // End of translateToOsm


// EXPORT
// translateToOgr - takes 'tags' + geometry and returns 'attrs' + tableName
//    This version converts OSM+ tags to NFDD "English" attributes
function translateToOgr(tags, elementType, geometryType)
{
  return etds61_osm.toOSM(tags, elementType, geometryType);
} // End of translateToOgr






