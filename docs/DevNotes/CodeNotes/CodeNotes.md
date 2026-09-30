# Code Notes

This document is primarily intended for me to make sure I have a solid understanding of the initially generated AI
code before making any modifications or further developments by me.

## How I Am Studying Each File

For each file or major block, I want to understand:

1. Its responsibility in the larger program.
2. Its inputs, outputs, and dependencies.
3. The C++ or library features being used.
4. Resource ownership and lifetime.
5. Normal control flow and failure paths.
6. Why this design was chosen and what alternatives exist.
7. What behavior should be covered by tests.

## Notes by Project

### Content

- [AssetCatalog.hpp and AssetCatalog.cpp](Content/AssetCatalog.md)

### Desktop

- [main.cpp](Desktop/main.md)

### Game

- [Game.hpp and Game.cpp](Game/Game.md)

### Tests

- [GameTests.cpp](Tests/GameTests.md)
- [AssetCatalogTests.cpp](Tests/AssetCatalogTests.md)

Tests are executable specifications. The unit test isolates deterministic game behavior without SDL or files. The
integration tests exercise the content boundary with temporary authored fixtures instead of committing extracted game
assets.

## Recommended Reading Order

1. [Desktop: main.cpp](Desktop/main.md)
2. [Game: Game.hpp and Game.cpp](Game/Game.md)
3. [Content: AssetCatalog.hpp and AssetCatalog.cpp](Content/AssetCatalog.md)
4. [Tests: GameTests.cpp](Tests/GameTests.md)
5. [Tests: AssetCatalogTests.cpp](Tests/AssetCatalogTests.md)

This order follows execution from the desktop entry point into the game and content libraries, then uses the tests to
confirm my understanding of their contracts.
