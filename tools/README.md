# Salamander Tools (uv Workflow)

## Prerequisites
- Install [uv](https://docs.astral.sh/uv/) so the Python utilities can manage their own environment.
  - Windows (PowerShell): `irm https://astral.sh/uv/install.ps1 | iex`
  - Linux / macOS (shell): `curl -Ls https://astral.sh/uv/install.sh | sh`
  - Alternatively: `pipx install uv`
- Verify the installation with `uv --version`.

## Initial Setup
1. `cd tools` inside the Salamander repository.
2. Run `uv sync` to create/update the local virtual environment and install the locked dependencies.
   - Repeat `uv sync` whenever `pyproject.toml` or `uv.lock` changes.

## Running Utilities
- Execute any script via `uv run`. Examples:
  - `uv run comment-translation-status --help`
  - `uv run comment-translation-status --project-root ..\src --no-recursion --name-filter dialogs*.cpp`
  - `uv run comment-translation-status --project-root ..\src`
  - `uv run comment-find-czech-words --project-root ..\src --output czech_words.txt`
  - `uv run comment-find-czech-words --project-root ..\src\plugins\ --exclude shared\ --output plugins_czech_words.txt`
  - `uv run comment-word-counter --project-root ..\src --output word_counts.txt`
  - `uv run comment-code-guard --base-repo-path \salamander_code_guard`  
  - `uv run comment-code-guard --base-repo-path \salamander_code_guard --sub-path src\plugins\zip`  
- `uv run` ensures the command uses the environment defined by `uv sync`.

## Managing Dependencies
- Add or update dependencies with `uv add <package>` and then commit the updated `pyproject.toml` and `uv.lock`.
- To remove a dependency use `uv remove <package>` followed by `uv sync`.

## Troubleshooting
- If activation fails after switching Python versions, rerun `uv sync`.
- Delete the `.venv` directory inside `tools/` and `uv sync` again to rebuild a broken environment.
