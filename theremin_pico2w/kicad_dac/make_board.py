# Digimin DAC 版の基板を作るスクリプト（KiCad 10 付属の Python で実行する）
#   ~/Applications/KiCad.app/Contents/Frameworks/Python.framework/Versions/Current/bin/python3 make_board.py
#   （フットプリントの場所は環境変数 KICAD_FOOTPRINT_DIR で変えられる）
# 外形と取り付け穴は秋月のアクリル板 LPMxxxT2-B（77 x 100mm を縦に使う、角 R3、φ3 の穴 4 つ、穴の間隔 66 x 89mm）に合わせる。
# 座標は mm、原点は基板の左上、y は下向き。
# 上の縁に PCM5102A モジュールを横置き（ジャックの口が上の縁）、その下に Pico 2 WH（USB は上向き）。
# Pico はピンソケットで約 8.5mm 浮くので、USB のプラグは DAC モジュールの上を通る。
import os
import pcbnew

HERE = os.path.dirname(os.path.abspath(__file__))
FP = os.environ.get("KICAD_FOOTPRINT_DIR",
                    os.path.expanduser("~/Applications/KiCad.app/Contents/SharedSupport/footprints"))
OUT = os.path.join(HERE, "digimin-dac.kicad_pcb")

W, H, R = 77.0, 100.0, 3.0
HOLE_IN = 5.5
CX = W / 2

board = pcbnew.BOARD()
mm = pcbnew.FromMM


def V(x, y):
    return pcbnew.VECTOR2I(mm(x), mm(y))


# ---- 設計ルール（JLCPCB の標準に余裕を持たせる） ----
ds = board.GetDesignSettings()
ds.m_TrackMinWidth = mm(0.2)
ds.m_MinClearance = mm(0.2)
ds.m_ViasMinSize = mm(0.6)
ds.m_MinThroughDrill = mm(0.3)
ds.m_CopperEdgeClearance = mm(0.5)
ds.m_HoleClearance = mm(0.25)
ds.m_HoleToHoleMin = mm(0.25)
nc = ds.m_NetSettings.GetDefaultNetclass()
nc.SetClearance(mm(0.2))
nc.SetTrackWidth(mm(0.25))
nc.SetViaDiameter(mm(0.6))
nc.SetViaDrill(mm(0.3))

NETS = {}


def net(name):
    if name not in NETS:
        n = pcbnew.NETINFO_ITEM(board, name)
        board.Add(n)
        NETS[name] = n
    return NETS[name]


def place(lib, name, ref, value, x, y, rot=0, pads=None):
    fp = pcbnew.FootprintLoad(os.path.join(FP, lib + ".pretty"), name)
    assert fp is not None, name
    fp.SetReference(ref)
    fp.SetValue(value)
    fp.Value().SetVisible(False)
    fp.Reference().SetVisible(False)
    board.Add(fp)
    fp.SetPosition(V(x, y))
    if rot:
        fp.SetOrientationDegrees(rot)
    for num, nname in (pads or {}).items():
        hit = [p for p in fp.Pads() if p.GetNumber() == str(num)]
        assert hit, (ref, num)
        for p in hit:
            p.SetNet(net(nname))
    return fp


def padpos(fp, num):
    p = [p for p in fp.Pads() if p.GetNumber() == str(num)][0]
    v = p.GetPosition()
    return (round(pcbnew.ToMM(v.x), 3), round(pcbnew.ToMM(v.y), 3))


# ---- PCM5102A モジュール（紫、17 x 32mm）を横置き。3.5mm ジャックの口はモジュールの長い辺に向いているので、
# その辺を基板の上の縁に合わせる。ジャックは左の端、6 ピン列は右の端で、部品面を上にすると上から
# VIN / GND / LCK / DIN / BCK / SCK の順。6 ピンを直接はんだ付けする ----
MOD_X0, MOD_X1 = 15.0, 47.0
MOD_Y0, MOD_Y1 = 1.0, 18.0      # ジャックは上の縁から少しはみ出す
DPX = MOD_X1 - 1.3              # 6 ピン列
DAC_CY = (MOD_Y0 + MOD_Y1) / 2
DAC_ORDER = ["VIN", "GND", "LCK", "DIN", "BCK", "SCK"]
DAC_Y = {n: DAC_CY + (i - 2.5) * 2.54 for i, n in enumerate(DAC_ORDER)}
dac = place("Connector_PinHeader_2.54mm", "PinHeader_1x06_P2.54mm_Vertical", "J4", "PCM5102A",
            DPX, DAC_Y["VIN"], pads={1: "VBUS", 2: "GND", 3: "LCK", 4: "DIN", 5: "BCK", 6: "GND"})
assert padpos(dac, 6) == (round(DPX, 3), round(DAC_Y["SCK"], 3)), padpos(dac, 6)

# ---- Pico 2 WH（1x20 のピンソケット 2 本）。USB を上（DAC 側）に向ける ----
PICO_TOP = MOD_Y1 + 2.0         # Pico の基板の上の縁
PX = CX - 8.89
PY = PICO_TOP + 1.37
pico_nets = {
    1: "LCK", 2: "BCK", 4: "DIN", 7: "WIFI_OFF", 12: "WIFI_OPEN",
    15: "P_XSHUT", 16: "P_SDA", 17: "P_SCL",
    24: "V_SDA", 25: "V_SCL", 26: "V_XSHUT",
    36: "+3V3", 40: "VBUS",
}
for g in (3, 8, 13, 18, 23, 28, 33, 38):
    pico_nets[g] = "GND"
pico = place("Module", "RaspberryPi_Pico_Common_THT", "U1", "Pico 2 WH", PX, PY, pads=pico_nets)
Rr = PX + 17.78


def pin(n):
    if n <= 20:
        return (PX, PY + (n - 1) * 2.54)
    return (Rr, PY + (40 - n) * 2.54)


assert padpos(pico, 1) == tuple(round(v, 3) for v in pin(1))
assert padpos(pico, 24) == tuple(round(v, 3) for v in pin(24))

# ---- DAC の電源の安定用（VBUS）。モジュールの右 ----
C1X, C1Y = 51.0, DAC_Y["VIN"] + 0.85
c1 = place("Capacitor_THT", "CP_Radial_D6.3mm_P2.50mm", "C1", "100uF", C1X, C1Y, pads={1: "VBUS", 2: "GND"})
C2X, C2Y = 51.0, 10.5
c2 = place("Capacitor_THT", "C_Disc_D3.0mm_W1.6mm_P2.50mm", "C2", "0.1uF", C2X, C2Y, pads={1: "VBUS", 2: "GND"})

# ---- Wi-Fi ジャンパー（GP5 / GND / GP9）。Pico の左 ----
JX = 24.0
JY = pin(7)[1]
j1 = place("Connector_PinHeader_2.54mm", "PinHeader_1x03_P2.54mm_Vertical", "J1", "WiFi", JX, JY,
           pads={1: "WIFI_OFF", 2: "GND", 3: "WIFI_OPEN"})

# ---- センサー（6 ピンのピンヘッダー、メスメスのケーブルでつなぐ）。並びはセンサーと同じ VIN / GND / SCL / SDA / GPIO1 / XSHUT。
# 180° 回して 1 番（VIN）を下にする ----
S_VIN_Y = pin(18)[1] + 2.8
SLX, SRX = 19.0, 58.0
j2 = place("Connector_PinHeader_2.54mm", "PinHeader_1x06_P2.54mm_Vertical", "J2", "PITCH", SLX, S_VIN_Y, rot=180,
           pads={1: "+3V3", 2: "GND", 3: "P_SCL", 4: "P_SDA", 6: "P_XSHUT"})
j3 = place("Connector_PinHeader_2.54mm", "PinHeader_1x06_P2.54mm_Vertical", "J3", "VOLUME", SRX, S_VIN_Y, rot=180,
           pads={1: "+3V3", 2: "GND", 3: "V_SCL", 4: "V_SDA", 6: "V_XSHUT"})
SY = {n: S_VIN_Y - i * 2.54 for i, n in enumerate(["VIN", "GND", "SCL", "SDA", "GPIO1", "XSHUT"])}
assert padpos(j2, 6) == (SLX, round(SY["XSHUT"], 3)), padpos(j2, 6)

# ---- 取り付け穴（M3） ----
for i, (x, y) in enumerate([(HOLE_IN, HOLE_IN), (W - HOLE_IN, HOLE_IN), (HOLE_IN, H - HOLE_IN), (W - HOLE_IN, H - HOLE_IN)]):
    place("MountingHole", "MountingHole_3.2mm_M3", "H%d" % (i + 1), "M3", x, y)

# ---- DAC の枕（3.5mm ジャックの下。針金を通す穴 2 つ、どこにもつながない） ----
PILLOW_X = MOD_X0 + 1.2  # モジュールの左の端のすぐ内側に、縦に 2 つ（ジャックの下）
PILLOW_YS = (DAC_CY - 4.5, DAC_CY + 4.5)
pillow = pcbnew.FOOTPRINT(board)
pillow.SetReference("P1")
pillow.Reference().SetVisible(False)
pillow.SetValue("pillow")
pillow.Value().SetVisible(False)
board.Add(pillow)
for py in PILLOW_YS:
    pad = pcbnew.PAD(pillow)
    pad.SetNumber("")
    pad.SetAttribute(pcbnew.PAD_ATTRIB_PTH)
    pad.SetShape(pcbnew.PAD_SHAPE_CIRCLE)
    pad.SetSize(pcbnew.VECTOR2I(mm(1.8), mm(1.8)))
    pad.SetDrillSize(pcbnew.VECTOR2I(mm(1.0), mm(1.0)))
    pad.SetLayerSet(pcbnew.PAD.PTHMask())
    pillow.Add(pad)
    pad.SetPosition(V(PILLOW_X, py))

# ---- 配線 ----
F, B = pcbnew.F_Cu, pcbnew.B_Cu


def track(nname, pts, layer, width=0.25):
    for (x1, y1), (x2, y2) in zip(pts, pts[1:]):
        t = pcbnew.PCB_TRACK(board)
        t.SetStart(V(x1, y1))
        t.SetEnd(V(x2, y2))
        t.SetWidth(mm(width))
        t.SetLayer(layer)
        t.SetNet(net(nname))
        board.Add(t)


def via(nname, x, y):
    v = pcbnew.PCB_VIA(board)
    v.SetPosition(V(x, y))
    v.SetWidth(mm(0.6))
    v.SetDrill(mm(0.3))
    v.SetNet(net(nname))
    board.Add(v)


# I2S: DAC の 6 ピン列から左へ出て、モジュールの下を通って Pico の左の列の上の端へ
track("LCK", [(DPX, DAC_Y["LCK"]), (PX, DAC_Y["LCK"]), pin(1)], F)
track("BCK", [(DPX, DAC_Y["BCK"]), (PX - 1.8, DAC_Y["BCK"]), (PX - 1.8, pin(2)[1]), pin(2)], B)
track("DIN", [(DPX, DAC_Y["DIN"]), (PX - 2.8, DAC_Y["DIN"]), (PX - 2.8, pin(4)[1]), pin(4)], B)

# VBUS: Pico 40 番 → DAC VIN、C1、C2（表）
track("VBUS", [pin(40), (Rr, DAC_Y["VIN"]), (DPX, DAC_Y["VIN"])], F, 0.4)
track("VBUS", [(Rr, C1Y), (C1X, C1Y), (C2X, C2Y)], F, 0.4)

# Wi-Fi ジャンパー
track("WIFI_OFF", [pin(7), (JX, JY)], B)
track("GND", [pin(8), (JX, JY + 2.54)], B, 0.4)
track("WIFI_OPEN", [pin(12), (PX - 3.1, pin(12)[1]), (PX - 3.1, JY + 5.08), (JX, JY + 5.08)], B)

# 音程センサー（左）
track("P_XSHUT", [pin(15), (PX - 4.6, pin(15)[1]), (PX - 4.6, SY["XSHUT"]), (SLX, SY["XSHUT"])], B)
track("P_SDA", [pin(16), (PX - 3.6, pin(16)[1]), (PX - 3.6, SY["SDA"]), (SLX, SY["SDA"])], B)
track("P_SCL", [pin(17), (PX - 3.6, pin(17)[1]), (PX - 3.6, SY["SCL"]), (SLX, SY["SCL"])], B)
track("GND", [pin(18), (PX - 3.6, pin(18)[1]), (PX - 3.6, SY["GND"]), (SLX, SY["GND"])], B, 0.4)

# 音量センサー（右）
track("V_XSHUT", [pin(26), (Rr + 3.6, pin(26)[1]), (Rr + 3.6, SY["XSHUT"]), (SRX, SY["XSHUT"])], B)
track("V_SCL", [pin(25), (Rr + 5.6, pin(25)[1]), (Rr + 5.6, SY["SCL"]), (SRX, SY["SCL"])], F)
track("V_SDA", [pin(24), (Rr + 6.6, pin(24)[1]), (Rr + 6.6, SY["SDA"]), (SRX, SY["SDA"])], B)
track("GND", [pin(23), (Rr + 4.6, pin(23)[1]), (Rr + 4.6, SY["GND"]), (SRX, SY["GND"])], B, 0.4)

# 3V3: Pico 36 番 → 右のセンサー → 基板の下を回って左のセンサー（Pico のアンテナの下は通らない）
ant_y0 = pin(20)[1] - 4.5
ant_y1 = ant_y0 + 8.0
V3X = SRX + 4.0
V3Y = ant_y1 + 1.5
track("+3V3", [pin(36), (V3X, pin(36)[1]), (V3X, SY["VIN"]), (SRX, SY["VIN"])], B, 0.4)
track("+3V3", [(V3X, SY["VIN"]), (V3X, V3Y), (SLX - 4.0, V3Y), (SLX - 4.0, SY["VIN"]), (SLX, SY["VIN"])], B, 0.4)

# GND の縫い合わせビア
for (x, y) in [(10, 25), (10, 40), (10, 55), (10, 70), (22, 22), (66, 20), (66, 35), (66, 50), (70, 62), (70, 76),
               (15, 88), (62, 88), (38.5, 96), (35, 35), (42, 35), (35, 50), (42, 50), (35, 4), (40, 16.5), (20, 4),
               (56, 15)]:
    via("GND", x, y)


# ---- 外形（角 R3） ----
def edge_line(x1, y1, x2, y2):
    s = pcbnew.PCB_SHAPE(board)
    s.SetShape(pcbnew.SHAPE_T_SEGMENT)
    s.SetStart(V(x1, y1))
    s.SetEnd(V(x2, y2))
    s.SetLayer(pcbnew.Edge_Cuts)
    s.SetWidth(mm(0.1))
    board.Add(s)


def edge_arc(cx, cy, sx, sy, deg):
    s = pcbnew.PCB_SHAPE(board)
    s.SetShape(pcbnew.SHAPE_T_ARC)
    s.SetCenter(V(cx, cy))
    s.SetStart(V(sx, sy))
    s.SetArcAngleAndEnd(pcbnew.EDA_ANGLE(deg, pcbnew.DEGREES_T), True)
    s.SetLayer(pcbnew.Edge_Cuts)
    s.SetWidth(mm(0.1))
    board.Add(s)


edge_line(R, 0, W - R, 0)
edge_line(W, R, W, H - R)
edge_line(W - R, H, R, H)
edge_line(0, H - R, 0, R)
edge_arc(W - R, R, W - R, 0, 90)
edge_arc(W - R, H - R, W, H - R, 90)
edge_arc(R, H - R, R, H, 90)
edge_arc(R, R, 0, R, 90)


# ---- ベタ GND（両面）とアンテナの下の禁止領域 ----
def poly_zone(layer_or_set, pts, nname=None, rule=False):
    z = pcbnew.ZONE(board)
    if rule:
        z.SetIsRuleArea(True)
        z.SetDoNotAllowZoneFills(True)
        z.SetDoNotAllowTracks(True)
        z.SetDoNotAllowVias(True)
        z.SetDoNotAllowPads(False)
        z.SetDoNotAllowFootprints(False)
        z.SetLayerSet(layer_or_set)
    else:
        z.SetLayer(layer_or_set)
        z.SetNet(net(nname))
        z.SetMinThickness(mm(0.25))
        z.SetLocalClearance(mm(0.3))
        z.SetThermalReliefGap(mm(0.4))
        z.SetThermalReliefSpokeWidth(mm(0.5))
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_THERMAL)
    ol = z.Outline()
    ol.NewOutline()
    for x, y in pts:
        ol.Append(mm(x), mm(y))
    board.Add(z)
    return z


for layer in (F, B):
    poly_zone(layer, [(0, 0), (W, 0), (W, H), (0, H)], "GND")
ls = pcbnew.LSET()
ls.AddLayer(F)
ls.AddLayer(B)
poly_zone(ls, [(PX - 2.5, ant_y0), (Rr + 2.5, ant_y0), (Rr + 2.5, ant_y1), (PX - 2.5, ant_y1)], rule=True)


# ---- シルク ----
def text(s, x, y, size=1.0, rot=0, bold=False, just=0):
    t = pcbnew.PCB_TEXT(board)
    t.SetText(s)
    t.SetPosition(V(x, y))
    t.SetLayer(pcbnew.F_SilkS)
    t.SetTextSize(pcbnew.VECTOR2I(mm(size), mm(size)))
    t.SetTextThickness(mm(size * (0.2 if bold else 0.15)))
    if rot:
        t.SetTextAngleDegrees(rot)
    if just < 0:
        t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_LEFT)
    elif just > 0:
        t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_RIGHT)
    board.Add(t)


def silk_rect(x0, y0, x1, y1, width=0.15):
    for (a, b, c, d) in [(x0, y0, x1, y0), (x1, y0, x1, y1), (x1, y1, x0, y1), (x0, y1, x0, y0)]:
        s = pcbnew.PCB_SHAPE(board)
        s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(V(a, b))
        s.SetEnd(V(c, d))
        s.SetLayer(pcbnew.F_SilkS)
        s.SetWidth(mm(width))
        board.Add(s)


text("Digimin DAC", CX, 82, 2.4, bold=True)
text("SliP LLC, Tokyo.", CX, 86, 1.1)
text("github.com/Satachito/Digimin", CX, 89, 0.8)

# DAC モジュールの外形とピン名
silk_rect(MOD_X0, MOD_Y0, MOD_X1, MOD_Y1)
text("PCM5102A module", 33.0, 16.3, 1.0)
text("jack ^", 21.0, MOD_Y0 + 1.8, 1.0)
text("pillow", PILLOW_X + 1.4, DAC_CY, 0.9, just=-1)
for n, y in DAC_Y.items():
    text(n, DPX - 1.5, y, 0.8, just=1)
text("C1 100uF", C1X + 5.6, C1Y, 0.8, just=-1)
text("C2", C2X + 3.8, C2Y, 0.8, just=-1)

text("Pico 2 WH", CX, PY + 24, 1.5)
text("USB ^", CX, PY + 2.0, 1.1)
text("antenna: no copper", CX, ant_y1 - 0.8, 0.8)

# Wi-Fi ジャンパー
text("WiFi", JX, JY - 2.3, 1.0)
for lbl, k in (("GP5", 0), ("GND", 1), ("GP9", 2)):
    text(lbl, JX - 1.5, JY + k * 2.54, 0.8, just=1)
text("5-G: off", 13.5, JY + 7.4, 0.8)
text("none: on", 13.5, JY + 8.9, 0.8)
text("G-9: open", 13.5, JY + 10.4, 0.8)

# センサー
text("PITCH", SLX, SY["XSHUT"] - 2.4, 1.0)
text("VOLUME", SRX, SY["XSHUT"] - 2.4, 1.0)
for n, y in SY.items():
    lbl = n if n != "GPIO1" else "GPIO1(nc)"
    text(lbl, SLX - 1.5, y, 0.8, just=1)
    text(lbl, SRX + 1.5, y, 0.8, just=-1)

filler = pcbnew.ZONE_FILLER(board)
filler.Fill(board.Zones())
board.Save(OUT)
print("saved", OUT)
