import numpy as np

import sif_parser

import spectrum as spe 

filename = '/home/tim/Documents/AS/data/andor/20250917/1OD_500uW_sapphire_200umFiber_.sif'
#filename = '/home/tim/Documents/AS/data/andor/20250908/monochrom_430_700_10_LED_2.sif'

spec = spe.Spectrum.read_sif(filename)

spec.show_pretty_info()

#print(spec.info['DetectorType'])

print(spec.info['FrameAxis'])
print(spec.info['DataType'])
print(spec.info['ImageAxis'])
print(spec.timestamps)

print(f"wavelength_min {spec.wavelengths.min()}\n          _max {spec.wavelengths.max()}")

print(spec.data[0,0,:10])
