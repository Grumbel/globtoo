// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

package io.github.Grumbel.globtoo;

import android.os.Bundle;
import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import org.libsdl.app.SDLActivity;

public final class GlobtooActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL3", "main" };
    }

    @Override
    protected void onCreate(Bundle state) {
        // Native resource readers use ordinary filesystem paths.
        try {
            copyAssets("assets");
            copyAssets("shaders");
        } catch (IOException e) {
            throw new IllegalStateException("Cannot unpack Globtoo resources", e);
        }
        super.onCreate(state);
    }

    private void copyAssets(String directory) throws IOException {
        File destination = new File(getFilesDir(), directory);
        if (!destination.mkdirs() && !destination.isDirectory()) {
            throw new IOException("Cannot create " + destination);
        }
        byte[] buffer = new byte[16384];
        for (String name : getAssets().list(directory)) {
            try (InputStream input = getAssets().open(directory + "/" + name);
                 FileOutputStream output = new FileOutputStream(new File(destination, name))) {
                int count;
                while ((count = input.read(buffer)) != -1) {
                    output.write(buffer, 0, count);
                }
            }
        }
    }
}
