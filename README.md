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

