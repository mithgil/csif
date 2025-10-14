# SiferTypes.jl

export SIFData 

struct SIFData
    data::AbstractArray
    metadata::Dict{Any, Any}
end

