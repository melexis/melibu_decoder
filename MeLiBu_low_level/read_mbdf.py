# -*- coding: utf-8 -*-
"""
Created on Wed Feb 28 10:14:10 2024

@author: lstamatovic
"""

import logging
import shutil
import sys
from pathlib import Path

import unittest

import sys
import os

# sys.path = []
# sys.path.append("C:\\Program Files\\Logic\\resources\\windows-x64\\pythonlibs\\python38.zip")
# sys.path.append("C:\\Program Files\\Logic\\resources\\windows-x64\\pythonlibs\\lib\\site-packages")
# sys.path.append("C:\\Program Files\\Logic\\resources\\windows-x64\\pythonlibs\\DLLs")
# MY_ADDITIONAL_PACKAGES_PATH = os.path.abspath(os.path.dirname(__file__)) + "\melexis\python\lib\\site-packages"  
MY_ADDITIONAL_PACKAGES_PATH = "C:\\Program Files\\Logic\\resources\\windows-x64\\pythonlibs\\lib\\site-packages"
if MY_ADDITIONAL_PACKAGES_PATH not in sys.path:
      sys.path.append(MY_ADDITIONAL_PACKAGES_PATH)

from pymbdfparser import ParserApplication
from pymbdfparser.model import CommandFrameMelibu1, LedFrameMelibu1, FrameMelibu2
from pymbdfparser.model.script_frame import ScriptFrameBase

filepath = sys.argv[1]
# filepath = "C:\\Projects\\melibu_decoder\\validation\\melibu_testing\\melibu_2\\Melexis_MeLiBu_MLX80142.MBDF"
app = ParserApplication(Path(filepath))
app.run()
model = app.model
print(model.bus_protocol_version)
print(model.bus_speed)

for node in model.slave_nodes:
    print(model.slave_nodes[node].configured_nad)
    print(model.slave_nodes[node].m2s_ack)
    