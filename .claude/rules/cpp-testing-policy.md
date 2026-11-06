---
description: Non-negotiable policies when writing or running tests
---

# Testing Policy

- When the user asks to only test (create/modify/revise/remove tests), NEVER
  change any non-test source files.
- If a bug is discovered while testing, DO NOT fix it unless the user explicitly
  said to. Let the test keep failing so it surfaces the bug to the user.
- The one exception: if the bug is IN the test itself, fix the test.
- Failing tests indicate suspected production bugs unless setup, object
  lifetimes, or key definitions are wrong. Do not delete or weaken a test to
  make it pass.
