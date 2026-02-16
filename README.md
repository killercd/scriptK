# scriptK

Script per scrivere velocemente e simulare copia/incolla in ambienti dove la clipboard è disabilitata.

## Versione C (Windows)

Implementazione in **C puro** con sole **API native Windows** e interfaccia Win32 minimale.

### Requisiti

- Windows
- Compilatore: MSVC (Visual Studio) oppure MinGW-w64 (gcc)

### Compilazione

**Con Microsoft Visual C (Developer Command Prompt):**
```bat
cl scriptK.c user32.lib
```

**Con MinGW-w64 (es. `x86_64-w64-mingw32-gcc`):**
```bat
x86_64-w64-mingw32-gcc scriptK.c -o scriptK.exe -luser32 -mwindows -municode
```

`-mwindows` crea un’applicazione finestra (senza console).

### Utilizzo

1. Avviare `scriptK.exe`.
2. Incollare o digitare il testo nella casella "Text data:".
3. Impostare i secondi di attesa in "Waiting seconds:" (0 = nessuna attesa).
4. Cliccare **Start**.
5. Confermare il messaggio "Waiting N seconds...".
6. Dopo N secondi (o subito se 0), posizionare il focus nella finestra di destinazione: il testo verrà battuto carattere per carattere (ritardo 250 ms, come nella versione Python).

### Comportamento

- La digitazione avviene in un thread separato, così la finestra resta utilizzabile.
- Si usa `SendInput` con `KEYEVENTF_UNICODE` per supportare caratteri Unicode.
- Nessuna dipendenza da .NET o da librerie esterne: solo `user32` e CRT standard.
# scriptK
