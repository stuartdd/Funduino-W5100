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
import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.OpenOption;
import java.nio.file.StandardOpenOption;
import java.util.List;

/**
 *
 * @author stuar
 */
public class PrepHtmlForSketch {

    private static StringBuilder sb = new StringBuilder();
    private static boolean appendNL = false;

    /**
     * @param args the command line arguments
     */
    public static void main(String[] args) throws IOException {
        if (args.length == 0) {
            System.out.println("File Name required");
            System.exit(1);
        }
        if (args.length > 1) {
            appendNL = args[1].equalsIgnoreCase("nl");
        } else {
            appendNL = false;
        }
        File f = new File(args[0]);
        if (f.exists()) {
            List<String> lines = Files.readAllLines(f.toPath());
            processLines(lines, sb);
//            sb.setLength(sb.length()-2);
            String out = "const char htmlPage[] PROGMEM = " + sb.toString().trim() + ";";
            System.out.println(out);
            File outFile = new File(args[0] + ".cpp");
            if (outFile.exists()) {
                outFile.delete();
            }
            Files.write(outFile.toPath(), out.getBytes(), StandardOpenOption.CREATE);
        } else {
            System.out.println("File does not exist.");
            System.exit(1);
        }
    }

    private static void processLines(List<String> lines, StringBuilder sb) {
        for (String l : lines) {
            processLine(l, sb);
        }
    }

    private static void processLine(String l, StringBuilder sb) {
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
}
