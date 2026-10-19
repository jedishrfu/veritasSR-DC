"""Checks shared CSV preservation and benchmark accounting without external codecs."""
import csv
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import lc_lossless_compress as runner


class BenchmarkTests(unittest.TestCase):
    def test_shared_csv_appends_and_rejects_different_schema(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/'common.csv'
            row = dict.fromkeys(runner.FIELDS,'')
            row['file'] = 'input, one.f32'
            runner.append_row(path,row)
            runner.append_row(path,row)
            with path.open(newline='') as stream:
                rows = list(csv.reader(stream))
            self.assertEqual(rows[0],runner.FIELDS)
            self.assertEqual(len(rows),3)
            self.assertEqual(rows[1][0],'input, one.f32')
            path.write_text('wrong,header\n')
            with self.assertRaises(ValueError):
                runner.append_row(path,row)
            self.assertEqual(path.read_text(),'wrong,header\n')

    def test_timings_use_original_bytes_and_reconstruction_is_checked(self):
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            inp,compressed,decoded = [directory/name for name in ['input','compressed','decoded']]
            inp.write_bytes(b'abcd'*8)
            compressed.write_bytes(b'abcd')
            decoded.write_bytes(inp.read_bytes())
            with patch.object(runner,'timed',side_effect=[2.0,4.0]):
                row = runner.measure(inp,compressed,decoded,['encoder','argument with spaces'],['decoder'],directory)
            self.assertEqual(row['compression_ratio'],8.0)
            self.assertEqual(row['compression_throughput_MB_s'],32/1_000_000/2)
            self.assertEqual(row['decompression_throughput_MB_s'],32/1_000_000/4)
            self.assertTrue(row['exact_reconstruction'])
            decoded.write_bytes(b'xxxx'*8)
            with patch.object(runner,'timed',side_effect=[2.0,4.0]):
                row = runner.measure(inp,compressed,decoded,['encoder'],['decoder'],directory)
            self.assertFalse(row['exact_reconstruction'])


if __name__ == '__main__':
    unittest.main()
