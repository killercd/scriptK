# scriptK

**scriptK** is a minimal Windows utility written in pure C that
simulates keyboard typing to input text into systems where **clipboard
and copy/paste are disabled**.

It sends real keyboard events using native Win32 APIs (`SendInput`),
without using the clipboard.

------------------------------------------------------------------------

## Features

-   Text typing simulation\
-   Multiline input support\
-   Configurable start delay\
-   No clipboard usage\
-   Native Win32 GUI\
-   No external dependencies

------------------------------------------------------------------------

## Build

### MSVC

``` bash
cl scriptK.c user32.lib
```

### MinGW

``` bash
x86_64-w64-mingw32-gcc scriptK.c -o scriptK.exe -luser32 -mwindows -municode
```

------------------------------------------------------------------------

## Usage

1.  Enter text in the text box\
2.  Set delay (seconds)\
3.  Click **Start**\
4.  Focus the target window\
5.  Text is typed automatically

------------------------------------------------------------------------

## Debug mode

To verify that the typed output matches the input (e.g. after changes to
newline handling), run:

``` bash
scriptK.exe --debug path\to\input.txt
```

-   The app loads the file into the text area, sets delay to 1 second,
    and starts typing **without** showing the confirmation dialog.
-   In debug mode, no keys are sent; instead, the “typed” sequence is
    written to a buffer and then saved to **output.txt** (UTF-8).
-   The app then compares the normalized input with this output.
-   In the console it prints **DEBUG ok** if they match, **DEBUG ko** if
    not. The window closes automatically after the check.

Example:

``` bash
scriptK.exe --debug test_debug.txt
```

A console window will appear with the result. Use this to ensure
input and output stay in sync (e.g. newlines and special characters).

