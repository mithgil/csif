
using Polynomials

function retrieveCalibration(meta::Dict{Any,Any})
    #=
    Returns
    -------
        1d vector of [width] if only 1 calibration is found.
        2d array of [NumberOfFrames x width] if multiple calibrations are found.
        nothing if no calibration is found
    =#

    width = meta["DetectorDimensions"][1]

    if haskey(meta, "ImageLength")
        width = meta["ImageLength"] 
    end
    # multiple calibration data is stored
    if haskey(meta, "Calibration_data_for_frame_1")
        calibration = Array{Float64}(undef, meta["NumberOfFrames"], width)

        for f in 1:(length(calibration))
            key = "Calibration_data_for_frame_$(f)";
            reverse_coef =reverse(meta[key])
            p = Polynomial(reverse(reverse_coef))
            calibration[f] = p.(1:width)
        end
        return calibration

    elseif haskey(meta, "Calibration_data")
        reverse_coef = reverse(meta["Calibration_data"])
        p = Polynomial(reverse(reverse_coef))
        return p.(1:width)
    else
        return nothing

    end
end

function Raman2Wavelength(excitation_wavelength_nm, raman_shift_cm_inv::Float64)::Float64

    excitation_wavelength_cm = excitation_wavelength_nm * 1e-7 # Convert nm to cm
    raman_wavelength_cm_inv = 1 / excitation_wavelength_cm - raman_shift_cm_inv
    raman_wavelength_nm = 1 / (raman_wavelength_cm_inv * 1e-7) # Convert cm⁻¹ to nm
    
    return raman_wavelength_nm
end

function Wavelength2Raman(excitation_wavelength_nm, raman_wavelength_nm::Float64)::Float64
    
    raman_shift = (1 / excitation_wavelength_nm - 1 / raman_wavelength_nm)* 1e7
    
    return raman_shift
end

using DelimitedFiles

function Export2Txt(sif::SIFData, outputFilename::String = "_data.txt")

    filename = outputFilename;
    metaData = sif.metadata;
    data_raw = sif.data # Original data (width, height, frames)
    
    local data_to_write # Declare data_to_write to be local
    local num_output_rows
    local num_output_cols

    if size(data_raw, 2) == 1 # Height dimension is 1 (e.g., spectrum data)
        data_to_write = dropdims(data_raw, dims = (2))
        # After dropdims, data_to_write will be (width, frames) or (width,)
        if ndims(data_to_write) == 1
            # If it's a single spectrum, writedlm expects a 2D array, so make it a column vector
            data_to_write = reshape(data_to_write, :, 1)
        end
        num_output_rows = size(data_to_write, 1)
        num_output_cols = size(data_to_write, 2)

    else # Height dimension is > 1 (e.g., image data)
        # Reshape (width, height, frames) to (width * height, frames)
        # This makes each column a frame, and each row a flattened (width*height) pixel location.
        data_to_write = reshape(data_raw, size(data_raw, 1) * size(data_raw, 2), size(data_raw, 3))
        num_output_rows = size(data_to_write, 1)
        num_output_cols = size(data_to_write, 2)
    end

    # Each column now represents a frame
    column_headers = ["Frame_$(i)" for i in 1:num_output_cols]

    open(filename, "w") do io
        
        # Sort metadata keys for consistent output order
        sorted_keys = sort(collect(keys(metaData))); 
        println(io, "# ------ METADATA ------")

        # Add derived dimensions to metadata for clarity
        println(io, "# Original Dimensions: $(size(data_raw))")
        println(io, "# Output Dimensions: ($(num_output_rows), $(num_output_cols))")
        println(io, "# Output Format: Rows = Flattened Width*Height (or Width if Height=1), Columns = Frames")

        for key in sorted_keys
            value = metaData[key]
            clean_value_str = replace(string(value), '\0' => "") 
            # Handle complex metadata values by converting to string if necessary
            println(io, "# $key: $(string(clean_value_str))")
        end
        println(io, "# ----------------------")
        println(io, "#") # Blank line for separation

		waveLengths = retrieveCalibration(metaData)
		axis_name = metaData["FrameAxis"] 
		writedlm(io, [axis_name], ',')
		
		isRaman = metaData["FrameAxis"] == "Raman Shift"
		if isRaman
			RamanExcitation = sifImage.metadata["RamanExWavelength"]
			RamanShift = sifer_utils.Wavelength2Raman.(RamanExcitation, waveLengths)
		end
		
		writedlm(io, isRaman ? RamanShift : waveLengths, ',')
		println(io, "---")
        # column headers
        writedlm(io, [column_headers], ',') # [column_headers] wraps it into a 1-row matrix
		
        # actual data
        writedlm(io, data_to_write, ',')
    end
    
    println("Data successfully exported to $filename.")
end

using Printf

function printSifHexFromat(filename::String)
    open(filename) do s
        for (i, byte) in enumerate(data)
            @printf("%02X ", byte)
            if i % 16 == 0
                println()
            end
        end
    end
end

function printSifEachline(filename::String)
    open(filename) do io
        i = 1
        numBytes = 0;
        while !eof(io)
            if i == 7
                pos = position(io)
                _ = readuntil(io, " ")
                numBytes = parse(Int, readline(io))
                @show numBytes
                seek(io, pos)
                line = readline(io)
            elseif i == 8
                line = read(io, numBytes) 
                read(io, 1) # read the newline character
            else
                line = readline(io)
            end

            println(i, "\t", repr(line))
            i += 1
        end
    end
end


