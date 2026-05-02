# Finding DC4 Memory Addresses with x32dbg

## Method 2: The String Search (Recommended)

Instead of stepping through every call, let's find the code directly by searching for text.

1. **Restart Debugging** (`Ctrl+F2` or File -> Reload).
2. **Go to CPU tab**.
3. **Right-click** in the code window -> **Search for** -> **All modules** -> **String references**.
4. **Wait** for the search to complete (progress bar at bottom).
5. **Filter** for: `D.C.4` (or `Da Capo`).
   - If nothing, try searching for `CIRCUS`.
6. **Double-click** the result.
7. **Scroll down** in the code view. Look for a call to `RegisterClassA`.
   - The code just before it prepares the `WNDCLASSA` structure.
   - Look for an instruction like `mov [ebp-XX], <ADDRESS>`.
   - That `<ADDRESS>` is likely the **WndProc**!

## Expected Pattern in Code
You'll see something like this:

```assembly
push <WndProc_Address>  ; The address we want!
pop ...
mov [ebp-04], ...       ; Storing WndProc
...
call RegisterClassA
```

Or:

```assembly
mov dword ptr [ebp-1C], <WndProc_Address>  ; <-- BINGO!
...
call RegisterClassA
```
