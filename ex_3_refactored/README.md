# VeritasSR Refactored Prototype

This ZIP reorganizes the current VeritasSR prototype into a clearer project tree.

## Build

```bash
make
```

## Test

```bash
make test
```

## Notes

The main prototype is split into smaller include fragments under `src/` and included by `src/main.cpp`. This preserves current behavior while creating logical module boundaries for a future full `.h`/`.cpp` refactor.
