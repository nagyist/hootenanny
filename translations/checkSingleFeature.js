#!/usr/bin/node

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
 * @copyright Copyright (C) 2021 Maxar (http://www.maxar.com/)
 */

//
// Script to check translations for a single F_CODE and attributes or translating OSM to a schema
//

var HOOT_HOME = process.env.HOOT_HOME;

transTest = require(HOOT_HOME + '/translations/checkTranslations.js');

// Set the logging level for the output
// error, warn, info, debug, trace
hoot.Log.setLogLevel("warn");

// Skip the TransportationGroundCrv type layers
hoot.Settings.set({"writer.thematic.structure":"true"});

//hoot.Settings.set({"ogr.debug.dumptags":"true"});

// LOTS of debug output
// hoot.Settings.set({"ogr.debug.dumptags":"true"});

// Set this to false to  keep  default/usless values
hoot.Settings.set({"reader.drop.defaults":"true"});

// ####################################################################################

// testTranslated:  schema, F_CODE, {attribute:value}. ['Point','Line','Area']
// testOSM:  schema, {tag:value}, ['Point','Line','Area']

// NOTE: if the geometry is not specified, the default is to try all geometries

// Schema List:
// TDSv40, TDSv61, TDSv70, TDSv71, MGCP, GGDMv30


//console.log('Just the F_CODE');
//transTest.testTranslated('TDSv71','AL013');

//console.log('Checking soil surface features');
//transTest.testTranslated('TDSv71','DA010',{'TSM':'13'});

//console.log('Checking waterway=boatyard features OSM to TDS');
//transTest.testOSM('TDSv71',{'landuse':'brownfield'});

//console.log('\nF_CODE with attributes');
//transTest.testTranslated('MGCP','AQ040',{'FUN':'6','NOS':'2','SDP':'DigitalGLobe','OSMTAGS':'{\"security:classification\":\"UNCLASSIFIED\"}'});

//console.log('\nF_CODE with default attributes');
//transTest.testTranslated('MGCP','AQ040',{'VOI':'N_A','OHB':'-32767.0','FUN':'0','NOS':'2','SDP':'DigitalGLobe','OSMTAGS':'{\"security:classification\":\"UNCLASSIFIED\"}'});

//console.log('\nF_CODE with attributes');
//transTest.testTranslated('MGCP','AL015',{'HWT':'20','FFN':'850'},['Point']);

//console.log('\nEB010 Area with names and unknown grass type');
//transTest.testTranslated('MGCP','EB010',{'NAM':'Feature Name','NFI':'NFI String','NFN':'NFN String','VEG':'0'},['Area']);

//console.log('\nEB010 Area with names and grass');
//transTest.testTranslated('MGCP','EB010',{'NAM':'Feature Name','NFI':'NFI String','NFN':'NFN String','VEG':'8'},['Area']);

//console.log('\nOSM to TDS');
//transTest.testOSM('TDSv71',{'bridge':'yes'});

console.log('\nOSM to MGCP');
transTest.testOSM('MGCP',{'aeroway':'launchpad'});
//transTest.testOSM('MGCP',{'water':'moat','natural':'water'});
//transTest.testOSM('MGCP',{'natural':'water'});

//console.log('\nOSM to GGDMv30');
//transTest.testOSM('GGDMv30',{'highway':'yes','width':'10'},['Line']);
//console.log('\nOSM to MGCP');
//transTest.testOSM('MGCP',{'poi':'yes','amenity':'cafe','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'});

//console.log('\namenity: language_school')
//transTest.testOSM('MGCP',{'amenity':'language_school','barrier':'wall','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'});

//console.log('\nHISTORIC: CASTLE\n\n\n')
//transTest.testOSM('MGCP',{'historic':'castle','castle_type':'fortress','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'});

//console.log('\ndepot:bus,landuse:brownfield')
//transTest.testOSM('MGCP',{'depot':'bus','landuse':'brownfield','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'});

//console.log('\namenity: community_centre')
//transTest.testOSM('MGCP',{'amenity':'community_centre','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'});

console.log('\nnatural:water,water:lagoon')
transTest.testOSM('MGCP',{'natural':'water','water':'lagoon','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'});

console.log('\nnatural:water,water:pond')
transTest.testOSM('MGCP',{'natural':'water','water':'pond','salt':'yes','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'});

console.log('\nwetland:wet_meadow,natural:wetland')
transTest.testOSM('MGCP',{'wetland':'wet_meadow','natural':'wetland','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'});

//console.log('\nlanduse:commercial')
//transTest.testOSM('MGCP',{'landuse':'commercial','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'});

//console.log('\namenity:refugee_site')
//transTest.testOSM('MGCP',{'amenity':'refugee_site','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'});

//console.log('\ncamp:yes')
//transTest.testOSM('MGCP',{'camp':'yes','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'});

//console.log('\nlanduse:orchard crop:sugarcane')
//transTest.testOSM('MGCP',{'landuse':'orchard','crop':'sugarcane','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})
//console.log('\nOSM to GGDMv30');
//transTest.testOSM('GGDMv30',{'highway':'yes','width':'10'},['Line']);

console.log('\nman_made=causeway (fake osm tag)')
transTest.testOSM('MGCP',{'man_made':'causeway','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log('\nembankment=yes')
transTest.testOSM('MGCP',{'highway':'motorway','embankment':'yes','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log('\nman_made=embankment')
transTest.testOSM('MGCP',{'man_made':'embankment','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log('\namenity=checkpoint')
transTest.testOSM('MGCP',{'amenity':'checkpoint','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log('\nnatural=esker')
transTest.testOSM('MGCP',{'natural':'esker','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log('\nnatural=ridge ridge=esker')
transTest.testOSM('MGCP',{'natural':'ridge','ridge':'esker','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log('\nlanduse=farmyard')
transTest.testOSM('MGCP',{'landuse':'farmyard','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log('\nman_made=works')
transTest.testOSM('MGCP',{'man_made':'works','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log('\nlanduse=industrial utilities=*')
transTest.testOSM('MGCP',{'landuse':'industrial','utilities':'asdf','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log('\nlanduse=commercial')
transTest.testOSM('MGCP',{'landuse':'commercial','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log('\npublic_transport=station')
transTest.testOSM('MGCP',{'public_transport':'station','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log('\ntourism=hotel')
transTest.testOSM('MGCP',{'tourism':'hotel','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log('\nlanduse=residential')
transTest.testOSM('MGCP',{'landuse':'residential','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log('\noffice=telecommunication')
transTest.testOSM('MGCP',{'office':'telecommunication','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log('\nlanduse=civic_admin')
transTest.testOSM('MGCP',{'landuse':'civic_admin','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log('\noffice=government')
transTest.testOSM('MGCP',{'office':'government','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log('\noffice=diplomatic')
transTest.testOSM('MGCP',{'office':'diplomatic','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log('\nlanduse=military')
transTest.testOSM('MGCP',{'landuse':'military','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log('\namenity=prison')
transTest.testOSM('MGCP',{'amenity':'prison','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log('\namenity=school OPTIONAL: office=educational_institution')
transTest.testOSM('MGCP',{'amenity':'school','office':'educational_institution','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log('\namenity=hospital (or clinic, doctors, dentist)')
transTest.testOSM('MGCP',{'amenity':'hospital','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log('\nleisure=garden OPTIONAL garden_type=botanical, or tourism=zoo')
transTest.testOSM('MGCP',{'leisure':'garden', 'uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log('\nleisure=sports_centre')
transTest.testOSM('MGCP',{'leisure':'sports_centre','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log('\nlanduse=religious')
transTest.testOSM('MGCP',{'landuse':'religious','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

//console.log('\nbuilding=yes collapse_poly test')
//transTest.testOSM('MGCP',{'building':'yes','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log('\nman_made=silo content=grain')
transTest.testOSM('MGCP',{'man_made':'silo','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log('\nlanduse=harbour')
transTest.testOSM('MGCP',{'landuse':'harbour','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log('\nlanduse=industrial, industrial=hydrocarbons_field')
transTest.testOSM('MGCP',{'landuse':'industrial', 'industrial':'hydrocarbons_field','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})
transTest.testOSM('MGCP',{'landuse':'industrial', 'industrial':'oil', 'uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log('\nnatural=ice_cliff')
console.log('\nnatural=cliff, surface=ice')
transTest.testOSM('MGCP',{'natural':'ice_cliff','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})
transTest.testOSM('MGCP',{'natural':'cliff', 'surface':'ice', 'uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log('\nnatural=peak, surface=ice')
transTest.testOSM('MGCP',{'natural':'peak', 'surface':'ice','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})
transTest.testOSM('MGCP',{'natural':'peak', 'uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log('\nmilitary=base')
transTest.testOSM('MGCP',{'military':'base', 'uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})
transTest.testOSM('MGCP',{'military':'installation', 'uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log('\nman_made=heap')
transTest.testOSM('MGCP',{'man_made':'heap', 'uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})
transTest.testOSM('MGCP',{'man_made':'heap', 'resource':'coal', 'uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})
transTest.testOSM('MGCP',{'man_made':'heap', 'resource':'dimension_stone', 'uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})
transTest.testOSM('MGCP',{'man_made':'heap', 'resource':'mithril', 'uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})
transTest.testOSM('MGCP',{'man_made':'heap', 'resource':'sand','uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})
transTest.testOSM('MGCP',{'landuse':'mineral_pile', 'uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log('\nman_made=mast mooring=yes')
transTest.testOSM('MGCP',{'seamark:type':'mooring', 'uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})
transTest.testOSM('MGCP',{'man_made':'mast', 'mooring':'yes', 'uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})
transTest.testOSM('MGCP',{'man_made':'tower', 'mooring':'yes', 'uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log('\nman_made=offshore_platform')
transTest.testOSM('MGCP',{'man_made':'offshore_construction', 'uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})
transTest.testOSM('MGCP',{'man_made':'offshore_platform', 'uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log('\nman_made=particle_accelerator')
transTest.testOSM('MGCP',{'man_made':'particle_accelerator', 'uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})
transTest.testOSM('MGCP',{'amenity':'research_institute', 'research':'particle_physics', 'uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})
transTest.testOSM('MGCP',{'amenity':'research_institute', 'research':'particle_physics', 'operator':'Anonymous Scientist', 'uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})

console.log("\nhighway=race_way sport=motor")
transTest.testOSM('MGCP',{'highway':'race_way', 'uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})
transTest.testOSM('MGCP',{'highway':'race_way', 'sport':'motor', 'uuid':'{4632d15b-7c44-4ba1-a0c4-8cfbb30e39d4}'})
// End
