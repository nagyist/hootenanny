var assert = require('assert'),
    osmtogeojson = require('osmtogeojson'),
    DOMParser = new require('@xmldom/xmldom').DOMParser,
    parser = new DOMParser();

var server = require('../TranslationServer.js');

describe('Landcover: Sand areas', function () {
    var schemas = [
        'TDSv71',
        'TDSv70',
        'TDSv61',
        'TDSv40',
        'MGCP',
        'GGDMv30'
    ];

    schemas.forEach(schema => {
        it('should translate "natural=sand" to ' + schema + ' Soil Surface Region (DA010) and back again', function() {
            var osm = '<osm version="0.6" upload="true" generator="hootenanny">\
            <node id="-10" lat="0.68307256979" lon="18.45073925651" />\
            <node id="-11" lat="0.68341620728" lon="18.45091527847" />\
            <node id="-12" lat="0.68306209303" lon="18.45157116983" />\
            <node id="-13" lat="0.68270797876" lon="18.45141400736" />\
            <way id="-19" >\
            <nd ref="-10" />\
            <nd ref="-11" />\
            <nd ref="-12" />\
            <nd ref="-13" />\
            <nd ref="-10" />\
            <tag k="natural" v="sand"/>\
            </way></osm>';
            // <tag k="area" v="yes"/>\


            //Test translating from OSM to Schema
            var translated = server.handleInputs({osm: osm, method: 'POST', translation: schema, path: '/translateTo'});
            var xml = parser.parseFromString(translated);
            assert.equal(xml.getElementsByTagName("osm")[0].getAttribute("schema"), schema);

            var tags = osmtogeojson(xml).features[0].properties;

            var fcode = (schema == 'MGCP') ? 'FCODE' : 'F_CODE';
            assert.equal(tags[fcode], 'DA010', fcode + ' should equal');


            //Test translation from Schema back to OSM
            osm = server.handleInputs({osm: translated, method: 'POST', translation: schema, path: '/translateFrom'});
            xml = parser.parseFromString(osm);
            assert.equal(xml.getElementsByTagName("osm")[0].getAttribute("schema"), "OSM");

            tags = osmtogeojson(xml).features[0].properties;

            assert.equal(tags['natural'], 'sand', 'natural tag should equal');
            // assert.equal(tags['area'], 'yes', 'area tag should equal');

        });
    });

});
