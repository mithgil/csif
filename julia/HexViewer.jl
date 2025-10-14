module HexViewer

export hexdump, HexViewer, display, move_cursor!, edit_byte!, save, interactive_hex_viewer

function hexdump(filename; bytes_per_line=16, max_lines=100)
    """
    類似 GHex 的十六進位查看器
    """
    filesize = stat(filename).size
    line_count = 0
    
    open(filename, "r") do file
        offset = 0
        
        while !eof(file) && line_count < max_lines
            # 讀取一行數據
            chunk = read(file, min(bytes_per_line, filesize - offset))
            
            # 顯示偏移量
            print(lpad(string(offset, base=16), 8, '0'), "  ")
            
            # 顯示十六進位數據
            for (i, byte) in enumerate(chunk)
                print(lpad(string(byte, base=16), 2, '0'), " ")
                if i == bytes_per_line ÷ 2
                    print(" ")
                end
            end
            
            # 填充空白（如果一行不滿）
            remaining = bytes_per_line - length(chunk)
            for i in 1:remaining
                print("   ")
                if i == bytes_per_line ÷ 2 - length(chunk)
                    print(" ")
                end
            end
            
            print(" ")
            
            # 顯示 ASCII 字符
            for byte in chunk
                if 0x20 <= byte <= 0x7E  # 可打印字符
                    print(Char(byte))
                else
                    print(".")
                end
            end
            
            println()
            
            offset += length(chunk)
            line_count += 1
        end
        
        if line_count == max_lines && offset < filesize
            println("\n... (只顯示前 $max_lines 行)")
        end
    end
end

struct HexViewer
    filename::String
    data::Vector{UInt8}
    cursor_pos::Int
    bytes_per_line::Int
end

function HexViewer(filename::String)
    data = read(filename)
    HexViewer(filename, data, 1, 16)
end

function Base.display(hexview::HexViewer)
    """
    顯示十六進位視圖
    """
    println("檔案: $(hexview.filename) | 大小: $(length(hexview.data)) 位元組")
    println("="^70)
    
    for line_start in 1:hexview.bytes_per_line:length(hexview.data)
        line_end = min(line_start + hexview.bytes_per_line - 1, length(hexview.data))
        line_data = hexview.data[line_start:line_end]
        
        # 偏移量
        print(lpad(string(line_start-1, base=16), 8, '0'), "  ")
        
        # 十六進位數據
        for (i, byte) in enumerate(line_data)
            byte_str = lpad(string(byte, base=16), 2, '0')
            
            # 高亮游標位置
            pos = line_start + i - 1
            if pos == hexview.cursor_pos
                print("\e[41m$byte_str\e[0m ")  # 紅色背景
            else
                print("$byte_str ")
            end
            
            if i == hexview.bytes_per_line ÷ 2
                print(" ")
            end
        end
        
        # 填充空白
        remaining = hexview.bytes_per_line - length(line_data)
        for i in 1:remaining
            print("   ")
            if i == hexview.bytes_per_line ÷ 2 - length(line_data)
                print(" ")
            end
        end
        
        print(" |")
        
        # ASCII 表示
        for byte in line_data
            if 0x20 <= byte <= 0x7E
                print(Char(byte))
            else
                print(".")
            end
        end
        
        println("|")
    end
end

function move_cursor!(hexview::HexViewer, direction::Symbol)
    """
    移動游標
    """
    if direction == :right && hexview.cursor_pos < length(hexview.data)
        hexview.cursor_pos += 1
    elseif direction == :left && hexview.cursor_pos > 1
        hexview.cursor_pos -= 1
    elseif direction == :down
        hexview.cursor_pos = min(hexview.cursor_pos + hexview.bytes_per_line, length(hexview.data))
    elseif direction == :up
        hexview.cursor_pos = max(hexview.cursor_pos - hexview.bytes_per_line, 1)
    end
end

function edit_byte!(hexview::HexViewer, new_byte::UInt8)
    """
    編輯位元組
    """
    if 1 <= hexview.cursor_pos <= length(hexview.data)
        hexview.data[hexview.cursor_pos] = new_byte
    end
end

function save(hexview::HexViewer, new_filename="")
    """
    儲存檔案
    """
    filename = isempty(new_filename) ? hexview.filename : new_filename
    write(filename, hexview.data)
    println("檔案已儲存: $filename")
end

function interactive_hex_viewer(filename)
    """
    簡單的互動式十六進位查看器
    """
    hexview = HexViewer(filename)
    
    println("十六進位查看器 - $(filename)")
    println("指令: ←→↑↓ 移動 | e 編輯 | s 儲存 | q 離開")
    
    while true
        display(hexview)
        println("\n游標位置: $(hexview.cursor_pos-1) (0x$(string(hexview.cursor_pos-1, base=16)))")
        print("指令: ")
        
        cmd = readline()
        
        if cmd == "q"
            break
        elseif cmd == "→"
            move_cursor!(hexview, :right)
        elseif cmd == "←"
            move_cursor!(hexview, :left)
        elseif cmd == "↓"
            move_cursor!(hexview, :down)
        elseif cmd == "↑"
            move_cursor!(hexview, :up)
        elseif startswith(cmd, "e ")
            # 編輯位元組: e FF
            try
                byte_str = split(cmd)[2]
                new_byte = parse(UInt8, byte_str, base=16)
                edit_byte!(hexview, new_byte)
                println("已修改: 0x$byte_str")
            catch e
                println("錯誤: 無效的位元組格式")
            end
        elseif cmd == "s"
            save(hexview)
        else
            println("未知指令")
        end
        
        println()
    end
end

end # module HexViewer