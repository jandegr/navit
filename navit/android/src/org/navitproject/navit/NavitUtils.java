package org.navitproject.navit;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;

class NavitUtils {


    static void removeFileIfExists(String source) {
        File file = new File(source);

        if (!file.exists()) {
            return;
        }

        file.delete();
    }

    static void copyFileIfExists(String source, String destination) throws IOException {
        File file = new File(source);

        if (!file.exists()) {
            return;
        }

        FileInputStream is = null;
        FileOutputStream os = null;

        try {
            is = new FileInputStream(source);
            os = new FileOutputStream(destination);

            int len;
            byte[] buffer = new byte[1024];

            while ((len = is.read(buffer)) != -1) {
                os.write(buffer, 0, len);
            }
        } finally {
            /* Close the FileStreams to prevent Resource leaks */
            if (is != null) {
                is.close();
            }

            if (os != null) {
                os.close();
            }
        }
    }

    // mapFilenamePath =  some absolute path optionally ending with /, can be null
    // filetype is null, bin, .bin, txt, .txt .....
    static String pathConcat(String mapFilenamePath, String fileName, String filetype) {

        String returnPath = "";
        if (mapFilenamePath != null && mapFilenamePath.length() > 0) {
            returnPath = mapFilenamePath;
            if (!mapFilenamePath.endsWith("/") && !fileName.startsWith("/")) {
                returnPath = returnPath + '/';
            }
        }

        returnPath = returnPath + fileName;

        if (filetype != null && !filetype.startsWith(".")) {
            returnPath = returnPath + '.';
        }
        if (filetype != null) {
            returnPath = returnPath + filetype;
        }


        return returnPath;
    }
}
