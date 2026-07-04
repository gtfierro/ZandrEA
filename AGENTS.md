# Repository Guidelines

## Project Structure & Module Organization
`libEA/` contains the core C++ analysis engine. `EAd/` wraps that engine in the REST daemon and includes API docs in `EAd/REST-API-v3.md`. `EAbacnet/` holds the Python BACnet polling adapter and sample config under `EAbacnet/config/`. `eajscli/` is the React frontend. REST tests live in `EAd/tests/`. Longer operational docs are under `docs/` and `docs/more_README/`.

## Build, Test, and Development Commands
Use the top-level `Makefile` as the main entrypoint.

- `make docker-up` builds and starts the development stack with Docker Compose.
- `make docker-down` stops the development stack.
- `make pushtestdata` pushes sample data into a running stack.
- `make test` runs the native C++ test flow plus the `EAd/tests` REST suite.
- `make compile` builds native binaries into `bin/` and `lib/`.
- `make jscli` starts the React app locally from `eajscli/`.
- `cd eajscli && npm test` runs frontend tests.

For ad hoc Python execution, prefer `uv` instead of creating manual virtualenvs, for example `uv run --with requests --with python-dateutil python EAd/tests/ead-functest-combined.py`.

## Coding Style & Naming Conventions
Match the surrounding file rather than reformatting broadly. C++ code uses header/source pairs, project-specific class prefixes such as `C*` and `A*`, and typically 3-4 space indentation. Python uses snake_case names. React components in `eajscli/src/Components/` use PascalCase filenames; hooks and helpers use camelCase, such as `useInterval.js` and `rest.js`. Frontend linting follows the default `react-scripts` ESLint config.

## Testing Guidelines
Add tests for each touched layer. REST regression tests are small Node or Python scripts in `EAd/tests/`; keep the `ead-*` naming pattern. Frontend tests use Jest and React Testing Library through `react-scripts`. There is no enforced coverage gate, so cover changed endpoints, BACnet flows, or UI states directly.

## Commit & Pull Request Guidelines
Recent history favors short, descriptive commit subjects, sometimes with a scope or PR number, for example `Remove I-P unit test data...` or `Dev260415 (#9)`. Keep subjects imperative and specific. Every commit must include a DCO sign-off: `Signed-off-by: Your Name <email>`. PRs should describe the affected subsystem, list commands run (`make test`, `npm test`, Docker smoke tests), link related issues, and include screenshots for `eajscli` UI changes.

## Security & Configuration Tips
This project has no built-in authentication. Do not expose it to the public Internet. Keep BACnet and REST configuration in local files under `EAbacnet/config/`, and use sample files such as `BACpypes.ini.sample` and `ead.ini.sample` as starting points.

Document usage with examples and motivation.
Don't make reference to earlier designs unless I explicitly tell you to.
