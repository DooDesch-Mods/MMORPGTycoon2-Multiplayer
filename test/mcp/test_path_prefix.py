"""Regression for the actual extended-drive response publication failure."""
import sys
import unittest
from pathlib import Path
from unittest.mock import Mock
sys.path.insert(0,str(Path(__file__).resolve().parents[2]/'tools/mcp'))
from runtime import Runtime,RuntimeErrorDetail

class PathPrefixTests(unittest.TestCase):
    def setUp(self):
        self.root=Path(__file__).resolve().parent
        self.runtime=object.__new__(Runtime)

    def resolved(self,value):
        source=Mock();source.resolve.return_value=Path(value)
        return source

    def test_extended_response_inside_root(self):
        expected=self.root/'response.json'
        self.assertEqual(self.runtime._within(self.resolved('\\\\?\\'+str(expected)),self.root),expected)

    def test_extended_root_and_normal_response(self):
        expected=self.root/'response.json'
        self.assertEqual(self.runtime._within(self.resolved(str(expected)),self.resolved('\\\\?\\'+str(self.root))),expected)

    def test_resolved_junction_outside_stays_rejected(self):
        outside=self.root.parent/'outside'/'response.json'
        with self.assertRaises(RuntimeErrorDetail) as caught:
            self.runtime._within(self.resolved('\\\\?\\'+str(outside)),self.root)
        self.assertEqual(caught.exception.code,'path_escape')

    def test_same_prefix_sibling_is_not_inside(self):
        sibling=Path(str(self.root)+'-other')/'response.json'
        with self.assertRaises(RuntimeErrorDetail):
            self.runtime._within(self.resolved('\\\\?\\'+str(sibling)),self.root)

    def test_device_namespace_stays_rejected(self):
        with self.assertRaises(RuntimeErrorDetail):
            self.runtime._within(self.resolved('\\\\?\\GLOBALROOT\\Device\\HarddiskVolume1\\response.json'),self.root)

if __name__=='__main__':unittest.main()
