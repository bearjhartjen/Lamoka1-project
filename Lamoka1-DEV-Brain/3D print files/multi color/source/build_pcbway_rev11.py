#!/usr/bin/env python3
"""Build the PCBWay-accurate, visually reliable Secret-DEV-Brain REV11 package."""

from __future__ import annotations

import argparse
import csv
import hashlib
import importlib.util
import json
import shutil
import sys
import textwrap
import zipfile
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path
from xml.etree import ElementTree as ET


def load_toolkit(path: Path):
    spec = importlib.util.spec_from_file_location("lamoka_mesh_toolkit", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"Unable to import mesh toolkit: {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def csv_contains_reference(path: Path, reference: str) -> bool:
    with path.open(newline="", encoding="utf-8-sig") as handle:
        for row in csv.reader(handle):
            for cell in row:
                references = {
                    item.strip() for item in cell.replace(";", ",").split(",")
                }
                if reference in references:
                    return True
    return False


def rectangle_perimeter(
    center_x: float,
    center_y: float,
    width: float,
    height: float,
) -> list[tuple[float, float]]:
    half_width = width / 2.0
    half_height = height / 2.0
    return [
        (center_x - half_width, center_y - half_height),
        (center_x + half_width, center_y - half_height),
        (center_x + half_width, center_y + half_height),
        (center_x - half_width, center_y + half_height),
    ]


def subdivided_rectangle_perimeter(
    center_x: float,
    center_y: float,
    width: float,
    height: float,
    segments_per_edge: int = 16,
) -> list[tuple[float, float]]:
    corners = rectangle_perimeter(center_x, center_y, width, height)
    points: list[tuple[float, float]] = []
    for edge in range(4):
        start = corners[edge]
        end = corners[(edge + 1) % 4]
        for step in range(segments_per_edge):
            fraction = step / segments_per_edge
            points.append(
                (
                    start[0] + (end[0] - start[0]) * fraction,
                    start[1] + (end[1] - start[1]) * fraction,
                )
            )
    return points


def overlaps(
    low: tuple[float, float, float],
    high: tuple[float, float, float],
    x_low: float,
    x_high: float,
    y_low: float,
    y_high: float,
) -> bool:
    return not (
        high[0] < x_low
        or low[0] > x_high
        or high[1] < y_low
        or low[1] > y_high
    )


def build_parts(core, source_obj: Path):
    source_vertices, source_faces = core.parse_obj(source_obj)
    blue = core.mesh_from_source_faces(
        "A1_Blue_NeoPixels_D2_D4_D6_D8_D10",
        "AMS_A1_BLUE",
        source_vertices,
        source_faces["AMS_A1_BLUE"],
    )
    red = core.mesh_from_source_faces(
        "A2_Red_NeoPixels_D1_D3_D5_D7_D9",
        "AMS_A2_RED",
        source_vertices,
        source_faces["AMS_A2_RED"],
    )
    black = core.mesh_from_source_faces(
        "A3_Black_Original_PCB_and_Components",
        "AMS_A3_BLACK",
        source_vertices,
        source_faces["AMS_A3_BLACK"],
    )
    silver = core.Mesh(
        name="A4_Silver_Original_Metal_and_Bare_J1_PTH_Pads",
        material="AMS_A4_SILVER",
    )

    source_components = core.connected_face_components(
        source_vertices,
        source_faces["AMS_A4_SILVER"],
    )
    cleanup_counts: Counter[str] = Counter()
    cleanup_faces: Counter[str] = Counter()
    retained_records: list[dict[str, object]] = []

    for component in source_components:
        low, high, size = core.source_face_bounds(source_vertices, component)

        # Replace the overly tessellated source export for J1 with clean,
        # dimensionally exact plated-through-hole geometry. PCBWay is not
        # installing a connector: J1 is excluded from both BOM and placement.
        if overlaps(low, high, 10.8, 44.5, 1.5, 4.25):
            disposition = "replaced_source_J1_with_clean_bare_PTH_geometry"
            keep = False
        else:
            exact_logo_or_text = (
                low[0] >= 47.8
                and high[1] <= 13.5
                and low[2] >= 1.60
                and high[2] <= 1.86
            )
            exact_usb_shell = (
                size[0] > 7.0
                and size[1] > 8.0
                and size[2] > 3.5
                and high[0] < 8.2
            )
            exact_led_body = (
                low[1] > 21.0
                and size[0] > 4.5
                and size[1] > 4.5
                and size[2] > 0.10
            )
            exact_button_body = (
                high[0] < 10.0
                and low[1] > 20.5
                and size[0] > 2.0
                and size[1] > 1.0
            )
            protected = (
                exact_logo_or_text
                or exact_usb_shell
                or exact_led_body
                or exact_button_body
            )

            # These 0.04 mm sheets are below even one 0.08 mm print layer.
            # They overlap the PCB surface and caused the random silver flecks
            # visible in the physical print. Removing them changes no readable
            # component body or placement.
            if (
                not protected
                and size[2] <= 0.061
                and max(size[0], size[1]) < 3.0
            ):
                disposition = "removed_sub_layer_silver_sheet"
                keep = False
            elif (
                not protected
                and min(size[0], size[1]) < 0.20
                and max(size[0], size[1]) < 0.70
                and size[2] < 0.80
            ):
                disposition = "removed_sub_nozzle_detached_sliver"
                keep = False
            else:
                disposition = "retained_original_geometry"
                keep = True

        cleanup_counts[disposition] += 1
        cleanup_faces[disposition] += len(component)
        if keep:
            silver.add(
                core.mesh_from_source_faces(
                    disposition,
                    "AMS_A4_SILVER",
                    source_vertices,
                    component,
                )
            )
            retained_records.append(
                {
                    "bounds_mm": [
                        [round(value, 5) for value in low],
                        [round(value, 5) for value in high],
                    ],
                    "triangles": len(component),
                }
            )

    # Exact production footprint and drill data:
    # - J1 is DNP and absent from PCBWay's BOM and pick-and-place files.
    # - 13 plated holes, 2.54 mm pitch.
    # - 1.00 mm drill, 1.70 mm copper pads.
    # - KiCad's visible top solder-mask openings are 2.20 mm for pin 1 and
    #   2.00 mm for pins 2-13.
    # - Pin 1 is square; pins 2-13 are round.
    #
    # The original black PCB mesh already contains the real 1.00 mm bores.
    # A flush overlapping silver volume can disappear beneath the black object
    # in a slicer. These rings therefore begin exactly at the PCB top and rise
    # two 0.08 mm layers. The tiny relief is the minimum reliable physical
    # representation of the essentially flush plated surface.
    pitch = 2.54
    copper_pad_diameter = 1.70
    round_mask_opening = 2.00
    square_mask_opening = 2.20
    drill_diameter = 1.00
    board_top = 1.60
    top_relief = 0.16
    pad_centers = [
        (12.655 + pitch * index, 2.92) for index in range(13)
    ]

    for index, (center_x, center_y) in enumerate(pad_centers, start=1):
        inner = core.circle_perimeter(
            center_x,
            center_y,
            drill_diameter / 2.0,
            64,
        )
        if index == 1:
            outer = subdivided_rectangle_perimeter(
                center_x,
                center_y,
                square_mask_opening,
                square_mask_opening,
                16,
            )
            shape = "square"
        else:
            outer = core.circle_perimeter(
                center_x,
                center_y,
                round_mask_opening / 2.0,
                64,
            )
            shape = "round"
        silver.add(
            core.annular_prism(
                f"J1_pad_{index:02d}_{shape}_visible_top_PTH_ring",
                "AMS_A4_SILVER",
                outer,
                inner,
                board_top,
                board_top + top_relief,
            )
        )

    cleanup = {
        "original_silver_connected_components": len(source_components),
        "component_dispositions": dict(cleanup_counts),
        "triangle_dispositions": dict(cleanup_faces),
        "retained_original_component_records": retained_records,
        "pcbway_J1_delivery_state": {
            "assembly_state": "DNP; no connector installed",
            "bom": "J1 absent",
            "pick_and_place": "J1 absent",
            "configuration": "13 bare plated through-holes",
            "pitch_mm": pitch,
            "copper_pad_outer_mm": copper_pad_diameter,
            "visible_round_mask_opening_mm": round_mask_opening,
            "visible_square_pin_1_mask_opening_mm": square_mask_opening,
            "drill_mm": drill_diameter,
            "pin_1_shape": "square",
            "pins_2_through_13_shape": "round",
            "top_relief_mm": top_relief,
            "geometry_concessions": (
                "0.16 mm two-layer top relief makes the silver survive slicing; "
                "the visible XY boundary follows the KiCad solder-mask opening"
            ),
            "print_representation": (
                "dedicated non-overlapping silver top rings; no male or female "
                "connector"
            ),
        },
    }
    return [blue, red, black, silver], cleanup


def combined_bounds(parts):
    lows = []
    highs = []
    for part in parts:
        low, high = part.bounds()
        lows.append(low)
        highs.append(high)
    return (
        tuple(min(low[axis] for low in lows) for axis in range(3)),
        tuple(max(high[axis] for high in highs) for axis in range(3)),
    )


def validate_3mf(path: Path, expected_build_items: int):
    namespace = {
        "m": "http://schemas.microsoft.com/3dmanufacturing/core/2015/02"
    }
    with zipfile.ZipFile(path) as archive:
        required = {"[Content_Types].xml", "_rels/.rels", "3D/3dmodel.model"}
        names = set(archive.namelist())
        model = ET.fromstring(archive.read("3D/3dmodel.model"))
    objects = model.findall(".//m:resources/m:object", namespace)
    items = model.findall(".//m:build/m:item", namespace)
    materials = [
        item.attrib["name"]
        for item in model.findall(".//m:basematerials/m:base", namespace)
    ]
    return {
        "required_entries_present": required.issubset(names),
        "xml_parse_ok": True,
        "resource_object_count": len(objects),
        "build_item_count": len(items),
        "build_item_count_ok": len(items) == expected_build_items,
        "materials": materials,
    }


def write_readme(path: Path):
    path.write_text(
        textwrap.dedent(
            """\
            # Secret-DEV-Brain — PCBWAY ACCURATE REV11

            This model represents the assembled Lamoka1-DEV-Brain that PCBWay
            is actually delivering. It is a refinement of the original
            realistic multicolor model, not a visual redesign. The original
            USB-C, button, RP2354A and other component bodies, exact KiCad
            flamingo/cactus logo, wordmark, and recessed red/blue NeoPixels
            remain intact.

            ## Preferred file

            Open `Secret-DEV-Brain_PCBWAY_ACCURATE_REV11.3mf` in Bambu Studio.
            It is an editable four-part model and contains no pre-sliced G-code.

            The package also includes an editable three-up 3MF, OBJ/MTL,
            four aligned fallback STLs, previews, validation, checksums, and
            reproducible source.

            ## AMS order

            1. A1 — Blue
            2. A2 — Red
            3. A3 — Black
            4. A4 — Silver

            ## PCBWay-delivered J1

            J1 is DNP. PCBWay installs no male header, female header, or other
            connector. The model contains exactly the thirteen bare plated
            through-holes that remain on the manufactured PCB.

            - 2.54 mm pitch.
            - 1.70 mm copper-pad diameter/width.
            - 1.00 mm finished-model bore matching the production drill.
            - Pin 1 has the KiCad square pad; pins 2–13 are round.
            - The bores pass through the full PCB thickness.
            - J1 is absent from the PCBWay BOM and pick-and-place files.
            - The silver top rings use the real KiCad solder-mask openings:
              2.20 mm square for pin 1 and 2.00 mm round for pins 2–13.
            - The rings stand 0.16 mm above the black surface—two layers at the
              recommended profile—so the slicer cannot hide the silver.

            ## Physical-print cleanup

            The previous print exposed 0.04 mm silver sheets and detached
            sub-nozzle slivers that are smaller than one selected print layer.
            Those non-readable artifacts were removed. Actual component bodies,
            packages, logo, text, USB-C shell, button, and LED housings were not
            restyled or enlarged.

            ## P1S settings

            - 0.4 mm nozzle.
            - 0.08 mm layer height.
            - Arachne wall generator.
            - Slow small perimeters to about 15–20 mm/s for clean J1 rings.
            - Prime tower on; use calibrated black-to-silver flushing.
            - Support only beneath the USB-C shell as with the successful print.
            - No support is needed at J1 because it is a bare hole row.
            """
        ),
        encoding="utf-8",
    )


def write_accuracy_notes(path: Path, cleanup: dict[str, object]):
    dispositions = cleanup["component_dispositions"]
    path.write_text(
        textwrap.dedent(
            f"""\
            # PCBWAY ACCURATE REV11 decisions

            ## Geometry retained

            The starting point is the exact repository multicolor OBJ. Blue and
            red meshes are byte-for-geometry unchanged. The original black PCB
            and component mesh—including its real drilled bores—is unchanged.
            The original silver geometry is retained except for the overly
            tessellated J1 export and print artifacts listed below.

            ## J1 source of truth

            - PCB footprint attribute: through-hole, excluded from BOM, and
              excluded from position files.
            - PCBWay BOM: no J1 row.
            - PCBWay pick-and-place: no J1 row.
            - PCBWay quotation: no J1 line item.
            - Production drill report: exactly thirteen 1.000 mm PTH holes.
            - KiCad pads: 1.70 mm, with square pin 1 and round pins 2–13.

            The replacement geometry keeps the 1.00 mm bores and uses the real
            2.20/2.00 mm solder-mask openings for the visible silver boundary.
            A 0.16 mm top relief is deliberately added because a co-planar,
            overlapping silver ring was being hidden by the black PCB volume in
            the slicer. There is no connector body and there are no projecting
            pins.

            ## Removed source islands

            - Overly tessellated source J1 shells replaced:
              {dispositions.get('replaced_source_J1_with_clean_bare_PTH_geometry', 0)}
            - 0.04 mm sub-layer sheets removed:
              {dispositions.get('removed_sub_layer_silver_sheet', 0)}
            - Detached sub-nozzle slivers removed:
              {dispositions.get('removed_sub_nozzle_detached_sliver', 0)}
            - Original silver component shells retained:
              {dispositions.get('retained_original_geometry', 0)}

            These removals target the silver flecks and strings seen in the
            physical print without simplifying the recognizable board.
            """
        ),
        encoding="utf-8",
    )


def write_manifest(root: Path, manifest_path: Path):
    lines = []
    for file_path in sorted(root.rglob("*")):
        if file_path.is_file() and file_path != manifest_path:
            lines.append(
                f"{sha256(file_path)}  {file_path.relative_to(root).as_posix()}"
            )
    manifest_path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def build_package(
    core,
    toolkit_path: Path,
    source_obj: Path,
    source_mtl: Path,
    source_pcb: Path,
    pcbway_bom: Path,
    pcbway_pick_and_place: Path,
    fabrication_zip: Path,
    output_root: Path,
):
    if output_root.exists():
        shutil.rmtree(output_root)
    output_root.mkdir(parents=True)
    parts_dir = output_root / "parts"
    source_dir = output_root / "source"
    production_dir = source_dir / "production_evidence"
    parts_dir.mkdir()
    source_dir.mkdir()
    production_dir.mkdir()

    pcb_text = source_pcb.read_text(encoding="utf-8")
    j1_marker = '(property "Reference" "J1"'
    j1_reference_position = pcb_text.find(j1_marker)
    if j1_reference_position < 0:
        raise ValueError("Could not locate J1 in the production KiCad PCB.")
    j1_footprint_start = pcb_text.rfind(
        "\n\t(footprint",
        0,
        j1_reference_position,
    )
    j1_footprint_end = pcb_text.find(
        "\n\t(footprint",
        j1_reference_position + len(j1_marker),
    )
    if j1_footprint_end < 0:
        j1_footprint_end = len(pcb_text)
    j1_footprint_text = pcb_text[j1_footprint_start:j1_footprint_end]
    with zipfile.ZipFile(fabrication_zip) as archive:
        drill_report = archive.read("Lamoka1-DEV-board-drl.rpt")
    drill_report_text = drill_report.decode("utf-8")
    production_evidence = {
        "J1_present_in_BOM": csv_contains_reference(pcbway_bom, "J1"),
        "J1_present_in_pick_and_place": csv_contains_reference(
            pcbway_pick_and_place,
            "J1",
        ),
        "kicad_J1_excluded_from_bom_and_position": (
            '(attr through_hole exclude_from_pos_files exclude_from_bom)'
            in j1_footprint_text
        ),
        "kicad_J1_has_exact_pad_and_drill_geometry": (
            j1_footprint_text.count("(size 1.7 1.7)") == 13
            and j1_footprint_text.count("(drill 1)") == 13
            and '(pad "1" thru_hole rect' in j1_footprint_text
            and all(
                f'(pad "{index}" thru_hole circle' in j1_footprint_text
                for index in range(2, 14)
            )
        ),
        "drill_report_has_13_x_1mm_PTH": (
            "T3  1.000mm  0.0394\"  (13 holes)" in drill_report_text
        ),
    }
    expected_evidence = {
        "J1_present_in_BOM": False,
        "J1_present_in_pick_and_place": False,
        "kicad_J1_excluded_from_bom_and_position": True,
        "kicad_J1_has_exact_pad_and_drill_geometry": True,
        "drill_report_has_13_x_1mm_PTH": True,
    }
    if production_evidence != expected_evidence:
        raise ValueError(
            "Production files no longer prove the expected bare/DNP J1 state: "
            f"{production_evidence}"
        )

    parts, cleanup = build_parts(core, source_obj)
    base_name = "Secret-DEV-Brain_PCBWAY_ACCURATE_REV11"
    obj_path = output_root / f"{base_name}.obj"
    mtl_path = output_root / f"{base_name}.mtl"
    one_3mf = output_root / f"{base_name}.3mf"
    three_3mf = (
        output_root / "Secret-DEV-Brain_3-up_PCBWAY_ACCURATE_REV11.3mf"
    )

    core.write_mtl(mtl_path)
    core.write_obj(obj_path, mtl_path.name, parts)
    core.write_3mf(one_3mf, parts)
    core.write_3mf(
        three_3mf,
        parts,
        copies=((0.0, 0.0, 0.0), (0.0, 40.0, 0.0), (0.0, 80.0, 0.0)),
    )

    stl_names = [
        "01_A1_Blue_NeoPixels.stl",
        "02_A2_Red_NeoPixels.stl",
        "03_A3_Black_Original_PCB_and_Components.stl",
        "04_A4_Silver_Original_Metal_and_Bare_J1_PTH_Pads.stl",
    ]
    for name, part in zip(stl_names, parts):
        core.write_binary_stl(parts_dir / name, part)

    top_preview = output_root / "PREVIEW_TOP_PCBWAY_ACCURATE_REV11.png"
    angle_preview = output_root / "PREVIEW_ANGLE_PCBWAY_ACCURATE_REV11.png"
    core.render_preview(
        top_preview,
        parts,
        camera=(35.6225, 15.0, 170.0),
        target=(35.6225, 15.0, 2.0),
        camera_up_hint=(0.0, 1.0, 0.0),
        output_size=(1600, 760),
    )
    core.render_preview(
        angle_preview,
        parts,
        camera=(100.0, -112.0, 218.0),
        target=(35.6225, 15.0, 2.4),
        camera_up_hint=(0.0, 0.0, 1.0),
        output_size=(1600, 900),
    )

    write_readme(output_root / "README_SLICING.md")
    write_accuracy_notes(
        output_root / "PCBWAY_ACCURACY_NOTES_REV11.md",
        cleanup,
    )

    shutil.copy2(source_obj, source_dir / "Secret-DEV-Brain_Multicolor_BASE.obj")
    shutil.copy2(source_mtl, source_dir / "Secret-DEV-Brain_Multicolor_BASE.mtl")
    shutil.copy2(toolkit_path, source_dir / "mesh_toolkit.py")
    shutil.copy2(Path(__file__), source_dir / Path(__file__).name)
    shutil.copy2(
        source_pcb,
        production_dir / "Lamoka1-DEV-board.kicad_pcb",
    )
    shutil.copy2(
        pcbway_bom,
        production_dir / "Lamoka1-DEV-board_BOM.csv",
    )
    shutil.copy2(
        pcbway_pick_and_place,
        production_dir / "Lamoka1-DEV-board_Pick_and_Place.csv",
    )
    (
        production_dir / "Lamoka1-DEV-board_drl.rpt"
    ).write_bytes(drill_report)

    low, high = combined_bounds(parts)
    part_stats = {}
    for part in parts:
        part_low, part_high = part.bounds()
        part_stats[part.name] = {
            "material": part.material,
            "vertices": len(part.vertices),
            "triangles": len(part.faces),
            "connected_shells": part.component_count(),
            "bounds_mm": [
                [round(value, 6) for value in part_low],
                [round(value, 6) for value in part_high],
            ],
            "edge_stats": part.edge_stats(),
        }

    validation = {
        "model": "Secret-DEV-Brain PCBWAY ACCURATE REV11",
        "generated_utc": datetime.now(timezone.utc).isoformat().replace(
            "+00:00", "Z"
        ),
        "source_sha256": {
            "repository_multicolor_obj": sha256(source_obj),
            "repository_multicolor_mtl": sha256(source_mtl),
            "production_kicad_pcb": sha256(source_pcb),
            "pcbway_bom_csv": sha256(pcbway_bom),
            "pcbway_pick_and_place_csv": sha256(pcbway_pick_and_place),
            "fabrication_archive": sha256(fabrication_zip),
        },
        "pcbway_delivery_evidence": production_evidence,
        "dimensions_mm": {
            "bbox": [
                [round(value, 6) for value in low],
                [round(value, 6) for value in high],
            ],
            "size": [round(high[index] - low[index], 6) for index in range(3)],
        },
        "ams_order": ["A1 Blue", "A2 Red", "A3 Black", "A4 Silver"],
        "accuracy_and_cleanup": cleanup,
        "parts": part_stats,
        "containers": {
            one_3mf.name: validate_3mf(one_3mf, 1),
            three_3mf.name: validate_3mf(three_3mf, 3),
        },
        "print_limit_disclosures": [
            "J1 is bare: no male header, female header, or projecting pins.",
            "J1 keeps the production 1.00 mm bore and uses the KiCad 2.20/2.00 mm solder-mask openings as its visible silver boundary.",
            "J1 rings rise 0.16 mm above the black PCB so two complete 0.08 mm silver layers remain visible after slicing.",
            "Silver represents the exposed plated pad within the fixed four-color AMS set.",
            "No recognizable original component body was stylized or enlarged.",
        ],
    }
    (output_root / "MODEL_VALIDATION_REV11.json").write_text(
        json.dumps(validation, indent=2) + "\n",
        encoding="utf-8",
    )
    write_manifest(output_root, output_root / "MANIFEST_SHA256.txt")

    zip_path = output_root.with_suffix(".zip")
    if zip_path.exists():
        zip_path.unlink()
    with zipfile.ZipFile(
        zip_path,
        "w",
        compression=zipfile.ZIP_DEFLATED,
        compresslevel=9,
    ) as archive:
        for file_path in sorted(output_root.rglob("*")):
            if file_path.is_file():
                archive.write(
                    file_path,
                    (
                        Path(output_root.name)
                        / file_path.relative_to(output_root)
                    ).as_posix(),
                )
    return zip_path


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--toolkit", type=Path, required=True)
    parser.add_argument("--source-obj", type=Path, required=True)
    parser.add_argument("--source-mtl", type=Path, required=True)
    parser.add_argument("--source-pcb", type=Path, required=True)
    parser.add_argument("--pcbway-bom", type=Path, required=True)
    parser.add_argument("--pcbway-pick-and-place", type=Path, required=True)
    parser.add_argument("--fabrication-zip", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    arguments = parser.parse_args()
    core = load_toolkit(arguments.toolkit.resolve())
    output = build_package(
        core,
        arguments.toolkit.resolve(),
        arguments.source_obj.resolve(),
        arguments.source_mtl.resolve(),
        arguments.source_pcb.resolve(),
        arguments.pcbway_bom.resolve(),
        arguments.pcbway_pick_and_place.resolve(),
        arguments.fabrication_zip.resolve(),
        arguments.output_root.resolve(),
    )
    print(output)


if __name__ == "__main__":
    main()
