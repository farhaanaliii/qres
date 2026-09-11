# Contributing

## Getting started

Fork the repo, clone it, and install in editable mode:

```
git clone https://github.com/farhaanaliii/qres
cd qres
pip install -e .
```

## Making changes

- C sources live in `src/`. The extension is `qres._qres` (private), the public API is `qres/`.
- Match the existing code style: PascalCase for C structs and types, snake_case everywhere else.
- Keep changes minimal and focused. One concern per PR.

## Submitting a pull request

- Open an issue first for anything non-trivial.
- Make sure the extension compiles and all tests pass (`pytest`) before opening a PR.
- Keep the PR title under 70 characters. Put detail in the description.

## Reporting bugs

Use the bug report template. Include the `.qrc` that triggers the issue, the expected output, and the actual output or error.
