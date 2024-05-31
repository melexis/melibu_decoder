# High Level Analyzer
# For more information and documentation, please go to https://support.saleae.com/extensions/high-level-analyzer-extensions

from saleae.analyzers import HighLevelAnalyzer, AnalyzerFrame, StringSetting, NumberSetting, ChoicesSetting
from saleae.data import GraphTimeDelta, GraphTime

import sys
import os

# MY_ADDITIONAL_PACKAGES_PATH = os.path.abspath(os.path.dirname(__file__)) + "\melexis\python\lib\\site-packages" 
# if MY_ADDITIONAL_PACKAGES_PATH not in sys.path:
#     sys.path.append(MY_ADDITIONAL_PACKAGES_PATH)

import logging
import shutil
from pathlib import Path
from time import sleep

from pymbdfparser import ParserApplication
from pymbdfparser.model import CommandFrameMelibu1, LedFrameMelibu1, FrameMelibu2
from pymbdfparser.model.script_frame import ScriptFrameBase
from pymbdfparser.model.signal_encoding_type import PhysicalEncoding

from ast import literal_eval
from datetime import datetime

# High level analyzers must subclass the HighLevelAnalyzer class.
class Hla(HighLevelAnalyzer):
    
    # List of settings that a user can set for this High Level Analyzer.
    mbdf_filepath = StringSetting()

    # An optional list of types this analyzer produces, providing a way to customize the way frames are displayed in Logic 2.
    result_types = {
        'header': {
            'format': 'frame: {{data.frame}}, info: {{data.frame_info}}'
        },
        'data': {
            'format': 'signal: {{data.signal}}, raw value: {{data.raw_value}}, actual value: {{data.actual_value}}'
        },
        'multiple_data': {
            'format':
                'sig alternative 1:{{data.sig_alternative_1}},raw alternative 1: {{data.raw_alternative_1}}, actual alternative 1: {{data.actual_alternative_1}},\
                    sig alternative 2: {{data.sig_alternative_2}}, raw alternative 2: {{data.raw_alternative_2}}, actual alternative 2: {{data.actual_alternative_2}},\
                        sig alternative 3: {{data.sig_alternative_3}}, raw alternative 3: {{data.raw_alternative_3}}, actual alternative 3: {{data.actual_alternative_3}},\
                            sig alternative 4: {{data.sig_alternative_4}}, raw alternative 4: {{data.raw_alternative_4}}, actual alternative 4: {{data.actual_alternative_4}},\
                                sig alternative 5: {{data.sig_alternative_5}}, raw alternative 5: {{data.raw_alternative_5}}, actual alternative 5: {{data.actual_alternative_5}}'
        },
        'raw data': {
            'format': 'raw value: {{data.raw_value}}'
        },
        'error': {
            'format': 'ERROR: {{data.error_type}}'
        },
        'crc': {
            'format': 'crc value: {{data.raw_value}}'
        }
    }

    def __init__(self):
        '''
        Initialize HLA.
        Settings can be accessed using the same name used above.
        '''
        # erase " from begining and end if there are in string
        path = self.mbdf_filepath
        if (path[0] == '\"') | (path[0] == '\''):
            path = path[1:]
        if (path[-1] == '\"') | (path[-1] == '\''):
            path = path[:-1]
        
        # run python mbdf parser and save protocol version
        path = Path(path)
        app = ParserApplication(path)
        app.run()
        self.model = app.model
        self.protocol_version = self.model.bus_protocol_version
        
        self.info_str = ''
        self.id1 = 0
        self.id2 = 0
        self.inst1 = 0
        self.inst2 = 0
        self.inst_word = 0
        self.data_length = 0
        self.data_array = []
        
        self.crc = 0
        self.crc_start = GraphTime(datetime.now())
        self.id1_start = GraphTime(datetime.now())
        
        self.current_frame = None
        self.matched_frames = []
        
        self.found_slave = False
        self.matched_frame = False
        self.unique_frame = True
                
    def get_info_from_ids_melibu1(self):      
        # extract values from id fields
        slave_adr = (self.id1 & 0xfc) >> 2
        rt = (self.id1 & 0x02) >> 1
        func = self.id1 & 0x01
        inst = 0
        frame_size = 0
        
        if func == 0:
            inst = (self.id2 & 0xe0) >> 5
            frame_size = (self.id2 & 0x1c) >> 2
            self.info_str = 'SLAVE = {slave}, R/T = {RT}, F = {f}, INST = {i}, SIZE = {s}'.format(slave = '0x' + hex(slave_adr)[2:].upper(), RT = rt, f = func, i = inst, s = frame_size)
        else:
            frame_size = (self.id2 & 0xfc) >> 2
            self.info_str = 'SLAVE = {slave}, R/T = {RT}, F = {f}, SIZE = {s}'.format(slave = '0x' + hex(slave_adr)[2:].upper(), RT = rt, f = func, s = frame_size)
            
        return slave_adr, rt, func, inst, frame_size
        
    def match_frame_melibu1(self, frame: AnalyzerFrame, rt_bit, f_bit, inst, frame_size):
        # iterate through all frames in mbdf file and match with frame with same values from id's
        self.matched_frame = False
        self.unique_frame = True
        frame_names = []
        self.matched_frames = []
        for frame_name in self.model.frames.keys():
            curr_frame = self.model.frames.get(frame_name)
            
            f_type = 0
            if curr_frame.function_type != "Command":
                f_type = 1
            
            if(f_type == 0):
                if (curr_frame.r_t_bit == rt_bit) & (f_type == f_bit) & (curr_frame.sub_address == frame_size) & (curr_frame.ext_instruction == inst):
                    self.current_frame = curr_frame # set current frame for data decoding
                    self.matched_frame = True
                    frame_names.append(frame_name)
                    self.matched_frames.append(curr_frame)
            else:
                if (curr_frame.r_t_bit == rt_bit) & (f_type == f_bit) & (curr_frame.sub_address == frame_size):
                    self.current_frame = curr_frame # set current frame for data decoding
                    self.matched_frame = True
                    frame_names.append(frame_name)
                    self.matched_frames.append(curr_frame)
        if self.matched_frame == True:
            if len(frame_names) > 1:
                self.unique_frame = False
            self.data_length = (self.matched_frames[0]).size
            return AnalyzerFrame('header', self.id1_start, frame.end_time, {'frame': ', '.join(frame_names), 'frame_info': self.info_str})
            
        # if we are here that means no matching fram has ben found and we will return unknown frame
        return AnalyzerFrame('header', self.id1_start, frame.end_time, {'frame': "Unknown", 'frame_info': self.info_str})
    
    
    def get_info_from_ids_melibu2(self):
        
        # extract values from id fields
        slave_adr = self.id1
        rt = (self.id2 & 0x01)
        func = (self.id2 & 0x02) >> 1
        inst = (self.id2 & 0x04) >> 2
        pl_len = (self.id2 & 0x38) >> 3
        
        if inst == 0:
            self.info_str = 'SLAVE = {slave}, R/T = {RT}, F = {f}, I = {i}, PL LENGTH = {pl}'.format(slave = hex(slave_adr), RT = rt, f = func, i = inst, pl = pl_len)
        else:
            self.info_str = 'SLAVE = {slave}, R/T = {RT}, F = {f}, I = {i}, PL LENGTH = {pl}, INSTRUCTION = {inst}'.format(slave = '0x' + hex(slave_adr)[2:].upper(), RT = rt, f = func, i = inst, pl = pl_len, inst = self.inst_word)
        return slave_adr, rt, func, inst, pl_len
    
    def match_frame_melibu2(self, frame: AnalyzerFrame, rt_bit, f_bit, i_bit, frame_size, inst_word):
        # iterate through all frames in mbdf file and match with frame with same values from id's
        self.matched_frame = False
        self.unique_frame = True
        frame_names = []
        self.matched_frames = []
        for frame_name in self.model.frames.keys():
            curr_frame = self.model.frames.get(frame_name)
            
            f_type = 0
            if curr_frame.function_type != "Command":
                f_type = 1
            
            if(curr_frame.i_bit == 0):
                if (curr_frame.r_t_bit == rt_bit) & (f_type == f_bit) & (curr_frame.pl_length == frame_size) & (curr_frame.i_bit == i_bit):
                    self.current_frame = curr_frame # set current frame for data decoding
                    self.matched_frame = True
                    frame_names.append(frame_name)
                    self.matched_frames.append(curr_frame)
            else:
                if (curr_frame.r_t_bit == rt_bit) & (f_type == f_bit) & (curr_frame.pl_length == frame_size) & (curr_frame.i_bit == i_bit) & (curr_frame.instruction_word == inst_word):
                    self.current_frame = curr_frame # set current frame for data decoding
                    self.matched_frame = True
                    frame_names.append(frame_name)
                    self.matched_frames.append(curr_frame)
        if self.matched_frame == True:
            if len(frame_names) > 1:
                self.unique_frame = False
            self.data_length = (self.matched_frames[0]).size
            return AnalyzerFrame('header', self.id1_start, frame.end_time, {'frame': ', '.join(frame_names), 'frame_info': self.info_str})
            
        # if we are here that means no matching fram has ben found and we will return unknown frame
        return AnalyzerFrame('header', self.id1_start, frame.end_time, {'frame': "Unknown", 'frame_info': self.info_str})
            
    def check_slave(self, slave_address):
        self.found_slave = False
        for node in self.model.nodes:
            if (self.model.nodes[node].__class__.__name__ == 'SlaveNode'):
                if self.model.nodes[node].configured_nad == slave_address:
                    self.found_slave = True

    def decode_id1(self, frame:AnalyzerFrame):
        # only save id value and start time; this frame will be connected with id2 frame
        self.id1 = literal_eval((frame.data['data'])) # 'data' is the name of column from low level analyzer; .data is dictionary with added column names
        self.id1_start = frame.start_time
        
    def decode_id2(self, frame:AnalyzerFrame):
        
        self.id2 = literal_eval((frame.data['data']))
        self.data_array = [] # empty data array
        self.data_length = 0 # reset data length
        self.info_str = ''
        self.inst_word = 0
        
        # current frame is frame found in mbdf file based on id1 and id2 value; current frame is used later when reading data in frame
        # initialize current frame with first frame; this is not important because current frame will have true value 
        self.current_frame = self.model.frames.get(list(self.model.frames.keys())[0])  
        if self.protocol_version < 2.0: # for MeLiBu 1 there is no instruction word; just connect id1 and id2
            slave_adr, rt, func, inst, frame_size = self.get_info_from_ids_melibu1()
            self.check_slave(slave_adr)
            return self.match_frame_melibu1(frame, rt, func, inst, frame_size)       
        else: # for MeLiBu 2connect id1 and id2 if there is no inst word, otherwise return nothing
            slave_adr, rt, func, inst, frame_size = self.get_info_from_ids_melibu2()
            self.check_slave(slave_adr)
            if inst == 0:
                return self.match_frame_melibu2(frame, rt, func, inst, frame_size, self.inst_word)
            else:
                return []
        
    def decode_inst1(self, frame:AnalyzerFrame):
        self.inst1 = literal_eval((frame.data['data']))
    
    def decode_inst2(self, frame:AnalyzerFrame):
        self.inst2 = literal_eval((frame.data['data']))
        self.inst_word = (self.inst2 << 8) | self.inst1 # create instruction word from inst1 & inst2 byte fields
        slave_adr, rt, func, inst, frame_size = self.get_info_from_ids_melibu2()
        self.check_slave(slave_adr)
        return self.match_frame_melibu2(frame, rt, func, inst, frame_size, self.inst_word)
       
    def get_signal_chunks(self, frame):
        if self.protocol_version < 2:
            return frame.signal_chunks_big_endian
        else:
            return frame.signal_chunks_little_endian
        
    def decode_data(self, frame:AnalyzerFrame):
        self.data_array.append(frame)  # add data byte to array
        
        # only if it is last data byte decode them and return list of frames
        if self.data_length == 1:
            frames = [] # array of frames to be returned; this is for tabular view and bar in UI
            
            delta_time = self.data_array[-1].end_time - self.data_array[0].start_time # all data bytes duration
            delta_time_float = delta_time.__float__()                                 # convert GraphTimeDelta to float [s]
            delta_per_bit = delta_time_float / (10 * len(self.data_array))            # 10 = 8 data bits + start bit + stop bit
            
            frame_ind = 0
            signals_info = []
            for frame in self.matched_frames:
                
                signals = self.get_signal_chunks(frame)
                signals_sorted = sorted(signals, key = lambda x: x.significance, reverse = False) 
                signals_dict = dict()
                
                for sig in signals_sorted:
                    # get name, offset and size of signal chunks
                    name = sig.signal.name
                    offset = sig.real_offset
                    size = sig.size
                    
                    value = 0
                    mask = 0
                    
                    # connect all bytes values where chunk is located
                    for i in range(offset//8, (offset + size - 1)//8 + 1):
                        
                        curr_val = literal_eval((self.data_array[i].data['data']))
                        
                        if self.protocol_version < 2:
                            value = (value << 8) | curr_val # MSB byte first
                        else:
                            value = (curr_val << 8 * (i - offset//8)) | value # LSB byte first
                            
                        mask = (mask << 8) | 255 # mask will be all ones with length 8*(number of bytes containing value)
                         
                    # extract only part of number -> from local offset with local size
                    number_size = 8 * ((offset + size - 1)//8 - offset//8 + 1)
                    mask = (mask >> (offset % 8)) & (mask << (number_size - size - offset % 8))
                    value = value & mask # extract important bits
                    value = value >> (number_size - size - offset % 8) # shift value to start from lsb bit
                    
                    # connect chunk value with previous
                    if name in signals_dict:
                        signals_dict[name] = (signals_dict[name] << size) | value
                    else:
                        signals_dict[name] = value
                        
                signals_sorted = sorted(signals, key=lambda x: x.real_offset, reverse=False)
                added_signals = []
                sig_ind = 0
                for sig in signals_sorted:
                    if sig.signal.name not in added_signals:
                        added_signals.append(sig.signal.name)
                        physical_value = 'None'
                        if sig.signal.encoding_type != None:
                            for encoding in sig.signal.encoding_type.encodings:
                                if encoding.__class__.__name__ == 'PhysicalEncoding':
                                    if encoding.min_value <= signals_dict[sig.signal.name] <= encoding.max_value:
                                        physical_value = sig.signal.decode(signals_dict[sig.signal.name])
                                    break
                                else:
                                    physical_value = sig.signal.decode(signals_dict[sig.signal.name])
                        else:
                            physical_value = sig.signal.decode(signals_dict[sig.signal.name])
                        
                        
                        if (sig_ind + 1) > len(signals_info):
                            sig_info = [""] * (5*3) # 5 is size of max multiple matched frames; 3 is for sig name, raw val and actual val
                            sig_info[3 * frame_ind] = sig.signal.name
                            sig_info[3 * frame_ind + 1] = hex(signals_dict[sig.signal.name])
                            sig_info[3 * frame_ind + 2] = physical_value
                            signals_info.append(sig_info)
                        else:
                            signals_info[sig_ind][3 * frame_ind] = sig.signal.name
                            signals_info[sig_ind][3 * frame_ind + 1] = hex(signals_dict[sig.signal.name])
                            signals_info[sig_ind][3 * frame_ind + 2] = physical_value

                        sig_ind = sig_ind + 1

                frame_ind = frame_ind + 1
                if frame_ind == 5: # max matched frames is 5
                    break
                 
            frames = []
            sig_len = len(signals_info)
            delta_per_sig = delta_time_float / sig_len - 2 * delta_per_bit / 100
            for i in range(sig_len):
                frames.append(AnalyzerFrame('multiple_data',
                                            self.data_array[0].start_time + GraphTimeDelta(second = (2 * i + 1) * delta_per_bit / 100 + i * delta_per_sig),
                                            self.data_array[0].start_time + GraphTimeDelta(second = (2 * i + 2) * delta_per_bit / 100 + (i + 1) * delta_per_sig),
                                            {'sig_alternative_1': signals_info[i][0],'raw_alternative_1': signals_info[i][1], 'actual_alternative_1': signals_info[i][2],
                                             'sig_alternative_2': signals_info[i][3],'raw_alternative_2': signals_info[i][4], 'actual_alternative_2': signals_info[i][5],
                                             'sig_alternative_3': signals_info[i][6],'raw_alternative_3': signals_info[i][7], 'actual_alternative_3': signals_info[i][8],
                                             'sig_alternative_4': signals_info[i][9],'raw_alternative_4': signals_info[i][10], 'actual_alternative_4': signals_info[i][11],
                                             'sig_alternative_5': signals_info[i][12],'raw_alternative_5': signals_info[i][13], 'actual_alternative_5': signals_info[i][14]}))
            return frames
                
        self.data_length = self.data_length - 1 # update data length
        return []
    
    def decode_unique_data(self, frame:AnalyzerFrame):
        self.data_array.append(frame)  # add data byte to array
        
        # only if it is last data byte decode them and return list of frames
        if self.data_length == 1:
            frames = [] # array of frames to be returned; this is for tabular view and bar in UI
            
            delta_time = self.data_array[-1].end_time - self.data_array[0].start_time # all data bytes duration
            delta_time_float = delta_time.__float__()                                 # convert GraphTimeDelta to float [s]
            delta_per_bit = delta_time_float / (10 * len(self.data_array))            # 10 = 8 data bits + start bit + stop bit
            
            
            signals = self.get_signal_chunks(self.matched_frames[0])
            signals_sorted = sorted(signals, key = lambda x: x.significance, reverse = False) 
            signals_dict = dict()
            
            for sig in signals_sorted:
                # get name, offset and size of signal chunks
                name = sig.signal.name
                offset = sig.real_offset
                size = sig.size
                
                value = 0
                mask = 0
                
                # connect all bytes values where chunk is located
                for i in range(offset//8, (offset + size - 1)//8 + 1):
                    
                    curr_val = literal_eval((self.data_array[i].data['data']))
                    
                    if self.protocol_version < 2:
                        value = (value << 8) | curr_val # MSB byte first
                    else:
                        value = (curr_val << 8 * (i - offset//8)) | value # LSB byte first
                        
                    mask = (mask << 8) | 255 # mask will be all ones with length 8*(number of bytes containing value)
                     
                # extract only part of number -> from local offset with local size
                number_size = 8 * ((offset + size - 1)//8 - offset//8 + 1)
                mask = (mask >> (offset % 8)) & (mask << (number_size - size - offset % 8))
                value = value & mask # extract important bits
                value = value >> (number_size - size - offset % 8) # shift value to start from lsb bit
                
                # connect chunk value with previous
                # maybe not working properly for melibu 2, but for melibu 2 there won't be more chunks for one signal
                # good for both melibu 1 & 2 because chunks are sorted by significance
                if name in signals_dict:
                    signals_dict[name] = (signals_dict[name] << size) | value
                else:
                    signals_dict[name] = value
                    
            signals_sorted = sorted(signals, key=lambda x: x.real_offset, reverse=False)
            added_signals = []
            for sig in signals_sorted:
                if sig.signal.name not in added_signals:
                    added_signals.append(sig.signal.name)
                    physical_value = 'None'
                    if sig.signal.encoding_type != None:
                        for encoding in sig.signal.encoding_type.encodings:
                            if encoding.__class__.__name__ == 'PhysicalEncoding':
                                if encoding.min_value <= signals_dict[sig.signal.name] <= encoding.max_value:
                                    physical_value = sig.signal.decode(signals_dict[sig.signal.name])
                                break
                            else:
                                physical_value = sig.signal.decode(signals_dict[sig.signal.name])
                    else:
                        physical_value = sig.signal.decode(signals_dict[sig.signal.name])
            
                    # calculate delta time between start time of first data frame and start and end of extracted raw values frames
                    
                    # offset = real_offset + start bit and stop bit of data before + start bit of current data
                    delta_start = GraphTimeDelta(second = (sig.real_offset + (sig.real_offset // 8) * 2 + 1) * delta_per_bit)
                    # offset = real_offset + start bit and stop bit of data before + start bit of current data + signal size + start and stop bit of transitions between data bytes
                    delta_end = GraphTimeDelta(second = (sig.real_offset + (sig.real_offset // 8) * 2 + 1 + sig.size + ((sig.real_offset % 8 + sig.size - 1) // 8) * 2) * delta_per_bit)
                        
                    frames.append(AnalyzerFrame('data', self.data_array[0].start_time + delta_start, self.data_array[0].start_time + delta_end, {'signal': sig.signal.name,'raw_value': hex(signals_dict[sig.signal.name]), 'actual_value': physical_value}))
                    
            return frames
                
        self.data_length = self.data_length - 1 # update data length
        return []
    
    def decode_crc1(self, frame:AnalyzerFrame):
        self.crc_start = frame.start_time
        self.crc = literal_eval((frame.data['data']))
        return []
        
    def decode_crc2(self, frame:AnalyzerFrame):
        if self.protocol_version < 2.0:
            self.crc = (self.crc << 8) | literal_eval((frame.data['data']))
        else:
            self.crc = (literal_eval((frame.data['data'])) << 8) | self.crc
        return AnalyzerFrame('crc', self.crc_start, frame.end_time, {'raw_value': '0x' + hex(self.crc)[2:].upper()})
        

    def decode(self, frame: AnalyzerFrame):
        '''
        Process a frame from the input analyzer, and optionally return a single `AnalyzerFrame` or a list of `AnalyzerFrame`s.

        The type and data values in `frame` will depend on the input analyzer.
        '''
        if frame.type == 'breakfield':
            return AnalyzerFrame('breakfield', frame.start_time, frame.end_time)
        elif frame.type == 'header_ID1':
            self.decode_id1(frame)
        elif frame.type == 'header_ID2':
            return self.decode_id2(frame)
        elif frame.type == 'instruction_byte1':
            return self.decode_inst1(frame)
        elif frame.type == 'instruction_byte2':
            return self.decode_inst2(frame)
        elif (frame.type == 'data') & (self.matched_frame == True) & (self.unique_frame == True):
            return self.decode_unique_data(frame)
        elif (frame.type == 'data') & (self.matched_frame == True) & (self.unique_frame == False):
            return self.decode_data(frame)
        elif frame.type == 'data':
            return AnalyzerFrame('raw data', frame.start_time, frame.end_time, {'raw_value': frame.data['data']})
        elif frame.type == 'crc1':
            return self.decode_crc1(frame)
        elif frame.type == 'crc2':
            return self.decode_crc2(frame)
        elif frame.type == 'ack':
            # Return the data frame itself
            return AnalyzerFrame(frame.type, frame.start_time, frame.end_time)
        else:
            return AnalyzerFrame('error', frame.start_time, frame.end_time, {'error_type': frame.type})
    
    