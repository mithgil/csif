import pandas as pd
from tabulate import tabulate  
import numpy as np
import matplotlib.pyplot as plt
import matplotlib as mpl
mpl.rcParams['font.family'] = 'sans-serif'
mpl.rcParams['font.sans-serif'] = ['Arial']

from typing import Optional
import sif_parser

class Spectrum:
    def __init__(
        self,
        filename: str,
        data: np.ndarray,
        wavelengths: np.ndarray,
        timestamps: np.ndarray,
        info: Optional[dict] = None
    ) -> None:
        self.filename = filename
        self.data = data
        self.wavelengths = wavelengths
        self.timestamps = timestamps
        self.ramans = np.array([])
        self.info = info or {}
        
        if not isinstance(data, np.ndarray):
            raise TypeError(f"data must be numpy.ndarray, got {type(data).__name__}")
        if not isinstance(wavelengths, np.ndarray):
            raise TypeError(f"wavelengths must be numpy.ndarray, got {type(wavelengths).__name__}")
        if not isinstance(timestamps, np.ndarray):
            raise TypeError(f"timestamps must be numpy.ndarray, got {type(timestamps).__name__}")
        
    @staticmethod
    def read_sif(filename: str) -> 'Spectrum':
        data, info = sif_parser.np_open(filename)
        wavelengths = sif_parser.utils.extract_calibration(info)
        timestamps = np.array([v for (k,v) in info.items() if k.startswith('timestamp')])# / 1e6
    
        return Spectrum(filename, data, wavelengths, timestamps, info)
        
    def show_pretty_info(self, off_timestamp = True, off_tile = True):

        info = self.info
        
        filtered_items = [
            (k, v) for k, v in info.items()
            if not k.startswith('timestamp') and k != 'tile'
        ]
        
        print(tabulate(filtered_items, headers=["Key", "Value"]))

        if not off_timestamp:
            timestamps = [
                (k, v) for k, v in info.items()
                if k.startswith('timestamp')
            ]
            
            print(tabulate(timestamps, headers=["Key", "Value"]))
            
        elif not off_tile:
            tiles = [
                (k, v) for k, v in info.items()
                if k.startswith('tile')
            ]
            
            print(tabulate(tiles, headers=["Key", "Value"]))            
        
    def convert_xaxis_unit(self):

        self.ramans = ((1/self.info['RamanExWavelength']) - 1/(self.wavelengths))*1e7
    
    def plot_sif(self, axis) -> None: 

        def is_1_1_whatever(arr):
            if arr.ndim >= 2:
                return arr.shape[0] == 1 and arr.shape[1] == 1
            return False
        
        if is_1_1_whatever(self.data):
            fig, ax = plt.subplots(1,1,figsize = (11,6))
            counts = self.data[0,0,:] # eliminate unity dimensions
            
            wvs_allowed_keywords = {'wavelengths', 'lambda', 'wvl'}
            rams_allowed_keywords = {'raman', 'ramans','rams'}
            
            if axis.lower() in wvs_allowed_keywords:
                ax.plot(self.wavelengths, counts)
                ax.set_xlabel("Wavelength (nm)", fontsize = 22)
                
            elif axis.lower() in rams_allowed_keywords:
    
                if self.ramans.size == 0: 
                    try:
                        self.convert_xaxis_unit()
                    except KeyError:
                        raise ValueError(
                            "Cannot convert to Raman shift. 'RamanExWavelength' key "
                            "not found in the spectrum's info dictionary."
                        )
                if self.ramans.size == 0:
                    raise ValueError("Raman conversion failed - empty array")
            
                if self.ramans.shape != counts.shape:
                    raise ValueError(
                        f"Shape mismatch: ramans {self.ramans.shape} vs counts {counts.shape}"
                    )
                ax.plot(self.ramans, counts)
                ax.set_xlabel(r'Raman shift (cm$^{-1}$)', fontsize = 22)
                
            else:
                raise ValueError(f"Invalid axis keyword: '{axis}'. Allowed keywords are: {wvs_allowed_keywords}, {rams_allowed_keywords}")
                    
            ax.tick_params(axis='both', labelsize=14) 
            ax.set_ylabel('Counts', fontsize = 22)
            
            plt.savefig("{}_.png".format(self.filename[:-4]), dpi=500, bbox_inches = 'tight')
            print("{}_.png saved".format(self.filename[:-4]))
            
            plt.show()

        else:
            return
            