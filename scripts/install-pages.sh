#!/bin/bash

set -ev

(cd src && scons)

outputbase=.

output=$outputbase/GrappleMap

mkdir -p $output/{gifframes,composer,search}

wget --no-verbose -O $output/babylon.js "https://raw.githubusercontent.com/BabylonJS/Babylon.js/master/dist/preview%20release/babylon.js"
wget --no-verbose -O $output/hand.js "https://raw.githubusercontent.com/deltakosh/handjs/master/bin/hand.min.js"

cp src/gm.js $output/
cp src/composer.html $output/composer/index.html
cp src/composer.js $output/composer/
cp src/search.html $output/search/index.html
cp src/search.js $output/search/

src/grapplemap-browse --output_dir=$outputbase
