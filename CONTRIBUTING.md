## Contributing to bakkesmod-llm-chat

This document captures the design decisions, requirements, and development workflow for the BakkesMod plugin that enables LLM-powered chat replies in Rocket League.

The goal: When any non-quickchat, typed message is sent by another player (not the local user), the plugin forwards recent chat history to OpenAI Chat Completions and posts the LLM's reply back to the same channel, trimmed to 120 characters.

### Product requirements (agreed)
- **Platform**: Windows + Visual Studio (Release x64).
- **Repository**: Use `mdlindsey/bakkesmod-llm-chat` (fork of the BakkesMod plugin template) as the base.
- **OpenAI API**: Chat Completions with model `gpt-4o-mini`.
- **Dependencies**: Avoid reinventing the wheel; use vcpkg for deps (JSON library, etc.). Prefer native `WinHTTP` for HTTP; JSON via `nlohmann-json`.
- **Trigger**: Only respond to typed messages from other players. Do not respond to local user's messages. Ignore quickchat events entirely.
- **Reply channel**: Mirror the message's channel (Team/Party/Global).
- **History**: Include typed messages from all players. Maintain a per-match buffer and reset after match end.
- **Limits**: Use last 10 messages, enforce a 120 character output cap.
- **Config persistence/UI**: Store in BakkesMod config (no masking required). Expose fields in the plugin's ImGui window (F2 panel).
- **Rate limiting**: No cooldown. Discard stale responses if a new incoming message arrives before the previous response comes back.
- **Errors**: Log errors to the BakkesMod console only (do not post to public chat).
- **Default system prompt**: Only enforce the 120-char limit.

### References
- BakkesMod plugin docs: [BakkesMod plugin wiki](https://wiki.bakkesplugins.com/)
- OpenAI Chat Completions API: [OpenAI API reference: Chat Completions](https://platform.openai.com/docs/api-reference/chat/create)

### High-level architecture
- **Event capture**
  - Hook Rocket League typed chat events (exclude quickchats).
  - Filter out messages from the local user; only process messages from other players.
  - Capture the chat channel (Team/Party/Global) for reply mirroring.
- **History buffer**
  - Keep a per-match ring buffer (size 10) of typed chat messages from all players, plus prior assistant (plugin) replies for context.
  - Reset on match end (and on next match start as a safety).
- **Request build**
  - Construct Chat Completions `messages` with: one `system` message (system prompt), recent chat as `user` messages, previous assistant replies as `assistant`, and the latest triggering message as the final `user` entry.
- **HTTP/async**
  - Use a worker thread to call OpenAI via `WinHTTP` over HTTPS with reasonable timeouts and minimal retries.
  - Maintain a monotonically increasing `requestId`. If a new incoming message arrives while a request is in flight, increment `requestId`. When responses return, discard those whose `requestId` does not match the latest.
- **Response handling**
  - Parse `choices[0].message.content`, collapse newlines, trim whitespace, and truncate to 120 characters. Avoid re-triggering on our own injected messages.
  - Send the final text to the same channel as the triggering message.
- **Logging & safety**
  - Log errors, timeouts, and parsing failures to the BakkesMod console.
  - Persist configuration in BakkesMod; keep API key in plain text (per requirement).

### Configuration (CVars) and defaults
- `llm_enabled` (bool, default: `1`)
- `llm_api_key` (string, default: empty)
- `llm_system_prompt` (string, default: `Reply in ≤120 characters.`)
- `llm_model` (string, default: `gpt-4o-mini`)
- `llm_history_len` (int, default: `10`)
- `llm_max_chars` (int, default: `120`)

These will be persisted by BakkesMod and exposed in the plugin's F2 panel.

### Plugin UI (F2 panel)
- Checkbox: Enable/disable plugin.
- Text input: OpenAI API key (plain text).
- Textarea: System prompt.
- Text input: Model name.
- Int inputs: History length and max chars.
- Read-only status line: idle/in-flight/last error.

### OpenAI API contract
- Endpoint: `https://api.openai.com/v1/chat/completions`
- Auth: HTTP header `Authorization: Bearer <API_KEY>`
- Request (representative):
```json
{
  "model": "gpt-4o-mini",
  "messages": [
    { "role": "system", "content": "Reply in ≤120 characters." },
    { "role": "user", "content": "<older message 1>" },
    { "role": "assistant", "content": "<our previous reply>" },
    { "role": "user", "content": "<triggering message>" }
  ],
  "temperature": 0.7,
  "max_tokens": 80
}
```
- Response parse: `choices[0].message.content`.

### Dependencies and build with vcpkg
- **Libraries**
  - JSON: `nlohmann-json`
  - HTTP: Use native Windows `WinHTTP` (no external package needed)
- **Manifest-based setup (recommended)**
  - We will add a `vcpkg.json` manifest to declare `nlohmann-json`. Visual Studio in manifest mode will auto-install the dependency on first build.
- **Manual setup (optional)**
  - Install vcpkg and integrate with VS:
```powershell
# One-time setup
# See official docs: https://learn.microsoft.com/vcpkg/ for details

# Clone and bootstrap vcpkg
cd C:\
git clone https://github.com/microsoft/vcpkg.git
C:\vcpkg\bootstrap-vcpkg.bat

# Integrate with Visual Studio
C:\vcpkg\vcpkg.exe integrate install

# Install dependency (x64)
C:\vcpkg\vcpkg.exe install nlohmann-json:x64-windows
```
  - In Visual Studio, ensure the vcpkg toolchain is active (after `integrate install`, VS discovers it automatically). If using CMake or custom toolchain files, point to `C:\vcpkg\scripts\buildsystems\vcpkg.cmake`.

### Local dev environment
- Visual Studio 2022 (preferred) or 2019, x64 toolset, Windows 10 SDK.
- Open the solution/project in this repo (`BakkesPluginTemplate.vcxproj`).
- Build configuration: `Release | x64` (BakkesMod requires release DLLs).
- Build outputs: `Release\*.dll`.

### Installing the plugin for manual testing
- Copy the built DLL to the BakkesMod plugins folder:
  - `%AppData%\bakkesmod\bakkesmod\plugins\LLMChatPlugin.dll`
- Load the plugin in-game:
  - In BakkesMod console: `plugin load LLMChatPlugin`
  - Or add it to `plugins.cfg` to auto-load.
- Open F2 to access the plugin's UI panel; set the API key and system prompt.

### Development workflow
- Branching: feature branches; PRs into `main`.
- Style: Match template's C++ style; clear, readable names.
- Logging: Use BakkesMod's logging utilities; avoid noisy public chat on errors.
- Testing: Manual, in a private match; verify channel mirroring, history size, 120-char cap, and stale-response discard logic.

### Continuous integration and releases
- GitHub Actions will build the plugin on Windows and produce downloadable artifacts.
- Prerequisite for CI: the BakkesMod SDK must be present on the runner. The project resolves include/lib paths via the registry key `HKEY_CURRENT_USER\Software\BakkesMod\AppPath@BakkesModPath` (see `BakkesMod.props`).
  - For CI, we will add a setup step to populate this registry value and cache a copy of the SDK under a known path (e.g., `C:\bakkesmod`). Alternatively, run a one-time script that installs the SDK layout to that path.
- On pushes and pull requests, the workflow will:
  - Use vcpkg manifest mode to install dependencies (e.g., `nlohmann-json`).
  - Build the `Release|x64` DLL via MSBuild.
  - Upload the built DLL as a workflow artifact (downloadable from the Actions run page).
- On tagged pushes (e.g., tags like `v1.0.0`), the workflow will:
  - Create or update a GitHub Release for the tag.
  - Upload the built DLL (and a ZIP containing the DLL and README snippet) as release assets.

Representative workflow outline (for reference only; actual file will be added under `.github/workflows/build.yml`):
```yaml
name: build
on:
  push:
    branches: [ main ]
    tags: [ 'v*' ]
  pull_request:
    branches: [ main ]

jobs:
  windows:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-msbuild@v1.3.2
      - uses: microsoft/setup-vcpkg@v1
        with:
          vcpkg-triplet: x64-windows
          # Manifest mode auto-installs deps declared in vcpkg.json
      - name: Build (Release x64)
        run: msbuild BakkesPluginTemplate.vcxproj /p:Configuration=Release /p:Platform=x64
      - name: Upload artifact
        if: success()
        uses: actions/upload-artifact@v4
        with:
          name: bakkesmod-llm-chat-dll
          path: Release/*.dll
      - name: Create GitHub Release
        if: startsWith(github.ref, 'refs/tags/v')
        uses: softprops/action-gh-release@v2
        with:
          files: |
            Release/*.dll
```

How to download prebuilt binaries:
- For tagged releases: navigate to the repository's Releases page and download the DLL from the latest release.
- For PRs or non-tag builds: open the corresponding Actions run and download the artifact named `bakkesmod-llm-chat-dll`.

### Implementation plan (sequenced tasks)
1. Add `vcpkg.json` manifest with `nlohmann-json` dependency and hook up VS manifest mode.
2. Add CVars for configuration, persistence, and default values.
3. Implement ImGui UI in the plugin F2 panel for enable toggle, API key, system prompt, model, history/char limits, and a status line.
4. Hook typed chat events (exclude quickchats); identify local vs. remote sender; capture the chat channel.
5. Implement per-match ring buffer storing last N messages and prior assistant replies; reset on match end.
6. Build Chat Completions payload; serialize with `nlohmann-json`.
7. Implement async HTTP call via `WinHTTP` (worker thread), with timeouts and minimal retry.
8. Parse response, sanitize, enforce 120 char cap; reply to the same channel; prevent re-triggering on self messages.
9. Add request invalidation logic (monotonic `requestId`) to discard stale responses.
10. Harden logging and error paths; ensure all errors go to console only.
11. Add GitHub Actions workflow to build on Windows, upload PR artifacts, and publish DLL to GitHub Releases on tags.
12. Validate end-to-end; adjust defaults; document any event names used from RL.

### Acceptance criteria
- Responds only to other players' typed messages; ignores quickchats and local user's messages.
- Replies in the same channel; responses are ≤120 characters.
- Uses the last 10 messages of per-match history (including assistant replies) and resets correctly after match end.
- No cooldown; stale responses are discarded when a newer message arrives mid-flight.
- Errors are visible in BakkesMod console; configuration is adjustable via CVars and the F2 panel; settings persist.
- CI builds succeed on Windows; artifacts are available for PRs; tagged builds publish a DLL to GitHub Releases.

### Notes and open items
- Exact RL event names for typed chat and match end will be confirmed against the [BakkesMod plugin wiki](https://wiki.bakkesplugins.com/) during implementation.
- We will ensure that plugin-injected messages do not recursively trigger LLM responses.
- Future enhancements: model selector, temperature slider, clear-history button, prefix-based trigger, endpoint configurability (e.g., OpenRouter, local endpoints).