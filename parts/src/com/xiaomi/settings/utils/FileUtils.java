/*
 * Copyright (C) 2022-2024 Paranoid Android
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package com.xiaomi.settings.utils;

import android.util.Log;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;

public final class FileUtils {

    private static final String TAG = "XiaomiPartsFileUtils";
    private static final boolean DEBUG = true;

    public static String readLine(String fileName) {
        String line = null;
        BufferedReader reader = null;
        try {
            reader = new BufferedReader(new FileReader(fileName), 512);
            line = reader.readLine();
        } catch (FileNotFoundException e) {
            Log.w(TAG, "No such file " + fileName + " for reading", e);
        } catch (IOException e) {
            Log.e(TAG, "Could not read from file " + fileName, e);
        } finally {
            try {
                if (reader != null) {
                    reader.close();
                }
            } catch (IOException e) { }
        }
        return line;
    }

    public static int readLineInt(String fileName) {
        try {
            return Integer.parseInt(readLine(fileName).replace("0x", ""));
        }
        catch (NumberFormatException e) {
            Log.e(TAG, "Could not convert string to int from file " + fileName, e);
        }
        return 0;
    }

    public static void writeLine(String fileName, String value) {
        BufferedWriter writerValue = null;
        try {
            writerValue = new BufferedWriter(new FileWriter(fileName));
            writerValue.write(value);
        } catch (FileNotFoundException e) {
            Log.w(TAG, "No such file " + fileName + " for writing", e);
        } catch (IOException e) {
            Log.e(TAG, "Could not write to file " + fileName, e);
        } finally {
            try {
                if (writerValue != null) {
                    writerValue.close();
                }
            } catch (IOException e) {
                // Ignored
            }
        }
    }

    public static void writeLine(String fileName, int value) {
        writeLine(fileName, Integer.toString(value));
    }

    public static boolean fileExists(String fileName) {
        final File file = new File(fileName);
        return file.exists();
    }

    public static boolean isFileReadable(String fileName) {
        final File file = new File(fileName);
        return file.exists() && file.canRead();
    }

    public static boolean isFileWritable(String fileName) {
        final File file = new File(fileName);
        return file.exists() && file.canWrite();
    }

    public static boolean delete(String fileName) {
        final File file = new File(fileName);
        boolean ok = false;
        try {
            ok = file.delete();
        } catch (SecurityException e) {
            Log.w(TAG, "SecurityException trying to delete " + fileName, e);
        }
        return ok;
    }

    public static boolean rename(String srcPath, String dstPath) {
        final File srcFile = new File(srcPath);
        final File dstFile = new File(dstPath);
        boolean ok = false;
        try {
            ok = srcFile.renameTo(dstFile);
        } catch (SecurityException e) {
            Log.w(TAG, "SecurityException trying to rename " + srcPath + " to " + dstPath, e);
        } catch (NullPointerException e) {
            Log.e(TAG, "NullPointerException trying to rename " + srcPath + " to " + dstPath, e);
        }
        return ok;
    }

    public static String getFileValue(String fileName, String defaultValue) {
        String value = defaultValue;
        BufferedReader reader = null;

        try {
            reader = new BufferedReader(new FileReader(fileName), 512);
            value = reader.readLine();
        } catch (FileNotFoundException e) {
            Log.w(TAG, "No such file " + fileName + " for reading", e);
        } catch (IOException e) {
            Log.e(TAG, "Could not read from file " + fileName, e);
        } finally {
            try {
                if (reader != null) {
                    reader.close();
                }
            } catch (IOException e) {
                // Ignored
            }
        }
        return value;
    }
}