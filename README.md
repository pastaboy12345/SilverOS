# SilverOS
A cool OS project made by me.

## Web App Direction
SilverOS is being wired toward a `WebKit` renderer path for desktop web UIs.

- Page UI target: `WebKit + JavaScriptCore`
- Seeded app entry: `/home/user/webapp/index.html`
- React source entry: `/home/user/webapp/src/main.jsx`
- Bundled page script: `/home/user/webapp/app.js`
- Native companion script: `/home/user/webapp/native.js`

Inside the desktop terminal you can use:

- `webapp` to open the seeded Web App window
- `webinfo` to inspect the WebKit/React/V8 layout
- `native [file]` to run a native V8 script when V8 is enabled in the build

`V8` remains the native scripting runtime for OS-side logic. The in-page web app runtime is `JavaScriptCore` once the WebKit port is ready.

* youtube: @minecraftisalldrills
