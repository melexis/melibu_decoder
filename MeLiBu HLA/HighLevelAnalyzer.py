# High Level Analyzer
# For more information and documentation, please go to https://support.saleae.com/extensions/high-level-analyzer-extensions

from saleae.analyzers import HighLevelAnalyzer, AnalyzerFrame, StringSetting, NumberSetting, ChoicesSetting
from saleae.data import GraphTimeDelta

import sys
import os

MY_ADDITIONAL_PACKAGES_PATH = os.path.abspath(os.path.dirname(__file__)) + "\melexis\python\lib\\site-packages" 
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

# High level analyzers must subclass the HighLevelAnalyzer class.
class Hla(HighLevelAnalyzer):
    
    # List of settings that a user can set for this High Level Analyzer.
    mbdf_filepath = StringSetting()

    # An optional list of types this analyzer produces, providing a way to customize the way frames are displayed in Logic 2.
    result_types = {
        'header1': {
            # 'format': 'frame: {{data.frame}}, slave_addr: {{data.slave}}, R/T: {{data.r}}, F: {{data.f}}, instr_ext: {{data.instr}}'
            'format': 'frame: {{data.frame}}, info: {{data.frame_info}}'
        },
        'header2': {
            'format': 'slave_addr: {{data.int}}, R/T: {{data.r}}, F:{{data.f}}, I:{{data.i}}, PL_length: {{data.len}}'
        },
        'data': {
            'format': 'signal: {{data.signal_name}}, value: {{data.signal_value}}, physical_val: {{data.physical_value}}'
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

    def decode_id1(self, frame:AnalyzerFrame):
        # only save id value and start time; this frame will be joined with id2 frame
        self.id1 = literal_eval((frame.data['data'])) # 'data' is the name of column from low level analyzer
        self.id1_start = frame.start_time
        
    def decode_id2(self, frame:AnalyzerFrame):
        
        self.id2 = literal_eval((frame.data['data']))
        self.data_array = [] # empty data array
        self.data_length = 0 # reset data length
        
        # initialize current frame with first frame; this is not important because current frame will have true value 
        # current frame is frame found based on id1 and id2 value; current frame is used later when reading data in frame
        self.current_frame = self.model.frames.get(list(self.model.frames.keys())[0])  
        if self.protocol_version < 2.0:
            # extract values found in header id fields
            func = self.id1 & 1
            rt = (self.id1 & 2) >> 1
            slave_adr = (self.id1 & 252) >> 2
            inst = 0
            if func == 0:
                frame_size = (self.id2 & 28) >> 2
                inst = (self.id2 & 224) >> 5
                info_str = 'R/T = {RT}, F = {f}, INST = {i}, SIZE = {s}, SLAVE = {slave}'.format(RT = rt, f = func, i = inst, s = frame_size, slave = hex(slave_adr))
            else:
                frame_size = (self.id2 & 252) >> 2
                info_str = 'R/T = {RT}, F = {f}, SIZE = {s}, SLAVE = {slave}'.format(RT = rt, f = func, s = frame_size, slave = hex(slave_adr))
                
            self.data_length = bin(frame_size).count("1") * 6
            
            # iterate through all frames in mbdf file and match with frame with same values from id's
            for frame_name in self.model.frames.keys():
                curr_frame = self.model.frames.get(frame_name)
                
                f_type = 0
                if curr_frame.function_type == "LED":
                    f_type = 1
                
                if(f_type == 0):
                    if (curr_frame.r_t_bit == rt) & (f_type == func) & (curr_frame.sub_address == frame_size) & (curr_frame.ext_instruction == inst):
                        self.current_frame = curr_frame # set current frame for data decoding
                        return AnalyzerFrame('header1', self.id1_start, frame.end_time, {'frame': frame_name, 'frame_info': info_str})
                else:
                    if (curr_frame.r_t_bit == rt) & (f_type == func) & (curr_frame.sub_address == frame_size):
                        self.current_frame = curr_frame # set current frame for data decoding
                        return AnalyzerFrame('header1', self.id1_start, frame.end_time, {'frame': frame_name, 'frame_info': info_str})   
                
            # if we are here that means no matching fram has ben found and we will return unknown frame
            return AnalyzerFrame('header1', self.id1_start, frame.end_time, {'frame': "Unknown", 'frame_info': info_str})
        else:
            # slave_adr = self.id1
            # rt = id2 & 1
            # func = id2 & 2
            # return AnalyzerFrame('id1', frame.start_time, frame.end_time, {'slave address': slave_adr})
            return []
        
    def decode_inst1(self, frame:AnalyzerFrame):
        self.inst1 = literal_eval((frame.data['data']))
    
    def decode_inst2(self, frame:AnalyzerFrame):
        self.inst2 = literal_eval((frame.data['data']))
        
        slave_adr = self.id1
        rt = self.id2 & 1
        func = (self.id2 & 2) >> 1
        self.data_length = ((self.id2 & 0x38) >> 3) + 1
        
        if func == 0:
            self.data_length = self.data_length * 2
        else:
            self.data_length = self.data_length * 6
        
    def decode_data(self, frame:AnalyzerFrame):
        self.data_array.append(frame)  # add data byte to array
        
        # only if it is last data byte decode them and return list of frames
        if self.data_length == 1:
            frames = [] # array of frames to be returned; this is for tabular view and bar in UI
            
            delta_time = self.data_array[-1].end_time - self.data_array[0].start_time # all data bytes duration
            delta_time_float = delta_time.__float__()                                 # convert GraphTimeDelta to float [s]
            delta_per_bit = delta_time_float / (10 * len(self.data_array))            # 10 = 8 data bits + start bit + stop bit
            
            for sig in self.current_frame.signals:
                sig_value = 0 # initialize raw signal value
                
                name = sig.signal.name
                offset = sig.offset
                size = sig.signal.size
                
                temp_size = size
                temp_offset = offset
                # iterate through data array frames that have parts of signal value 
                for i in range(offset//8, (size + offset + 1)//8 ):
                    curr_frame = self.data_array[i] # index frame
                    array_value = literal_eval((curr_frame.data['data'])) # extract frame value
                    
                    # calculate offset and size of part of signal value in current frame
                    local_offset = temp_offset % 8
                    local_size = 0
                    if temp_size > (8 - local_offset):
                        local_size = 8 - local_offset
                    else:
                        local_size = temp_size
                    mask = (255 >> local_offset) & (255 << (8 - local_size - local_offset)) # mask for extracting part of signal value in frame 
                    chunk_value = array_value & mask
                    
                    # update temp size and temp offset
                    temp_size = temp_size - local_size
                    temp_offset = temp_offset + local_size
                    
                    # add chunk_value to whole signal value
                    sig_value = (sig_value << local_size) | chunk_value
                    
                # sig.bus_value(sig_value)
                physical_value = sig.signal.decode(sig_value)
                    
                # calculate delta time between start time of first data frame and startand end of extracted raw values frames
                delta_start = GraphTimeDelta(second = (offset + (offset // 8) * 2 + 1) * delta_per_bit)
                delta_end = GraphTimeDelta(second = (offset + (offset // 8) * 2 + size + (size // 8) * 2 - 1) * delta_per_bit)
                
                frames.append(AnalyzerFrame('data', self.data_array[0].start_time + delta_start, self.data_array[0].start_time + delta_end, {'signal_name': name,'signal_value': hex(sig_value), 'physical_value': physical_value}))
            
            return frames
                
        self.data_length = self.data_length - 1 # update data length
        return []
        

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
        elif frame.type == 'data':
            return self.decode_data(frame)
        else:
            # Return the data frame itself
            return AnalyzerFrame(frame.type, frame.start_time, frame.end_time)
    
    