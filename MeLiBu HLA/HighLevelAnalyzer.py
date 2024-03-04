# High Level Analyzer
# For more information and documentation, please go to https://support.saleae.com/extensions/high-level-analyzer-extensions

from saleae.analyzers import HighLevelAnalyzer, AnalyzerFrame, StringSetting, NumberSetting, ChoicesSetting

import sys
 
#MY_ADDITIONAL_PACKAGES_PATH = "C:\\Program Files (x86)\\Melexis\\Python\\Lib\\site-packages"
MY_ADDITIONAL_PACKAGES_PATH = "C:\\Projects\\MeLiBu_in_C++\\HLA_MeLiBu\\.venv\\Lib\\site-packages"
 
if MY_ADDITIONAL_PACKAGES_PATH not in sys.path:
 
    sys.path.append(MY_ADDITIONAL_PACKAGES_PATH)

import logging
import shutil
from pathlib import Path
from time import sleep

from pymbdfparser import ParserApplication
from pymbdfparser.model import CommandFrameMelibu1, LedFrameMelibu1, FrameMelibu2
from pymbdfparser.model.script_frame import ScriptFrameBase

from ast import literal_eval
#sys.path.append("C:\\Program Files (x86)\\Melexis\\Python\\Lib\\site-packages")
# High level analyzers must subclass the HighLevelAnalyzer class.
class Hla(HighLevelAnalyzer):
    
    # List of settings that a user can set for this High Level Analyzer.
    mbdf_filepath = StringSetting()
    #my_number_setting = NumberSetting(min_value=0, max_value=100)
    #my_choices_setting = ChoicesSetting(choices=('A', 'B'))

    # An optional list of types this analyzer produces, providing a way to customize the way frames are displayed in Logic 2.
    result_types = {
        'header1': {
            'format': 'slave_addr: {{data.slave}}, R/T: {{data.r}}, F: {{data.f}}, instr_ext: {{data.instr}}'
        },
        'header2': {
            'format': 'slave_addr: {{data.int}}, R/T: {{data.r}}, F:{{data.f}}, I:{{data.i}}, PL_length: {{data.len}}'
        }
    }

    def __init__(self):
        '''
        Initialize HLA.

        Settings can be accessed using the same name used above.
        '''
        
        path = Path(self.mbdf_filepath)
        app = ParserApplication(path)
        app.run()
        self.model = app.model
        self.protocol_version = self.model.bus_protocol_version
        self.ack = self.model
        
        print("Settings:", self.mbdf_filepath)

    def decode_id1(self, frame:AnalyzerFrame):
        n = literal_eval((frame.data['data']))
        self.id1 = n
        self.id1_start = frame.start_time
        
    def decode_id2(self, frame:AnalyzerFrame):
        n = literal_eval((frame.data['data']))
        if self.protocol_version < 2.0:
            func = self.id1 & 1
            rt = (self.id1 & 2) >> 1
            slave_adr = (self.id1 & 252) >> 2
            inst = (n & 252) >> 2
            return AnalyzerFrame('header1', self.id1_start, frame.end_time, {'slave': slave_adr, 'r': rt, 'f': func, 'instr': inst})
        else:
            slave_adr = n
            return AnalyzerFrame('id1', frame.start_time, frame.end_time, {'slave address': slave_adr})

    def decode(self, frame: AnalyzerFrame):
        '''
        Process a frame from the input analyzer, and optionally return a single `AnalyzerFrame` or a list of `AnalyzerFrame`s.

        The type and data values in `frame` will depend on the input analyzer.
        '''
        
        if frame.type == 'header_break':
            return AnalyzerFrame('header break', frame.start_time, frame.end_time)
        elif frame.type == 'header_ID1':
            self.decode_id1(frame)
        elif frame.type == 'header_ID2':
            return self.decode_id2(frame)
        else:
            # Return the data frame itself
            return AnalyzerFrame('', frame.start_time, frame.end_time, {'input_type': frame.type, 'value': 1})
    
    