# globalInput

> Send mouse/keyboard input events.

Process: [Main](../glossary.md#main-process)

The `globalInput` module can send mouse/keyboard input events.

**Note:** This module cannot be used before the `ready`
event of the app module is emitted.

## Methods

The `globalInput` module has the following methods:

### `globalInput.sendMouse(id, action, buttons, x, y, width, height, alt, ctrl, shift, meta)`

* `id` string - id of [`DesktopCapturerSource`] (structures/desktop-capturer-source.md) object.
* `action` Integer - mouse motion type, 0: move, 1: down, 2: up, 3: wheel, 4: trackpad.
* `buttons` Integer - which buttons are pressed, 0: none, 001: left, 010: right, 100: middle.
* `x` Integer - mouse horizontal position relative to the viewport.
* `y` Integer - mouse vertical position relative to the viewport.
* `width` Integer - width of the viewport.
* `height` Integer - height of the viewport.
* `alt` boolean - whether the alt key is pressed or not.
* `ctrl` boolean - whether the ctrl key is pressed or not.
* `shift` boolean - whether the shift key is pressed or not.
* `meta` boolean - whether the meta key is pressed or not.

Returns `boolean` - Whether or not the mouse input event was sent successfully.

### `globalInput.sendKeyboard(down, key, alt, ctrl, shift, meta)`

* `down` boolean - whether the key is pressed or not.
* `vkey` Integer - Virtual Keycode (https://developer.mozilla.org/en-US/docs/Web/API/UI_Events/Keyboard_event_key_values).
* `alt` boolean - whether the alt key is pressed or not.
* `ctrl` boolean - whether the ctrl key is pressed or not.
* `shift` boolean - whether the shift key is pressed or not.
* `meta` boolean - whether the meta key is pressed or not.

Returns `boolean` - Whether or not the keyboard input event was sent successfully.
