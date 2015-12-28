A branch to work on routing a bit.

- focussed on OSM only
- a node and a segment can be driven more than once to allow P-turns and such
- the path building/updating became direction aware
- has been tested on Western Europe maps mostly and North-America a bit
- contains no-HOV for North-America
- applies a penalty for (really)sharp turns
- can work with both Dijkstra and A*, but A* is not preferred at present
- for the changes in maptool look in the maptool branches in this repo

circleci
========

[![Circle CI](https://circleci.com/gh/jandegr/navit/tree/ext_graph_prep.svg?style=svg)](https://circleci.com/gh/jandegr/navit/tree/ext_graph_prep)
