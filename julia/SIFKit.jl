module SIFKit

using Polynomials, Printf  

include("SiferTypes.jl") 
include("sifer.jl")
include("utils.jl")

export load, SIFData, retrieveCalibration, 
       Raman2Wavelength, Wavelength2Raman, printSifHexFromat, printSifEachline, Export2Txt

end 
