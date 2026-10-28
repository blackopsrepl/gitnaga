#pragma once

namespace GitNaga::askpass {

// GitNaga answers git's and ssh's credential prompts itself: network
// operations point GIT_ASKPASS and SSH_ASKPASS at this binary and mark the
// environment with GITNAGA_ASKPASS=1 (see gitsync::withCredentials), because
// both programs pass the prompt as the only argument. `gitnaga --askpass
// <prompt>` asks explicitly. The answer goes to stdout; exit 1 means the user
// cancelled.
//
// Returns the exit code when this process was started to answer a prompt, or
// -1 when it is the ordinary application. Call before any QGuiApplication.
//
// For headless checks GITNAGA_ASKPASS_TYPE=<text> types the text into the
// field and presses Enter (GITNAGA_ASKPASS_CANCEL=1: Escape instead);
// GITNAGA_ASKPASS_SHOT=<png> saves the dialog first.
int run(int argc, char *argv[]);

} // namespace GitNaga::askpass
