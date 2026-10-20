#!/usr/bin/env python3
"""Turn the KiCad netlist into a PCBWay-format assembly BOM for the Lamoka1/2.

The KiCad BOM export cannot produce the PCBWay 9-column template, and the
schematic carries Manufacturer data on almost nothing. This script does both:
it takes per-reference MPN data from the exported netlist, applies a
hand-maintained manufacturer/description map, and emits one line per part
number.

Usage:

    kicad-cli sch export netlist --output /tmp/Lamoka1-half.net \
        HARDWARE/Kicadfiles/Lamoka1-half/Lamoka1-half.kicad_sch
    python3 tools/make_fab_bom.py /tmp/Lamoka1-half.net \
        "HARDWARE/Lamoka1-half fab files/Lamoka1-half BOM.csv"

Add --check to report problems without writing the file. Nothing outside the
output path is modified. See HARDWARE/Lamoka1-half fab files/README.md.
"""

import csv
import re
import sys
from collections import OrderedDict

# refs that are PCB features, not parts: mounting holes, wire solder pads, net tie
NOT_A_PART = {"H1", "H2", "J4", "J6", "NT1"}

# MPN (or Value, when the schematic leaves MPN blank) -> (Manufacturer, Description)
PARTS = {
    # --- semiconductors -----------------------------------------------------
    "UCC27735DR": (
        "Texas Instruments",
        "Dual-channel high- and low-side gate driver for the H-bridge, SOIC-14",
    ),
    "TPS62160DSGR": (
        "Texas Instruments",
        "Synchronous step-down (buck) converter for the 5 V control rail, WSON-8",
    ),
    "TMP235A2DBZR": ("Texas Instruments", "Analog temperature sensor, SOT-23-3"),
    "LM5122MHX/NOPB": (
        "Texas Instruments",
        "Wide-input synchronous boost controller with integrated gate drivers, "
        "HTSSOP-20 (PowerPAD)",
    ),
    "INA240A2DR": (
        "Texas Instruments",
        "Bidirectional current-sense amplifier with enhanced PWM rejection, SOIC-8",
    ),
    "ADS7128IRTER": (
        "Texas Instruments",
        "8-channel 12-bit analog-to-digital converter with I2C interface, WQFN-16",
    ),
    "CSD19536KCS": ("Texas Instruments", "N-channel NexFET power MOSFET, TO-220, H-bridge switch"),
    "CSD17304Q3": (
        "Texas Instruments",
        "N-channel NexFET power MOSFET with thermal pad, VSON-8, 30 V, "
        "boost converter switching pair",
    ),
    "SN74AHCT1G125DBVR": (
        "Texas Instruments",
        "Single tri-state buffer, SOT-23-5, LED data level shifter",
    ),
    "RP2354A": (
        "Raspberry Pi",
        "RP2354A microcontroller, dual Cortex-M33 / Hazard3, 2 MB flash, VQFN-60",
    ),
    "AMS1117-3.3": (
        "Advanced Monolithic Systems",
        "1 A low-dropout linear regulator, fixed 3.3 V, SOT-223",
    ),
    "WS2812B": ("Worldsemi", "Addressable RGB LED with integrated controller, PLCC-4 5.0 x 5.0 mm"),
    "SS14-E3/61T": ("Vishay Semiconductors", "40 V, 1 A Schottky rectifier, SMA (DO-214AC)"),
    "SK215ATR": (
        "SMC Diode Solutions",
        "150 V, 2 A Schottky rectifier, SMA (DO-214AC), boost bootstrap diode",
    ),
    # --- inductors and crystal ---------------------------------------------
    "XAL1580-182MEB": ("Coilcraft", "Shielded power inductor, 1.8 uH, XAL1580 series"),
    "PA4332.222NLT": (
        "Pulse Electronics",
        "Shielded drum-core power inductor, 2.2 uH, 4.2 x 4.2 x 2.0 mm",
    ),
    "AOTA-B201610S3R3-101-T": (
        "Abracon",
        "Molded shielded power inductor, 3.3 uH, 2.0 x 1.6 x 1.0 mm, MCU core regulator",
    ),
    "ABM8-272-T3": ("Abracon", "Crystal, 27.12 MHz, 18 pF load, 3.2 x 2.5 mm"),
    # --- current sense resistors -------------------------------------------
    "CSNL2512FT2L00": (
        "Stackpole",
        "Current-sense resistor, 2 mOhm, 1 %, 2 W, 2512 (boost switch and input monitor shunts)",
    ),
    # --- resistors ----------------------------------------------------------
    "RC1206FR-0710RL": ("Yageo", "Resistor, 10 Ohm, 1 %, 1206"),
    "RC0603FR-0710KL": ("Yageo", "Resistor, 10 kOhm, 1 %, 0603"),
    "RC0603FR-0722RL": ("Yageo", "Resistor, 22 Ohm, 1 %, 0603"),
    "RC0603FR-0727RL": ("Yageo", "Resistor, 27 Ohm, 1 %, 0603"),
    "RC0603FR-07330RL": ("Yageo", "Resistor, 330 Ohm, 1 %, 0603"),
    "RC0603FR-0733RL": ("Yageo", "Resistor, 33 Ohm, 1 %, 0603"),
    "RC0603FR-0736K5L": ("Yageo", "Resistor, 36.5 kOhm, 1 %, 0603"),
    "RC0603FR-0768KL": ("Yageo", "Resistor, 68 kOhm, 1 %, 0603"),
    "RC0603FR-074K7L": ("Yageo", "Resistor, 4.7 kOhm, 1 %, 0603"),
    "RC0603FR-07180KL": ("Yageo", "Resistor, 180 kOhm, 1 %, 0603"),
    "RC0603FR-075K1L": ("Yageo", "Resistor, 5.1 kOhm, 1 %, 0603, USB-C CC pull-down"),
    "RC0402FR-071KL": ("Yageo", "Resistor, 1 kOhm, 1 %, 0402"),
    "RC0201FR-0749K9L": ("Yageo", "Resistor, 49.9 kOhm, 1 %, 0201, slope compensation"),
    "RC0201FR-0780K6L": ("Yageo", "Resistor, 80.6 kOhm, 1 %, 0201, boost loop compensation"),
    "CRCW040218K2FKED": ("Vishay", "Resistor, 18.2 kOhm, 1 %, 0402"),
    "CRCW04023K79FKED": ("Vishay", "Resistor, 3.79 kOhm, 1 %, 0402"),
    "CRCW04023R01FKED": ("Vishay", "Resistor, 3.01 Ohm, 1 %, 0402"),
    "CRCW040249K9FKED": ("Vishay", "Resistor, 49.9 kOhm, 1 %, 0402"),
    "CRCW040264K9FKED": ("Vishay", "Resistor, 64.9 kOhm, 1 %, 0402, boost bus feedback divider"),
    "CRCW04027K15FKED": ("Vishay", "Resistor, 7.15 kOhm, 1 %, 0402"),
    "CRCW0402953KFKED": ("Vishay", "Resistor, 953 kOhm, 1 %, 0402"),
    "CRCW0402100KFKED": ("Vishay", "Resistor, 100 kOhm, 1 %, 0402"),
    # --- ceramic capacitors -------------------------------------------------
    "CL10B104KO8NNNC": ("Samsung Electro-Mechanics", "Capacitor, 100 nF, 0603"),
    "CL10B104KB8NNNC": (
        "Samsung Electro-Mechanics",
        "Capacitor, 100 nF, 50 V X7R, 0603, gate driver VDD bypass and monitor-input filter",
    ),
    "CL10A105KA8NNNC": ("Samsung Electro-Mechanics", "Capacitor, 1 uF, 0603"),
    "CL10A475KP8NNNC": ("Samsung Electro-Mechanics", "Capacitor, 4.7 uF, 0603"),
    "CL10B473KB8NNNC": ("Samsung Electro-Mechanics", "Capacitor, 47 nF, 0603"),
    "CL05A106MQ5NUNC": ("Samsung Electro-Mechanics", "Capacitor, 10 uF, 0402, AMS1117 5 V input"),
    "CL05B104KO5NNNC": ("Samsung Electro-Mechanics", "Capacitor, 100 nF, 0402"),
    "GRM155R71A333KA01D": ("Murata", "Capacitor, 33 nF, 0402"),
    "GRM1555C1H448JA01D": ("Murata", "Capacitor, 44.8 pF, C0G, 0402, boost loop compensation"),
    "GRM21BR71H105KA12L": ("Murata", "Capacitor, 1 uF, 0805"),
    "GRM21BZ71E106KE15L": ("Murata", "Capacitor, 10 uF, 25 V X7R, 0805, gate driver VDD bypass"),
    "GRM31CR61A107MEA8K": ("Murata", "Capacitor, 100 uF, 1206"),
    "GRM31CR71A226KE15L": ("Murata", "Capacitor, 22 uF, 1206"),
    "0402CG150J500NT": ("Fenghua (Guangdong Fenghua Advanced Tech)", "Capacitor, 15 pF, C0G, 0402"),
    "C1005X5R1V474K050BC": ("TDK", "Capacitor, 470 nF, 0402"),
    "C3216X5R1V226M160AC": ("TDK", "Capacitor, 22 uF, 35 V X5R, 1206, boost output bus"),
    "CGA4F1C0G2A153J085AE": ("TDK", "Capacitor, 15 nF, C0G, 0805, boost loop compensation"),
    "TMK212BBJ106KG-T": ("Taiyo Yuden", "Capacitor, 10 uF, 0805"),
    "TMK212BJ474KD-T": ("Taiyo Yuden", "Capacitor, 470 nF, 0805"),
    "MSAST21GAB5475MTNAO1": ("Taiyo Yuden", "Capacitor, 4.7 uF, 0805"),
    "08053C104KAT2A": ("KYOCERA AVX", "Capacitor, 100 nF, 0805"),
    # --- electrolytic / polymer / tantalum capacitors ------------------------
    "35SVPK180M": (
        "Panasonic Industry",
        "Polymer aluminium capacitor, 180 uF, 35 V, 8 x 11.9 mm, boost output",
    ),
    "20SVPF56MX": (
        "Panasonic Industry",
        "Polymer aluminium capacitor, 56 uF, 20 V, 5 x 5.9 mm, boost input",
    ),
    "T491A226K010AT": (
        "KEMET",
        "Tantalum capacitor, 22 uF, 10 V, 3216 (A case), 3.3 V rail output",
    ),
    # --- connectors, switch, electromechanical ------------------------------
    "2606-1108/010-000": ("WAGO", "8-pin THT terminal block, 3.5 mm pitch, transformer interface"),
    "2606-1352": ("WAGO", "2-pin THT terminal block, 3.5 mm pitch, DC input"),
    "2604-1103": ("WAGO", "3-pin THT terminal block, 3.5 mm pitch, AC output"),
    "TYPE-C-31-M-12": (
        "Korean Hroparts Elec",
        "USB 2.0 Type-C receptacle, 16 pin SMD with through-hole shield, right angle",
    ),
    "B3F-4055": ("Omron", "Through-hole tactile switch, 12 x 12 mm, user button"),
}

# (Value, footprint) -> MPN, for symbols whose MPN field is empty and which have no
# sibling carrying one. Each target MPN appears in the KiCad BOM export's MPN column for
# its own value/footprint group, and the resistance/capacitance code is embedded in the
# part number itself, so the mapping is unambiguous.
VALUE_ALIASES = {
    ("4.7k", "R_0603_1608Metric"): "RC0603FR-074K7L",
    ("36.5k", "R_0603_1608Metric"): "RC0603FR-0736K5L",
    ("68k", "R_0603_1608Metric"): "RC0603FR-0768KL",
    ("49.9k", "R_0402_1005Metric"): "CRCW040249K9FKED",
    ("64.9k", "R_0402_1005Metric"): "CRCW040264K9FKED",
    ("49.9k", "R_0201_0603Metric"): "RC0201FR-0749K9L",
    ("80.6k", "R_0201_0603Metric"): "RC0201FR-0780K6L",
    ("1uF", "C_0603_1608Metric"): "CL10A105KA8NNNC",
    ("47nF", "C_0603_1608Metric"): "CL10B473KB8NNNC",
    ("100nF", "C_0603_1608Metric"): "CL10B104KB8NNNC",
    ("44.8pF", "C_0402_1005Metric"): "GRM1555C1H448JA01D",
    ("15nF", "C_0805_2012Metric"): "CGA4F1C0G2A153J085AE",
    ("10uF", "C_0805_2012Metric"): "GRM21BZ71E106KE15L",
    ("22uF", "C_1206_3216Metric"): "C3216X5R1V226M160AC",
}

# Footprint (without library prefix) -> (Package as printed on the BOM, assembly type)
FOOTPRINT_MAP = {
    "C_0402_1005Metric": ("0402", "SMT"),
    "C_0603_1608Metric": ("0603", "SMT"),
    "C_0805_2012Metric": ("0805", "SMT"),
    "C_1206_3216Metric": ("1206", "SMT"),
    "R_0201_0603Metric": ("0201", "SMT"),
    "R_0402_1005Metric": ("0402", "SMT"),
    "R_0603_1608Metric": ("0603", "SMT"),
    "R_1206_3216Metric": ("1206", "SMT"),
    "R_2512_6332Metric": ("2512", "SMT"),
    "CAPC2114X145N": ("0805", "SMT"),
    "G-31_MUR": ("1206", "SMT"),
    "RC0603N_YAG": ("0603", "SMT"),
    "RC0402N_VIS": ("0402", "SMT"),
    "RES_CRCW_0402_VIS": ("0402", "SMT"),
    "CP_Elec_8x11.9": ("SMD radial can 8 x 11.9 mm", "SMT"),
    "CP_Elec_8x11.9_Cout1": ("SMD radial can 8 x 11.9 mm", "SMT"),
    "CP_Elec_8x11.9_Cout2": ("SMD radial can 8 x 11.9 mm", "SMT"),
    "CP_Elec_8x11.9_Cout3": ("SMD radial can 8 x 11.9 mm", "SMT"),
    "CP_Elec_5x5.9": ("SMD radial can 5 x 5.9 mm", "SMT"),
    "CP_EIA-3216-18_Kemet-A": ("3216 (A case)", "SMT"),
    "LED_WS2812B_PLCC4_5.0x5.0mm_P3.2mm": ("PLCC-4 5.0 x 5.0 mm", "SMT"),
    "DO-214AC_VIS": ("SMA (DO-214AC)", "SMT"),
    "D_SMA": ("SMA (DO-214AC)", "SMT"),
    "L_Coilcraft_XAL1580-182": ("XAL1580", "SMT"),
    "IND_PA4332.222NLT_PUL": ("4.2 x 4.2 x 2.0 mm", "SMT"),
    "AOTAB201610S3R3101T": ("2.0 x 1.6 x 1.0 mm", "SMT"),
    "ABM8272T3": ("3.2 x 2.5 mm", "SMT"),
    "QFN-60-1EP_7x7mm_P0.4mm_EP3.4x3.4mm": ("VQFN-60 7 x 7 mm", "SMT"),
    "SOT-223-3_TabPin2": ("SOT-223", "SMT"),
    "SOT-23-5": ("SOT-23-5", "SMT"),
    "SOIC_14L_TEX": ("SOIC-14", "SMT"),
    "SOIC127P600X175-8N": ("SOIC-8", "SMT"),
    "WQFN16_RTE_TEX": ("WQFN-16", "SMT"),
    "DBZ0003A-IPC_A": ("SOT-23-3", "SMT"),
    "PWP0020A_N": ("HTSSOP-20 (PWP)", "SMT"),
    "DQG0008A": ("VSON-8 (DQG)", "SMT"),
    "DSG8_1P6X0P9": ("WSON-8 (DSG)", "SMT"),
    "TO-220-3_Vertical": ("TO-220", "THT"),
    "SW_B3F-4050_OMR": ("12 x 12 mm", "THT"),
    "CONN8_2606-1108&slash_010-000_WAG": ("3.5 mm pitch, 8 pin", "THT"),
    "CONN2_2606-1352_WAG": ("3.5 mm pitch, 2 pin", "THT"),
    "CONN3_2604-1103_WAG": ("3.5 mm pitch, 3 pin", "THT"),
    "USB_C_Receptacle_HRO_TYPE-C-31-M-12": ("16 pin, right angle", "SMT + THT shield"),
}

COMP_FIELD = re.compile(
    r'\(comp\s+\(ref "([^"]+)"\)\s*\(value "([^"]*)"\)\s*\(footprint "([^"]*)"\)(.*?)\n\s*\)', re.S
)
FIELD = re.compile(r'\(name "([^"]*)"\)\s*("([^"]*)")?')


def read_netlist(path):
    src = open(path, encoding="utf-8", errors="replace").read()
    out = []
    for ref, value, fpid, rest in COMP_FIELD.findall(src):
        fields = {name: (val or "") for name, _q, val in FIELD.findall(rest)}
        fp = fpid.split(":")[-1]
        out.append(
            {
                "ref": ref,
                "value": value,
                "footprint": fp,
                "mpn": fields.get("MPN", ""),
                "description": fields.get("Description", ""),
            }
        )
    return out


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    netlist, out_path = sys.argv[1], sys.argv[2]
    check_only = "--check" in sys.argv

    comps = [c for c in read_netlist(netlist) if c["ref"] not in NOT_A_PART]

    # Some schematic symbols carry no MPN field. When every sibling with the same
    # value and footprint agrees on one MPN, that is the same part by the user's own
    # grouping; assign it and print the assignment so it can be audited.
    inferred = []
    by_value_fp = {}
    for c in comps:
        if c["mpn"]:
            by_value_fp.setdefault((c["value"], c["footprint"]), set()).add(c["mpn"])
    for c in comps:
        if c["mpn"]:
            continue
        candidates = by_value_fp.get((c["value"], c["footprint"]), set())
        if len(candidates) == 1:
            c["mpn"] = next(iter(candidates))
            c["inferred"] = True
            inferred.append(f"{c['ref']} ({c['value']}, {c['footprint']}) -> {c['mpn']}")
    if inferred:
        print("Inferred MPN from an identical sibling (value + footprint):")
        for line in inferred:
            print("  -", line)

    aliased = []
    for c in comps:
        if c["mpn"]:
            continue
        alias = VALUE_ALIASES.get((c["value"], c["footprint"]))
        if alias:
            c["mpn"] = alias
            aliased.append(f"{c['ref']} ({c['value']}) -> {alias}")
    if aliased:
        print("Resolved MPN from the value table:")
        for line in aliased:
            print("  -", line)

    problems = []
    groups = OrderedDict()
    for c in comps:
        key = c["mpn"] or c["value"]
        groups.setdefault(key, []).append(c)

    rows = []
    for i, (key, members) in enumerate(sorted(groups.items()), start=1):
        part = PARTS.get(key)
        if part is None:
            problems.append(
                f"no PARTS entry for {key!r} (refs: {','.join(m['ref'] for m in members)})"
            )
            manufacturer, description = "", ""
        else:
            manufacturer, description = part
        fp = members[0]["footprint"]
        package, ptype = FOOTPRINT_MAP.get(fp, ("", ""))
        if not package:
            problems.append(f"no FOOTPRINT_MAP entry for {fp!r} ({members[0]['ref']})")
        rows.append(
            {
                "Line #": i,
                "Quantity Per Board": len(members),
                "Reference Designator": ",".join(sorted(m["ref"] for m in members)),
                "Part Number": key,
                "Part Description": description,
                "Package": package,
                "Type": ptype,
                "Manufacturer": manufacturer,
                "Manufacturer Part Number": key,
            }
        )

    header = [
        "Line #",
        "Quantity Per Board",
        "Reference Designator",
        "Part Number",
        "Part Description",
        "Package",
        "Type",
        "Manufacturer",
        "Manufacturer Part Number",
    ]
    if problem := [p for p in problems]:
        print("PROBLEMS:")
        for p in problem:
            print("  -", p)
    print(
        f"{len(rows)} BOM lines covering {sum(r['Quantity Per Board'] for r in rows)} placements "
        f"(excluded as PCB features: {sorted(NOT_A_PART)})"
    )
    if check_only:
        return 1 if problems else 0
    with open(out_path, "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=header, quoting=csv.QUOTE_MINIMAL)
        w.writeheader()
        w.writerows(rows)
    print("wrote", out_path)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
