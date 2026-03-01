# Contributing

## Commit Style
- Use conventional commits (`feat`, `fix`, `docs`, `chore`).

## Branch Naming
- Use `phase/<name>` for phase work.
- Use `feature/<name>` for independent features.

## Code Guidelines
- Keep modules focused and testable.
- Follow ESP-IDF logging and error handling conventions.

## Component Layout
- `components/<module>/include/*.h` for public headers.
- `components/<module>/*.c` for implementation.

## Hardware Pins
- Keep board mapping updates in sync with code and docs.
