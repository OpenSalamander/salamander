#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
Runs the Clang preprocessor on C/C++ files in two repositories and compares the
output to find code changes that are not just in comments.

This script is used to detect changes in C/C++ code between two repositories
(typically "upstream" and a local working copy). It uses the Clang
preprocessor (-E) to remove comments and expand macros, which allows for
comparing the core logic of the code itself. It then utilizes `git diff`
to display the differences between the processed files.

Main steps:
1. Verifies the existence of `clang++.exe` and the specified base repository.
2. Sets the paths to both repositories.
3. Gets a list of C/C++ files (`.cpp`, `.h`) to compare, ignoring defined
   excluded directories.
4. For each file, it runs the Clang preprocessor on the version in the current
   and the base repository.
5. Saves the Clang outputs to temporary files.
6. Compares these temporary files using `git diff` and displays any differences.
7. Deletes the temporary files upon completion.
"""

import argparse
import os
import platform
import re
import subprocess
import sys
import tempfile
import fnmatch
from pathlib import Path

# --- Configuration ---

# Path to the Clang compiler
CLANG_EXE = "clang++"
if platform.system() == "Windows":
    CLANG_EXE = "C:\\Program Files\\LLVM\\bin\\clang++.exe"

# Directories excluded from comparison (third-party libs, generated code)
EXCLUDE_PATTERNS = [
    'src/common/dep',
    'src/sfx7zip/7zip',
    'src/sfx7zip/branch',
    'src/sfx7zip/lzma',
    'src/plugins/7zip/7za/c',
    'src/plugins/7zip/7za/cpp',
    'src/plugins/7zip/patch',
    'src/plugins/automation/generated',
    'src/plugins/checksum/tomcrypt',
    'src/plugins/ftp/openssl',
    'src/plugins/ieviewer/cmark-gfm/build/src',
    'src/plugins/ieviewer/cmark-gfm/src',
    'src/plugins/mmviewer/ogg/vorbis',
    'src/plugins/mmviewer/wma/wmsdk',
    'src/plugins/pictview/exif/libexif',
    'src/plugins/pictview/exif/libjpeg',
    'src/plugins/pictview/twain',
    'src/plugins/portables/wtl',
    'src/plugins/unchm/chmlib',
    'src/plugins/winscp/core',
    'src/plugins/winscp/forms',
    'src/plugins/winscp/packages',
    'src/plugins/winscp/putty',
    'src/plugins/winscp/resource',
    'src/plugins/winscp/windows',
    'src/tools'
]

def get_clang_args(repo_path: Path) -> list:
    """Get Clang arguments with expanded system include paths.

    Args:
        repo_path: Root path of the repository to expand relative paths against

    Returns:
        List of clang arguments with expanded -isystem paths
    """
    base_args = [
        '-E',                                 # Preprocessor only
        '-P',                                 # No #line directives
        '-DSAFE_ALLOC',
        '-DFILE_ATTRIBUTE_ENCRYPTED=0x00004000',
        '-Wno-deprecated',
        '-Wno-comment',
        '-Wno-nonportable-include-path',
        '-Wno-extra-tokens',
        '-Wno-invalid-token-paste',
        '-Wno-macro-redefined',
        '-Wno-microsoft-include',
        '-Wno-#pragma-messages',
        '-fkeep-system-includes',
        '-fdiagnostics-color',
        '-fansi-escape-codes'
    ]

    # Include paths relative to repository root
    isystem_paths = [
        'src',
        'src/common',
        'src/plugins/shared',
        'src/common/dep',
        'src/plugins/automation',
        'src/plugins/7zip/7za/cpp/Common',
        'src/plugins/7zip/7za/cpp',
        'src/plugins/automation/generated',
        'src/plugins/shared/lukas',
        'src/plugins/ftp',
        'src/plugins/ftp/openssl',
        'src/plugins/ieviewer/cmark-gfm/build/src',
        'src/plugins/ieviewer/cmark-gfm/build/extensions',
        'src/plugins/ieviewer/cmark-gfm/extensions',
        'src/plugins/ieviewer/cmark-gfm/src',
        'src/plugins/pictview/exif',
        'src/plugins/tar',
        'src/plugins/undelete/library',
        'src/plugins/zip',
        'src/plugins/winscp/core',
        'src/plugins/winscp/windows',
        'src/plugins/winscp/resource',
        'src/plugins/winscp/forms',
        'src/plugins/wmobile/rapi',
        'tools/comments/code_guard_stubs'
    ]

    # Convert to absolute paths with -isystem flags
    expanded_args = base_args.copy()
    for path in isystem_paths:
        expanded_args.extend(['-isystem', str(repo_path / path)])

    return expanded_args
    

def find_clang_executable(exe_path: str) -> str:
    """Checks for Clang in the specified path or in system PATH.

    Args:
        exe_path: Path to the clang executable to check

    Returns:
        Valid path to clang executable

    Exits:
        If clang is not found
    """
    if platform.system() == "Windows":
        if Path(exe_path).exists():
            return exe_path
    else:
        result = subprocess.run(['which', exe_path], capture_output=True, text=True)
        if result.returncode == 0:
            return exe_path.strip()

    sys.stderr.write(f"Clang compiler not found at path '{exe_path}'. Please check the path in this script.\n")
    sys.exit(1)

def get_files_to_process(root_path: Path, include_patterns: list, exclude_regex: re.Pattern, file_name_filter: str):
    """Recursively finds files matching criteria, sorted for consistency.

    Args:
        root_path: Root directory to search from
        include_patterns: List of file patterns to include (e.g., ['*.cpp', '*.h'])
        exclude_regex: Compiled regex pattern for paths to exclude
        file_name_filter: Additional filter for file names

    Returns:
        List of Path objects for files to process
    """
    files_to_process = []
    for root, dirs, files in os.walk(root_path, topdown=True):
        dirs.sort()
        files.sort()

        # Remove excluded directories to avoid walking into them
        dirs[:] = [d for d in dirs if not exclude_regex.search(str(Path(root, d)))]

        for file in files:
            file_path = Path(root, file)
            if exclude_regex.search(str(file_path)):
                continue

            is_included = any(fnmatch.fnmatch(file, pattern) for pattern in include_patterns)
            matches_filter = fnmatch.fnmatch(file, file_name_filter)

            if is_included and matches_filter:
                files_to_process.append(file_path)

    return files_to_process

def main():
    """Main execution logic.

    Processes command line arguments, finds files to compare,
    runs clang preprocessor on both repositories, and shows differences.
    """
    parser = argparse.ArgumentParser(description="Compares C/C++ files between two repositories, ignoring comments.")
    def validate_base_repo_path(path):
        """Validates that the base repository path exists."""
        repo_path = Path(path)
        if not repo_path.exists():
            raise argparse.ArgumentTypeError(f"Base repository path '{path}' does not exist.")
        if not repo_path.is_dir():
            raise argparse.ArgumentTypeError(f"Base repository path '{path}' is not a directory.")
        return repo_path

    parser.add_argument(
        "--base-repo-path",
        type=validate_base_repo_path,
        required=True,
        help="Path to the base (upstream) repository to compare against."
    )
    parser.add_argument(
        "--file-name-filter",
        type=str,
        default="*",
        help="Filter for file names (e.g., '*.cpp', 'my_file.*'). Default is '*'."
    )
    parser.add_argument(
        "--debug",
        action="store_true",
        default=False,
        help="Enable debug mode to print clang command lines before execution."
    )
    parser.add_argument(
        "--sub-path",
        type=str,
        default=".",
        help="Relative path inside both repositories where scanning should start. Default is '.'."
    )
    parser.add_argument(
        "--keep-temp-files",
        action="store_true",
        default=False,
        help="Do not delete the generated clang output files."
    )
    args = parser.parse_args()

    clang_exe_path = find_clang_executable(CLANG_EXE)

    current_repo_path = Path(__file__).resolve().parent.parent.parent
    base_repo_path = args.base_repo_path

    sub_path = Path(args.sub_path)
    if sub_path.is_absolute():
        parser.error("--sub-path must be a relative path.")

    current_search_root = (current_repo_path / sub_path).resolve()
    base_search_root = (base_repo_path / sub_path).resolve()

    if not current_search_root.exists() or not current_search_root.is_dir():
        parser.error(f"Sub-path '{sub_path}' does not exist in the current repository.")

    if not base_search_root.exists() or not base_search_root.is_dir():
        parser.error(f"Sub-path '{sub_path}' does not exist in the base repository.")

    print("Searching for C/C++ code changes (ignoring comments)...")
    print(f"Repository 1 (Base):    {base_repo_path}")
    print(f"Repository 2 (Current): {current_repo_path}")
    print(f"Scanning Sub-path:      {sub_path}")
    print("-----------------------------------------------------------------")

    # Create OS-agnostic regex for excluding directories
    exclude_paths = [p.replace('/', os.sep) for p in EXCLUDE_PATTERNS]
    exclude_regex = re.compile('|'.join(re.escape(p) for p in exclude_paths))

    files_to_process = get_files_to_process(
        current_search_root,
        ['*.cpp', '*.h'],
        exclude_regex,
        args.file_name_filter
    )

    def create_temp_files():
        temp_file1_obj = tempfile.NamedTemporaryFile(
            mode='w+', delete=False, encoding='utf-8', suffix='.tmp', prefix='clang_out1_'
        )
        temp_file2_obj = tempfile.NamedTemporaryFile(
            mode='w+', delete=False, encoding='utf-8', suffix='.tmp', prefix='clang_out2_'
        )
        temp_paths = Path(temp_file1_obj.name), Path(temp_file2_obj.name)
        temp_file1_obj.close()
        temp_file2_obj.close()
        return temp_paths

    # Create temporary files for clang output
    temp_file1 = None
    temp_file2 = None
    if not args.keep_temp_files:
        temp_file1, temp_file2 = create_temp_files()

    try:
        for file_path in files_to_process:
            relative_path = file_path.relative_to(current_repo_path)
            file_in_base_repo = base_repo_path / relative_path

            if not file_in_base_repo.exists():
                print(f"Warning: File '{file_in_base_repo}' not found in the base repository. Skipping.")
                continue

            print(f'Comparing "{relative_path}"')

            try:
                clang_args_current = get_clang_args(current_repo_path)
                clang_args_base = get_clang_args(base_repo_path)

                # Apply plugin-specific flags to both invocations so preprocessing stays comparable
                relative_path_str = str(relative_path).replace('\\', '/')
                if 'src/plugins/renamer' in relative_path_str:
                    clang_args_current.append('-D_CHAR_UNSIGNED')
                    clang_args_base.append('-D_CHAR_UNSIGNED')
                if 'src/plugins/zip/selfextr' in relative_path_str:
                    clang_args_current.append('-DLANG_DEFINED')
                    clang_args_current.append('-DEXT_VER')
                    clang_args_base.append('-DLANG_DEFINED')
                    clang_args_base.append('-DEXT_VER')

                cmd1 = [clang_exe_path, *clang_args_current, str(file_path)]
                if args.debug:
                    print(f"DEBUG: Running clang on current repo: {' '.join(cmd1)}")
                res1 = subprocess.run(cmd1, capture_output=True, text=True, encoding='utf-8', check=True, cwd=current_repo_path)
                if res1.stderr.strip():
                    print(res1.stderr)

                cmd2 = [clang_exe_path, *clang_args_base, str(file_in_base_repo)]
                if args.debug:
                    print(f"DEBUG: Running clang on base repo: {' '.join(cmd2)}")
                res2 = subprocess.run(cmd2, capture_output=True, text=True, encoding='utf-8', check=True, cwd=base_repo_path)
                if res2.stderr.strip():
                    print(res2.stderr)
            except subprocess.CalledProcessError as e:
                sys.stderr.write(f"Error running Clang on {relative_path}:\n{e.stderr}\n")
                continue
            except FileNotFoundError:
                sys.stderr.write(f"Error: '{clang_exe_path}' not found. Please ensure Clang is installed and the path is correct.\n")
                sys.exit(1)


            # Normalize paths to avoid false-positives from __FILE__ macro
            def normalize_paths(content, repo_path):
                # Handle different path representations in preprocessor output
                path_single_backslash = str(repo_path).replace('/', '\\')
                path_forward = str(repo_path).replace('\\', '/')
                path_double_backslash = path_single_backslash.replace('\\', '\\\\')
                path_octal = path_single_backslash.replace('\\', '\\134')

                # Replace with consistent placeholder (order matters for specificity)
                content = re.sub(re.escape(path_octal), "REPO_ROOT", content)
                content = re.sub(re.escape(path_double_backslash), "REPO_ROOT", content)
                content = re.sub(re.escape(path_single_backslash), "REPO_ROOT", content)
                content = re.sub(re.escape(path_forward), "REPO_ROOT", content)

                return content

            content1 = normalize_paths(res1.stdout, current_repo_path)
            content2 = normalize_paths(res2.stdout, base_repo_path)

            if args.keep_temp_files:
                temp_file1, temp_file2 = create_temp_files()

            temp_file1.write_text(content1, encoding='utf-8')
            temp_file2.write_text(content2, encoding='utf-8')

            diff_process = subprocess.run(
                ['git', 'diff', '--no-index', '--unified=3', '--color=always', str(temp_file2), str(temp_file1)],
                capture_output=True, text=True, encoding='utf-8'
            )

            if diff_process.returncode != 0:
                separator = "=" * 80
                print()
                print(f"\033[36m{separator}\033[0m")
                print(f"\033[93m!!! CODE DIFFERENCE FOUND IN FILE \"{relative_path}\" !!!\033[0m")
                print(f"Base File:    {file_in_base_repo}")
                print(f"Current File: {file_path}")
                print(f"\033[36m{separator}\033[0m")
                print(diff_process.stdout)
                print(f"\033[36m{separator}\033[0m")
                print()
                if args.keep_temp_files:
                    print(f"Kept clang outputs: {temp_file2} vs {temp_file1}")
            elif args.keep_temp_files:
                print(f"Kept clang outputs: {temp_file2} vs {temp_file1}")

    finally:
        # Cleanup temporary files
        if not args.keep_temp_files:
            if temp_file1 and temp_file1.exists():
                temp_file1.unlink()
            if temp_file2 and temp_file2.exists():
                temp_file2.unlink()
        print("Done.")

if __name__ == "__main__":
    main()
