package com.kryga;

import org.libsdl.app.SDLActivity;

/**
 * Kryga engine entry activity. Delegates lifecycle and surface management to
 * SDL's SDLActivity; native code is loaded from libmain.so which defines the
 * SDL_main entry (via SDL2's main redirection).
 */
public class KrygaActivity extends SDLActivity {

    @Override
    protected String[] getLibraries() {
        // Order matters: SDL2 must load before libmain, which depends on it.
        return new String[] {
            "SDL2",
            "main"
        };
    }
}
