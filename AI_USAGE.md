# AI-assisted development

OpenRideMirror was built with substantial AI assistance, primarily through OpenAI Codex.

AI was used to help implement and refactor firmware and Monkey C code, diagnose BLE behavior, iterate on the interface, write the protocol/tooling/tests, and prepare documentation. The maintainer chose the product direction, hardware architecture, interaction design and trade-offs, reviewed the output, and performed the physical build, flashing and on-device testing.

This disclosure is not a claim that generated code is automatically correct. Contributions—human or AI-assisted—are held to the same requirements: understand the change, preserve protocol compatibility, avoid private data and secrets, run the relevant checks, and distinguish compiled support from hardware-tested support.

If you use an AI coding assistant on a contribution, saying so in the pull request is welcome. Include what it helped with and what you personally verified. No particular tool is required.
