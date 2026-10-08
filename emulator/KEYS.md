# Page Up, Page Down, Home, End, Insert and Delete for vi

The emulator types the UCSD (L2) editor's own commands for these keys,
because the P-System has no codes for them:

| Key | Sent | In the L2 editor |
|---|---|---|
| Page Down | `>P` | page forward |
| Page Up | `<P>` | page back |
| Home | `JB` | jump to the beginning |
| End | `JE` | jump to the end |
| Insert | `I` | insert |
| Delete | `D` ^U ^C | delete a character |

vi takes those letters as its own commands. With this change a program can
ask for one code per key instead: while SYSCOM^.EXPANSION[1] is 25605
(`PX_KEYS` in Tiny-C's `psys.h`) the emulator sends

| Key | Code | psys.h | vi |
|---|---|---|---|
| Home | 0x84 | `KEY_HOME` | start of the line |
| End | 0x85 | `KEY_END` | end of the line |
| Insert | 0x86 | `KEY_INSERT` | insert mode |
| Delete | 0x87 | `KEY_DELETE` | delete the character (x) |
| Page Up | 0x88 | `KEY_PGUP` | a screen back (^B) |
| Page Down | 0x89 | `KEY_PGDN` | a screen forward (^F) |

From **version 2.01** the mouse wheel too, while the word is set: a notch
away from you sends 0x96 (`KEY_WHEELUP`), towards you 0x97 (`KEY_WHEELDN`);
vi scrolls 3 lines a notch (as ^Y / ^E) in command mode.

From **version 2.02** the word may be 25606 (`PX_KEYS_BLOCK`) instead: the
same keys and wheel, and the cursor drawn as a block (its cell inverted)
rather than a line under the character. vi sets 25606; the shell, 25605.

vi (TOOLS:VI.CODE) sets the word when it starts and clears it when it ends,
so the L2 editor and everything else get the keys as before. (If vi ever
stops with an execution error the word can stay set; running vi again and
leaving it clears it.)

## Changing the emulator

The emulator has it from **version 2.00**
([UCSD-Pascal_Windows_Emulator](https://github.com/Bio-Printer/UCSD-Pascal_Windows_Emulator)):
rebuild from there.  For an older copy: `PSystemEngine-keys.patch` (this
folder) is the change as a unified diff
against v1.88 (`patch -p1` in the `UCSD-Pascal---P-Machine_work` folder).
In a later emulator, by hand:

1. `UCSDPascal\PSystemEngine.h`, in the class, after `void PostKey(uint8_t ch);`:
   ```cpp
       // A program that wants Page Up, Page Down, Home, End, Insert and Delete
       // as one code each (Tiny-C's vi) sets SYSCOM^.EXPANSION[1] to 25605
       // (PX_KEYS in Tiny-C's psys.h) while it runs. See MainFrm OnKeyDown.
       bool WantsKeyCodes() const;
   ```
2. `UCSDPascal\PSystemEngine.cpp`, before `void PSystemEngine::PostKey(uint8_t ch) {`:
   ```cpp
   // SYSCOM^.EXPANSION[1] (SYSCOM is at 0x02E4; EXPANSION[0] at +36) = 25605
   bool PSystemEngine::WantsKeyCodes() const {
       const uint16_t a = PM_V(0x030A);
       return (uint16_t)(m_mem[a] | (m_mem[(uint16_t)(a + 1)] << 8)) == 25605;
   }
   ```
   (`PM_V` moves the address down in the relocated P-Code layout, as for the
   other SYSCOM fields.)
3. `UCSDPascal\MainFrm.cpp`, in `CMainFrame::OnKeyDown`, right after the
   line `auto send = ...;` and before `switch (nChar) {`:
   ```cpp
       if (m_engine->WantsKeyCodes()) {
           switch (nChar) {
               case VK_HOME:   m_engine->PostKey(0x84); return;
               case VK_END:    m_engine->PostKey(0x85); return;
               case VK_INSERT: m_engine->PostKey(0x86); return;
               case VK_DELETE: m_engine->PostKey(0x87); return;
               case VK_PRIOR:  m_engine->PostKey(0x88); return; // Page Up
               case VK_NEXT:   m_engine->PostKey(0x89); return; // Page Down
               default: break;
           }
       }
   ```
4. Rebuild (Release).

Quick check: from the shell, `VI` a file: Page Down and Page Up move a
screen, Home and End go to the start and end of the line, Delete deletes
the character under the cursor, Insert starts inserting (ESC ends it).
Then the Editor (E at the Command: prompt): the keys do what they did.

Test: `tools/vitest.py` ("PC keys": vi given these codes on the P-System
saves the same file as the Linux build given the terminal's sequences).
