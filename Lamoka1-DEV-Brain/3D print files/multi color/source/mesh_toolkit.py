#!/usr/bin/env python3
"""
Build the display-optimized four-color Lamoka1 Secret-DEV-Brain REV8 model.

The source OBJ is the exact multicolor model committed to the Lamoka1-project
repository on 2026-07-23.  REV8 deliberately keeps the 1:1 outline, USB-C
shell, RP2354A/component placement, exact KiCad flamingo/cactus polygons, and
Secret-DEV-Brain text while replacing nozzle-hostile decorative geometry.

The script only uses the Python standard library for model generation.
Pillow is used for the optional PNG previews.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
import os
import shutil
import struct
import textwrap
import zipfile
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from datetime import datetime, timezone
from pathlib import Path
from typing import Iterable, Sequence
from xml.etree import ElementTree as ET


MATERIALS = [
    ("AMS_A1_BLUE", "#1457D9", "A1_Blue_NeoPixels_D2_D4_D6_D8_D10"),
    ("AMS_A2_RED", "#D71920", "A2_Red_NeoPixels_D1_D3_D5_D7_D9"),
    ("AMS_A3_BLACK", "#101010", "A3_Black_PCB_Components_and_Supports"),
    ("AMS_A4_SILVER", "#B7BCC2", "A4_Silver_USB_Logo_Text_LED_Frames_and_GPIO"),
]

BOARD_TOP_Z = 1.61
LED_CENTERS = [(13.535 + 6.0 * index, 24.32) for index in range(10)]
HEADER_CENTERS = [(12.655 + 2.54 * index, 2.92) for index in range(13)]


def fmt(value: float) -> str:
    text = f"{value:.6f}".rstrip("0").rstrip(".")
    return "0" if text in {"", "-0"} else text


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def add3(a: Sequence[float], b: Sequence[float]) -> tuple[float, float, float]:
    return (a[0] + b[0], a[1] + b[1], a[2] + b[2])


def sub3(a: Sequence[float], b: Sequence[float]) -> tuple[float, float, float]:
    return (a[0] - b[0], a[1] - b[1], a[2] - b[2])


def mul3(a: Sequence[float], factor: float) -> tuple[float, float, float]:
    return (a[0] * factor, a[1] * factor, a[2] * factor)


def dot3(a: Sequence[float], b: Sequence[float]) -> float:
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]


def cross3(a: Sequence[float], b: Sequence[float]) -> tuple[float, float, float]:
    return (
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    )


def length3(a: Sequence[float]) -> float:
    return math.sqrt(dot3(a, a))


def unit3(a: Sequence[float]) -> tuple[float, float, float]:
    length = length3(a)
    if length <= 1e-15:
        return (0.0, 0.0, 0.0)
    return mul3(a, 1.0 / length)


@dataclass
class Mesh:
    name: str
    material: str
    vertices: list[tuple[float, float, float]] = field(default_factory=list)
    faces: list[tuple[int, int, int]] = field(default_factory=list)

    def add(self, other: "Mesh") -> None:
        offset = len(self.vertices)
        self.vertices.extend(other.vertices)
        self.faces.extend(
            (a + offset, b + offset, c + offset) for a, b, c in other.faces
        )

    def bounds(self) -> tuple[tuple[float, float, float], tuple[float, float, float]]:
        if not self.vertices:
            return ((0.0, 0.0, 0.0), (0.0, 0.0, 0.0))
        return (
            tuple(min(vertex[axis] for vertex in self.vertices) for axis in range(3)),
            tuple(max(vertex[axis] for vertex in self.vertices) for axis in range(3)),
        )

    def component_count(self) -> int:
        if not self.faces:
            return 0
        parent = list(range(len(self.faces)))
        sizes = [1] * len(self.faces)

        def find(index: int) -> int:
            while parent[index] != index:
                parent[index] = parent[parent[index]]
                index = parent[index]
            return index

        def union(left: int, right: int) -> None:
            left_root = find(left)
            right_root = find(right)
            if left_root == right_root:
                return
            if sizes[left_root] < sizes[right_root]:
                left_root, right_root = right_root, left_root
            parent[right_root] = left_root
            sizes[left_root] += sizes[right_root]

        first_face_for_vertex: dict[int, int] = {}
        for face_index, face in enumerate(self.faces):
            for vertex_index in face:
                if vertex_index in first_face_for_vertex:
                    union(face_index, first_face_for_vertex[vertex_index])
                else:
                    first_face_for_vertex[vertex_index] = face_index
        return len({find(index) for index in range(len(self.faces))})

    def edge_stats(self) -> dict[str, int]:
        multiplicities: Counter[tuple[int, int]] = Counter()
        for a, b, c in self.faces:
            multiplicities[tuple(sorted((a, b)))] += 1
            multiplicities[tuple(sorted((b, c)))] += 1
            multiplicities[tuple(sorted((c, a)))] += 1
        return {
            "total_unique_edges": len(multiplicities),
            "boundary_edges": sum(1 for count in multiplicities.values() if count == 1),
            "two_face_edges": sum(1 for count in multiplicities.values() if count == 2),
            "nonmanifold_edges": sum(1 for count in multiplicities.values() if count > 2),
        }


def parse_obj(
    path: Path,
) -> tuple[
    list[tuple[float, float, float] | None],
    dict[str, list[tuple[int, int, int]]],
]:
    vertices: list[tuple[float, float, float] | None] = [None]
    faces: dict[str, list[tuple[int, int, int]]] = defaultdict(list)
    material: str | None = None
    with path.open("r", encoding="utf-8", errors="replace") as handle:
        for line in handle:
            if line.startswith("v "):
                tokens = line.split()
                vertices.append(tuple(float(value) for value in tokens[1:4]))
            elif line.startswith("usemtl "):
                material = line.split(maxsplit=1)[1].strip()
            elif line.startswith("f "):
                if material is None:
                    raise ValueError("OBJ face appeared before usemtl")
                indexes = []
                for token in line.split()[1:]:
                    index = int(token.split("/")[0])
                    if index < 0:
                        index = len(vertices) + index
                    indexes.append(index)
                for offset in range(1, len(indexes) - 1):
                    triangle = (indexes[0], indexes[offset], indexes[offset + 1])
                    if len(set(triangle)) == 3:
                        faces[material].append(triangle)
    return vertices, faces


def mesh_from_source_faces(
    name: str,
    material: str,
    source_vertices: Sequence[tuple[float, float, float] | None],
    source_faces: Iterable[tuple[int, int, int]],
) -> Mesh:
    mesh = Mesh(name=name, material=material)
    remap: dict[int, int] = {}
    for face in source_faces:
        new_face = []
        for source_index in face:
            if source_index not in remap:
                vertex = source_vertices[source_index]
                if vertex is None:
                    raise ValueError(f"Missing OBJ vertex {source_index}")
                remap[source_index] = len(mesh.vertices)
                mesh.vertices.append(vertex)
            new_face.append(remap[source_index])
        if len(set(new_face)) == 3:
            mesh.faces.append(tuple(new_face))
    return mesh


def connected_face_components(
    source_vertices: Sequence[tuple[float, float, float] | None],
    faces: Sequence[tuple[int, int, int]],
) -> list[list[tuple[int, int, int]]]:
    parent = list(range(len(faces)))
    sizes = [1] * len(faces)

    def find(index: int) -> int:
        while parent[index] != index:
            parent[index] = parent[parent[index]]
            index = parent[index]
        return index

    def union(left: int, right: int) -> None:
        left_root = find(left)
        right_root = find(right)
        if left_root == right_root:
            return
        if sizes[left_root] < sizes[right_root]:
            left_root, right_root = right_root, left_root
        parent[right_root] = left_root
        sizes[left_root] += sizes[right_root]

    first_face_for_point: dict[tuple[float, float, float], int] = {}
    for face_index, face in enumerate(faces):
        for vertex_index in face:
            vertex = source_vertices[vertex_index]
            if vertex is None:
                continue
            point = tuple(round(value, 5) for value in vertex)
            if point in first_face_for_point:
                union(face_index, first_face_for_point[point])
            else:
                first_face_for_point[point] = face_index

    components: dict[int, list[tuple[int, int, int]]] = defaultdict(list)
    for face_index, face in enumerate(faces):
        components[find(face_index)].append(face)
    return list(components.values())


def source_face_bounds(
    source_vertices: Sequence[tuple[float, float, float] | None],
    faces: Iterable[tuple[int, int, int]],
) -> tuple[
    tuple[float, float, float],
    tuple[float, float, float],
    tuple[float, float, float],
]:
    points = [
        source_vertices[vertex_index]
        for face in faces
        for vertex_index in face
        if source_vertices[vertex_index] is not None
    ]
    low = tuple(min(point[axis] for point in points) for axis in range(3))
    high = tuple(max(point[axis] for point in points) for axis in range(3))
    size = tuple(high[axis] - low[axis] for axis in range(3))
    return low, high, size


def superellipse_perimeter(
    center_x: float,
    center_y: float,
    width: float,
    height: float,
    power: float,
    segments: int = 64,
) -> list[tuple[float, float]]:
    half_width = width / 2.0
    half_height = height / 2.0
    exponent = 2.0 / power
    points = []
    for index in range(segments):
        angle = 2.0 * math.pi * index / segments
        cosine = math.cos(angle)
        sine = math.sin(angle)
        x = math.copysign(abs(cosine) ** exponent, cosine) * half_width
        y = math.copysign(abs(sine) ** exponent, sine) * half_height
        points.append((center_x + x, center_y + y))
    return points


def circle_perimeter(
    center_x: float,
    center_y: float,
    radius: float,
    segments: int = 64,
) -> list[tuple[float, float]]:
    return [
        (
            center_x + radius * math.cos(2.0 * math.pi * index / segments),
            center_y + radius * math.sin(2.0 * math.pi * index / segments),
        )
        for index in range(segments)
    ]


def solid_prism(
    name: str,
    material: str,
    perimeter: Sequence[tuple[float, float]],
    z_low: float,
    z_high: float,
) -> Mesh:
    mesh = Mesh(name=name, material=material)
    count = len(perimeter)
    mesh.vertices.extend((x, y, z_low) for x, y in perimeter)
    mesh.vertices.extend((x, y, z_high) for x, y in perimeter)
    center_x = sum(point[0] for point in perimeter) / count
    center_y = sum(point[1] for point in perimeter) / count
    bottom_center = len(mesh.vertices)
    mesh.vertices.append((center_x, center_y, z_low))
    top_center = len(mesh.vertices)
    mesh.vertices.append((center_x, center_y, z_high))

    for index in range(count):
        next_index = (index + 1) % count
        bottom_i = index
        bottom_j = next_index
        top_i = index + count
        top_j = next_index + count
        mesh.faces.append((bottom_center, bottom_j, bottom_i))
        mesh.faces.append((top_center, top_i, top_j))
        mesh.faces.append((bottom_i, bottom_j, top_j))
        mesh.faces.append((bottom_i, top_j, top_i))
    return mesh


def annular_prism(
    name: str,
    material: str,
    outer: Sequence[tuple[float, float]],
    inner: Sequence[tuple[float, float]],
    z_low: float,
    z_high: float,
) -> Mesh:
    if len(outer) != len(inner):
        raise ValueError("Annular boundaries must have equal vertex counts")
    mesh = Mesh(name=name, material=material)
    count = len(outer)
    mesh.vertices.extend((x, y, z_low) for x, y in outer)
    mesh.vertices.extend((x, y, z_low) for x, y in inner)
    mesh.vertices.extend((x, y, z_high) for x, y in outer)
    mesh.vertices.extend((x, y, z_high) for x, y in inner)

    outer_bottom = 0
    inner_bottom = count
    outer_top = count * 2
    inner_top = count * 3

    for index in range(count):
        next_index = (index + 1) % count
        ob_i = outer_bottom + index
        ob_j = outer_bottom + next_index
        ib_i = inner_bottom + index
        ib_j = inner_bottom + next_index
        ot_i = outer_top + index
        ot_j = outer_top + next_index
        it_i = inner_top + index
        it_j = inner_top + next_index

        mesh.faces.extend(
            [
                (ot_i, ot_j, it_j),
                (ot_i, it_j, it_i),
                (ob_i, ib_j, ob_j),
                (ob_i, ib_i, ib_j),
                (ob_i, ob_j, ot_j),
                (ob_i, ot_j, ot_i),
                (ib_i, it_i, it_j),
                (ib_i, it_j, ib_j),
            ]
        )
    return mesh


def select_silver_source_components(
    source_vertices: Sequence[tuple[float, float, float] | None],
    silver_faces: Sequence[tuple[int, int, int]],
) -> tuple[Mesh, list[dict[str, object]], int]:
    components = connected_face_components(source_vertices, silver_faces)
    output = Mesh(
        name="A4_Silver_USB_Logo_Text_LED_Frames_and_GPIO",
        material="AMS_A4_SILVER",
    )
    retained: list[dict[str, object]] = []

    for component in components:
        low, high, size = source_face_bounds(source_vertices, component)
        reason: str | None = None

        # Preserve the exact USB-C shell and its four structural board tabs,
        # but deliberately drop its sub-nozzle decorative lettering/contact
        # slivers.  The user confirmed the shell/support behavior is correct.
        if size[0] > 7.0 and size[1] > 8.0 and size[2] > 3.5 and high[0] < 8.2:
            reason = "exact USB-C shell"
        elif (
            high[0] < 9.0
            and low[1] > 9.5
            and high[1] < 20.1
            and size[2] > 1.0
            and size[0] > 1.2
            and size[1] > 0.8
        ):
            reason = "USB-C structural tab"

        # Exact, already-strengthened KiCad logo polygons and the established
        # Secret-DEV-Brain wordmark from the previous accepted model.
        elif (
            low[0] >= 47.8
            and high[1] <= 13.3
            and low[2] >= 1.60
            and high[2] <= 1.86
        ):
            reason = "exact logo or Secret-DEV-Brain text"

        # Retain only two deliberately readable metal component accents in the
        # center of the board.  Everything smaller becomes black-on-black.
        elif (
            low[0] >= 27.3
            and high[0] <= 31.0
            and low[1] >= 10.9
            and high[1] <= 13.9
            and high[2] > 2.2
            and size[0] > 1.5
            and size[1] > 1.5
        ):
            reason = "large crystal package"
        elif (
            low[0] >= 38.8
            and high[0] <= 41.0
            and low[1] >= 14.5
            and high[1] <= 18.2
            and high[2] > 3.0
            and size[0] > 1.4
            and size[1] > 2.8
        ):
            reason = "large capacitor package"

        if reason is not None:
            component_mesh = mesh_from_source_faces(
                reason,
                "AMS_A4_SILVER",
                source_vertices,
                component,
            )
            output.add(component_mesh)
            retained.append(
                {
                    "reason": reason,
                    "triangle_count": len(component),
                    "bounds": [
                        [round(value, 5) for value in low],
                        [round(value, 5) for value in high],
                    ],
                }
            )

    return output, retained, len(components)


def build_parts(
    source_obj: Path,
) -> tuple[list[Mesh], dict[str, object]]:
    source_vertices, source_faces = parse_obj(source_obj)
    required_materials = {material for material, _, _ in MATERIALS}
    missing = required_materials - set(source_faces)
    if missing:
        raise ValueError(f"Source OBJ is missing materials: {sorted(missing)}")

    black = mesh_from_source_faces(
        "A3_Black_PCB_Components_and_Supports",
        "AMS_A3_BLACK",
        source_vertices,
        source_faces["AMS_A3_BLACK"],
    )
    silver, retained_source, original_silver_components = (
        select_silver_source_components(
            source_vertices,
            source_faces["AMS_A4_SILVER"],
        )
    )
    blue = Mesh(
        name="A1_Blue_NeoPixels_D2_D4_D6_D8_D10",
        material="AMS_A1_BLUE",
    )
    red = Mesh(
        name="A2_Red_NeoPixels_D1_D3_D5_D7_D9",
        material="AMS_A2_RED",
    )

    # Clean NeoPixel hero row:
    # - exact established 6 mm center pitch and alternating color order
    # - 5.0 mm superellipse body (no printable lead droplets/under-plates)
    # - 0.70 mm minimum frame width for a 0.4 mm nozzle
    # - black support pedestal beneath each colored optical insert
    for index, (center_x, center_y) in enumerate(LED_CENTERS, start=1):
        outer = superellipse_perimeter(center_x, center_y, 5.0, 5.0, 6.0, 64)
        inner = circle_perimeter(center_x, center_y, 1.80, 64)
        silver.add(
            annular_prism(
                f"D{index}_clean_silver_housing",
                "AMS_A4_SILVER",
                outer,
                inner,
                BOARD_TOP_Z,
                3.17,
            )
        )
        black.add(
            solid_prism(
                f"D{index}_black_lens_support",
                "AMS_A3_BLACK",
                circle_perimeter(center_x, center_y, 1.66, 48),
                BOARD_TOP_Z,
                2.82,
            )
        )
        lens = solid_prism(
            f"D{index}_{'red' if index % 2 else 'blue'}_optical_insert",
            "AMS_A2_RED" if index % 2 else "AMS_A1_BLUE",
            circle_perimeter(center_x, center_y, 1.64, 64),
            2.82,
            2.98,
        )
        (red if index % 2 else blue).add(lens)

    # Print-safe GPIO annular pads.  The prior source used an approximately
    # 0.15 mm radial ring; REV8 uses 0.63 mm and retains a square pin-1 pad.
    for index, (center_x, center_y) in enumerate(HEADER_CENTERS, start=1):
        inner = circle_perimeter(center_x, center_y, 0.42, 48)
        if index == 1:
            outer = superellipse_perimeter(
                center_x,
                center_y,
                2.20,
                2.20,
                12.0,
                48,
            )
        else:
            outer = circle_perimeter(center_x, center_y, 1.05, 48)
        silver.add(
            annular_prism(
                f"J1_pin_{index:02d}_annular_pad",
                "AMS_A4_SILVER",
                outer,
                inner,
                BOARD_TOP_Z,
                1.85,
            )
        )
        black.add(
            solid_prism(
                f"J1_pin_{index:02d}_dark_hole",
                "AMS_A3_BLACK",
                circle_perimeter(center_x, center_y, 0.42, 32),
                0.0,
                1.85,
            )
        )

    # Clean visual proxy for the tactile button.  It occupies the exact prior
    # switch envelope but avoids the tiny detached leads visible in the print.
    silver.add(
        solid_prism(
            "SW1_clean_silver_housing",
            "AMS_A4_SILVER",
            superellipse_perimeter(5.985, 23.12, 5.80, 3.60, 6.0, 48),
            BOARD_TOP_Z,
            2.90,
        )
    )
    black.add(
        solid_prism(
            "SW1_black_actuator",
            "AMS_A3_BLACK",
            superellipse_perimeter(5.985, 23.12, 2.80, 1.15, 6.0, 40),
            2.90,
            3.45,
        )
    )

    parts = [blue, red, black, silver]
    metadata = {
        "original_silver_component_count": original_silver_components,
        "retained_source_silver_component_count": len(retained_source),
        "retained_source_silver_components": retained_source,
        "new_silver_shells": {
            "clean_neopixel_housings": 10,
            "gpio_annular_pads": 13,
            "clean_tactile_button_housing": 1,
        },
    }
    return parts, metadata


def write_mtl(path: Path) -> None:
    lines = [
        "# Secret-DEV-Brain DISPLAY REV8 four-color material library",
        "# AMS order: A1 Blue, A2 Red, A3 Black, A4 Silver",
        "",
    ]
    for material, color, _ in MATERIALS:
        red = int(color[1:3], 16) / 255.0
        green = int(color[3:5], 16) / 255.0
        blue = int(color[5:7], 16) / 255.0
        lines.extend(
            [
                f"newmtl {material}",
                f"Kd {red:.6f} {green:.6f} {blue:.6f}",
                "Ka 0.000000 0.000000 0.000000",
                "Ks 0.150000 0.150000 0.150000",
                "Ns 20.000000",
                "d 1.000000",
                "illum 2",
                "",
            ]
        )
    path.write_text("\n".join(lines), encoding="utf-8")


def write_obj(path: Path, mtl_name: str, parts: Sequence[Mesh]) -> None:
    with path.open("w", encoding="utf-8", newline="\n") as handle:
        handle.write("# Secret-DEV-Brain DISPLAY REV8\n")
        handle.write("# 1:1 visual print model; units are millimeters\n")
        handle.write(f"mtllib {mtl_name}\n")
        vertex_offset = 1
        for part in parts:
            handle.write(f"o {part.name}\n")
            handle.write(f"g {part.name}\n")
            handle.write(f"usemtl {part.material}\n")
            for x, y, z in part.vertices:
                handle.write(f"v {fmt(x)} {fmt(y)} {fmt(z)}\n")
            for a, b, c in part.faces:
                handle.write(
                    f"f {a + vertex_offset} {b + vertex_offset} {c + vertex_offset}\n"
                )
            vertex_offset += len(part.vertices)


def triangle_normal(
    a: Sequence[float],
    b: Sequence[float],
    c: Sequence[float],
) -> tuple[float, float, float]:
    return unit3(cross3(sub3(b, a), sub3(c, a)))


def write_binary_stl(path: Path, mesh: Mesh) -> None:
    header_text = f"Secret-DEV-Brain REV8 {mesh.name}".encode("ascii", "replace")[:80]
    header = header_text + b"\0" * (80 - len(header_text))
    with path.open("wb") as handle:
        handle.write(header)
        handle.write(struct.pack("<I", len(mesh.faces)))
        for face in mesh.faces:
            a, b, c = (mesh.vertices[index] for index in face)
            normal = triangle_normal(a, b, c)
            handle.write(
                struct.pack(
                    "<12fH",
                    normal[0],
                    normal[1],
                    normal[2],
                    a[0],
                    a[1],
                    a[2],
                    b[0],
                    b[1],
                    b[2],
                    c[0],
                    c[1],
                    c[2],
                    0,
                )
            )


def xml_bytes(element: ET.Element) -> bytes:
    return ET.tostring(element, encoding="utf-8", xml_declaration=True)


def zip_write_bytes(
    archive: zipfile.ZipFile,
    name: str,
    data: bytes,
) -> None:
    info = zipfile.ZipInfo(name)
    info.date_time = (2026, 7, 23, 12, 0, 0)
    info.compress_type = zipfile.ZIP_DEFLATED
    info.external_attr = 0o644 << 16
    archive.writestr(info, data)


def write_3mf(
    path: Path,
    parts: Sequence[Mesh],
    copies: Sequence[tuple[float, float, float]] = ((0.0, 0.0, 0.0),),
) -> None:
    namespace = "http://schemas.microsoft.com/3dmanufacturing/core/2015/02"
    ET.register_namespace("", namespace)
    q = lambda tag: f"{{{namespace}}}{tag}"
    model = ET.Element(
        q("model"),
        {
            "unit": "millimeter",
            "{http://www.w3.org/XML/1998/namespace}lang": "en-US",
        },
    )
    for name, value in [
        ("Application", "OpenAI Codex / Secret-DEV-Brain REV8 builder"),
        ("CreationDate", "2026-07-23"),
        ("Title", "Secret-DEV-Brain DISPLAY REV8 Four-Color AMS"),
        (
            "Description",
            "Editable four-part 1:1 display model: Blue, Red, Black, Silver.",
        ),
    ]:
        metadata = ET.SubElement(model, q("metadata"), {"name": name})
        metadata.text = value

    resources = ET.SubElement(model, q("resources"))
    base_materials = ET.SubElement(resources, q("basematerials"), {"id": "1"})
    for material, color, _ in MATERIALS:
        ET.SubElement(
            base_materials,
            q("base"),
            {"name": material, "displaycolor": color},
        )

    object_ids: list[int] = []
    for material_index, part in enumerate(parts):
        object_id = 2 + material_index
        object_ids.append(object_id)
        object_element = ET.SubElement(
            resources,
            q("object"),
            {
                "id": str(object_id),
                "type": "model",
                "pid": "1",
                "pindex": str(material_index),
                "name": part.name,
            },
        )
        mesh_element = ET.SubElement(object_element, q("mesh"))
        vertices_element = ET.SubElement(mesh_element, q("vertices"))
        for x, y, z in part.vertices:
            ET.SubElement(
                vertices_element,
                q("vertex"),
                {"x": fmt(x), "y": fmt(y), "z": fmt(z)},
            )
        triangles_element = ET.SubElement(mesh_element, q("triangles"))
        for a, b, c in part.faces:
            ET.SubElement(
                triangles_element,
                q("triangle"),
                {"v1": str(a), "v2": str(b), "v3": str(c)},
            )

    assembly_id = 2 + len(parts)
    assembly = ET.SubElement(
        resources,
        q("object"),
        {
            "id": str(assembly_id),
            "type": "model",
            "name": "Secret-DEV-Brain_Multicolor_Assembly",
        },
    )
    components = ET.SubElement(assembly, q("components"))
    for object_id in object_ids:
        ET.SubElement(components, q("component"), {"objectid": str(object_id)})

    build = ET.SubElement(model, q("build"))
    for translate_x, translate_y, translate_z in copies:
        transform = (
            "1 0 0 0 1 0 0 0 1 "
            f"{fmt(translate_x)} {fmt(translate_y)} {fmt(translate_z)}"
        )
        attributes = {
            "objectid": str(assembly_id),
            "partnumber": "Secret-DEV-Brain_Multicolor",
        }
        if any(abs(value) > 1e-12 for value in (translate_x, translate_y, translate_z)):
            attributes["transform"] = transform
        ET.SubElement(build, q("item"), attributes)

    content_types = b"""<?xml version="1.0" encoding="UTF-8"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
  <Default Extension="model" ContentType="application/vnd.ms-package.3dmanufacturing-3dmodel+xml"/>
</Types>
"""
    relationships = b"""<?xml version="1.0" encoding="UTF-8"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Target="/3D/3dmodel.model" Id="rel0" Type="http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel"/>
</Relationships>
"""

    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        zip_write_bytes(archive, "[Content_Types].xml", content_types)
        zip_write_bytes(archive, "_rels/.rels", relationships)
        zip_write_bytes(archive, "3D/3dmodel.model", xml_bytes(model))


def hex_to_rgb(color: str) -> tuple[int, int, int]:
    return (
        int(color[1:3], 16),
        int(color[3:5], 16),
        int(color[5:7], 16),
    )


def shade_rgb(
    color: tuple[int, int, int],
    factor: float,
) -> tuple[int, int, int]:
    return tuple(max(0, min(255, round(channel * factor))) for channel in color)


def render_preview(
    path: Path,
    parts: Sequence[Mesh],
    camera: tuple[float, float, float],
    target: tuple[float, float, float],
    camera_up_hint: tuple[float, float, float],
    output_size: tuple[int, int],
    background: tuple[int, int, int] = (236, 239, 243),
) -> None:
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        return

    supersample = 2
    width = output_size[0] * supersample
    height = output_size[1] * supersample
    image = Image.new("RGB", (width, height), background)
    draw = ImageDraw.Draw(image)

    forward = unit3(sub3(target, camera))
    right = unit3(cross3(forward, camera_up_hint))
    up = unit3(cross3(right, forward))

    all_vertices = [vertex for part in parts for vertex in part.vertices]
    projected_vertices = [
        (
            dot3(sub3(vertex, target), right),
            dot3(sub3(vertex, target), up),
        )
        for vertex in all_vertices
    ]
    min_u = min(point[0] for point in projected_vertices)
    max_u = max(point[0] for point in projected_vertices)
    min_v = min(point[1] for point in projected_vertices)
    max_v = max(point[1] for point in projected_vertices)
    padding = 0.07
    scale = min(
        width * (1.0 - 2.0 * padding) / max(1e-9, max_u - min_u),
        height * (1.0 - 2.0 * padding) / max(1e-9, max_v - min_v),
    )
    center_u = (min_u + max_u) / 2.0
    center_v = (min_v + max_v) / 2.0
    screen_center_x = width / 2.0
    screen_center_y = height / 2.0

    material_colors = {
        material: hex_to_rgb(color) for material, color, _ in MATERIALS
    }
    light = unit3((-0.35, -0.45, 0.82))
    triangles = []
    for part in parts:
        base_color = material_colors[part.material]
        for face in part.faces:
            a, b, c = (part.vertices[index] for index in face)
            normal = triangle_normal(a, b, c)
            centroid = mul3(add3(add3(a, b), c), 1.0 / 3.0)
            depth = dot3(sub3(centroid, camera), forward)
            light_amount = max(0.0, dot3(normal, light))
            view_amount = abs(dot3(normal, mul3(forward, -1.0)))
            factor = 0.68 + 0.30 * light_amount + 0.08 * view_amount
            if part.material == "AMS_A3_BLACK":
                factor += 0.32
            color = shade_rgb(base_color, factor)
            screen_points = []
            for vertex in (a, b, c):
                relative = sub3(vertex, target)
                u = dot3(relative, right)
                v = dot3(relative, up)
                screen_points.append(
                    (
                        screen_center_x + (u - center_u) * scale,
                        screen_center_y - (v - center_v) * scale,
                    )
                )
            area = abs(
                (screen_points[1][0] - screen_points[0][0])
                * (screen_points[2][1] - screen_points[0][1])
                - (screen_points[2][0] - screen_points[0][0])
                * (screen_points[1][1] - screen_points[0][1])
            )
            if area > 0.02:
                triangles.append((centroid[2], depth, screen_points, color))

    # This model is a shallow relief object.  Draw lower Z layers first, then
    # far-to-near within each height.  That prevents a giant PCB-top triangle
    # from hiding a small raised logo/LED merely because their centroids are at
    # different Y positions.
    triangles.sort(key=lambda item: (round(item[0], 3), -item[1]))
    for _, _, points, color in triangles:
        draw.polygon(points, fill=color)

    image = image.resize(output_size, Image.Resampling.LANCZOS)
    image.save(path, optimize=True)


def combined_bounds(
    parts: Sequence[Mesh],
) -> tuple[tuple[float, float, float], tuple[float, float, float]]:
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


def write_readme(path: Path) -> None:
    path.write_text(
        textwrap.dedent(
            """\
            # Secret-DEV-Brain — DISPLAY REV8 four-color AMS print

            This revision is tuned for what the physical model is actually for:
            a clean, cool-looking 1:1 visual mockup. It preserves the real board
            outline, locations, USB-C shell, exact flamingo/cactus art, and
            `Secret-DEV-Brain` name, but intentionally simplifies details that a
            0.4 mm nozzle cannot reproduce cleanly.

            ## Use this file

            Open `Secret-DEV-Brain_DISPLAY_REV8.3mf` in Bambu Studio. It is an
            editable four-part assembly and contains **no pre-sliced G-code**.

            The package also includes:

            - `Secret-DEV-Brain_3-up_DISPLAY_REV8.3mf` — three aligned copies.
            - OBJ + MTL — aligned fallback with material colors.
            - `parts/*.stl` — load all four together as one multipart object.
            - top and angled preview PNGs.
            - validation data, change log, checksums, and reproducible source.

            ## AMS mapping

            1. A1 — Blue
            2. A2 — Red
            3. A3 — Black
            4. A4 — Silver

            NeoPixels alternate D1 Red, D2 Blue, D3 Red, through D10 Blue.

            ## Recommended P1S setup

            - Printer/nozzle: Bambu Lab P1S, 0.4 mm nozzle.
            - Layer height: 0.08 mm.
            - Wall generator: Arachne.
            - Top surface pattern: monotonic.
            - Prime tower: on.
            - Ironing: off.
            - Supports: build-plate-only/manual support under the USB-C shell.
              The rest of the model is designed to print without support.
            - Keep the model flat on the PCB back and drop it to the bed.
            - Confirm all four named parts map to the AMS slots above.
            - Use calibrated flushing; black-to-silver flushing matters most.

            ## What REV8 fixes

            - GPIO: first pad remains square; all 13 pads now have a printable
              0.63 mm radial silver ring and a clean 0.84 mm dark center.
            - NeoPixels: clean 5.0 mm housings with no tiny leads or oversized
              under-plates; colored inserts are supported and remain recessed.
            - Button: simplified into one deliberate housing and actuator.
            - Board detail: hundreds of tiny silver pad/lead islands were
              removed. Only the USB-C shell, logo/text, the two readable central
              metal packages, LEDs, button, and GPIO pads remain silver.
            - The 3-up file is geometry-only and must be sliced for your printer;
              it does not contain stale or printer-specific G-code.

            This is a display model, not a substitute for the STEP/KiCad model
            when checking mechanical clearances or manufacturing dimensions.
            """
        ),
        encoding="utf-8",
    )


def write_changelog(path: Path, cleanup: dict[str, object]) -> None:
    old_count = int(cleanup["original_silver_component_count"])
    retained = int(cleanup["retained_source_silver_component_count"])
    new_shells = sum(int(value) for value in cleanup["new_silver_shells"].values())
    final_count = retained + new_shells
    reduction = 100.0 * (old_count - final_count) / old_count
    path.write_text(
        textwrap.dedent(
            f"""\
            # DISPLAY REV8 change log

            ## Physical-print diagnosis

            The previous model treated tiny solder pads, component leads, LED
            leads, and header plating as separate silver print islands. The
            photographed P1S print showed those islands as blobs, seams, strings,
            and irregular GPIO rings. The board silhouette, USB-C shell, red/blue
            rhythm, logo, and wordmark were already the right visual foundation.

            ## Geometry decisions

            - Kept the exact 1:1 X/Y/Z envelope and USB-C geometry.
            - Kept the exact strengthened KiCad flamingo and cactus polygons.
            - Kept the established `Secret-DEV-Brain` wordmark.
            - Rebuilt ten NeoPixel housings as clean 5.0 mm rounded-square frames.
            - Added black lens pedestals so every colored insert is supported.
            - Rebuilt J1 as twelve round annular pads plus one square pin-1 pad.
            - Increased printable GPIO radial ring width from roughly 0.15 mm to
              0.63 mm.
            - Rebuilt SW1 as one silver housing with one black actuator.
            - Retained only the large crystal and large capacitor as central
              silver component accents.
            - Removed all other sub-nozzle silver component/pad/lead islands.

            The silver part went from {old_count} disconnected source components
            to {final_count} intentional retained/new shells, a {reduction:.1f}%
            reduction in separate silver features.
            """
        ),
        encoding="utf-8",
    )


def write_manifest(root: Path, path: Path) -> None:
    lines = []
    for file_path in sorted(root.rglob("*")):
        if file_path.is_file() and file_path != path:
            lines.append(f"{sha256(file_path)}  {file_path.relative_to(root).as_posix()}")
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def validate_3mf(path: Path, expected_build_items: int) -> dict[str, object]:
    with zipfile.ZipFile(path, "r") as archive:
        names = set(archive.namelist())
        required = {"[Content_Types].xml", "_rels/.rels", "3D/3dmodel.model"}
        missing = sorted(required - names)
        model_data = archive.read("3D/3dmodel.model")
    root = ET.fromstring(model_data)
    namespace = {"m": "http://schemas.microsoft.com/3dmanufacturing/core/2015/02"}
    objects = root.findall(".//m:resources/m:object", namespace)
    build_items = root.findall(".//m:build/m:item", namespace)
    return {
        "zip_entries_present": not missing,
        "missing_zip_entries": missing,
        "xml_parse_ok": True,
        "resource_object_count": len(objects),
        "build_item_count": len(build_items),
        "expected_build_item_count": expected_build_items,
        "build_item_count_ok": len(build_items) == expected_build_items,
    }


def build_package(
    source_obj: Path,
    source_mtl: Path,
    package_root: Path,
) -> Path:
    if package_root.exists():
        shutil.rmtree(package_root)
    package_root.mkdir(parents=True)
    parts_dir = package_root / "parts"
    source_dir = package_root / "source"
    parts_dir.mkdir()
    source_dir.mkdir()

    parts, cleanup = build_parts(source_obj)
    blue, red, black, silver = parts

    base_name = "Secret-DEV-Brain_DISPLAY_REV8"
    mtl_path = package_root / f"{base_name}.mtl"
    obj_path = package_root / f"{base_name}.obj"
    one_3mf_path = package_root / f"{base_name}.3mf"
    three_3mf_path = package_root / "Secret-DEV-Brain_3-up_DISPLAY_REV8.3mf"
    write_mtl(mtl_path)
    write_obj(obj_path, mtl_path.name, parts)
    write_3mf(one_3mf_path, parts)
    write_3mf(
        three_3mf_path,
        parts,
        copies=((0.0, 0.0, 0.0), (0.0, 38.0, 0.0), (0.0, 76.0, 0.0)),
    )

    stl_names = [
        "01_A1_Blue_NeoPixels.stl",
        "02_A2_Red_NeoPixels.stl",
        "03_A3_Black_PCB_Components_and_Supports.stl",
        "04_A4_Silver_Hero_Details.stl",
    ]
    for stl_name, part in zip(stl_names, parts):
        write_binary_stl(parts_dir / stl_name, part)

    top_preview = package_root / "PREVIEW_TOP_REV8.png"
    angle_preview = package_root / "PREVIEW_ANGLE_REV8.png"
    render_preview(
        top_preview,
        parts,
        camera=(35.6225, 15.0, 110.0),
        target=(35.6225, 15.0, 1.3),
        camera_up_hint=(0.0, 1.0, 0.0),
        output_size=(1600, 760),
    )
    render_preview(
        angle_preview,
        parts,
        # A high three-quarter camera keeps the simple painter renderer's
        # depth ordering stable while still exposing the component heights.
        camera=(98.0, -108.0, 205.0),
        target=(35.6225, 15.0, 1.4),
        camera_up_hint=(0.0, 0.0, 1.0),
        output_size=(1600, 900),
    )

    readme_path = package_root / "README_SLICING.md"
    changelog_path = package_root / "PRINT_CHANGELOG_REV8.md"
    write_readme(readme_path)
    write_changelog(changelog_path, cleanup)

    shutil.copy2(source_obj, source_dir / "Secret-DEV-Brain_Multicolor_BASE.obj")
    shutil.copy2(source_mtl, source_dir / "Secret-DEV-Brain_Multicolor_BASE.mtl")
    shutil.copy2(Path(__file__), source_dir / Path(__file__).name)

    low, high = combined_bounds(parts)
    part_stats = {}
    for part in parts:
        part_low, part_high = part.bounds()
        part_stats[part.name] = {
            "material": part.material,
            "vertex_count": len(part.vertices),
            "triangle_count": len(part.faces),
            "connected_shell_count": part.component_count(),
            "bounds_mm": [
                [round(value, 6) for value in part_low],
                [round(value, 6) for value in part_high],
            ],
            "edge_stats": part.edge_stats(),
        }

    validation = {
        "model": "Secret-DEV-Brain DISPLAY REV8",
        "generated_utc": datetime.now(timezone.utc).isoformat().replace("+00:00", "Z"),
        "purpose": (
            "1:1 display-optimized visual print; print-scale detail takes "
            "priority over microscopic component replication"
        ),
        "source_sha256": {
            "repository_multicolor_obj": sha256(source_obj),
            "repository_multicolor_mtl": sha256(source_mtl),
        },
        "dimensions_mm": {
            "bbox": [
                [round(value, 6) for value in low],
                [round(value, 6) for value in high],
            ],
            "size": [round(high[i] - low[i], 6) for i in range(3)],
        },
        "ams_order": ["A1 Blue", "A2 Red", "A3 Black", "A4 Silver"],
        "colors": {
            "blue": "#1457D9",
            "red": "#D71920",
            "black": "#101010",
            "silver": "#B7BCC2",
        },
        "display_geometry": {
            "gpio": {
                "pin_count": 13,
                "pitch_mm": 2.54,
                "pin_1_shape": "square superellipse",
                "pins_2_to_13_shape": "round",
                "round_outer_diameter_mm": 2.10,
                "pin_1_outer_size_mm": [2.20, 2.20],
                "dark_center_diameter_mm": 0.84,
                "minimum_radial_silver_width_mm": 0.63,
                "raised_height_mm": 0.24,
            },
            "neopixels": {
                "count": 10,
                "center_pitch_mm": 6.0,
                "housing_size_mm": [5.0, 5.0],
                "housing_top_z_mm": 3.17,
                "housing_minimum_frame_width_mm": 0.70,
                "lens_diameter_mm": 3.28,
                "lens_z_mm": [2.82, 2.98],
                "lens_recess_below_housing_top_mm": 0.19,
                "black_support_pedestal": True,
                "sequence": [
                    {
                        "designator": f"D{index}",
                        "color": "red" if index % 2 else "blue",
                        "center_mm": [round(x, 3), round(y, 3)],
                    }
                    for index, (x, y) in enumerate(LED_CENTERS, start=1)
                ],
            },
            "button": {
                "silver_housing_size_mm": [5.8, 3.6],
                "black_actuator_size_mm": [2.8, 1.15],
            },
        },
        "silver_cleanup": cleanup,
        "parts": part_stats,
        "containers": {
            one_3mf_path.name: validate_3mf(one_3mf_path, 1),
            three_3mf_path.name: validate_3mf(three_3mf_path, 3),
        },
        "notes": [
            "The inherited repository black mesh contains boundary edges but was already proven sliceable by the photographed print.",
            "All REV8-generated LED, GPIO, button, lens, and support primitives are closed solids.",
            "The 3MF files contain geometry only and no printer G-code.",
        ],
    }
    validation_path = package_root / "MODEL_VALIDATION_REV8.json"
    validation_path.write_text(
        json.dumps(validation, indent=2, sort_keys=False) + "\n",
        encoding="utf-8",
    )

    manifest_path = package_root / "MANIFEST_SHA256.txt"
    write_manifest(package_root, manifest_path)

    zip_path = package_root.with_suffix(".zip")
    if zip_path.exists():
        zip_path.unlink()
    with zipfile.ZipFile(
        zip_path,
        "w",
        compression=zipfile.ZIP_DEFLATED,
        compresslevel=9,
    ) as archive:
        for file_path in sorted(package_root.rglob("*")):
            if file_path.is_file():
                archive.write(
                    file_path,
                    arcname=(
                        Path(package_root.name)
                        / file_path.relative_to(package_root)
                    ).as_posix(),
                )
    return zip_path


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-obj", type=Path, required=True)
    parser.add_argument("--source-mtl", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    arguments = parser.parse_args()
    zip_path = build_package(
        arguments.source_obj.resolve(),
        arguments.source_mtl.resolve(),
        arguments.output_root.resolve(),
    )
    print(zip_path)


if __name__ == "__main__":
    main()
