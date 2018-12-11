circleci
========

[![Circle CI](https://circleci.com/gh/jandegr/navit/tree/settings.svg?style=svg)](https://circleci.com/gh/jandegr/navit/tree/settings)


A branch to work on routing a bit.

- focussed on OSM only
- a node and a segment can be driven more than once to allow P-turns and such
- the path building/updating became direction aware
- has been tested on Western Europe maps mostly and North-America a bit
- contains no-HOV for North-America
- applies a penalty for (really)sharp turns
- can work with both Dijkstra and A_star, but A_star is not preferred at present
