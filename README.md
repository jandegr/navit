

[![Circle CI](https://circleci.com/gh/jandegr/navit/tree/arm64_2.svg?style=svg)](https://circleci.com/gh/jandegr/navit/tree/arm64_2)
[![CodeFactor](https://www.codefactor.io/repository/github/jandegr/navit/badge/arm64_2)](https://www.codefactor.io/repository/github/jandegr/navit/overview/arm64_2)

- windows 64 build includes maptool

- focussed on OSM only
- a node and a segment can be driven more than once to allow P-turns and such
- the path building/updating became direction aware
- contains no-HOV for North-America
- applies a penalty for (really)sharp turns
- can work with both Dijkstra and A_star, but A_star is not preferred at present
