"""Count occurrences of predefined Czech words across source files."""

from __future__ import annotations

import argparse
import collections
import fnmatch
import os
import sys
from pathlib import Path
from typing import Iterable

from unidecode import unidecode

from .translation_status import (
    DEFAULT_EXTENSIONS,
    WORD_RE,
    _path_is_excluded,
)

REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_WORDS = (
    "a",
    "aby",
    "ale",
    "aniz",
    "ano",
    "asi",
    "az",
    "bez",
    "bude",
    "budem",
    "budes",
    "by",
    "byl",
    "byla",
    "byli",
    "bylo",
    "byt",
    "ci",
    "clanku",
    "co",
    "com",
    "coz",
    "cz",
    "das",
    "do",
    "email",
    "ho",
    "i",
    "jak",
    "jake",
    "jako",
    "je",
    "jeho",
    "jej",
    "jeji",
    "jejich",
    "jemuz",
    "jen",
    "jenz",
    "jeste",
    "jez",
    "ji",
    "jiz",
    "jine",
    "jiri",
    "jsem",
    "jses",
    "jsi",
    "jsme",
    "jsou",
    "jste",
    "k",
    "kam",
    "kde",
    "kdo",
    "kdyz",
    "ke",
    "ktery",
    "ktera",
    "ktere",
    "kteri",
    "kterou",
    "ku",
    "ma",
    "mate",
    "me",
    "meho",
    "mezi",
    "mi",
    "mit",
    "mne",
    "mnou",
    "muj",
    "mu",
    "muze",
    "my",
    "na",
    "nad",
    "nam",
    "napiste",
    "nas",
    "nase",
    "nasi",
    "ne",
    "nebo",
    "nebot",
    "necht",
    "nej",
    "nejsme",
    "neni",
    "net",
    "nez",
    "ni",
    "nic",
    "nove",
    "novy",
    "o",
    "od",
    "ode",
    "on",
    "org",
    "pak",
    "po",
    "pod",
    "podle",
    "pokud",
    "potom",
    "pouze",
    "prave",
    "pred",
    "pres",
    "pri",
    "pro",
    "proc",
    "proto",
    "protoze",
    "prvni",
    "pta",
    "s",
    "se",
    "si",
    "smime",
    "snad",
    "spolecnosti",
    "sve",
    "svuj",
    "svych",
    "svym",
    "svymi",
    "ta",
    "tak",
    "take",
    "takze",
    "tam",
    "tamhle",
    "tato",
    "te",
    "tedy",
    "tehdy",
    "ten",
    "tento",
    "teto",
    "tim",
    "timto",
    "to",
    "tohoto",
    "tom",
    "tomto",
    "totiz",
    "tu",
    "tuto",
    "tvuj",
    "ty",
    "tyto",
    "u",
    "uz",
    "v",
    "vam",
    "vas",
    "ve",
    "vedle",
    "vsak",
    "vsechen",
    "vy",
    "vzdy",
    "z",
    "za",
    "zatimco",
    "ze",
    "zpet",
    "zprava",
    "zpravy",
)


def _parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Count predefined Czech words in the codebase.")
    parser.add_argument(
        "--project-root",
        type=Path,
        default=REPOSITORY_ROOT / "src",
        help="Directory to scan (default: repository src).",
    )
    parser.add_argument(
        "--extensions",
        nargs="+",
        default=None,
        help="File extensions to include (default matches translation_status).",
    )
    parser.add_argument(
        "--name-filter",
        nargs="+",
        default=None,
        help="Optional glob patterns to restrict the scanned files.",
    )
    parser.add_argument(
        "--no-recursion",
        action="store_true",
        help="Do not descend into subdirectories when scanning.",
    )
    parser.add_argument(
        "--words-file",
        type=Path,
        default=None,
        help="Load target words from a text file (one word per line).",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=None,
        help="Optional path where the results are saved (text format).",
    )
    return parser.parse_args(argv)


def _iter_source_files(
    project_root: Path,
    extensions: tuple[str, ...],
    *,
    name_filters: tuple[str, ...] | None,
    no_recursion: bool,
) -> Iterable[Path]:
    """Yield files from *project_root* matching the provided configuration."""
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


def _load_target_words(words_file: Path | None) -> set[str]:
    if not words_file:
        return {word.lower() for word in DEFAULT_WORDS}

    words: set[str] = set()
    for raw_line in words_file.read_text(encoding="utf-8").splitlines():
        line = raw_line.strip()
        if line and not line.startswith("#"):
            words.add(line.lower())
    return words


def main(argv: list[str] | None = None) -> int:
    args = _parse_args(argv)
    project_root = args.project_root.resolve()
    if not project_root.exists() or not project_root.is_dir():
        print(f"Project root '{project_root}' is not a directory.", file=sys.stderr)
        return 1

    extensions = tuple(args.extensions) if args.extensions else DEFAULT_EXTENSIONS
    name_filters = tuple(args.name_filter) if args.name_filter else None
    target_words = _load_target_words(args.words_file)
    counter: collections.Counter[str] = collections.Counter()

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

        # Normalize accented characters so we can compare tokens with ASCII word lists.
        normalized_tokens = (unidecode(token).lower() for token in WORD_RE.findall(content))
        for token in normalized_tokens:
            if token in target_words:
                counter[token] += 1

    lines = [f"{word}: {counter[word]}" for word in sorted(target_words)]
    output = "\n".join(lines)

    if args.output:
        try:
            args.output.write_text(output + "\n", encoding="utf-8")
        except Exception as exc:
            print(f"Unable to write output file '{args.output}': {exc}", file=sys.stderr)
            return 1
    else:
        print(output)

    return 0


if __name__ == "__main__":
    sys.exit(main())



