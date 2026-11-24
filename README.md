# fcitx5-vim-im-restore

This Fcitx 5 addon switches to English when you leave Insert mode in Vim with `Esc`. When you re-enter Insert mode, it restores whichever input method you were using in your Fcitx group.

## Features

- Restores the saved input method for the first keystroke after `i`, `a`, `o`, `I`, `A`, or `O` enters Insert mode, or `R` enters Replace mode.
- Keeps English active for the `i` in `diw`, the `a` in `fa`, search patterns, and the `:` command line.
- Remembers the original input method across repeated presses of `Esc`. Switching input methods manually cancels the pending restore.

## Requirements

- Linux with Fcitx 5.1.13 or later, including its development headers and CMake package files
- A C++20 compiler, CMake 3.20 or later, and Make or Ninja

## Installation

Clone this repository, then build and install the addon from its root directory.

```sh
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build
sudo cmake --install build
```

Restart Fcitx or log out and back in. In Fcitx settings, enable **Vim Input Method Restore** on the **Addons** tab. On the **Input Method** tab, add both the English keyboard (default `keyboard-us`) and the input method you want to restore to the current group. If the group lacks the configured English keyboard, the current input method stays active.

<details>
<summary>Uninstalling</summary>

To uninstall, check the paths in `install_manifest.txt` in the build directory used for installation. For the default Arch Linux and CachyOS paths, run the following command and restart Fcitx.

```sh
sudo rm -- /usr/lib/fcitx5/libviescape.so /usr/share/fcitx5/addon/viescape.conf
```

</details>

## Configuration

In the addon settings, **English input method ID** selects the keyboard used for commands, and **Excluded program IDs** lists applications to skip. The addon applies to all applications by default. If you have another addon that switches input methods on `Esc`, enable only one. Whichever addon switches first changes the input method ID the other one sees.

To configure the addon by hand, edit `~/.config/fcitx5/conf/viescape.conf` and restart Fcitx. This example uses the US keyboard and excludes Firefox.

```ini
EnglishInputMethod=keyboard-us

[ExcludedPrograms]
0=firefox
```

Each program ID must match the name Fcitx receives from the application. When the Fcitx D-Bus module is available, run the following command and look for the `program:` field.

```sh
busctl --user call org.fcitx.Fcitx5 /controller org.fcitx.Fcitx.Controller1 DebugInfo
```

## License

[MIT License](LICENSE)
