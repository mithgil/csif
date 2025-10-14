

function _skip_spaces(io::IO)
    while true
        current_pos = position(io)

        # Read one character. `read(io, Char)` handles multi-byte UTF-8 characters.
        # If EOF is reached, `read` will throw an EOFError by default.
        try
            c = read(io, Char)
            if c != ' ' && c != '\n' # Check if it's not space or newline
                # If it's a non-whitespace character, seek back to its beginning.
                # `seek(io, current_pos)` moves to the position before `c` was read.
                seek(io, current_pos)
                return io # Found non-whitespace, positioned for next read
            end
        catch e
            throw(e)
        end
    end
end

function split_bytes(data::Vector{UInt8}, delimiter::Vector{UInt8})
    result = Vector{UInt8}[]
    start = 1
    for i in 1:length(data)-length(delimiter)+1
        if data[i:i+length(delimiter)-1] == delimiter
            push!(result, data[start:i-1])
            start = i + length(delimiter)
        end
    end
    push!(result, data[start:end]) # Add the last segment
    return result
end

function extract_user_text(meta::Dict{Any,Any})
    #=
    Extract information from meta["user_text"].
        Current known info is
        + "Calibration data for frame {}"
    =#

    user_text = meta["user_text"];
    target = "Calibration data for" 

    if occursin(target, String(user_text[1:min(20, end)]))

        println("target here")

        texts = split_bytes(user_text, Vector{UInt8}("\n"))

        for i in 1:(meta["NumberOfFrames"])
            key = "Calibration_data_for_frame_$(i)"
            coefs = split(strip(texts[i][length(key)+2:end]), ',')
            meta[key] = [float(c) for c in coefs]
        end
        # Calibration data should be nothing 
        meta["Calibration_data"] = nothing
    else

        calib_data = meta["Calibration_data"]
        
        if calib_data isa String
            coefs = split(strip(calib_data))
            try
                meta["Calibration_data"] = [parse(Float64, c) for c in coefs]
            catch
                delete!(meta, "Calibration_data")
            end
        else
            delete!(meta, "Calibration_data")
        end
        
    end

    delete!(meta, "user_text")

    return meta
end

_MAGIC = "Andor Technology Multi-Channel File\n"

function load(filename::String)
    io = open(filename, "r");

    if String(read(io, 36)) != _MAGIC
        error(filename, " not a SIF file")
    end
    
    l = ""
    while !eof(io) && isempty(l)
        l = strip(readline(io))
    end
    l == "65538 1" || error("Unknown Andor version number at line 2: " * l)
    
    l = strip(readline(io))
    fields = split(l)
    metadata = Dict{Any,Any}()

    fields[1] == "65547" || fields[1] == "65558" || fields[1] == "65567" ||
        error("Unknown TInstaImage version number at line 3: " * fields[1])
    
    metadata["SifVersion"] = parse(Int, fields[1])
    metadata["dataType"] = parse(Int, fields[2])
    metadata["active"] = parse(Int, fields[3])
    metadata["structureVers"] = parse(Int, fields[4])
    metadata["date"] = parse(Int, fields[5])
    metadata["DetectorTemperature"] = max(parse(Float64, fields[6]), parse(Float64, fields[48]))
    metadata["isTemperatureStable"] = parse(Float64, fields[6]) != -999
    metadata["Head"] = fields[7]
    metadata["StoreType"] = fields[8]
    metadata["DataType"] = fields[9]
    metadata["Mode"] = fields[10]
    metadata["TriggerSource"] = fields[11]
    metadata["TriggerLevel"] = fields[12]
    metadata["ExposureTime"] = parse(Float64, fields[13])
    metadata["FrameDelay"] = parse(Float64, fields[14])
    metadata["IntegrationCycleTime"] = parse(Float64, fields[15])
    metadata["NoIntegrations"] = parse(Int, fields[16])
    metadata["sync"] = fields[17]
    metadata["kin_cycle_time"] = parse(Float64, fields[18])
    metadata["PixelReadoutTime"] = parse(Float64, fields[19])
    metadata["noPoints"] = parse(Int, fields[20])
    metadata["fastTrackHeight"] = parse(Int, fields[21])
    metadata["GainDAC"] = parse(Int, fields[22])
    metadata["gateDelay"] = parse(Float64, fields[23])
    metadata["gateWidth"] = parse(Float64, fields[24])
    metadata["gateStep"] = parse(Float64, fields[25])
    metadata["trackHeight"] = parse(Int, fields[26])
    metadata["seriesLength"] = parse(Int, fields[27])
    metadata["readPattern"] = fields[28]
    metadata["shutterDelay"] = fields[29]
    metadata["stCenterRow"] = parse(Int, fields[30])
    metadata["mtOffset"] = parse(Int, fields[31])
    metadata["operationMode"] = fields[32]
    metadata["flipx"] = fields[33]
    metadata["flipy"] = fields[34]
    metadata["clock"] = fields[35]
    metadata["aclock"] = fields[36]
    metadata["MCP"] = fields[37]
    metadata["prop"] = fields[38]
    metadata["IOC"] = fields[39]
    metadata["freq"] = fields[40]
    metadata["vertClockAmp"] = fields[41]
    metadata["dataVShiftSpeed"] = parse(Float64, fields[42])
    metadata["outputAmp"] = fields[43]
    metadata["preAmpGain"] = parse(Float64, fields[44])
    metadata["serial"] = parse(Int, fields[45])
    metadata["numPulses"] = parse(Int, fields[46])
    metadata["m_frame_transfer_acq_mode"] = parse(Int, fields[47])
    metadata["unstabilized_temperature"] = parse(Float64, fields[48])
    metadata["m_baseline_clamp"] = parse(Int, fields[49])
    metadata["m_pre_scan"] = parse(Int, fields[50])
    metadata["m_em_real_gain"] = parse(Int, fields[51])
    metadata["m_baseline_offset"] = parse(Int, fields[52])
    #_ = fields[53]
    #_ = fields[54]
    metadata["sw_vers1"] = parse(Int, fields[55])
    metadata["sw_vers2"] = parse(Int, fields[56])
    metadata["sw_vers3"] = parse(Int, fields[57])
    metadata["sw_vers4"] = parse(Int, fields[58])
    
    metadata["CameraModel"] = strip(readline(io))
    # line 5 camera dimensions
    l = strip(readline(io))
    fields = split(l)
    metadata["DetectorDimensions"] = (parse(Int, fields[1]), parse(Int, fields[2]))
      
    # line 6 filename
    metadata["OriginalFilename"] = strip(readline(io))
    
    # line 7
    line = strip(readline(io))
    fields = split(line)
    usertextlen = parse(Int,fields[2])

    # line 8 user text
    # note newline coincides with ASCII 0x0a char, which is recodnized in julia, so here readline will fail
    usertext = read(io, usertextlen)
    read(io, 1)  # Read the newline character 
    metadata["user_text"] = usertext

    # line 9 TShutter
    _ = readuntil(io, ' '); # 65538

    read(io, 8);# some bytes and 0
    line = readline(io);
    str1, str2 = split(line);

    metadata["ShutterTime"] = (parse(Float64, str1), parse(Float64, str2))

    # Version-based line skips and spectrograph parsing 
    if 65548 <= metadata["SifVersion"] <= 65557
        for _ in 1:2
            readline(io)
        end
    elseif metadata["SifVersion"] == 65558
        for _ in 1:5
            readline(io)
        end
    elseif metadata["SifVersion"] in [65559, 65564]
        for _ in 1:8
            readline(io)  # Skip to Line 18
        end
        metadata["spectrograph"] = split(strip(readline(io)))[2]  # Second field
    elseif metadata["SifVersion"] == 65565
        for _ in 1:15
            readline(io)
        end
    elseif metadata["SifVersion"] > 65565
        for _ in 1:8
            readline(io)  # Skip to Line 22
        end
        metadata["spectrograph"] = split(strip(readline(io)))[2]  # Second field
        readline(io)  # Skip intensifier line
        for _ in 1:3
            read(io, Float64)  # Skip 3 floats
        end
        metadata["GateGain"] = read(io, Float64)
        read(io, Float64)  # Skip 1 float
        read(io, Float64)  # Skip 1 float
        metadata["GateDelay"] = read(io, Float64) * 1e-12  # Convert to seconds
        metadata["GateWidth"] = read(io, Float64) * 1e-12  # Convert to seconds
        for _ in 1:4
            readline(io)
        end
    end

    if !haskey(metadata, "spectrograph")
        metadata["spectrograph"] = "sif version not checked yet"
    end

    metadata["SifCalbVersion"] = parse(Int ,readuntil(io, ' '))

    if metadata["SifCalbVersion"] == 65540
        readline(io)
    end

    if occursin("Mechelle", metadata["spectrograph"])
        metadata["PixelCalibration"] = [float(jj) for jj in split(strip(readline(io)))]
    else
        metadata["Calibration_data"] = readline(io)
    end

    readline(io) # 0 1 0 0 newline
    readline(io) # 0 1 0 0 newline
        
    raman = strip(readline(io))

    try
        metadata["RamanExWavelength"] = parse(Float64, raman)
    catch
        metadata["RamanExWavelength"] = NaN
    end

    for i in 1:4
        _ = readline(io)
    end

    line = readline(io)
    line = replace(line, r"\d+$" => "")
    metadata["FrameAxis"] = strip(line)
    line = readline(io)
    line = replace(line, r"\d+$" => "")
    metadata["DataType"] = strip(line)

    line =  readline(io)

    m = match(r"([A-Za-z\s]+)(\d+ .*)", line) 
    text_part = m.captures[1] # "Pixel number"
    numbers_part = m.captures[2] 
    metadata["ImageAxis"] = strip(text_part) 

    numbers_parts = split(numbers_part)
    no_images = parse(Int, numbers_parts[6])
    no_subimages = parse(Int, numbers_parts[7])

    total_length = parse(Int, numbers_parts[8])
    image_length = parse(Int, numbers_parts[9])

    metadata["NumberOfFrames"] = no_images
    metadata["NumberOfSubImages"] = no_subimages
    metadata["TotalLength"] = total_length
    metadata["ImageLength"] = image_length

    width, height = 0,0
    for i in 1:(no_subimages)
        # read subimage information
        _ = readuntil(io, ' ') # 65538

        frame_area = split(strip(readline(io)));
        x0, y1, x1, y0, ybin, xbin = parse.(Int,frame_area[1:6])
        metadata["imageFormatLeft"] = x0
        metadata["imageFormatTop"] = y1
        metadata["imageFormatRight"] = x1
        metadata["imageFormatBottom"] = y0
        metadata["imageFormatYbin"] = ybin
        metadata["imageFormatXbin"] = xbin
        
        width = Int((1 + x1 - x0) / xbin)
        height = Int((1 + y1 - y0) / ybin)
    end

    size = (Int(width), Int(height) * no_subimages)
    tile = []
    
    io = _skip_spaces(io)

    for f in 1:(no_images)
        metadata["timestamp_of_$(f)"] = parse(Int, readline(io));
    end

    offset = position(io)

    try # remove extra 0 if it exits.
        flag = parse(Int, readline(io))
        if flag == 0
            offset = position(io)
        end
        # remove another extra 1
        if flag == 1
            if metadata["SifVersion"] == 65567
                """
                In version 65567 after timestamp_array there is 1 and array of bigger numbers
                total of number of frames
                Maybe offset should be moved further in this case
                """
                for i in 1:(no_images)
                    readline(io)
                end
                    
                offset = position(io)
            end
        end
    catch
        seek(io, offset)
    end

    tile = [
        (
            "raw",                                     # Literal string
            (0, 0, size[1], size[2]),                  # Tuple concatenation (Julia needs explicit indexing or splatting)
            offset + f * width * height * no_subimages * 4, # Calculation for byte offset
            ("F;32F", 0, 1)                            # The data is stored in Fortran-order (column-major) as 32-bit
        )
        for f in 0:(no_images-1)                      
    ]
                     
    metadata["size"] = size
    metadata["tile"] = tile
    metadata["offset"] = offset
    
    metadata = extract_user_text(metadata)
    
    # real data reading
    dataArray = Array{Float32}(undef, width, height, no_images)
    read!(io, dataArray)
    
    # leftover content in the file could be in xml, which will be ignored
    close(io)

    return SIFData(dataArray, metadata)
    
end

