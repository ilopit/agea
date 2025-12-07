"""Tests for arapi.utils module."""
import unittest
import tempfile
import os
import arapi.utils


class TestExtstrip(unittest.TestCase):
    """Test extstrip function."""

    def test_extstrip_removes_spaces(self):
        result = arapi.utils.extstrip('  value  ')
        self.assertEqual(result, 'value')

    def test_extstrip_removes_quotes(self):
        result = arapi.utils.extstrip('"value"')
        self.assertEqual(result, 'value')

    def test_extstrip_removes_newlines(self):
        result = arapi.utils.extstrip('value\n')
        self.assertEqual(result, 'value')

    def test_extstrip_removes_tabs(self):
        result = arapi.utils.extstrip('\tvalue\t')
        self.assertEqual(result, 'value')

    def test_extstrip_removes_all_characters(self):
        result = arapi.utils.extstrip('  "value"\n\t')
        self.assertEqual(result, 'value')

    def test_extstrip_empty_string(self):
        result = arapi.utils.extstrip('')
        self.assertEqual(result, '')


class TestReplaceBlocksWithSortedInsert(unittest.TestCase):
    """Test replace_blocks_with_sorted_insert function."""

    def setUp(self):
        self.temp_file = tempfile.NamedTemporaryFile(mode='w', delete=False, suffix='.h')
        self.temp_file.close()

    def tearDown(self):
        if os.path.exists(self.temp_file.name):
            os.unlink(self.temp_file.name)

    def test_single_block_replacement(self):
        """Test replacing a single block."""
        content = """#pragma once

// block start test
old content
// block end test
"""
        with open(self.temp_file.name, 'w') as f:
            f.write(content)

        replacements = {
            'test': 'new content'
        }

        result = arapi.utils.replace_blocks_with_sorted_insert(self.temp_file.name, replacements)

        self.assertTrue(result)
        with open(self.temp_file.name, 'r') as f:
            output = f.read()
            self.assertIn('// block start test', output)
            self.assertIn('new content', output)
            self.assertNotIn('old content', output)
            self.assertIn('// block end test', output)

    def test_multiple_blocks_replacement(self):
        """Test replacing multiple blocks."""
        content = """#pragma once

// block start block1
old1
// block end block1

// block start block2
old2
// block end block2
"""
        with open(self.temp_file.name, 'w') as f:
            f.write(content)

        replacements = {
            'block1': 'new1',
            'block2': 'new2'
        }

        result = arapi.utils.replace_blocks_with_sorted_insert(self.temp_file.name, replacements)

        self.assertTrue(result)
        with open(self.temp_file.name, 'r') as f:
            output = f.read()
            self.assertIn('new1', output)
            self.assertIn('new2', output)
            self.assertNotIn('old1', output)
            self.assertNotIn('old2', output)

    def test_preserves_indentation(self):
        """Test that indentation is preserved from block start line."""
        content = """    // block start test
    old
    // block end test
"""
        with open(self.temp_file.name, 'w') as f:
            f.write(content)

        replacements = {
            'test': 'new'
        }

        result = arapi.utils.replace_blocks_with_sorted_insert(self.temp_file.name, replacements)

        self.assertTrue(result)
        with open(self.temp_file.name, 'r') as f:
            lines = f.readlines()
            # Check that replacement line has same indentation
            self.assertTrue(any('    new' in line for line in lines))

    def test_no_replacement_when_block_not_in_dict(self):
        """Test that blocks not in replacements dict have content removed."""
        content = """// block start test1
content1
// block end test1

// block start test2
content2
// block end test2
"""
        with open(self.temp_file.name, 'w') as f:
            f.write(content)

        replacements = {
            'test1': 'new1'
        }

        result = arapi.utils.replace_blocks_with_sorted_insert(self.temp_file.name, replacements)

        self.assertTrue(result)
        with open(self.temp_file.name, 'r') as f:
            output = f.read()
            self.assertIn('new1', output)
            # test2 block content is removed when not in replacements dict
            self.assertIn('// block start test2', output)
            self.assertIn('// block end test2', output)
            self.assertNotIn('content2', output)

    def test_no_changes_returns_false(self):
        """Test that function returns False when no changes are made."""
        content = """// block start test
content
// block end test
"""
        with open(self.temp_file.name, 'w') as f:
            f.write(content)

        # Replacement is same as original
        replacements = {
            'test': 'content'
        }

        result = arapi.utils.replace_blocks_with_sorted_insert(self.temp_file.name, replacements)

        self.assertFalse(result)

    def test_multiline_replacement(self):
        """Test replacing with multiple lines."""
        content = """// block start test
old
// block end test
"""
        with open(self.temp_file.name, 'w') as f:
            f.write(content)

        replacements = {
            'test': 'line1\nline2\nline3'
        }

        result = arapi.utils.replace_blocks_with_sorted_insert(self.temp_file.name, replacements)

        self.assertTrue(result)
        with open(self.temp_file.name, 'r') as f:
            output = f.read()
            self.assertIn('line1', output)
            self.assertIn('line2', output)
            self.assertIn('line3', output)
            self.assertNotIn('old', output)

    def test_empty_replacement(self):
        """Test replacing with empty content."""
        content = """// block start test
old content
// block end test
"""
        with open(self.temp_file.name, 'w') as f:
            f.write(content)

        replacements = {
            'test': ''
        }

        result = arapi.utils.replace_blocks_with_sorted_insert(self.temp_file.name, replacements)

        self.assertTrue(result)
        with open(self.temp_file.name, 'r') as f:
            output = f.read()
            self.assertIn('// block start test', output)
            self.assertIn('// block end test', output)
            self.assertNotIn('old content', output)

    def test_block_with_whitespace(self):
        """Test block with extra whitespace in markers."""
        content = """  // block start test  
    old
  // block end test  
"""
        with open(self.temp_file.name, 'w') as f:
            f.write(content)

        replacements = {
            'test': 'new'
        }

        result = arapi.utils.replace_blocks_with_sorted_insert(self.temp_file.name, replacements)

        self.assertTrue(result)
        with open(self.temp_file.name, 'r') as f:
            output = f.read()
            self.assertIn('new', output)
            self.assertNotIn('old', output)

    def test_preserves_lines_outside_blocks(self):
        """Test that lines outside blocks are preserved."""
        content = """#pragma once
#include <header.h>

// block start test
old
// block end test

void function();
"""
        with open(self.temp_file.name, 'w') as f:
            f.write(content)

        replacements = {
            'test': 'new'
        }

        result = arapi.utils.replace_blocks_with_sorted_insert(self.temp_file.name, replacements)

        self.assertTrue(result)
        with open(self.temp_file.name, 'r') as f:
            output = f.read()
            self.assertIn('#pragma once', output)
            self.assertIn('#include <header.h>', output)
            self.assertIn('void function();', output)
            self.assertIn('new', output)
            self.assertNotIn('old', output)

    def test_block_with_multiple_lines_original(self):
        """Test replacing block that originally had multiple lines."""
        content = """// block start test
line1
line2
line3
// block end test
"""
        with open(self.temp_file.name, 'w') as f:
            f.write(content)

        replacements = {
            'test': 'single line'
        }

        result = arapi.utils.replace_blocks_with_sorted_insert(self.temp_file.name, replacements)

        self.assertTrue(result)
        with open(self.temp_file.name, 'r') as f:
            output = f.read()
            self.assertIn('single line', output)
            self.assertNotIn('line1', output)
            self.assertNotIn('line2', output)
            self.assertNotIn('line3', output)

    def test_empty_replacements_dict(self):
        """Test with empty replacements dict (content removed, file changes)."""
        content = """// block start test
content
// block end test
"""
        with open(self.temp_file.name, 'w') as f:
            f.write(content)

        replacements = {}

        result = arapi.utils.replace_blocks_with_sorted_insert(self.temp_file.name, replacements)

        # File changes because block content is removed
        self.assertTrue(result)
        with open(self.temp_file.name, 'r') as f:
            output = f.read()
            # Block markers remain but content is removed
            self.assertIn('// block start test', output)
            self.assertIn('// block end test', output)
            self.assertNotIn('content', output)

    def test_block_name_with_spaces(self):
        """Test block name that has spaces."""
        content = """// block start my block
old content
// block end my block
"""
        with open(self.temp_file.name, 'w') as f:
            f.write(content)

        replacements = {
            'my block': 'new content'
        }

        result = arapi.utils.replace_blocks_with_sorted_insert(self.temp_file.name, replacements)

        self.assertTrue(result)
        with open(self.temp_file.name, 'r') as f:
            output = f.read()
            self.assertIn('new content', output)
            self.assertNotIn('old content', output)

    def test_complex_indentation(self):
        """Test with complex indentation patterns."""
        content = """namespace test {
    // block start inner
        deeply nested
    // block end inner
}
"""
        with open(self.temp_file.name, 'w') as f:
            f.write(content)

        replacements = {
            'inner': 'replaced'
        }

        result = arapi.utils.replace_blocks_with_sorted_insert(self.temp_file.name, replacements)

        self.assertTrue(result)
        with open(self.temp_file.name, 'r') as f:
            lines = f.readlines()
            # Check that replacement preserves the indentation of block start
            found_replacement = False
            for line in lines:
                if 'replaced' in line:
                    found_replacement = True
                    # Should have same indentation as "    // block start inner"
                    self.assertTrue(line.startswith('    '))
            self.assertTrue(found_replacement)

    def test_file_with_only_blocks(self):
        """Test file that contains only blocks."""
        content = """// block start test1
content1
// block end test1
// block start test2
content2
// block end test2
"""
        with open(self.temp_file.name, 'w') as f:
            f.write(content)

        replacements = {
            'test1': 'new1',
            'test2': 'new2'
        }

        result = arapi.utils.replace_blocks_with_sorted_insert(self.temp_file.name, replacements)

        self.assertTrue(result)
        with open(self.temp_file.name, 'r') as f:
            output = f.read()
            self.assertIn('new1', output)
            self.assertIn('new2', output)
            self.assertNotIn('content1', output)
            self.assertNotIn('content2', output)

    def test_insert_missing_block(self):
        """Test that missing blocks are inserted in sorted order."""
        content = """#pragma once
// block start existing
content
// block end existing
"""
        with open(self.temp_file.name, 'w') as f:
            f.write(content)

        replacements = {
            'existing': 'updated',
            'missing': 'new block content'
        }

        result = arapi.utils.replace_blocks_with_sorted_insert(self.temp_file.name, replacements)

        self.assertTrue(result)
        with open(self.temp_file.name, 'r') as f:
            output = f.read()
            self.assertIn('updated', output)
            self.assertIn('new block content', output)
            # Missing block should be inserted
            self.assertIn('// block start missing', output)
            self.assertIn('// block end missing', output)

    def test_insert_multiple_missing_blocks_sorted(self):
        """Test that multiple missing blocks are inserted in sorted order at the end (no root)."""
        content = """#pragma once
// block start zblock
content
// block end zblock
"""
        with open(self.temp_file.name, 'w') as f:
            f.write(content)

        replacements = {
            'zblock': 'updated',
            'ablock': 'first',
            'mblock': 'middle'
        }

        result = arapi.utils.replace_blocks_with_sorted_insert(self.temp_file.name, replacements)

        self.assertTrue(result)
        with open(self.temp_file.name, 'r') as f:
            lines = f.readlines()
            # Find positions of block starts
            ablock_pos = None
            mblock_pos = None
            zblock_pos = None
            
            for i, line in enumerate(lines):
                if '// block start ablock' in line:
                    ablock_pos = i
                elif '// block start mblock' in line:
                    mblock_pos = i
                elif '// block start zblock' in line:
                    zblock_pos = i
            
            # Missing blocks should be in sorted order among themselves
            # Since there's no root, they're inserted at the end (after zblock)
            self.assertIsNotNone(ablock_pos)
            self.assertIsNotNone(mblock_pos)
            self.assertIsNotNone(zblock_pos)
            self.assertLess(ablock_pos, mblock_pos)
            # zblock was already in file, so it comes first
            self.assertLess(zblock_pos, ablock_pos)
            self.assertLess(zblock_pos, mblock_pos)

    def test_root_block_always_last(self):
        """Test that root block is always inserted last."""
        content = """#pragma once
// block start other
content
// block end other
"""
        with open(self.temp_file.name, 'w') as f:
            f.write(content)

        replacements = {
            'other': 'updated',
            'root': 'root content',
            'another': 'another content'
        }

        result = arapi.utils.replace_blocks_with_sorted_insert(self.temp_file.name, replacements)

        self.assertTrue(result)
        with open(self.temp_file.name, 'r') as f:
            lines = f.readlines()
            # Find positions
            another_pos = None
            other_pos = None
            root_pos = None
            
            for i, line in enumerate(lines):
                if '// block start another' in line:
                    another_pos = i
                elif '// block start other' in line:
                    other_pos = i
                elif '// block start root' in line:
                    root_pos = i
            
            # Root should be last
            self.assertIsNotNone(another_pos)
            self.assertIsNotNone(other_pos)
            self.assertIsNotNone(root_pos)
            self.assertLess(another_pos, root_pos)
            self.assertLess(other_pos, root_pos)

    def test_insert_missing_before_existing_root(self):
        """Test that missing blocks are inserted before existing root block."""
        content = """#pragma once
// block start root
root content
// block end root
"""
        with open(self.temp_file.name, 'w') as f:
            f.write(content)

        replacements = {
            'root': 'updated root',
            'newblock': 'new content'
        }

        result = arapi.utils.replace_blocks_with_sorted_insert(self.temp_file.name, replacements)

        self.assertTrue(result)
        with open(self.temp_file.name, 'r') as f:
            lines = f.readlines()
            # Find positions
            newblock_pos = None
            root_pos = None
            
            for i, line in enumerate(lines):
                if '// block start newblock' in line:
                    newblock_pos = i
                elif '// block start root' in line:
                    root_pos = i
            
            # New block should be before root
            self.assertIsNotNone(newblock_pos)
            self.assertIsNotNone(root_pos)
            self.assertLess(newblock_pos, root_pos)

    def test_insert_missing_with_default_indentation(self):
        """Test that inserted blocks use default indentation."""
        content = """#pragma once
"""
        with open(self.temp_file.name, 'w') as f:
            f.write(content)

        replacements = {
            'newblock': 'content'
        }

        result = arapi.utils.replace_blocks_with_sorted_insert(self.temp_file.name, replacements)

        self.assertTrue(result)
        with open(self.temp_file.name, 'r') as f:
            lines = f.readlines()
            # Find the content line in the new block
            for line in lines:
                if 'content' in line and 'block' not in line:
                    # Should have default indentation (4 spaces)
                    self.assertTrue(line.startswith('    '))
                    break


if __name__ == '__main__':
    unittest.main()

