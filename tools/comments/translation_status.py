import argparse
import csv
import fnmatch
import os
import re
import sys
from functools import lru_cache
from pathlib import Path
from typing import Iterable

try:
    import plotly.graph_objects as go
    from unidecode import unidecode
    from wordfreq import top_n_list
except ImportError:
    print("The 'wordfreq', 'unidecode', and 'plotly' libraries are required. Please install them by running: pip install wordfreq unidecode plotly", file=sys.stderr)
    sys.exit(1)

import tree_sitter_c as ts_c
import tree_sitter_cpp as ts_cpp
from tree_sitter import Language, Parser

WORD_RE = re.compile(r"[A-Za-z]+")


MANUAL_ENGLISH_WORDS = frozenset(
    {
        "arton",
        "bzip",
        "cvut",
        "dpb",
        "filezilla",
        "ftps",
        "hlist",
        "isel",
        "mkdir",
        "msie",
        "ocsp",
        "partl",
        "pecl",
        "ramdisk",
        "regedit",
        "srand",
        "tgz",
        "ubyte",
        "ulong",
        "winapi",
        "winscp",
    }
)


def _build_word_set(lang: str, count: int) -> set[str]:
    """Return a set with the *count* most common words for *lang* without diacritics."""
    # Normalize the most frequent words so tokens compare consistently even when the source comment uses punctuation or diacritics.
    words: set[str] = set()
    for word in top_n_list(lang, count):
        ascii_word = re.sub("[^A-Za-z]", "", unidecode(word)).lower()
        if ascii_word:
            words.add(ascii_word)
    return words


@lru_cache(maxsize=1)
def _load_word_sets() -> tuple[frozenset[str], frozenset[str]]:
    """Load and cache the Czech and English vocabularies used for classification."""
    # Limit the number of entries to keep the cached sets lightweight but representative for each language.
    cs_words = _build_word_set("cs", 500_000)
    en_words = _build_word_set("en", 200_000)

    # Remove any overlap between the automatically generated vocabularies.
    cs_words -= en_words

    # Manual overrides capture technical identifiers frequently used in the codebase that
    # would otherwise be classified as Czech due to their absence in the English frequency list.
    cs_words -= MANUAL_ENGLISH_WORDS
    en_words |= MANUAL_ENGLISH_WORDS

    return frozenset(cs_words), frozenset(en_words)


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]



def _path_sort_key(path: str) -> tuple[int, str]:
    return path.count("/"), path.casefold()


CPP_LANGUAGE = Language(ts_cpp.language())
C_LANGUAGE = Language(ts_c.language())
CPP_PARSER = Parser(CPP_LANGUAGE)
C_PARSER = Parser(C_LANGUAGE)
DEFAULT_EXTENSIONS = (".h", ".rh", ".c", ".cpp", ".rc")


def get_parser(file_path: Path) -> Parser:
    """Return a tree-sitter parser based on file suffix."""
    if file_path.suffix == ".c":
        return C_PARSER
    return CPP_PARSER


def extract_comments_from_file(file_path: Path) -> list[str]:
    """Return every comment found in *file_path*."""
    parser = get_parser(file_path)
    try:
        with file_path.open("r", encoding="utf-8", errors="ignore") as handle:
            code = handle.read()
    except Exception as exc:  # pragma: no cover - defensive I/O guard
        print(f"Error reading file {file_path}: {exc}", file=sys.stderr)
        return []

    tree = parser.parse(code.encode("utf-8"))
    comments: list[str] = []

    def visit(node) -> None:
        if "comment" in node.type:
            comments.append(code[node.start_byte : node.end_byte])
        for child in node.children:
            visit(child)

    visit(tree.root_node)
    return comments


def _token_counts(text: str) -> tuple[int, int]:
    """Return counts of (czech_like, english_like) tokens in *text*."""
    cs_words, en_words = _load_word_sets()
    cs_total = en_total = 0
    for token in WORD_RE.findall(text):
        normalized = unidecode(token).lower()
        if normalized in cs_words:
            cs_total += 1
        elif normalized in en_words:
            en_total += 1
    return cs_total, en_total


def classify_language(comment: str) -> str:
    """Classify *comment* as 'cs', 'en', or 'unknown' using token overlap."""
    cs_total, en_total = _token_counts(comment)
    total = cs_total + en_total
    if total == 0:
        return "unknown"
    if cs_total >= en_total * 2:
        return "cs"
    if en_total >= cs_total * 2:
        return "en"
    if cs_total >= en_total:
        return "cs"
    if en_total >= cs_total:
        return "en"
    return "unknown"


def _normalize_excluded(entries: Iterable[str]) -> tuple[str, ...]:
    """Strip misleading characters and normalise separators in excluded paths."""
    normalised = []
    for entry in entries:
        cleaned = entry.strip().strip("/\\")
        if cleaned:
            normalised.append(cleaned.replace("\\", "/"))
    return tuple(normalised)


EXCLUDED_DIRS = _normalize_excluded(
    {
        "src/common/dep",
        "src/sfx7zip/7zip",
        "src/sfx7zip/branch",
        "src/sfx7zip/lzma",
        "src/plugins/7zip/7za/c",
        "src/plugins/7zip/7za/cpp",
        "src/plugins/automation/generated",
        "src/plugins/checksum/tomcrypt",
        "src/plugins/ftp/openssl",
        "src/plugins/ieviewer/cmark-gfm",
        "src/plugins/mmviewer/ogg/vorbis",
        "src/plugins/mmviewer/wma/wmsdk",
        "src/plugins/pictview/exif/libexif",
        "src/plugins/pictview/exif/libjpeg",
        "src/plugins/pictview/twain",
        "src/plugins/portables/wtl",
        "src/plugins/shared/sqlite",
        "src/plugins/unchm/chmlib",
        "src/plugins/winscp/core",
        "src/plugins/winscp/forms",
        "src/plugins/winscp/packages",
        "src/plugins/winscp/putty",
        "src/plugins/winscp/resource",
        "src/plugins/winscp/windows/",
        "tree-sitter-grammars",
        "tools",
    }
)


def _path_is_excluded(relative_dir: str) -> bool:
    if not relative_dir:
        return False

    rel = relative_dir.strip('/')
    # Create variants with and without the src/ prefix so exclusion entries are matched regardless of how the directory is referenced.
    candidates = {rel}

    if rel.startswith('src/'):
        candidates.add(rel[4:])
    elif rel != 'src':
        candidates.add(f"src/{rel}")

    normalised = {candidate.strip('/') for candidate in candidates if candidate}

    for candidate in normalised:
        for excluded in EXCLUDED_DIRS:
            if candidate == excluded or candidate.startswith(f"{excluded}/"):
                return True
    return False


def _parse_args(argv: list[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Analyse the translation status of source code comments."
    )
    parser.add_argument(
        "--project-root",
        type=Path,
        default=None,
        help="Root directory to scan (default: current working directory)",
    )
    parser.add_argument(
        "--output-dir",
        type=Path,
        default=None,
        help="Directory where reports are written (default: project root)",
    )
    parser.add_argument(
        "--extensions",
        nargs="+",
        default=None,
        help="Override file extensions to scan, e.g. --extensions .h .cpp",
    )
    parser.add_argument(
        "--no-recursion",
        action="store_true",
        help="Process only the specified directory without descending into subdirectories.",
    )
    parser.add_argument(
        "--name-filter",
        nargs="+",
        default=None,
        help="Glob patterns used to select files, e.g. --name-filter dialogs*.cpp *.h.",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    args = _parse_args(argv)

    project_root = (args.project_root or Path.cwd()).resolve()
    if not project_root.exists() or not project_root.is_dir():
        print(f"Project root '{project_root}' does not exist or is not a directory.", file=sys.stderr)
        return 1

    output_dir = (args.output_dir or project_root).resolve()
    try:
        output_dir.mkdir(parents=True, exist_ok=True)
    except Exception as exc:
        print(f"Unable to create output directory '{output_dir}': {exc}", file=sys.stderr)
        return 1

    target_extensions = tuple(args.extensions) if args.extensions else DEFAULT_EXTENSIONS
    # Optional glob patterns let us limit scanning to a subset of files.
    name_filters = tuple(args.name_filter) if args.name_filter else None
    output_file_path = output_dir / "comments.txt"

    file_stats: dict[str, dict[str, int]] = {}
    print(f"Starting comment extraction from '{project_root}'...")

    with output_file_path.open("w", encoding="utf-8") as out_file:
        for root, dirs, files in os.walk(project_root):
            # Skip version-control metadata because it only contains tooling noise.
            dirs[:] = [d for d in dirs if d not in [".git", ".svn"]]
            dirs[:] = sorted(dirs, key=str.casefold)

            if args.no_recursion:
                # Honour --no-recursion by keeping traversal at the current directory.
                dirs[:] = []

            relative_dir = Path(os.path.relpath(root, project_root)).as_posix()
            if relative_dir == ".":
                relative_dir = ""
            if _path_is_excluded(relative_dir):
                # Do not descend into excluded subtrees.
                dirs[:] = []
                continue

            for filename in sorted(files, key=str.casefold):
                if not filename.endswith(target_extensions):
                    continue

                file_path = Path(root) / filename
                try:
                    relative_path = file_path.relative_to(REPOSITORY_ROOT).as_posix()
                except ValueError:
                    relative_path = file_path.relative_to(project_root).as_posix()
                relative_display = relative_path.replace('/', os.sep)
                if name_filters and not any(
                    fnmatch.fnmatch(relative_path, pattern) or fnmatch.fnmatch(filename, pattern)
                    for pattern in name_filters
                ):
                    # Skip files that do not match --name-filter patterns (match on full and base names).
                    continue

                out_file.write(f"--- File: {relative_display} ---\n")

                comments = extract_comments_from_file(file_path)
                file_cs_size = 0
                file_en_size = 0

                for comment in comments:
                    comment_text = comment.strip()
                    if not comment_text:
                        continue

                    out_file.write(comment_text + "\n")

                    language = classify_language(comment_text)
                    comment_size = len(comment_text.encode("utf-8"))

                    if language == "cs":
                        file_cs_size += comment_size
                    elif language == "en":
                        file_en_size += comment_size

                if file_cs_size > 0 or file_en_size > 0:
                    file_stats[relative_path] = {"cs": file_cs_size, "en": file_en_size}

                out_file.write("\n")

    print(f"Comment extraction complete. Results saved to '{output_file_path}'")

    # Summarise extracted comment stats so the CLI output highlights translation progress.
    total_cs_bytes = sum(stats["cs"] for stats in file_stats.values())
    total_en_bytes = sum(stats["en"] for stats in file_stats.values())
    total_bytes = total_cs_bytes + total_en_bytes
    files_with_comments = len(file_stats)
    translated_pct = (total_en_bytes / total_bytes * 100) if total_bytes else 0.0
    print("\nOverall statistics:")
    print(f"  Files with comments: {files_with_comments}")
    print(f"  Czech comments: {total_cs_bytes} bytes")
    print(f"  English comments: {total_en_bytes} bytes")
    print(f"  Translated to English: {translated_pct:.1f}%")
    generate_treemap(file_stats, output_dir)
    return 0


def _filter_file_stats(file_stats: dict[str, dict[str, int]], prefix: Path | None) -> dict[str, dict[str, int]]:
    if prefix is None:
        return dict(file_stats)

    prefix_posix = prefix.as_posix()
    filtered: dict[str, dict[str, int]] = {}
    for path, stats in file_stats.items():
        if path == prefix_posix or path.startswith(f"{prefix_posix}/"):
            try:
                relative_path = Path(path).relative_to(prefix).as_posix()
            except ValueError:
                continue
            filtered[relative_path] = stats
    return filtered


def _aggregate_stats(file_stats: dict[str, dict[str, int]]) -> dict[str, dict[str, int]]:
    aggregated = {path: stats.copy() for path, stats in file_stats.items()}

    for path in list(file_stats):
        parent = Path(path).parent
        while True:
            parent_key = parent.as_posix()
            if parent_key not in aggregated:
                # Ensure every ancestor exists so the later roll-up can update it.
                aggregated[parent_key] = {"cs": 0, "en": 0}
            if parent_key in ("", "."):
                break
            parent = parent.parent

    aggregated.setdefault(".", {"cs": 0, "en": 0})

    for path in sorted([p for p in aggregated if p != "."], key=lambda item: item.count("/"), reverse=True):
        parent = Path(path).parent.as_posix()
        if parent in ("", "."):
            parent = "."
        aggregated[parent]["cs"] += aggregated[path]["cs"]
        aggregated[parent]["en"] += aggregated[path]["en"]

    return aggregated


def _format_directory_label(raw_label: str, stats: dict[str, int]) -> str:
    """Return *raw_label* suffixed with the aggregated English translation percentage."""
    total_size = stats["cs"] + stats["en"]
    translated_pct = (stats["en"] / total_size * 100) if total_size > 0 else 0.0
    return f"{raw_label} ({translated_pct:.1f}%)"


def generate_treemap(
    file_stats: dict[str, dict[str, int]],
    output_dir: Path,
    *,
    prefix: Path | None = None,
    filename_prefix: str = "comments",
    root_label: str = "Project Root",
) -> None:
    scoped_stats = _filter_file_stats(file_stats, prefix)
    if not scoped_stats:
        print(f"\nNo comments found to generate reports for '{root_label}'.")
        return

    csv_path = output_dir / f"{filename_prefix}_stats.csv"
    try:
        with csv_path.open("w", newline="", encoding="utf-8") as csvfile:
            writer = csv.writer(csvfile)
            writer.writerow(["File Path", "To Translate (bytes)", "English Comments (bytes)", "Czech Comments (bytes)", "English Translation (%)"])
            for path, stats in sorted(scoped_stats.items(), key=lambda item: _path_sort_key(item[0])):
                cs_size = stats["cs"]
                en_size = stats["en"]
                total_size = cs_size + en_size
                en_percentage = (en_size / total_size * 100) if total_size > 0 else 0
                writer.writerow([
                    path.replace('/', os.sep),
                    f"{cs_size}",
                    f"{en_size}",
                    f"{cs_size}",
                    f"{en_percentage:.1f}",
                ])
        print(f"\nCSV report saved to '{csv_path}'")
    except Exception as exc:
        print(f"\nError saving CSV report: {exc}", file=sys.stderr)

    aggregated_stats = _aggregate_stats(scoped_stats)
    project_key = root_label
    aggregated_stats[project_key] = aggregated_stats.pop(".", {"cs": 0, "en": 0})

    ids: list[str] = []
    labels: list[str] = []
    parents: list[str] = []
    values: list[int] = []
    marker_colors: list[str] = []
    customdata: list[list[str]] = []

    for path, stats in sorted(aggregated_stats.items(), key=lambda item: _path_sort_key(item[0])):
        total_size = stats["cs"] + stats["en"]
        if total_size == 0 and path != project_key:
            continue

        if path == project_key:
            label = _format_directory_label(project_key, stats)
            parent = ""
        else:
            label = Path(path).name or path
            parent_path = Path(path).parent.as_posix()
            if parent_path in ("", "."):
                parent = project_key
            elif parent_path in aggregated_stats:
                parent = parent_path
            else:
                parent = project_key

            if path not in scoped_stats:
                # Only directories are missing from scoped_stats because it stores file entries.
                label = _format_directory_label(label, stats)

        ids.append(path)
        labels.append(label)
        parents.append(parent)
        values.append(total_size)

        cs_ratio = stats["cs"] / total_size if total_size > 0 else 0.5
        if cs_ratio > 0.5:
            proportion = (cs_ratio - 0.5) * 2
            red = int(128 + proportion * 127)
            green = int(128 - proportion * 128)
            blue = int(128 - proportion * 128)
        else:
            proportion = cs_ratio * 2
            red = int(proportion * 128)
            green = int(160 - proportion * 52)
            blue = int(proportion * 128)
        # Encode the share of Czech comments directly into the block colour.
        marker_colors.append(f"rgb({red},{green},{blue})")

        customdata.append([
            f"{stats['cs'] / 1024:.1f}",
            f"{stats['en'] / 1024:.1f}",
            f"{(stats['en'] / total_size * 100) if total_size > 0 else 0:.1f}",
        ])

    if not ids:
        print(f"\nNo comments found to generate a treemap for '{root_label}'.")
        return

    figure = go.Figure(
        go.Treemap(
            ids=ids,
            labels=labels,
            parents=parents,
            values=values,
            branchvalues="total",
            marker_colors=marker_colors,
            customdata=customdata,
            textfont=dict(color="white"),
            texttemplate="<b>%{label}</b><br>EN/CZ:<br>%{customdata[1]} / %{customdata[0]} KB<br>%{customdata[2]}%",
            hovertemplate="<b>%{id}</b><br>English/Czech: %{customdata[1]} / %{customdata[0]} KB<br>Translated: %{customdata[2]}%<extra></extra>",
        )
    )

    figure.update_layout(margin=dict(t=3, l=3, r=3, b=3))

    treemap_path = output_dir / f"{filename_prefix}_treemap.html"
    try:
        figure.write_html(str(treemap_path))
        print(f"\nTreemap visualization saved to '{treemap_path}'")
    except Exception as exc:
        print(f"\nError saving treemap: {exc}", file=sys.stderr)


if __name__ == "__main__":
    sys.exit(main())
