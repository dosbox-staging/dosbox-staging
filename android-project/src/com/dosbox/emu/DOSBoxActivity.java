package com.dosbox.emu;

import org.libsdl.app.SDLActivity;
import android.content.Context;
import android.content.pm.ActivityInfo;
import android.content.res.Configuration;
import android.os.Bundle;
import android.view.Surface;
import android.view.KeyEvent;
import android.view.inputmethod.InputMethodManager;

/* 
 * A sample wrapper class that just calls SDLActivity 
 */ 

public class DOSBoxActivity extends SDLActivity {
    /* Based on volume keys related patch from bug report:
    http://bugzilla.libsdl.org/show_bug.cgi?id=1569     */

    // enable to intercept keys before SDL gets them
    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        switch (event.getKeyCode()) {
        // Show/Hide on-screen keyboard (but don't toggle text input mode)
        case KeyEvent.KEYCODE_BACK:
            if (event.getAction() == KeyEvent.ACTION_UP) {
                toggleOnScreenKeyboard();
            }
            return true;
        }
        return super.dispatchKeyEvent(event);
    }

    // Fix the initial orientation
    protected void onCreate(Bundle savedInstanceState) {
        /* Use deprecated getOrientation() rather
        than getRotation() to support API<8    */
/*
        int rotation = getWindowManager().getDefaultDisplay().getOrientation();
        if (getResources().getConfiguration().orientation == Configuration.ORIENTATION_PORTRAIT) {
            if ((rotation == Surface.ROTATION_0) || (rotation == Surface.ROTATION_90))
                setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
            else
                setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_REVERSE_PORTRAIT);
        } else { // Landscape
            if ((rotation == Surface.ROTATION_0) || (rotation == Surface.ROTATION_90))
                setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
            else
                setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_REVERSE_LANDSCAPE);
        }
*/
        super.onCreate(savedInstanceState); // Initialize the rest (e.g. SDL)
    }

    public void toggleOnScreenKeyboard() {
        InputMethodManager imm = (InputMethodManager) getSystemService(Context.INPUT_METHOD_SERVICE);
        imm.toggleSoftInput(0, 0);
    }
}
