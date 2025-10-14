#using SIFKit
#using Printf # 
"""
include("SIFKit.jl")

using .SIFKit: load
"""

filename = "/home/tim/Documents/AS/data/andor/20250908/monochrom_430_700_10_LED_2.sif"

sifImage = load(filename)
@show size(sifImage.data)      # (width, height, frames)

const metadata = sifImage.metadata

@show metadata["ExposureTime"]

@show metadata["FrameAxis"]

@show metadata["RamanExWavelength"]

printSifEachline(filename)

