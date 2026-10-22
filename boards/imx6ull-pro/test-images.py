import importlib.util, struct, unittest
from pathlib import Path
spec=importlib.util.spec_from_file_location('check',Path(__file__).with_name('verify-images.py'));m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
class ImageBoundaryTests(unittest.TestCase):
    def loader(self):
        b=bytearray(4096);b[:4]=bytes.fromhex('d1002040')
        struct.pack_into('<7I',b,4,0x87800000,0,0x877ff42c,0x877ff420,0x877ff400,0,0)
        struct.pack_into('<3I',b,32,0x877ff000,len(b)+1024,0);b[44:48]=bytes.fromhex('d2000440');return b
    def disk(self):
        b=bytearray(512);b[510:]=b'\x55\xaa';start=16384
        for n,kind in enumerate([12,131,131]):
            at=446+16*n;b[at]=128 if n==0 else 0;b[at+4]=kind;struct.pack_into('<II',b,at+8,start,65536);start+=65536
        return b
    def test_valid_loader_bounds(self):self.assertEqual(m.ivt(self.loader())['dcd_bytes'],4)
    def test_loader_tampering_and_other_memory_map_refuse(self):
        for at,value in [(0,0),(32,0),(36,0),(44,0),(4,0)]:
            b=self.loader();b[at]=value
            if at in (32,4):struct.pack_into('<I',b,at,0x40200000)
            if at==36:struct.pack_into('<I',b,at,1)
            with self.assertRaises(ValueError):m.ivt(b)
    def test_three_partition_layout(self):self.assertEqual(len(m.partitions(self.disk(),110*1024*1024)),3)
    def test_overlap_wrong_type_truncation_and_missing_partition_refuse(self):
        cases=[]
        b=self.disk();struct.pack_into('<I',b,470,16384);cases.append((b,110*1024*1024))
        b=self.disk();b[466]=12;cases.append((b,110*1024*1024))
        b=self.disk();b[478:494]=bytes(16);cases.append((b,110*1024*1024))
        cases.append((self.disk(),40*1024*1024))
        for b,size in cases:
            with self.assertRaises(ValueError):m.partitions(b,size)
    def test_uuu_board_sequence(self):
        script=Path(__file__).with_name('flash-sdp.uuu').read_text();m.uuu(script)
        for bad in [script.replace('FB: done','FB: ucmd reset\nFB: done'),script.replace('mmc dev 1','mmc dev 0'),script+'FB: ucmd erase\n']:
            with self.assertRaises(ValueError):m.uuu(bad)
if __name__=='__main__':unittest.main(verbosity=2)
