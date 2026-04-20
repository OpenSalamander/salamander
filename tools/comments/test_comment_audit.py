from __future__ import annotations

import contextlib
import io
import tempfile
import unittest
from pathlib import Path

from comments.find_czech_words import main as find_czech_words_main
from comments.translation_status import detect_czech_residue_tokens
from comments.word_counter import main as word_counter_main


class CommentAuditTests(unittest.TestCase):
    def test_detect_czech_residue_in_mixed_comment(self) -> None:
        comment = (
            "// TRUE, mel by viewer vratit system-event 'lock' v nonsignaled stavu, "
            "do signaled stavu 'lock'"
        )
        tokens = detect_czech_residue_tokens(comment)
        self.assertIn("vratit", tokens)
        self.assertIn("stavu", tokens)

    def test_ignore_single_false_positive_token(self) -> None:
        self.assertEqual(detect_czech_residue_tokens("// hitem"), [])

    def test_ignore_short_non_czech_token_clusters(self) -> None:
        self.assertEqual(detect_czech_residue_tokens("// bej chv dzo khm smj votic zho"), [])

    def test_find_czech_words_uses_comments_only(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "mixed.cpp").write_text(
                'int hitem = 0;\n'
                '// POZOR: volat HANDLES_CAN_USE_TRACE() tesne po inicializaci "dbg.h" modulu\n',
                encoding="utf-8",
            )
            (root / "identifier_only.cpp").write_text("int windir = 0;\n", encoding="utf-8")
            (root / "false_positive_comment.cpp").write_text("// hitem\n", encoding="utf-8")

            stdout = io.StringIO()
            with contextlib.redirect_stdout(stdout):
                exit_code = find_czech_words_main(["--project-root", str(root)])

            output = stdout.getvalue()
            self.assertEqual(exit_code, 0)
            self.assertIn("--- mixed.cpp ---", output)
            self.assertIn("pozor", output)
            self.assertNotIn("--- identifier_only.cpp ---", output)
            self.assertNotIn("--- false_positive_comment.cpp ---", output)

    def test_word_counter_counts_only_residue_comments(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            (root / "sample.cpp").write_text(
                'int windir = 0;\n'
                '// pokud je vetsi nez nula, probiha prave prikaz\n'
                '// hitem\n',
                encoding="utf-8",
            )
            (root / "words.txt").write_text("pokud\nprave\nhitem\nwindir\n", encoding="utf-8")

            stdout = io.StringIO()
            with contextlib.redirect_stdout(stdout):
                exit_code = word_counter_main(
                    ["--project-root", str(root), "--words-file", str(root / "words.txt")]
                )

            output = stdout.getvalue()
            self.assertEqual(exit_code, 0)
            self.assertIn("pokud: 1", output)
            self.assertIn("prave: 1", output)
            self.assertIn("hitem: 0", output)
            self.assertIn("windir: 0", output)


if __name__ == "__main__":
    unittest.main()
