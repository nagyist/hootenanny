#!/bin/bash
set -e

# This tests that any review relations with references to members missing in the data, those either passed into or generated 
# by changeset replacement derivation, do not crash the changeset generation. The actual stats themselves are of 
# secondary importance, as the correctness of changeset generation is tested throroughly elsewhere.

IN_DIR=test-files/cmd/glacial/ChangesetReplacementInvalidReviewRelationTest
OUT_DIR=test-output/cmd/glacial/ChangesetReplacementInvalidReviewRelationTest
mkdir -p $OUT_DIR

CONFIG="--warn -C Testing.conf -D writer.include.debug.tags=true -D uuid.helper.repeatable=true -D reader.add.source.datetime=false -D writer.include.circular.error.tags=false -D test.force.orthographic.projection=false"

# with conflation
# TODO: somehow the Collection Relation conflate part of this got *really* slow recently; re-enable after its sped up
#hoot changeset-derive $CONFIG -D bounds="-77.091256,39.05405,-77.078484,39.057755" $IN_DIR/input1.osm $IN_DIR/input2.osm $OUT_DIR/output1.osc --stats $OUT_DIR/stats1.json --replacement
#diff $IN_DIR/stats1.json $OUT_DIR/stats1.json

# without conflation
hoot changeset-derive $CONFIG -D bounds="-77.091256,39.05405,-77.078484,39.057755" $IN_DIR/input1.osm $IN_DIR/input2.osm $OUT_DIR/output2.osc --stats $OUT_DIR/stats2.json --replacement
# The stats happen to be the same with or without conflation.
diff $IN_DIR/stats2.json $OUT_DIR/stats2.json
