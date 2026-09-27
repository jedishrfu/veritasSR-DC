import struct
import tempfile
import unittest
from pathlib import Path
import pysr_to_zsr as z

class FormatTests(unittest.TestCase):
    def test_headers(self):
        for size in [256,512,1024,2048]:
            data=z.zsr_bytes([z.constant(2)],size,.025)
            version,flags,block,tol,count=struct.unpack('<BBIfH',data[:12])
            self.assertEqual((version,flags,block,count),(32,0,size,1))
            self.assertAlmostEqual(tol,.025)
            self.assertEqual(data[12:],struct.pack('<HBf',1,0x40,2))
    def test_node_encoding(self):
        t=('b',z.ADD,('x',),z.constant(3))
        self.assertEqual(z.node_count(t),3)
        self.assertEqual(z.encode(t),b'\xc1\x00\x40'+struct.pack('<f',3))
        self.assertEqual(z.evaluate(t,4),7)
    def test_quantization(self):
        c=z.constant(.1)
        self.assertEqual(c[1],struct.unpack('<f',struct.pack('<f',.1))[0])
        with self.assertRaises(ValueError): z.constant(float('nan'))
    def test_invalid_header(self):
        with self.assertRaises(ValueError): z.zsr_bytes([],256,.1)
        with self.assertRaises(ValueError): z.zsr_bytes([z.constant(1)],250,.1)
        with self.assertRaises(ValueError): z.zsr_bytes([z.constant(1)],256,0)
    def test_no_clobber(self):
        with tempfile.TemporaryDirectory() as folder:
            p=Path(folder)/'a.zsr';z.atomic_write(p,b'original')
            with self.assertRaises(FileExistsError): z.atomic_write(p,b'replacement')
            self.assertEqual(p.read_bytes(),b'original')
    def test_sympy_translation(self):
        try: import sympy as sp
        except ImportError: self.skipTest('sympy is not installed')
        u=sp.Symbol('u'); t=z.from_sympy(2*u+3,sp,('x',))
        self.assertEqual(z.evaluate(t,4),11)
        t=z.from_sympy(sp.sin(u)+u**2,sp,('x',))
        self.assertAlmostEqual(z.evaluate(t,0),0)
        with self.assertRaises(ValueError): z.from_sympy(sp.log(u),sp,('x',))

if __name__=='__main__': unittest.main()
