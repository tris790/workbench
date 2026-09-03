Workbench (wb)
# A developper tool that serves as a file explorer on steroids.

## Features
- Command palette (serves as search and commands if starts with '>')
- Support 1 or 2 panel layout ( trigger via command palette )
- Builtin terminal (aware of the cwd same as the file explorer)
- Can render preview
- Multi platform
- Extensible (user can define commands)
- Keyboard focused but mouse is supported
- Smooth user experience
- Dark theme but different theme can be used
- Use system fonts
- Insanely fast and lightweight
- Portable executable
- Has context menu (non bloated but customizable)
- Settings available to customize the app
- FileChooser portal backend for Wayland desktops

## Project requirements
- build.sh
- c99
- Follow handmade hero style
- No hacks, we take the time to do it right
- Professional UI with smooth animations, delightful user experience and design, modern feel
- Abstractions for movable parts, platform, theme, commands etc

## Tips
- The command palette in VSCode works well and I like the style
- Icons are cool and they can make the UI more compact. On the other hand emojis are not good, they look cheap.
- Create a Gemini.md file to document our coding style and project

## FileChooser portal

On Linux/Wayland, Workbench can provide the file chooser used by portal-aware
applications such as browsers and sandboxed applications. Install the
per-user D-Bus registration once:

```bash
./build/wb --install-portal
```

The portal broker starts Workbench automatically with `--portal` when an
application requests an open or save dialog. Portal mode returns selected
paths only; the requesting application remains responsible for reading,
writing, or uploading the selected files. The installer reloads the user
D-Bus activation registry; restart the portal service only if it was already
running while the preference was being changed.

When a request reaches an existing Workbench process, it asks the Wayland
compositor to activate that Workbench window before showing the picker. The
compositor may reject activation when no valid recent user-input token is
available.
