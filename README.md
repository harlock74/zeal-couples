[![License](https://img.shields.io/badge/License-Apache_2.0-blue.svg)](https://opensource.org/licenses/Apache-2.0)

# zeal-couples

zeal-couples is a memory-style game for the Zeal 8-bit Computer.

It runs at **640×480 @8bpp**

## Screenshots

| Splash Screen  | Gameplay |
| - | - |
| ![image](https://github.com/harlock74/zeal-couples/blob/main/docs/screenshots/1.png) | ![image](https://github.com/harlock74/zeal-couples/blob/main/docs/screenshots/2.png) |

## Rules

- Deck size: **36 cards**
- Ranks used: **A,2,3,4,5,6,J,Q,K** (7,8,9,10 are excluded)
- Suits: 4
- Layout: **9 cards per row**, 4 rows total
- All cards start face-down (red back)
- Match rule: cards match by **rank only**, suit is ignored

## Controls

- SNES mouse support enabled
- Splash: `ENTER` (or SNES `START`) starts the game
- `UP` `DOWN` `LEFT` `RIGHT` (or SNES D-pad): move the single hand marker across available card slots, skipping removed cards
- `SPACE`/`Z` (or SNES `B`) or mouse left click: reveal selected card
- `S` on keyboard (or SNES `X`): mute/resume gameplay music while playing
- `RIGHT_SHIFT` (or SNES `SELECT`): quit
- End states: `ENTER`/`SPACE` or mouse left click returns to splash for a fresh game

If using the native-emulator and not real hardware, click on the mouse middle button to hide the host mouse cursor.


## Assets

The main larger cards were made by me. Feel free to create your own cards by editing the provided `cards.aseprite`. All you need to do is change the tiles used for each suit pip. You can also be creative with the Jack, Queen, and King tiles. Have fun if you find my cards boring at some point.

## Attribution

Author: Zingot Games  
License: CC-BY 4.0  
https://opengameart.org/content/bitmap-font-pack  
A few changes to the original assets have been made including the color palette.

Author: (Pixel) Poker Cards  
License: CC-BY 4.0  
https://ivoryred.itch.io/pixel-poker-cards  
A few changes to the original assets have been made including the color palette.
