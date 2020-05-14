/*
 * Copyright (C) 2020 Stuart Davies
 *
 * This program is free software: you can redistribute it and/or modify
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
 */
package prephtmlforsketch;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.StandardOpenOption;
import java.util.List;
import java.util.Map;
import java.util.Properties;

/**
 *
 * @author stuar
 */
public class PrepHtmlForSketch {

    private static Properties prop;

    private static int filesCount = 0;

    /**
     * @param args the command line arguments
     */
    public static void main(String[] args) throws IOException {
        if (args.length == 1) {
            File f = new File(args[0]);
            if (f.exists()) {
                prop = new Properties();
                prop.load(new FileInputStream(f));
            } else {
                System.out.println("Properties file " + args[0] + " not found");
                System.exit(1);
            }
        } else {
            System.out.println("Properties file name is required");
            System.exit(1);
        }

        for (Map.Entry<Object, Object> nvp : prop.entrySet()) {
            String key = nvp.getKey().toString();
            if (nvp.getKey().toString().startsWith("name.")) {
                String name = nvp.getValue().toString();
                String fileName = prop.getProperty(name + ".filename", null);
                if (fileName == null) {
                    System.out.println("property " + name + ".filename not defined in file " + args[0]);
                    System.exit(1);
                }
                File f = new File(fileName);
                if (f.exists()) {
                    processFile(f, name);
                } else {
                    System.out.println("HTML file " + fileName + " not found");
                    System.exit(1);
                }
            }
        }

        File f = new File(args[0]);
        if (filesCount == 0) {
            System.out.println("No files were processed.");
            System.exit(1);

        }
    }

    private static void processFile(File f, String name) throws IOException {
        if (f.exists()) {
            StringBuilder sb = new StringBuilder();
            List<String> lines = Files.readAllLines(f.toPath());
            processLines(lines, sb, getBoolean(name+".nl", false));
            String wrap = prop.getProperty(name+".wrap", "%%");
            String out = wrap.replace("%%", sb.toString().trim());
            System.out.println(out);
            File outFile = new File(f.getAbsolutePath() + ".cpp");
            if (outFile.exists()) {
                outFile.delete();
            }
            Files.write(outFile.toPath(), out.getBytes(), StandardOpenOption.CREATE);
            filesCount++;
        } else {
            System.out.println("File does not exist.");
            System.exit(1);
        }

    }

    private static void processLines(List<String> lines, StringBuilder sb, boolean appendNL) {
        for (String l : lines) {
            processLine(l, sb, appendNL);
        }
    }

    private static void processLine(String l, StringBuilder sb, boolean appendNL) {
        StringBuilder lin1 = new StringBuilder();
        for (char c : l.toCharArray()) {
            if (c == '"') {
                lin1.append('\\').append('"');
            } else {
                lin1.append(c);
            }
        }
        StringBuilder lin2 = new StringBuilder();
        int linPos = 0;
        for (int i = 0; i < lin1.length(); i++) {
            if (lin1.charAt(i) > ' ') {
                linPos = i;
                break;
            }
            lin2.append(lin1.charAt(i));
        }
        lin2.append('"');
        for (int i = linPos; i < lin1.length(); i++) {
            lin2.append(lin1.charAt(i));
        }
        if (appendNL) {
            lin2.append("\\n");
        }
        lin2.append('"');
        if (lin2.length() > 2) {
            sb.append(lin2).append('\n');
        }
    }

    private static boolean getBoolean(String key, boolean defaultValue) {
        String v = prop.getProperty(key, Boolean.toString(defaultValue));
        return v.equalsIgnoreCase("true");
    }
}
