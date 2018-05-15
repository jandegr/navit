package org.navitproject.navit;

import java.io.File;

public class NavitMap {

    final String mapName;
    private final String fileName;
    private final String mapPath;

    NavitMap(String path, String mapFileName) {
        mapPath = path;
        fileName = mapFileName;
        if (mapFileName.endsWith(".bin")) {
            mapName = mapFileName.substring(0, mapFileName.length() - 4);
        } else {
            mapName = mapFileName;
        }
    }

    NavitMap(String mapLocation) {
        File mapFile = new File(mapLocation);

        mapPath = mapFile.getParent() + "/";
        fileName = mapFile.getName();
        if (fileName.endsWith(".bin")) {
            mapName = fileName.substring(0, fileName.length() - 4);
        } else {
            mapName = fileName;
        }
    }

    public long size() {
        File mapFile = new File(mapPath + fileName);
        return mapFile.length();
    }

    public String getLocation() {
        return mapPath + fileName;
    }
}
