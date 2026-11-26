# Making a release

1. Commit everything. The localization build records the commit and whether the tree was clean.
2. Build the runtime and the front page (HANDOFF §0) and run the C++ and Python tests.
3. Build the localization data from the original game:
   `python scripts/build_localization.py --game-dir <original game> --font <font> --output .local/p10-misc/out-NN`
4. Package (output under `.local/release/` only):
   ```sh
   python scripts/package_release.py --name U9CHT-<version> --build .local/p10-misc/out-NN \
     --runtime .local/p10-misc/build/Release --game-dir <original game> \
     --font .local/p10-misc/front-release/fonts/jf-openhuninn-2.1.ttf
   ```
   The script makes the patches (`PATCH_FORMAT.md`), applies each again, and refuses the release when it finds an
   FLX, a local path, or English in a patch. It writes `<name>.zip` and `<name>.report.json` (sizes and SHA-256 of
   every file).
5. Test the zip as a player would: unzip into a new folder, run `U9Cht.exe`, point it at a game, start the game.
   The first start builds `data/static/` from the patches.
6. Publishing is a separate decision of the project owner.

Contents of a release:

| Path | From |
| --- | --- |
| `U9Cht.exe`, `U9ChtLauncher.exe`, `U9ChtRuntime.dll`, `U9ChtProxy.dll` | runtime build |
| `data/build-manifest.json`, `data/runtime/allowlist.u9ct`, `data/runtime/movies.tsv` | localization build |
| `data/patches/*.u9cp` | made by the script |
| `data/patches/bitmap16.u9cs` | made by the script from `translation/textures/signposts/` (signposts) |
| `fonts/jf-openhuninn-2.1.ttf`, `fonts/jf-openhuninn-LICENSE.txt` | jf open huninn 2.1 (OFL 1.1) |
| `說明.txt` | `release/說明.txt` |
| `THIRD_PARTY_NOTICES.md` | repository |

The front page writes `u9cht.ini`, `front.ini`, `logs/`, `backup/`, `data/static/` and `data/redirect/` (the signpost copy of
`bitmap16.flx`, about 320 MB) beside itself at run time.

## Cloud saves

Saves stay where the game keeps them (`<game>\savegame`; the launcher starts `u9.exe` with the game folder as its
working folder). Platform clients sync only around a launch they start themselves.

- **GOG Galaxy** (the GOG release has cloud saves): with the game's custom executable set to `U9Cht.exe`, Galaxy
  treats the front page as the game and syncs when it closes. Tested by the project owner on 2026-11-18: works.
- **Steam**: whether the Steam release has Steam Cloud is not known; a launch option
  `"<folder>\U9Cht.exe" %command%` would work the same way (the front page ignores the extra argument). Not tested.
