import unittest
from PyBCY import *
class BCYTest(unittest.TestCase):
    def test_user_worklist(self):
        self.assertEqual(6,len(BCYCore().userWorkList(293381,"coser")))
    def test_cosplay_detail(self):
        self.assertEqual(6,len(BCYCore().cosDetail(73943,614138)["multi"]))
