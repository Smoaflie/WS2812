import tkinter as tk
from tkinter import colorchooser
import tkinter.filedialog as filedialog
import json
import serial
import serial.tools.list_ports

import struct
import binascii
import time

# 串口相关全局变量
selected_port = None  # 当前选择的串口
serial_connection = None  # 串口连接对象

# 创建主窗口
root = tk.Tk()
root.title("灯珠阵列")
root.geometry("600x500")

# 全局变量
current_color = "#000000"
is_left_mouse_pressed = False
is_right_mouse_pressed = False
MIN_BLOCK_SIZE = 30  # 每个矩形块的最小边长
rendered_blocks = {}  # 已渲染块集合 {(row, col): (rect_id, text_id)}

# 更新颜色显示
def update_color(new_color):
    global current_color
    current_color = new_color
    color_label.config(bg=current_color)
    hex_value.config(text=new_color)

# 选择颜色
def choose_color():
    color_code = colorchooser.askcolor()[1]
    if color_code:
        update_color(color_code)

# 生成矩阵
def generate_matrix():
    global n, m
    try:
        n = max(1, int(n_entry.get()))
        m = max(1, int(m_entry.get()))
    except ValueError:
        return  # 输入无效直接返回

    # 设置画布滚动范围
    canvas_width = n * MIN_BLOCK_SIZE + MIN_BLOCK_SIZE
    canvas_height = m * MIN_BLOCK_SIZE + MIN_BLOCK_SIZE
    matrix_canvas.config(scrollregion=(0, 0, canvas_width, canvas_height))

    # 清除已渲染记录
    matrix_canvas.delete("all")
    rendered_blocks.clear()

    # 刷新可视区域矩阵块
    update_visible_blocks()

# 动态加载可见区域的矩阵块
def update_visible_blocks(event=None):
    canvas_x1 = matrix_canvas.canvasx(0)
    canvas_y1 = matrix_canvas.canvasy(0)
    canvas_x2 = matrix_canvas.canvasx(matrix_canvas.winfo_width())
    canvas_y2 = matrix_canvas.canvasy(matrix_canvas.winfo_height())

    start_row = max(0, int(canvas_x1 // MIN_BLOCK_SIZE))
    end_row = min(n, int(canvas_x2 // MIN_BLOCK_SIZE) + 1)
    start_col = max(0, int(canvas_y1 // MIN_BLOCK_SIZE))
    end_col = min(m, int(canvas_y2 // MIN_BLOCK_SIZE) + 1)

    for row in range(start_row, end_row):
        for col in range(start_col, end_col):
            if (row, col) not in rendered_blocks:
                draw_rectangle(row, col)

# 绘制单个矩形块
def draw_rectangle(row, col):
    x1, y1 = row * MIN_BLOCK_SIZE, col * MIN_BLOCK_SIZE
    x2, y2 = x1 + MIN_BLOCK_SIZE, y1 + MIN_BLOCK_SIZE
    rect = matrix_canvas.create_rectangle(x1, y1, x2, y2, fill="#FFFFFF", outline="black", tags="rect")
    index = row * m + col + 1 if row % 2 == 0 else (row + 1) * m - col
    text = matrix_canvas.create_text((x1 + x2) // 2, (y1 + y2) // 2, text=index, font=("Arial", 8))
    rendered_blocks[(row, col)] = (rect, text)

# 更新块颜色
def update_block_color(event, color):
    x = matrix_canvas.canvasx(event.x)
    y = matrix_canvas.canvasy(event.y)
    row, col = int(x // MIN_BLOCK_SIZE), int(y // MIN_BLOCK_SIZE)
    if (row, col) in rendered_blocks:
        rect_id, _ = rendered_blocks[(row, col)]
        matrix_canvas.itemconfig(rect_id, fill=color)

# 鼠标事件处理
def auto_scroll():
    """检查鼠标是否超出画布范围并自动滚动"""
    global is_left_mouse_pressed

    if is_left_mouse_pressed:
        # 获取鼠标的全局位置和画布的相对位置
        mouse_x, mouse_y = root.winfo_pointerx(), root.winfo_pointery()
        canvas_x, canvas_y = matrix_canvas.winfo_rootx(), matrix_canvas.winfo_rooty()
        canvas_width, canvas_height = matrix_canvas.winfo_width(), matrix_canvas.winfo_height()

        # 计算鼠标相对于画布的位置
        relative_x = mouse_x - canvas_x
        relative_y = mouse_y - canvas_y

        # 设置滚动速度
        scroll_speed = 1  # 每次移动1个单位

        scroll_range = 5  # 距离边缘位置<当前值时移动
        # 判断鼠标是否超出画布范围
        if relative_x < scroll_range:
            matrix_canvas.xview_scroll(-scroll_speed, "units")  # 向左滚动
            update_visible_blocks()
        elif relative_x > canvas_width-scroll_range:
            matrix_canvas.xview_scroll(scroll_speed, "units")  # 向右滚动
            update_visible_blocks()

        if relative_y < scroll_range:
            matrix_canvas.yview_scroll(-scroll_speed, "units")  # 向上滚动
            update_visible_blocks()
        elif relative_y > canvas_height - scroll_range:
            matrix_canvas.yview_scroll(scroll_speed, "units")  # 向下滚动
            update_visible_blocks()

        # 再次检查
        root.after(50, auto_scroll)

# 修改鼠标按下事件处理，启动自动滚动功能
def on_mouse_press(event, button):
    global is_left_mouse_pressed, is_right_mouse_pressed
    if button == "left":
        is_left_mouse_pressed = True
        auto_scroll()  # 启动自动滚动检查
    elif button == "right":
        is_right_mouse_pressed = True

# 鼠标释放时停止滚动
def on_mouse_release(event, button):
    global is_left_mouse_pressed, is_right_mouse_pressed
    if button == "left":
        is_left_mouse_pressed = False
    elif button == "right":
        is_right_mouse_pressed = False

def on_mouse_move(event):
    if is_left_mouse_pressed:
        update_block_color(event, current_color)
    elif is_right_mouse_pressed:
        update_block_color(event, "#FFFFFF")

# GUI布局
setting_frame = tk.Frame(root)
setting_frame.pack(side="bottom", anchor="sw", padx=10, pady=10)

tk.Label(setting_frame, text="n:").grid(row=0, column=0, padx=5, pady=5)
n_entry = tk.Entry(setting_frame, width=10)
n_entry.insert(0, "10")
n_entry.grid(row=0, column=1, padx=5, pady=5)

tk.Label(setting_frame, text="m:").grid(row=1, column=0, padx=5, pady=5)
m_entry = tk.Entry(setting_frame, width=10)
m_entry.insert(0, "10")
m_entry.grid(row=1, column=1, padx=5, pady=5)

generate_button = tk.Button(setting_frame, text="生成矩阵", command=generate_matrix)
generate_button.grid(row=2, column=0, columnspan=2, pady=5)

choose_color_frame = tk.Frame(setting_frame)
choose_color_frame.grid(row=2, column=2, padx=10, pady=5)

color_label = tk.Label(choose_color_frame, width=2, height=1, bg=current_color)
color_label.bind("<Button-1>", lambda event: choose_color())
color_label.grid(row=0, column=0)

hex_value = tk.Label(choose_color_frame, text=current_color)
hex_value.grid(row=0, column=1)

# 操作界面
operation_frame = tk.Frame(setting_frame)
operation_frame.grid(row=2, column=4, padx=10, pady=5)  # 放在右侧

# 保存矩阵到文件
def save_to_file():
    # 打开系统文件保存对话框
    file_path = filedialog.asksaveasfilename(
        defaultextension=".json",
        filetypes=[("JSON Files", "*.json"), ("All Files", "*.*")]
    )
    if file_path:  # 用户选择了文件路径
        # 自定义要保存的内容
        save_data = {
            "n": n_entry.get(),
            "m": m_entry.get(),
            "color": current_color,
            "blocks": {f"{row},{col}": matrix_canvas.itemcget(rect_id, "fill")
                       for (row, col), (rect_id, _) in rendered_blocks.items()}
        }
        # 将数据写入文件
        with open(file_path, "w") as file:
            json.dump(save_data, file, indent=4)
        print(f"保存成功：{file_path}")

# 从文件加载矩阵
def load_from_file():
    # 打开系统文件选择对话框
    file_path = filedialog.askopenfilename(
        filetypes=[("JSON Files", "*.json"), ("All Files", "*.*")]
    )
    if file_path:  # 用户选择了文件
        try:
            with open(file_path, "r") as file:
                load_data = json.load(file)
            print(f"加载成功：{file_path}")
            # 更新矩阵参数
            n_entry.delete(0, tk.END)
            n_entry.insert(0, load_data["n"])
            m_entry.delete(0, tk.END)
            m_entry.insert(0, load_data["m"])
            update_color(load_data["color"])
            # 生成矩阵并填充颜色
            generate_matrix()
            for key, color in load_data["blocks"].items():
                row, col = map(int, key.split(","))
                if (row, col) in rendered_blocks:
                    rect_id, _ = rendered_blocks[(row, col)]
                    matrix_canvas.itemconfig(rect_id, fill=color)
        except Exception as e:
            print(f"加载失败：{e}")

save_button = tk.Button(operation_frame, text="save", command=save_to_file).grid(row=0, column=0)
load_button = tk.Button(operation_frame, text="load", command=load_from_file).grid(row=0, column=1)

# 获取串口设备列表
def get_serial_ports():
    return [port.device for port in serial.tools.list_ports.comports()]


# 刷新串口设备列表
def refresh_ports():
    ports = get_serial_ports()
    port_dropdown['menu'].delete(0, 'end')  # 清空下拉菜单
    for port in ports:
        port_dropdown['menu'].add_command(
            label=port,
            command=lambda p=port: port_var.set(p)
        )
    if ports:
        port_var.set(ports[0])  # 默认选择第一个
    else:
        port_var.set("No devices")


# 连接到选定的串口
def connect_serial():
    global serial_connection, selected_port
    selected_port = port_var.get()
    if not selected_port or selected_port == "No devices":
        print("没有可用的串口设备")
        return

    try:
        serial_connection = serial.Serial(selected_port, baudrate=115200, timeout=1)
        print(f"成功连接到串口：{selected_port}")
    except Exception as e:
        print(f"连接失败：{e}")

def calculate_crc16(data):
    """计算 CRC16 校验值"""
    crc = 0xFFFF  # 初始值
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc
# 向串口发送数据
def send_to_serial():
    global serial_connection
    if not serial_connection or not serial_connection.is_open:
        print("串口未连接")
        return

    def message_generator():
        leds = []
        color = "#FFFFFF"
        for (row, col), (rect_id, text_id) in rendered_blocks.items():
            rect_id, text_id = rendered_blocks[(row, col)]
            rect_color = matrix_canvas.itemcget(rect_id, "fill")
            rect_index = matrix_canvas.itemcget(text_id, "text")
            leds.append((rect_index,rect_color))
        
        blocks = []
        leds.sort(key=lambda x: x[0])
        for led in leds:
            led_index = led[0]
            led_color = led[1]
            if color != led_color:
                color = led_color
                blocks.append((led_index, color))
        print(blocks)
        messages = []
        head = 0xAA  # 包头
        command = 0xA0  # 命令码
        data_len = 0x0B  # 数据长度

        type_ = 0x01
        led_num = n*m
        block_num = blocks.__len__()
        reserve = 0x0000
        datas = struct.pack("<BIIH", type_, led_num, block_num, reserve)
        packet_without_crc = struct.pack("<BBB", head, command, data_len) + datas
        crc = calculate_crc16(packet_without_crc)
        crc = 0xABCD
        crc_bytes = struct.pack("<H", crc)  # CRC16 采用小端序
        complete_packet = packet_without_crc + crc_bytes
        
        messages.append(complete_packet)
        
        def hex_to_rgb_binary(hex_color):
            """
            将 #RRGGBB 格式的十六进制颜色字符串转换为 RGB 的 3 字节二进制表示
            :param hex_color: 颜色字符串 (例如 "#FFFFFF")
            :return: RGB 的 3 字节二进制表示
            """
            # 确保输入格式正确
            if not hex_color.startswith("#") or len(hex_color) != 7:
                raise ValueError("颜色格式必须为 #RRGGBB")
            
            # 提取 R、G、B 的十六进制值并转换为整数
            red = int(hex_color[1:3], 16)   # 提取第 1-2 位，并转为整数
            green = int(hex_color[3:5], 16) # 提取第 3-4 位，并转为整数
            blue = int(hex_color[5:7], 16)  # 提取第 5-6 位，并转为整数

            # 打包为 RGB 的 3 字节二进制
            rgb_binary = struct.pack("BBB", red, green, blue)
            return rgb_binary

        command = 0xA1
        data_len = 0x09
        for (index, color) in blocks:
            start_pos = struct.pack("<I", int(index))
            color = hex_to_rgb_binary(color)
            datas = start_pos + color + struct.pack("<H", reserve)
            packet_without_crc = struct.pack("<BBB", head, command, data_len) + datas
            crc = calculate_crc16(packet_without_crc)
            crc = 0xABCD
            crc_bytes = struct.pack("<H", crc)  # CRC16 采用小端序
            complete_packet = packet_without_crc + crc_bytes
            messages.append(complete_packet)
        return messages
    
    try:
        for message in message_generator():
            serial_connection.write(message)  # 发送数据
            time.sleep(0.5)

        print(f"发送成功")
    except Exception as e:
        print(f"发送失败：{e}")


# 添加串口选择下拉框和刷新按钮
port_var = tk.StringVar()
ports = get_serial_ports()
port_var.set(ports[0] if ports else "No devices")

port_dropdown = tk.OptionMenu(operation_frame, port_var, *ports)
port_dropdown.grid(row=0, column=3, padx=5)

refresh_button = tk.Button(operation_frame, text="🔄", command=refresh_ports)
refresh_button.grid(row=0, column=4, padx=5)

connect_button = tk.Button(operation_frame, text="Connect", command=connect_serial)
connect_button.grid(row=0, column=5, padx=5)

send_button = tk.Button(operation_frame, text="Send", command=send_to_serial)
send_button.grid(row=0, column=6, padx=5)

# 矩形区域
matrix_canvas = tk.Canvas(root, bg="white")
matrix_canvas.pack(fill="both", expand=True)

def scroll_x(*args):
    # 滚动横向滚动条并更新可见区域
    matrix_canvas.xview(*args)
    update_visible_blocks()

def scroll_y(*args):
    # 滚动纵向滚动条并更新可见区域
    matrix_canvas.yview(*args)
    update_visible_blocks()

x_scroll = tk.Scrollbar(matrix_canvas, orient="horizontal", command=scroll_x)
x_scroll.pack(side="bottom", fill="x")
y_scroll = tk.Scrollbar(matrix_canvas, orient="vertical", command=scroll_y)
y_scroll.pack(side="right", fill="y")
matrix_canvas.config(xscrollcommand=x_scroll.set, yscrollcommand=y_scroll.set)

matrix_canvas.bind("<Configure>", update_visible_blocks)
matrix_canvas.bind("<ButtonPress-1>", lambda event: on_mouse_press(event, "left"))
matrix_canvas.bind("<ButtonRelease-1>", lambda event: on_mouse_release(event, "left"))
matrix_canvas.bind("<ButtonPress-3>", lambda event: on_mouse_press(event, "right"))
matrix_canvas.bind("<ButtonRelease-3>", lambda event: on_mouse_release(event, "right"))
matrix_canvas.bind("<Motion>", on_mouse_move)

generate_matrix()
root.mainloop()
