"""Utilities for listing Czech words (normalized) that appear in source files."""

from __future__ import annotations

import fnmatch
import os

import argparse
import sys
from pathlib import Path
from typing import Iterable

from unidecode import unidecode

from .translation_status import (
    DEFAULT_EXTENSIONS,
    WORD_RE,
    _load_word_sets,
    _path_is_excluded,
    _path_sort_key,
)

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]


def _parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Locate unique Czech words (without diacritics) used in comments or identifiers.",
    )
    parser.add_argument(
        "--project-root",
        type=Path,
        default=REPOSITORY_ROOT / "src",
        help="Directory to scan for source files (default: repository src).",
    )
    parser.add_argument(
        "--extensions",
        nargs="+",
        default=None,
        help="File extensions to include, e.g. --extensions .cpp .h (default matches translation_status).",
    )
    parser.add_argument(
        "--name-filter",
        nargs="+",
        default=None,
        help="Optional glob patterns for filenames relative to the project root.",
    )
    parser.add_argument(
        "--no-recursion",
        action="store_true",
        help="Do not descend into subdirectories when scanning.",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="Optional path where the unique word list is written (text format).",
    )
    return parser.parse_args(argv)


def _iter_source_files(
    project_root: Path,
    extensions: tuple[str, ...],
    *,
    name_filters: tuple[str, ...] | None,
    no_recursion: bool,
) -> Iterable[Path]:
    """Yield files from *project_root* that match the provided configuration."""
    for root, dirs, files in os.walk(project_root):
        dirs[:] = [d for d in dirs if d not in [".git", ".svn"]]
        dirs[:] = sorted(dirs, key=str.casefold)
        if no_recursion:
            dirs[:] = []

        relative_dir = Path(root).relative_to(project_root).as_posix()
        if relative_dir == ".":
            relative_dir = ""
        if _path_is_excluded(relative_dir):
            dirs[:] = []
            continue

        for filename in sorted(files, key=str.casefold):
            if not filename.endswith(extensions):
                continue

            file_path = Path(root) / filename
            rel_path = file_path.relative_to(project_root).as_posix()
            if name_filters and not any(
                fnmatch.fnmatch(rel_path, pattern) or fnmatch.fnmatch(filename, pattern)
                for pattern in name_filters
            ):
                continue

            yield file_path


def _collect_words(text: str, cs_words: frozenset[str]) -> set[str]:
    """Return normalised Czech words appearing in *text*."""
    found: set[str] = set()
    for token in WORD_RE.findall(text):
        candidate = unidecode(token).lower()
        if candidate in cs_words:
            found.add(candidate)
    return found


def main(argv: list[str] | None = None) -> int:
    args = _parse_args(argv)
    project_root = args.project_root.resolve()
    if not project_root.exists() or not project_root.is_dir():
        print(f"Project root '{project_root}' is not a directory.", file=sys.stderr)
        return 1

    extensions = tuple(args.extensions) if args.extensions else DEFAULT_EXTENSIONS
    name_filters = tuple(args.name_filter) if args.name_filter else None

    cs_words, _ = _load_word_sets()
    files_with_words: dict[Path, set[str]] = {}
    unique_words: set[str] = set()

    for file_path in _iter_source_files(
        project_root,
        extensions,
        name_filters=name_filters,
        no_recursion=args.no_recursion,
    ):
        try:
            content = file_path.read_text(encoding="utf-8", errors="ignore")
        except Exception as exc:
            print(f"Failed to read {file_path.relative_to(project_root)}: {exc}", file=sys.stderr)
            continue

        words = _collect_words(content, cs_words)
        if words:
            files_with_words[file_path] = words
            unique_words.update(words)

    output_lines: list[str] = []
    for file_path in sorted(files_with_words, key=lambda path: _path_sort_key(path.relative_to(project_root).as_posix())):
        rel_display = file_path.relative_to(project_root).as_posix().replace("/", os.sep)
        output_lines.append(f"--- {rel_display} ---")
        for word in sorted(files_with_words[file_path]):
            output_lines.append(f"  {word}")
        output_lines.append("")

    output_lines.append("All unique Czech words (alphabetical):")
    for word in sorted(unique_words):
        output_lines.append(word)

    text_output = "\n".join(output_lines)
    if args.output:
        try:
            args.output.write_text(text_output, encoding="utf-8")
        except Exception as exc:
            print(f"Unable to write output file '{args.output}': {exc}", file=sys.stderr)
            return 1
    else:
        print(text_output)

    return 0


if __name__ == "__main__":
    sys.exit(main())




