# Third-party notices

Rivellon Renewed's own code is under the [MIT License](LICENSE). The components below are made by others and keep their own licenses. Where each license text is in this repository and in the release zip:

| Component | Version | Author | License | How it is used | License text |
|---|---|---|---|---|---|
| [MinHook](https://github.com/TsudaKageyu/minhook), with the Hacker Disassembler Engine 32 | 1.3.4 | Tsuda Kageyu; HDE32: Vyacheslav Patkov | BSD 2-Clause | statically linked into `RivellonRenewed.asi` (fetched at build time) | zip: `RivellonRenewed\licenses\MinHook.txt` |
| [stb_truetype](https://github.com/nothings/stb) | 1.26 | Sean Barrett | public domain or MIT (choice) | compiled into `RivellonRenewed.asi` (fetched at build time) | in the source file's footer |
| [Ultimate ASI Loader](https://github.com/ThirteenAG/Ultimate-ASI-Loader) | 9.7.4 | ThirteenAG | MIT | shipped unchanged as `winmm.dll`; loads the mod | zip: `RivellonRenewed\licenses\Ultimate-ASI-Loader.txt` |
| [Uncial Antiqua](https://github.com/google/fonts/tree/main/ofl/uncialantiqua) | – | Brian J. Bonislawsky (Astigmatic) | SIL Open Font License 1.1 | bundled font (menu) | `assets/fonts/OFL-UncialAntiqua.txt`; zip: `RivellonRenewed\fonts\` |
| [Fondamento](https://github.com/google/fonts/tree/main/ofl/fondamento) | – | Brian J. Bonislawsky (Astigmatic) | SIL Open Font License 1.1 | bundled font (books and letters) | `assets/fonts/OFL-Fondamento.txt`; zip: `RivellonRenewed\fonts\` |
| [Marcellus SC](https://github.com/google/fonts/tree/main/ofl/marcellussc) | – | Brian J. Bonislawsky (Astigmatic) | SIL Open Font License 1.1 | bundled font (interface) | `assets/fonts/OFL-MarcellusSC.txt`; zip: `RivellonRenewed\fonts\` |
| [Almendra SC](https://github.com/google/fonts/tree/main/ofl/almendrasc) | – | Ana Sanfelippo | SIL Open Font License 1.1 | bundled font (diary title) | `assets/fonts/OFL-AlmendraSC.txt`; zip: `RivellonRenewed\fonts\` |
| [DejaVu Sans Condensed Bold](https://dejavu-fonts.github.io/) | 2.37 | DejaVu fonts team | Bitstream Vera / DejaVu license (public domain changes) | bundled font (dialog, overlay) | `assets/fonts/LICENSE-DejaVu.txt`; zip: `RivellonRenewed\fonts\` |
| [dgVoodoo2](https://github.com/dege-diosg/dgVoodoo2) | 2.87.5 | Dege | its own terms (freeware) | **not shipped**: `RivellonRenewed-Setup.cmd` downloads it from its official release on the player's request | in the dgVoodoo2 package |

The fonts are used unchanged; the mod draws them with its own rasterizer and never installs them into the system.

**Divine Divinity** is © Larian Studios. Rivellon Renewed ships no game files and is not affiliated with or endorsed by Larian Studios.
