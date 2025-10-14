# 檢查是否已經載入過模組，如果沒有才載入
if !@isdefined HexViewer
    include("HexViewer.jl")
    using .HexViewer
end

function demo()
    # 創建測試檔案
    test_data = UInt8[0x48, 0x65, 0x6C, 0x6C, 0x6F, 0x20, 0x57, 0x6F, 0x72, 0x6C, 0x64]
    write("test.bin", test_data)
    
    println("=== 基礎十六進位查看 ===")
    hexdump("test.bin")
    
    println("\n=== 進階十六進位編輯器 ===")
    hexview = HexViewer("test.bin")
    display(hexview)
    
    println("\n=== 移動游標並編輯 ===")
    move_cursor!(hexview, :right)
    edit_byte!(hexview, 0x41)
    display(hexview)
    
    # 清理
    rm("test.bin")
end

function main()
    if length(ARGS) > 0
        filename = ARGS[1]
        if isfile(filename)
            println("開啟檔案: $filename")
            interactive_hex_viewer(filename)
        else
            println("錯誤: 檔案不存在: $filename")
        end
    else
        println("沒有指定檔案，運行演示模式...")
        demo()
    end
end

if abspath(PROGRAM_FILE) == @__FILE__
    main()
end