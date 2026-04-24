package @RA_ANDROID_PACKAGE@;

import org.libsdl.app.SDLActivity;

public class @RA_ANDROID_ACTIVITY@ extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] {
            "SDL3",
            "redalert"
        };
    }

    @Override
    protected String[] getArguments() {
        return new String[] {
            "-gamedata",
            "./"
        };
    }
}
