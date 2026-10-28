"""Portable synthetic tests; these are not physical touch acceptance evidence."""
import contextlib
import importlib.util
import io
import os
import tempfile
import base64
import hashlib
import json
from pathlib import Path
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('touch_diagnostics', ROOT/'scripts/touch_diagnostics.py')
d = importlib.util.module_from_spec(spec)
spec.loader.exec_module(d)


def frame(raw=b'{"ok":true}', packed=None, digest=None):
    if packed is None:
        packed = zlib.compress(raw)
    w = base64.b64encode(packed)
    return (b'DET_META %d %s %d\r\n' % (len(raw), (digest or hashlib.sha256(raw).hexdigest()).encode(), len(w))
            + b'DET_DATA ' + w + b'\r\nDET_END\r\n# \r\n')

class Diagnostics(unittest.TestCase):
    def test_prompt_with_kernel_same_line_is_not_success(self):
        # Synthetic fixtures: no real board log or device identity is shipped.
        r=d.observe(b'[ 1.000] bus error\n# [ 1.001] reset\n','DET')
        self.assertTrue(r['root_prompt_seen']);self.assertEqual(r['status'],'waiting')
        self.assertFalse(r['retry_allowed'])
    def test_failure_shapes_with_kernel_noise(self):
        for error in [b'binascii.Error: Incorrect padding',b'UnicodeDecodeError: invalid continuation byte',b'SyntaxError: invalid syntax']:
            for prompt in [b'#',b'# ',b'# [ 1.001] reset\n']:
                with self.subTest(error=error,prompt=prompt):
                    r=d.observe(b'  File "<string>", line 1\n'+error+b'\n'+prompt,'DET',finalized=True)
                    self.assertEqual(r['status'],'rejected');self.assertTrue(r['root_terminal'])
                    self.assertFalse(r['retry_allowed']);self.assertFalse(r['board_accepted'])
    def test_sealed_prompt_without_newline(self):
        b=b'SyntaxError: invalid syntax\n# '
        self.assertEqual(d.observe(b,'DET')['status'],'error_pending_terminal')
        self.assertEqual(d.observe(b,'DET',finalized=True)['status'],'rejected')
    def test_complete_frame_not_board_acceptance(self):
        r=d.observe(frame(),'DET');self.assertEqual(r['status'],'complete')
        self.assertEqual(r['payload'],{'ok':True});self.assertFalse(r['board_accepted'])
    def test_every_partial_byte_is_not_complete(self):
        b=frame()
        for i in range(len(b)):
            self.assertNotEqual(d.observe(b[:i],'DET')['status'],'complete',i)
    def test_stale_prompt_does_not_finish(self):
        self.assertEqual(d.observe(b'# \n'+frame().rsplit(b'#',1)[0],'DET')['status'],'waiting')
    def test_plain_and_qualified_error_names(self):
        for name in [b'Error',b'binascii.Error',b'SyntaxError',b'UnicodeDecodeError',b'RuntimeError']:
            self.assertEqual(d.observe(name+b': failed\n# \n','DET')['status'],'rejected')
    def test_error_requires_later_prompt(self):
        r=d.observe(b'# \nSyntaxError: invalid syntax\n','DET')
        self.assertEqual(r['status'],'error_pending_terminal')
    def test_error_overrides_valid_frame(self):
        r=d.observe(frame()+b'SyntaxError: late failure\n# [ 1.234] kernel\n','DET')
        self.assertEqual(r['status'],'rejected')
    def test_kernel_lines_between_records(self):
        r=d.observe(frame().replace(b'\r\n',b'\r\n[ 1.234] kernel log\r\n'),'DET')
        self.assertEqual(r['status'],'complete')
    def test_inline_kernel_corruption_rejected(self):
        b=frame().replace(b'DET_DATA ',b'DET_DATA [ 1.234] noise ')
        self.assertEqual(d.observe(b,'DET')['status'],'invalid')
    def test_duplicate_meta_rejected(self):
        b=frame();self.assertEqual(d.observe(b.split(b'\r\n')[0]+b'\r\n'+b,'DET')['status'],'invalid')
    def test_sealed_trailing_marker_without_newline(self):
        for tail in [b'DET_END',b'DET_DATA Zg==',b'DET_META']:
            self.assertEqual(d.observe(frame()+tail,'DET',finalized=True)['status'],'invalid')
    def test_extra_end_rejected(self):
        self.assertEqual(d.observe(frame()+b'DET_END\n','DET')['status'],'invalid')
    def test_out_of_order_rejected(self):
        self.assertEqual(d.observe(b'DET_DATA YQ==\n'+frame(),'DET')['status'],'invalid')
    def test_bad_sha_rejected(self):
        self.assertEqual(d.observe(frame(digest='0'*64),'DET')['status'],'invalid')
    def test_bad_base64_rejected(self):
        b=frame().replace(b'DET_DATA ',b'DET_DATA !')
        self.assertEqual(d.observe(b,'DET')['status'],'invalid')
    def test_trailing_zlib_rejected(self):
        self.assertEqual(d.observe(frame(packed=zlib.compress(b'{"ok":true}')+b'x'),'DET')['status'],'invalid')
    def test_truncated_zlib_rejected(self):
        self.assertEqual(d.observe(frame(packed=zlib.compress(b'{"ok":true}')[:-1]),'DET')['status'],'invalid')
    def test_expansion_beyond_declared_rejected(self):
        self.assertEqual(d.observe(frame(packed=zlib.compress(b'x'*1000000)),'DET')['status'],'invalid')
    def test_limits_rejected(self):
        self.assertEqual(d.observe(b'x'*(d.MAX_STREAM+1),'DET')['status'],'invalid')
        b=frame().replace(b'DET_META 11 ',b'DET_META 9999999 ')
        self.assertEqual(d.observe(b,'DET')['status'],'invalid')
    def test_non_object_rejected(self):
        self.assertEqual(d.observe(frame(raw=b'[]'),'DET')['status'],'invalid')
    def test_prompt_in_echo_not_terminal(self):
        b=frame().rsplit(b'#',1)[0]+b"echo '# '\n"
        self.assertEqual(d.observe(b,'DET')['status'],'waiting')
    def test_public_bytes_are_lossless_and_bounded(self):
        for data in [b'hello\n',b'\xca\x00\xff\r\n',bytes(range(256))*128]:
            record=d.public_bytes(data)
            self.assertEqual(base64.b64decode(record['b64'],validate=True),data)
            self.assertEqual(record['sha256'],hashlib.sha256(data).hexdigest())
        with self.assertRaises(ValueError):d.public_bytes(b'x'*32769)
    def test_preflight_does_not_execute(self):
        r=d.inline_preflight(b'raise RuntimeError("must not execute")\n','test.py')
        self.assertFalse(r['executed']);self.assertTrue(r['round_trip'])
    def test_bad_python_rejected(self):
        with self.assertRaises(SyntaxError):d.inline_preflight(b'if :\n','bad.py')
    def test_invalid_name_rejected(self):
        with self.assertRaises(ValueError):d.inline_preflight(b'pass\n','bad;name.py')
    def test_generated_source_preflight(self):
        r=d.inline_preflight(b'import json\nprint(json.dumps({"synthetic": True}))\n','sample.py')
        self.assertLessEqual(r['command_bytes'],3500)
    def test_long_command_rejected(self):
        source=b'x='+repr(''.join(hashlib.sha256(str(i).encode()).hexdigest() for i in range(150))).encode()+b'\n'
        with self.assertRaises(ValueError):d.inline_preflight(source,'long.py')

    def run_cli(self, args):
        output=io.StringIO()
        with contextlib.redirect_stdout(output):
            code=d.main(args)
        return code,json.loads(output.getvalue())

    def test_cli_inspects_sealed_log(self):
        with tempfile.TemporaryDirectory() as root:
            path=Path(root)/'synthetic.log';path.write_bytes(frame())
            code,r=self.run_cli(['inspect',str(path),'--prefix','DET','--finalized'])
            self.assertEqual(code,0);self.assertEqual(r['input_sha256'],hashlib.sha256(frame()).hexdigest())
            self.assertFalse(r['board_accepted']);self.assertEqual(r['payload'],{'ok':True})

    def test_cli_pending_rejected_and_invalid_exit_codes(self):
        with tempfile.TemporaryDirectory() as root:
            path=Path(root)/'synthetic.log'
            for data,expected in [(b'# \n',3),(b'SyntaxError: invalid\n# \n',2),(frame(digest='0'*64),4)]:
                path.write_bytes(data)
                code,r=self.run_cli(['inspect',str(path),'--prefix','DET'])
                self.assertEqual(code,expected);self.assertFalse(r['board_accepted'])

    def test_cli_preflight_never_executes_source(self):
        with tempfile.TemporaryDirectory() as root:
            path=Path(root)/'sample.py';sentinel=Path(root)/'should-not-exist'
            path.write_text('open('+repr(str(sentinel))+',"w").write("executed")\n')
            code,r=self.run_cli(['preflight',str(path)])
            self.assertEqual(code,0);self.assertFalse(r['executed']);self.assertFalse(sentinel.exists())

    def test_cli_invalid_python_does_not_echo_source(self):
        with tempfile.TemporaryDirectory() as root:
            path=Path(root)/'bad.py';path.write_text('PRIVATE_VALUE = (\n')
            code,r=self.run_cli(['preflight',str(path)])
            self.assertEqual(code,4);self.assertEqual(r['reason'],'SyntaxError')
            self.assertNotIn('PRIVATE_VALUE',json.dumps(r))

    def test_reader_rejects_oversize_missing_symlink_and_fifo(self):
        with tempfile.TemporaryDirectory() as root:
            path=Path(root)/'input';path.write_bytes(b'1234')
            self.assertEqual(d.read_regular_file(path,4),b'1234')
            with self.assertRaises(ValueError):d.read_regular_file(path,3)
            link=Path(root)/'link';link.symlink_to(path)
            fifo=Path(root)/'fifo';os.mkfifo(str(fifo))
            for rejected in [link,fifo,Path(root)/'missing']:
                code,r=self.run_cli(['inspect',str(rejected),'--prefix','DET'])
                self.assertEqual(code,4)

if __name__=='__main__':unittest.main(verbosity=2)
