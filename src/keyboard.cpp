// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#define NONE 0x00
#define CONTROL 0x01
#define ALT 0x02
#define SHIFT 0x04
#define CONTROL_SHIFT (CONTROL | SHIFT)
#define CONTROL_ALT (CONTROL | ALT)
#define ALT_SHIFT (ALT | SHIFT)

BOOL IsSalHotKey(WORD hotKey)
{
    BYTE vk = LOBYTE(hotKey);
    //TRACE_I("vk="<<std::hex<<(WORD)vk);
    BYTE hk_mods = HIBYTE(hotKey);

    BYTE mods = NONE;

    if ((hk_mods & HOTKEYF_CONTROL) != 0)
        mods += CONTROL;
    if ((hk_mods & HOTKEYF_SHIFT) != 0)
        mods += SHIFT;
    if ((hk_mods & HOTKEYF_ALT) != 0)
        mods += ALT;

    BOOL found = FALSE;
    switch (vk)
    {

    case VK_BACK: // 0x08
    {
        switch (mods)
        {
        case NONE:          // up directory
        case CONTROL:       // root
        case SHIFT:         // root
        case CONTROL_SHIFT: // delete word (command line)
            found = TRUE;
        }
        break;
    }

    case VK_TAB: // 0x09
    {
        switch (mods)
        {
        case NONE:    // switch panels
        case CONTROL: // go to/from command line
            found = TRUE;
        }
        break;
    }

    case VK_RETURN: // 0x0D
    {
        switch (mods)
        {
        case NONE:          // execute
        case CONTROL:       // brings filename to command line
        case ALT:           // open properties dialog box
        case CONTROL_SHIFT: // brings filename to command line (8.3)
            found = TRUE;
        }
        break;
    }

    case VK_SPACE: // 0x20
    {
        switch (mods)
        {
        case NONE:          // select + calculate directory size
        case CONTROL:       // brings current dir to command line
        case ALT:           // window menu
        case SHIFT:         // quick search
        case CONTROL_SHIFT: // brings current dir to command line (8.3)
            found = TRUE;
        }
        break;
    }

    case VK_PRIOR: // 0x21
    {
        switch (mods)
        {
        case NONE:    // page up
        case CONTROL: // root directory
        case SHIFT:   // select + page up
            found = TRUE;
        }
        break;
    }

    case VK_NEXT: // 0x22
    {
        switch (mods)
        {
        case NONE:    // page down
        case CONTROL: // enter
        case SHIFT:   // select + page down
            found = TRUE;
        }
        break;
    }

    case VK_END: // 0x23
    {
        switch (mods)
        {
        case NONE:  // end
        case SHIFT: // select + end
            found = TRUE;
        }
        break;
    }

    case VK_HOME: // 0x24
    {
        switch (mods)
        {
        case NONE:    // home
        case CONTROL: // first file
        case SHIFT:   // select + home
            found = TRUE;
        }
        break;
    }

    case VK_LEFT: // 0x25
    {
        switch (mods)
        {
        case NONE:          // left
        case CONTROL:       // scroll left, keep cursor
        case ALT:           // history: back
        case SHIFT:         // select + left
        case CONTROL_SHIFT: // focus in second panel
            found = TRUE;
        }
        break;
    }

    case VK_UP: // 0x26
    {
        switch (mods)
        {
        case NONE:    // up
        case CONTROL: // scroll up, keep cursor
        case ALT:     // up to selected item
        case SHIFT:   // select + up
            found = TRUE;
        }
        break;
    }

    case VK_RIGHT: // 0x27
    {
        switch (mods)
        {
        case NONE:          // right
        case CONTROL:       // scroll right, keep cursor
        case ALT:           // history: forward
        case SHIFT:         // select + right
        case CONTROL_SHIFT: // focus in second panel
            found = TRUE;
        }
        break;
    }

    case VK_DOWN: // 0x28
    {
        switch (mods)
        {
        case NONE:    // down
        case CONTROL: // scroll down, keep cursor
        case ALT:     // down to selected item
        case SHIFT:   // select + down
            found = TRUE;
        }
        break;
    }

    case VK_INSERT: // 0x2D
    {
        switch (mods)
        {
        case NONE:          // select/unselect
        case CONTROL:       // clipboard copy
        case ALT:           // copy path + name as text
        case SHIFT:         // clipboard paste
        case CONTROL_SHIFT: // copy UNC path + name as text
        case CONTROL_ALT:   // copy path as text
        case ALT_SHIFT:     // copy name as text
            found = TRUE;
        }
        break;
    }

    case VK_DELETE: // 0x2E
    {
        switch (mods)
        {
        case NONE:  // delete
        case SHIFT: // delete
            found = TRUE;
        }
        break;
    }

    // VK_0 thru VK_9 are the same as ASCII '0' thru '9' (0x30 - 0x39)
    case '0':
    {
        switch (mods)
        {
        case NONE:          // quick search/type in command line
        case CONTROL:       // hot paths
        case ALT:           // panel mode
        case SHIFT:         // hot paths
        case CONTROL_SHIFT: // define hot path
            found = TRUE;
        }
        break;
    }

    case '1':
    {
        switch (mods)
        {
        case NONE:          // quick search/type in command line
        case CONTROL:       // hot paths
        case ALT:           // panel mode
        case SHIFT:         // hot paths
        case CONTROL_SHIFT: // define hot path
            found = TRUE;
        }
        break;
    }

    case '2':
    {
        switch (mods)
        {
        case NONE:          // quick search/type in command line
        case CONTROL:       // hot paths
        case ALT:           // panel mode
        case SHIFT:         // hot paths
        case CONTROL_SHIFT: // define hot path
            found = TRUE;
        }
        break;
    }

    case '3':
    {
        switch (mods)
        {
        case NONE:          // quick search/type in command line
        case CONTROL:       // hot paths
        case ALT:           // panel mode
        case SHIFT:         // hot paths
        case CONTROL_SHIFT: // define hot path
            found = TRUE;
        }
        break;
    }

    case '4':
    {
        switch (mods)
        {
        case NONE:          // quick search/type in command line
        case CONTROL:       // hot paths
        case ALT:           // panel mode
        case SHIFT:         // hot paths
        case CONTROL_SHIFT: // define hot path
            found = TRUE;
        }
        break;
    }

    case '5':
    {
        switch (mods)
        {
        case NONE:          // quick search/type in command line
        case CONTROL:       // hot paths
        case ALT:           // panel mode
        case SHIFT:         // hot paths
        case CONTROL_SHIFT: // define hot path
            found = TRUE;
        }
        break;
    }

    case '6':
    {
        switch (mods)
        {
        case NONE:          // quick search/type in command line
        case CONTROL:       // hot paths
        case ALT:           // panel mode
        case SHIFT:         // hot paths
        case CONTROL_SHIFT: // define hot path
            found = TRUE;
        }
        break;
    }

    case '7':
    {
        switch (mods)
        {
        case NONE:          // quick search/type in command line
        case CONTROL:       // hot paths
        case ALT:           // panel mode
        case SHIFT:         // hot paths
        case CONTROL_SHIFT: // define hot path
            found = TRUE;
        }
        break;
    }

    case '8':
    {
        switch (mods)
        {
        case NONE:          // quick search/type in command line
        case CONTROL:       // hot paths
        case ALT:           // panel mode
        case SHIFT:         // hot paths
        case CONTROL_SHIFT: // define hot path
            found = TRUE;
        }
        break;
    }

    case '9':
    {
        switch (mods)
        {
        case NONE:          // quick search/type in command line
        case CONTROL:       // hot paths
        case ALT:           // panel mode
        case SHIFT:         // hot paths
        case CONTROL_SHIFT: // define hot path
            found = TRUE;
        }
        break;
    }

    // VK_A thru VK_Z are the same as ASCII 'A' thru 'Z' (0x41 - 0x5A)
    case 'A':
    {
        switch (mods)
        {
        case NONE:    // quick search/type in command line
        case CONTROL: // select all
        case ALT:     // enter menu
        case SHIFT:   // change drive
            found = TRUE;
        }
        break;
    }

    case 'B':
    {
        switch (mods)
        {
        case NONE:    // quick search/type in command line
        case CONTROL: //
        case ALT:     // enter menu
        case SHIFT:   // change drive
            found = TRUE;
        }
        break;
    }

    case 'C':
    {
        switch (mods)
        {
        case NONE:    // quick search/type in command line
        case CONTROL: // copy to clipboard
        case ALT:     // enter menu
        case SHIFT:   // change drive
            found = TRUE;
        }
        break;
    }

    case 'D':
    {
        switch (mods)
        {
        case NONE:    // quick search/type in command line
        case CONTROL: // unselect all
        case ALT:     // enter menu
        case SHIFT:   // change drive
            found = TRUE;
        }
        break;
    }

    case 'E':
    {
        switch (mods)
        {
        case NONE:    // quick search/type in command line
        case CONTROL: // email files
        case ALT:     // enter menu
        case SHIFT:   // change drive
            found = TRUE;
        }
        break;
    }

    case 'F':
    {
        switch (mods)
        {
        case NONE:    // quick search/type in command line
        case CONTROL: // find files
        case ALT:     // enter menu
        case SHIFT:   // change drive
            found = TRUE;
        }
        break;
    }

    case 'G':
    {
        switch (mods)
        {
        case NONE:    // quick search/type in command line
        case CONTROL: // change directory
        case ALT:     // enter menu
        case SHIFT:   // change drive
            found = TRUE;
        }
        break;
    }

    case 'H':
    {
        switch (mods)
        {
        case NONE:    // quick search/type in command line
        case CONTROL: // show/hide hidden files
        case ALT:     // enter menu
        case SHIFT:   // change drive
            found = TRUE;
        }
        break;
    }

    case 'I':
    {
        switch (mods)
        {
        case NONE:    // quick search/type in command line
        case CONTROL: // last plugin command
        case ALT:     // enter menu
        case SHIFT:   // change drive
            found = TRUE;
        }
        break;
    }

    case 'J':
    {
        switch (mods)
        {
        case NONE:    // quick search/type in command line
        case CONTROL: //
        case ALT:     // enter menu
        case SHIFT:   // change drive
            found = TRUE;
        }
        break;
    }

    case 'K':
    {
        switch (mods)
        {
        case NONE:    // quick search/type in command line
        case CONTROL: // convert files
        case ALT:     // enter menu
        case SHIFT:   // change drive
            found = TRUE;
        }
        break;
    }

    case 'L':
    {
        switch (mods)
        {
        case NONE:    // quick search/type in command line
        case CONTROL: // drive information
        case ALT:     // enter menu
        case SHIFT:   // change drive
            found = TRUE;
        }
        break;
    }

    case 'M':
    {
        switch (mods)
        {
        case NONE:    // quick search/type in command line
        case CONTROL: // files list
        case ALT:     // enter menu
        case SHIFT:   // change drive
            found = TRUE;
        }
        break;
    }

    case 'N':
    {
        switch (mods)
        {
        case NONE:    // quick search/type in command line
        case CONTROL: // smart/elastic columns
        case ALT:     // enter menu
        case SHIFT:   // change drive
            found = TRUE;
        }
        break;
    }

    case 'O':
    {
        switch (mods)
        {
        case NONE:    // quick search/type in command line
        case CONTROL: // security: owner
        case ALT:     // enter menu
        case SHIFT:   // change drive
            found = TRUE;
        }
        break;
    }

    case 'P':
    {
        switch (mods)
        {
        case NONE:    // quick search/type in command line
        case CONTROL: // security: permissions
        case ALT:     // enter menu
        case SHIFT:   // change drive
            found = TRUE;
        }
        break;
    }

    case 'Q':
    {
        switch (mods)
        {
        case NONE:    // quick search/type in command line
        case CONTROL: // occupied space
        case ALT:     // enter menu
        case SHIFT:   // change drive
            found = TRUE;
        }
        break;
    }

    case 'R':
    {
        switch (mods)
        {
        case NONE:    // quick search/type in command line
        case CONTROL: // refresh
        case ALT:     // enter menu
        case SHIFT:   // change drive
            found = TRUE;
        }
        break;
    }

    case 'S':
    {
        switch (mods)
        {
        case NONE:    // quick search/type in command line
        case CONTROL: // paste links
        case ALT:     // enter menu
        case SHIFT:   // change drive
            found = TRUE;
        }
        break;
    }

    case 'T':
    {
        switch (mods)
        {
        case NONE:    // quick search/type in command line
        case CONTROL: // focus shortcut or link target
        case ALT:     // enter menu
        case SHIFT:   // change drive
            found = TRUE;
        }
        break;
    }

    case 'U':
    {
        switch (mods)
        {
        case NONE:    // quick search/type in command line
        case CONTROL: // swap panels
        case ALT:     // enter menu
        case SHIFT:   // change drive
            found = TRUE;
        }
        break;
    }

    case 'V':
    {
        switch (mods)
        {
        case NONE:    // quick search/type in command line
        case CONTROL: // clipboard paste
        case ALT:     // enter menu
        case SHIFT:   // change drive
            found = TRUE;
        }
        break;
    }

    case 'W':
    {
        switch (mods)
        {
        case NONE:    // quick search/type in command line
        case CONTROL: // reselect
        case ALT:     // enter menu
        case SHIFT:   // change drive
            found = TRUE;
        }
        break;
    }

    case 'X':
    {
        switch (mods)
        {
        case NONE:    // quick search/type in command line
        case CONTROL: // cut
        case ALT:     // enter menu
        case SHIFT:   // change drive
            found = TRUE;
        }
        break;
    }

    case 'Y':
    {
        switch (mods)
        {
        case NONE:    // quick search/type in command line
        case CONTROL: //
        case ALT:     // enter menu
        case SHIFT:   // change drive
            found = TRUE;
        }
        break;
    }

    case 'Z':
    {
        switch (mods)
        {
        case NONE:    // quick search/type in command line
        case CONTROL: // undo in command line
        case ALT:     // enter menu
        case SHIFT:   // change drive
            found = TRUE;
        }
        break;
    }

    case VK_MULTIPLY: // 0x6A
    {
        switch (mods)
        {
        case NONE:    // invert selection
        case CONTROL: // invert selection all
            found = TRUE;
        }
        break;
    }

    case VK_ADD: // 0x6B
    {
        switch (mods)
        {
        case NONE:          // select
        case CONTROL:       // select all
        case ALT:           // restore selection
        case SHIFT:         // select by extension
        case CONTROL_SHIFT: // select by name
        case ALT_SHIFT:     // store selection
            found = TRUE;
        }
        break;
    }

    case VK_SUBTRACT: // 0x6D
    {
        switch (mods)
        {
        case NONE:          // unselect
        case CONTROL:       // unselect all
        case ALT:           // restore selection unselect
        case SHIFT:         // unselect by extension
        case CONTROL_SHIFT: // unselect by name
            found = TRUE;
        }
        break;
    }

    case VK_DIVIDE: // 0x6F
    {
        switch (mods)
        {
        case NONE: // shell prompt
            found = TRUE;
        }
        break;
    }

    case VK_F1: // 0x70
    {
        switch (mods)
        {
        case NONE:    // help
        case CONTROL: // drive information
        case ALT:     // left change drive
        case SHIFT:   // context help
            found = TRUE;
        }
        break;
    }

    case VK_F2: // 0x71
    {
        switch (mods)
        {
        case NONE:    // rename
        case CONTROL: // change attributes
        case ALT:     // right change drive
            found = TRUE;
        }
        break;
    }

    case VK_F3: // 0x72
    {
        switch (mods)
        {
        case NONE:          // view
        case CONTROL:       // sort by name
        case ALT:           // alternate view
        case SHIFT:         // open current folder
        case CONTROL_SHIFT: // view width
            found = TRUE;
        }
        break;
    }

    case VK_F4: // 0x73
    {
        switch (mods)
        {
        case NONE:          // edit
        case CONTROL:       // sort by extension
        case ALT:           // exit
        case SHIFT:         // edit new
        case CONTROL_SHIFT: // edit width
            found = TRUE;
        }
        break;
    }

    case VK_F5: // 0x74
    {
        switch (mods)
        {
        case NONE:          // copy
        case CONTROL:       // sort by time
        case ALT:           // pack
        case CONTROL_SHIFT: // save selection
            found = TRUE;
        }
        break;
    }

    case VK_F6: // 0x75
    {
        switch (mods)
        {
        case NONE:          // move
        case CONTROL:       // sort by size
        case ALT:           // unpack
        case CONTROL_SHIFT: // load selection
            found = TRUE;
        }
        break;
    }

    case VK_F7: // 0x76
    {
        switch (mods)
        {
        case NONE:    // make directory
        case CONTROL: // change case
        case ALT:     // find
        case SHIFT:   // change directory
            found = TRUE;
        }
        break;
    }

    case VK_F8: // 0x77
    {
        switch (mods)
        {
        case NONE:  // delete
        case SHIFT: // delete
            found = TRUE;
        }
        break;
    }

    case VK_F9: // 0x78
    {
        switch (mods)
        {
        case NONE:          // user menu
        case CONTROL:       // refresh
        case ALT:           // unpack
        case SHIFT:         // hot paths
        case CONTROL_SHIFT: // shares
            found = TRUE;
        }
        break;
    }

    case VK_F10: // 0x79
    {
        switch (mods)
        {
        case NONE:          // menu
        case CONTROL:       // compare
        case ALT:           // calculate occupied space
        case SHIFT:         // context menu
        case CONTROL_SHIFT: // calculate directory sizes
        case ALT_SHIFT:     // context menu for current directory
            found = TRUE;
        }
        break;
    }

    case VK_F11: // 0x80
    {
        switch (mods)
        {
        case NONE:          // connect
        case CONTROL:       // zoom panel
        case ALT:           // list of opened files
        case CONTROL_SHIFT: // full screen
            found = TRUE;
        }
        break;
    }

    case VK_F12: // 0x81
    {
        switch (mods)
        {
        case NONE:    // disconnect
        case CONTROL: // filter
        case ALT:     // list of working directories
            found = TRUE;
        }
        break;
    }

    case 0xBA: // ';'
    {
        switch (mods)
        {
        case NONE:    // quick search/type in command line
        case CONTROL: // documents
            found = TRUE;
        }
        break;
    }

    case 0xBB: // '='
    {
        switch (mods)
        {
        case NONE:    // quick search/type in command line
        case CONTROL: // select
            found = TRUE;
        }
        break;
    }

    case 0xBD: // '-'
    {
        switch (mods)
        {
        case NONE:    // quick search/type in command line
        case CONTROL: // unselect
            found = TRUE;
        }
        break;
    }

    case 0xBC: // ','
    {
        switch (mods)
        {
        case NONE:    // quick search/type in command line
        case CONTROL: // Network
            found = TRUE;
        }
        break;
    }

    case 0xBE: // '.'
    {
        switch (mods)
        {
        case NONE:    // quick search/type in command line
        case CONTROL: // as other panel
            found = TRUE;
        }
        break;
    }

    case 0xBF: // '/'
    {
        switch (mods)
        {
        case NONE:    // quick search/type in command line
        case CONTROL: // command shell
            found = TRUE;
        }
        break;
    }

    case 0xC0: // '`'
    {
        switch (mods)
        {
        case NONE:          // quick search/type in command line
        case CONTROL:       // Network
        case CONTROL_SHIFT: // as other panel
            found = TRUE;
        }
        break;
    }

    case 0xDB: // '['
    {
        switch (mods)
        {
        case NONE:          // quick search/type in command line
        case CONTROL:       // brings left dir to cmdline
        case CONTROL_SHIFT: // brings left dir to cmdline
            found = TRUE;
        }
        break;
    }

    case 0xDC: // '\'
    {
        switch (mods)
        {
        case NONE:    // quick search/type in command line
        case CONTROL: // root directory
            found = TRUE;
        }
        break;
    }

    case 0xDD: // ']'
    {
        switch (mods)
        {
        case NONE:          // quick search/type in command line
        case CONTROL:       // brings right dir to cmdline
        case CONTROL_SHIFT: // brings left dir to cmdline
            found = TRUE;
        }
        break;
    }

    case 0xDE: // '''
    {
        switch (mods)
        {
        case NONE: // quick search/type in command line
            found = TRUE;
        }
        break;
    }
    }

    return found;
}

/*  //
// Virtual Keys, Standard Set
//
#define VK_LBUTTON        0x01
#define VK_RBUTTON        0x02
#define VK_CANCEL         0x03
#define VK_MBUTTON        0x04    // NOT contiguous with L & RBUTTON

#define VK_BACK           0x08
#define VK_TAB            0x09

#define VK_CLEAR          0x0C
#define VK_RETURN         0x0D

#define VK_SHIFT          0x10
#define VK_CONTROL        0x11
#define VK_MENU           0x12
#define VK_PAUSE          0x13
#define VK_CAPITAL        0x14

#define VK_KANA           0x15
#define VK_HANGEUL        0x15  // old name - should be here for compatibility 
#define VK_HANGUL         0x15
#define VK_JUNJA          0x17
#define VK_FINAL          0x18
#define VK_HANJA          0x19
#define VK_KANJI          0x19

#define VK_ESCAPE         0x1B

#define VK_CONVERT        0x1C
#define VK_NONCONVERT     0x1D
#define VK_ACCEPT         0x1E
#define VK_MODECHANGE     0x1F

#define VK_SPACE          0x20
#define VK_PRIOR          0x21
#define VK_NEXT           0x22
#define VK_END            0x23
#define VK_HOME           0x24
#define VK_LEFT           0x25
#define VK_UP             0x26
#define VK_RIGHT          0x27
#define VK_DOWN           0x28
#define VK_SELECT         0x29
#define VK_PRINT          0x2A
#define VK_EXECUTE        0x2B
#define VK_SNAPSHOT       0x2C
#define VK_INSERT         0x2D
#define VK_DELETE         0x2E
#define VK_HELP           0x2F

// VK_0 thru VK_9 are the same as ASCII '0' thru '9' (0x30 - 0x39) 
// VK_A thru VK_Z are the same as ASCII 'A' thru 'Z' (0x41 - 0x5A) 

#define VK_LWIN           0x5B
#define VK_RWIN           0x5C
#define VK_APPS           0x5D

#define VK_NUMPAD0        0x60
#define VK_NUMPAD1        0x61
#define VK_NUMPAD2        0x62
#define VK_NUMPAD3        0x63
#define VK_NUMPAD4        0x64
#define VK_NUMPAD5        0x65
#define VK_NUMPAD6        0x66
#define VK_NUMPAD7        0x67
#define VK_NUMPAD8        0x68
#define VK_NUMPAD9        0x69
#define VK_MULTIPLY       0x6A
#define VK_ADD            0x6B
#define VK_SEPARATOR      0x6C
#define VK_SUBTRACT       0x6D
#define VK_DECIMAL        0x6E
#define VK_DIVIDE         0x6F
#define VK_F1             0x70
#define VK_F2             0x71
#define VK_F3             0x72
#define VK_F4             0x73
#define VK_F5             0x74
#define VK_F6             0x75
#define VK_F7             0x76
#define VK_F8             0x77
#define VK_F9             0x78
#define VK_F10            0x79
#define VK_F11            0x7A
#define VK_F12            0x7B
#define VK_F13            0x7C
#define VK_F14            0x7D
#define VK_F15            0x7E
#define VK_F16            0x7F
#define VK_F17            0x80
#define VK_F18            0x81
#define VK_F19            0x82
#define VK_F20            0x83
#define VK_F21            0x84
#define VK_F22            0x85
#define VK_F23            0x86
#define VK_F24            0x87

#define VK_NUMLOCK        0x90
#define VK_SCROLL         0x91

//
// VK_L* & VK_R* - left and right Alt, Ctrl and Shift virtual keys.
// Used only as parameters to GetAsyncKeyState() and GetKeyState().
// No other API or message will distinguish left and right keys in this way.
//
#define VK_LSHIFT         0xA0
#define VK_RSHIFT         0xA1
#define VK_LCONTROL       0xA2
#define VK_RCONTROL       0xA3
#define VK_LMENU          0xA4
#define VK_RMENU          0xA5

#if(WINVER >= 0x0400)
#define VK_PROCESSKEY     0xE5
#endif // WINVER >= 0x0400 

#define VK_ATTN           0xF6
#define VK_CRSEL          0xF7
#define VK_EXSEL          0xF8
#define VK_EREOF          0xF9
#define VK_PLAY           0xFA
#define VK_ZOOM           0xFB
#define VK_NONAME         0xFC
#define VK_PA1            0xFD
#define VK_OEM_CLEAR      0xFE
*/