import math
import pcbnew


BOARD_W = 78.0
BOARD_H = 48.0
CORNER_R = 3.0
EDGE_MARGIN = 4.0
TRACK_W = 0.5
POWER_W = 0.8
SILK_W = 0.15


def mm(value):
    return pcbnew.FromMM(value)


def pt(x_mm, y_mm):
    return pcbnew.VECTOR2I_MM(x_mm, y_mm)


def add_net(board, name):
    net = pcbnew.NETINFO_ITEM(board, name)
    board.Add(net)
    return net


def set_text(item, text, x, y, angle=0):
    item.SetText(text)
    item.SetPosition(pt(x, y))
    item.SetTextAngle(pcbnew.EDA_ANGLE(angle * 10, pcbnew.TENTHS_OF_A_DEGREE_T))
    item.SetTextSize(pcbnew.VECTOR2I_MM(1.0, 1.0))
    item.SetTextThickness(mm(0.15))


def add_ref_value(footprint, ref_text, value_text, x, y):
    ref = footprint.Reference()
    set_text(ref, ref_text, x, y - 2.6)
    ref.SetLayer(pcbnew.F_SilkS)
    val = footprint.Value()
    set_text(val, value_text, x, y + 2.6)
    val.SetLayer(pcbnew.F_Fab)


def add_line(board_or_fp, layer, x1, y1, x2, y2, width=SILK_W):
    shape = pcbnew.PCB_SHAPE(board_or_fp)
    shape.SetShape(pcbnew.S_SEGMENT)
    shape.SetLayer(layer)
    shape.SetStart(pt(x1, y1))
    shape.SetEnd(pt(x2, y2))
    shape.SetWidth(mm(width))
    board_or_fp.Add(shape)
    return shape


def add_text(board, text, x, y, layer=pcbnew.F_SilkS, size=1.0, angle=0):
    item = pcbnew.PCB_TEXT(board)
    item.SetText(text)
    item.SetLayer(layer)
    item.SetPosition(pt(x, y))
    item.SetTextSize(pcbnew.VECTOR2I_MM(size, size))
    item.SetTextThickness(mm(0.15))
    item.SetTextAngle(pcbnew.EDA_ANGLE(angle * 10, pcbnew.TENTHS_OF_A_DEGREE_T))
    board.Add(item)
    return item


def make_pad(parent, name, x, y, net, size_x, size_y, drill, shape, angle=0):
    pad = pcbnew.PAD(parent)
    pad.SetName(str(name))
    pad.SetAttribute(pcbnew.PAD_ATTRIB_PTH)
    pad.SetShape(shape)
    pad.SetSize(pcbnew.VECTOR2I_MM(size_x, size_y))
    pad.SetDrillSize(pcbnew.VECTOR2I_MM(drill, drill))
    pad.SetLayerSet(pad.PTHMask())
    pad.SetPosition(pt(x, y))
    pad.SetOrientation(pcbnew.EDA_ANGLE(angle * 10, pcbnew.TENTHS_OF_A_DEGREE_T))
    pad.SetNet(net)
    parent.Add(pad)
    return pad


def add_header(board, ref, value, x, y, pins, pitch, vertical, net_names, labels=None):
    footprint = pcbnew.FOOTPRINT(board)
    footprint.SetReference(ref)
    footprint.SetValue(value)
    footprint.SetPosition(pt(x, y))
    add_ref_value(footprint, ref, value, x, y)

    left = x - 1.6
    right = x + 1.6 if vertical else x + (pins - 1) * pitch + 1.6
    bottom = y - 1.6 if vertical else y - 1.6
    top = y + (pins - 1) * pitch + 1.6 if vertical else y + 1.6
    add_line(footprint, pcbnew.F_SilkS, left, bottom, right, bottom)
    add_line(footprint, pcbnew.F_SilkS, right, bottom, right, top)
    add_line(footprint, pcbnew.F_SilkS, right, top, left, top)
    add_line(footprint, pcbnew.F_SilkS, left, top, left, bottom)

    pads = {}
    for index, net_name in enumerate(net_names):
        px = x if vertical else x + index * pitch
        py = y + index * pitch if vertical else y
        shape = pcbnew.PAD_SHAPE_RECT if index == 0 else pcbnew.PAD_SHAPE_OVAL
        pad = make_pad(footprint, index + 1, px, py, board.FindNet(net_name), 1.8, 1.8, 1.0, shape)
        pads[index + 1] = pad
        if labels:
            if vertical:
                add_text(board, labels[index], px + 2.5, py, layer=pcbnew.F_Fab, size=0.8)
            else:
                add_text(board, labels[index], px, py + 2.8, layer=pcbnew.F_Fab, size=0.8, angle=0)

    board.Add(footprint)
    return footprint, pads


def add_terminal_3(board, ref, value, x, y, net_names, labels):
    footprint = pcbnew.FOOTPRINT(board)
    footprint.SetReference(ref)
    footprint.SetValue(value)
    footprint.SetPosition(pt(x, y))
    add_ref_value(footprint, ref, value, x + 5.0, y)
    add_line(footprint, pcbnew.F_SilkS, x - 2.5, y - 4.0, x + 12.5, y - 4.0)
    add_line(footprint, pcbnew.F_SilkS, x + 12.5, y - 4.0, x + 12.5, y + 4.0)
    add_line(footprint, pcbnew.F_SilkS, x + 12.5, y + 4.0, x - 2.5, y + 4.0)
    add_line(footprint, pcbnew.F_SilkS, x - 2.5, y + 4.0, x - 2.5, y - 4.0)
    pads = {}
    for index, net_name in enumerate(net_names):
        px = x + index * 5.0
        py = y
        pad = make_pad(footprint, index + 1, px, py, board.FindNet(net_name), 2.4, 2.4, 1.2, pcbnew.PAD_SHAPE_OVAL)
        pads[index + 1] = pad
        add_text(board, labels[index], px, py + 5.2, layer=pcbnew.F_Fab, size=0.8)
    board.Add(footprint)
    return footprint, pads


def add_resistor(board, ref, value, x1, y1, x2, y2, net1, net2):
    cx = (x1 + x2) / 2
    cy = (y1 + y2) / 2
    footprint = pcbnew.FOOTPRINT(board)
    footprint.SetReference(ref)
    footprint.SetValue(value)
    footprint.SetPosition(pt(cx, cy))
    add_ref_value(footprint, ref, value, cx, cy)
    make_pad(footprint, 1, x1, y1, board.FindNet(net1), 2.0, 2.0, 0.9, pcbnew.PAD_SHAPE_OVAL)
    make_pad(footprint, 2, x2, y2, board.FindNet(net2), 2.0, 2.0, 0.9, pcbnew.PAD_SHAPE_OVAL)
    add_line(footprint, pcbnew.F_SilkS, x1 + 1.2, y1, x2 - 1.2, y2)
    add_line(footprint, pcbnew.F_SilkS, x1 + 1.2, y1 - 1.2, x2 - 1.2, y2 - 1.2)
    add_line(footprint, pcbnew.F_SilkS, x1 + 1.2, y1 + 1.2, x2 - 1.2, y2 + 1.2)
    board.Add(footprint)
    return footprint


def add_capacitor(board, ref, value, x_left, y, x_right, net_left, net_right):
    cx = (x_left + x_right) / 2
    footprint = pcbnew.FOOTPRINT(board)
    footprint.SetReference(ref)
    footprint.SetValue(value)
    footprint.SetPosition(pt(cx, y))
    add_ref_value(footprint, ref, value, cx, y)
    make_pad(footprint, 1, x_left, y, board.FindNet(net_left), 2.4, 2.4, 1.0, pcbnew.PAD_SHAPE_CIRCLE)
    make_pad(footprint, 2, x_right, y, board.FindNet(net_right), 2.4, 2.4, 1.0, pcbnew.PAD_SHAPE_CIRCLE)
    add_line(footprint, pcbnew.F_SilkS, x_left + 2.0, y - 3.0, x_left + 2.0, y + 3.0)
    add_line(footprint, pcbnew.F_SilkS, x_right - 2.0, y - 3.0, x_right - 2.0, y + 3.0)
    add_text(board, "+", x_left - 1.4, y - 2.2, size=0.8)
    board.Add(footprint)
    return footprint


def add_trace(board, net_name, points, layer, width):
    net = board.FindNet(net_name)
    for start, end in zip(points, points[1:]):
        track = pcbnew.PCB_TRACK(board)
        track.SetLayer(layer)
        track.SetWidth(mm(width))
        track.SetNet(net)
        track.SetStart(pt(*start))
        track.SetEnd(pt(*end))
        board.Add(track)


def add_via(board, net_name, x, y, drill=0.4, diameter=0.8):
    via = pcbnew.PCB_VIA(board)
    via.SetNet(board.FindNet(net_name))
    via.SetPosition(pt(x, y))
    via.SetDrill(mm(drill))
    via.SetWidth(mm(diameter))
    board.Add(via)
    return via


def add_rounded_outline(board, x0, y0, width, height, radius):
    segments = [
        ((x0 + radius, y0), (x0 + width - radius, y0)),
        ((x0 + width, y0 + radius), (x0 + width, y0 + height - radius)),
        ((x0 + width - radius, y0 + height), (x0 + radius, y0 + height)),
        ((x0, y0 + height - radius), (x0, y0 + radius)),
    ]
    for start, end in segments:
        add_line(board, pcbnew.Edge_Cuts, start[0], start[1], end[0], end[1], width=0.1)

    arcs = [
        ((x0 + radius, y0 + radius), (x0 + radius, y0), (x0, y0 + radius)),
        ((x0 + width - radius, y0 + radius), (x0 + width, y0 + radius), (x0 + width - radius, y0)),
        ((x0 + width - radius, y0 + height - radius), (x0 + width - radius, y0 + height), (x0 + width, y0 + height - radius)),
        ((x0 + radius, y0 + height - radius), (x0, y0 + height - radius), (x0 + radius, y0 + height)),
    ]

    for center, start, end in arcs:
        arc = pcbnew.PCB_SHAPE(board)
        arc.SetShape(pcbnew.S_ARC)
        arc.SetLayer(pcbnew.Edge_Cuts)
        arc.SetCenter(pt(*center))
        arc.SetStart(pt(*start))
        arc.SetEnd(pt(*end))
        arc.SetWidth(mm(0.1))
        board.Add(arc)


def build_board():
    board = pcbnew.BOARD()

    for name in [
        "3V3", "GND", "5V_LED", "I2C_SDA", "I2C_SCL", "PIR_DATA",
        "REED_SIGNAL", "MIC_ANALOG", "MIC_GAIN", "MIC_AR", "LED_DATA", "LED_DATA_IN",
        "LED1_TO_LED2", "LED2_TO_LED3"
    ]:
        add_net(board, name)

    add_rounded_outline(board, 0, 0, BOARD_W, BOARD_H, CORNER_R)

    add_text(board, "Sentinel Passive Carrier v1", 39.0, 3.5, size=1.2)
    add_text(board, "Fits current 95x65 enclosure as a wire-up carrier", 39.0, 45.0, layer=pcbnew.F_Fab, size=0.8)

    j1, _ = add_header(
        board, "J1", "ESP32_WIRE_HARNESS", 8.0, 10.0, 9, 3.81, True,
        ["3V3", "GND", "5V_LED", "I2C_SDA", "I2C_SCL", "PIR_DATA", "REED_SIGNAL", "MIC_ANALOG", "LED_DATA"],
        ["3V3", "GND", "5V", "21", "22", "27", "32", "34", "25"],
    )

    add_header(
        board, "J2", "BME280", 44.0, 8.0, 4, 3.81, False,
        ["3V3", "GND", "I2C_SDA", "I2C_SCL"], ["V", "G", "SDA", "SCL"]
    )

    add_header(
        board, "J3", "AM312_PIR", 44.0, 18.0, 3, 3.81, False,
        ["3V3", "GND", "PIR_DATA"], ["V", "G", "OUT"]
    )

    add_header(
        board, "J4", "MAX9814", 36.0, 28.0, 5, 3.81, False,
        ["3V3", "GND", "MIC_ANALOG", "MIC_GAIN", "MIC_AR"], ["V", "G", "OUT", "GAIN", "AR"]
    )

    add_header(
        board, "J5", "REED_SWITCH", 58.0, 32.0, 2, 5.08, False,
        ["REED_SIGNAL", "GND"], ["SIG", "GND"]
    )

    add_terminal_3(board, "J6", "LED_STRIP", 42.0, 42.0, ["5V_LED", "LED_DATA_IN", "GND"], ["5V", "DIN", "GND"])
    add_resistor(board, "R1", "330R", 24.0, 40.48, 31.62, 40.48, "LED_DATA", "LED_DATA_IN")
    add_capacitor(board, "C1", "470uF", 58.0, 42.0, 64.0, "5V_LED", "GND")

    # Power and signal routing
    add_trace(board, "3V3", [(8.0, 10.0), (12.0, 10.0), (12.0, 6.0), (44.0, 6.0)], pcbnew.B_Cu, POWER_W)
    add_trace(board, "3V3", [(44.0, 6.0), (44.0, 8.0)], pcbnew.B_Cu, POWER_W)
    add_trace(board, "3V3", [(44.0, 6.0), (44.0, 18.0)], pcbnew.B_Cu, POWER_W)
    add_trace(board, "3V3", [(36.0, 6.0), (36.0, 28.0)], pcbnew.B_Cu, POWER_W)

    add_trace(board, "GND", [(8.0, 13.81), (16.0, 13.81), (16.0, 46.0), (64.0, 46.0)], pcbnew.B_Cu, POWER_W)
    add_trace(board, "GND", [(56.0, 46.0), (56.0, 10.0), (47.81, 10.0), (47.81, 8.0)], pcbnew.B_Cu, POWER_W)
    add_trace(board, "GND", [(52.0, 46.0), (52.0, 20.0), (47.81, 20.0), (47.81, 18.0)], pcbnew.B_Cu, POWER_W)
    add_trace(board, "GND", [(34.0, 46.0), (34.0, 30.0), (39.81, 30.0), (39.81, 28.0)], pcbnew.B_Cu, POWER_W)
    add_trace(board, "GND", [(52.0, 46.0), (52.0, 42.0)], pcbnew.B_Cu, POWER_W)
    add_trace(board, "GND", [(63.08, 45.0), (63.08, 32.0)], pcbnew.B_Cu, POWER_W)
    add_trace(board, "GND", [(64.0, 46.0), (64.0, 42.0)], pcbnew.B_Cu, POWER_W)

    add_trace(board, "5V_LED", [(8.0, 17.62), (10.0, 17.62), (10.0, 38.0), (58.0, 38.0)], pcbnew.B_Cu, POWER_W)
    add_trace(board, "5V_LED", [(42.0, 38.0), (42.0, 42.0)], pcbnew.B_Cu, POWER_W)
    add_trace(board, "5V_LED", [(58.0, 38.0), (58.0, 42.0)], pcbnew.B_Cu, POWER_W)

    add_trace(board, "I2C_SDA", [(8.0, 21.43), (26.0, 21.43), (26.0, 12.0), (51.62, 12.0), (51.62, 8.0)], pcbnew.F_Cu, TRACK_W)
    add_trace(board, "I2C_SCL", [(8.0, 25.24), (30.0, 25.24), (30.0, 15.0), (55.43, 15.0), (55.43, 8.0)], pcbnew.F_Cu, TRACK_W)
    add_trace(board, "PIR_DATA", [(8.0, 29.05), (34.0, 29.05), (34.0, 21.0), (51.62, 21.0), (51.62, 18.0)], pcbnew.F_Cu, TRACK_W)
    add_trace(board, "REED_SIGNAL", [(8.0, 32.86), (10.0, 32.86), (10.0, 29.0), (54.0, 29.0), (54.0, 32.0), (58.0, 32.0)], pcbnew.F_Cu, TRACK_W)
    add_trace(board, "MIC_ANALOG", [(8.0, 36.67), (32.0, 36.67), (32.0, 31.0), (43.62, 31.0), (43.62, 28.0)], pcbnew.F_Cu, TRACK_W)
    add_trace(board, "LED_DATA", [(8.0, 40.48), (24.0, 40.48)], pcbnew.F_Cu, TRACK_W)
    add_trace(board, "LED_DATA_IN", [(31.62, 40.48), (31.62, 37.2), (47.0, 37.2), (47.0, 42.0)], pcbnew.F_Cu, TRACK_W)

    return board


def main():
    board = build_board()
    output = r"d:\Source Code\ESP32\Sent\hardware\pcb\Sentinel_Passive_Carrier.kicad_pcb"
    pcbnew.SaveBoard(output, board)
    print(output)


if __name__ == "__main__":
    main()