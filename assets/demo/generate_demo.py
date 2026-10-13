#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 samiosu
# SPDX-License-Identifier: MIT
"""Regenerate the independently authored SpriteFace demo (requires Pillow).

These geometric illustrations and XML are original SpriteFace project assets,
licensed under the repository's MIT license. No SDK sample artwork is included.
"""
from pathlib import Path
import json
import xml.etree.ElementTree as ET
from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent


def node(parent, element_name, value=None, **attrs):
    element = ET.SubElement(parent, element_name, attrs)
    if value is not None:
        element.text = str(value)
    return element


def write_xml(name, root):
    ET.indent(root, space="  ")
    ET.ElementTree(root).write(HERE / name, encoding="utf-8", xml_declaration=True)


def settings(parent):
    result = node(parent, "settings")
    for tag, value in {
        "fps": 30, "frameCount": 60, "sortMode": "prio",
        "canvasSize": "512 512", "pivot": "0 0", "startFrame": 0, "endFrame": 59,
    }.items():
        node(result, tag, value)
    return result


def key(parent, attribute, values):
    attr = node(parent, "attribute", tag=attribute)
    for time, value in values:
        frame = node(attr, "key", time=str(time), ipType="linear")
        node(frame, "value", value)


def main():
    # Supersampling keeps edges smooth; final atlas dimensions remain powers of 2.
    image = Image.new("RGBA", (2048, 2048), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)
    def ellipse(box, color):
        draw.ellipse(tuple(int(x * 4) for x in box), fill=color)
    def round_rect(box, color, radius):
        draw.rounded_rectangle(tuple(int(x * 4) for x in box), radius=radius * 4, fill=color)
    round_rect((10, 10, 246, 246), "#6AC3B2", 87)
    round_rect((20, 14, 236, 233), "#A8E3CC", 83)
    ellipse((42, 33, 97, 54), "#CEF3DD")
    round_rect((273, 28, 495, 251), "#233F50", 60)
    round_rect((288, 43, 480, 247), "#31576B", 45)
    round_rect((349, 21, 420, 77), "#EDEAD7", 24)
    round_rect((350, 103, 418, 156), "#E9CF8F", 15)
    round_rect((357, 110, 411, 149), "#31576B", 10)
    ellipse((16, 268, 48, 311), "#19333F")
    ellipse((22, 273, 32, 284), "#F6F7E6")
    round_rect((70, 263, 122, 310), "#3A343B", 20)
    round_rect((78, 290, 114, 313), "#E797A6", 11)
    ellipse((137, 274, 183, 303), "#EAA8AB")
    ellipse((205, 267, 243, 308), "#EDD59D")
    image.resize((512, 512), Image.Resampling.LANCZOS).save(HERE / "atlas.png")

    project = ET.Element("SpriteStudioProject", version="2.00.01", edition="Professional")
    node(project, "name", "spriteface")
    conf = node(project, "settings")
    for directory in ("animeBaseDirectory", "cellMapBaseDirectory", "imageBaseDirectory", "effectBaseDirectory"):
        node(conf, directory, "")
    node(conf, "wrapMode", "clamp")
    node(conf, "filterMode", "linear")
    node(project, "cellmapNames")
    node(project.find("cellmapNames"), "value", "atlas.ssce")
    node(project, "animepackNames")
    node(project.find("animepackNames"), "value", "character.ssae")
    node(project, "effectFileNames")
    write_xml("spriteface.sspj", project)

    cells = {
        "head": (0, 0, 256, 256), "body": (256, 0, 256, 256),
        "eye": (0, 256, 64, 64), "mouth": (64, 256, 64, 64),
        "blush": (128, 256, 64, 64), "ornament": (192, 256, 64, 64),
    }
    cellmap = ET.Element("SpriteStudioCellMap", version="2.00.00")
    for tag, value in {"name": "atlas", "imagePath": "atlas.png", "pixelSize": "512 512",
                       "overrideTexSettings": 1, "wrapMode": "clamp", "filterMode": "linear"}.items():
        node(cellmap, tag, value)
    cell_list = node(cellmap, "cells")
    for name, (x, y, w, h) in cells.items():
        cell = node(cell_list, "cell")
        for tag, value in {"name": name, "pos": f"{x} {y}", "size": f"{w} {h}",
                           "pivot": "0 0", "rotated": 0, "ismesh": 0}.items():
            node(cell, tag, value)
    write_xml("atlas.ssce", cellmap)

    # Parent order is deliberate: head motion propagates to its eyes and mouth.
    parts = [
        ("root", -1, "null", None, 0, 0, 0),
        ("body", 0, "normal", "body", 0, -132, 0),
        ("head_pivot", 0, "null", None, 0, 64, 1),
        ("head", 2, "normal", "head", 0, 0, 1),
        ("eye_left", 2, "normal", "eye", -46, 23, 4),
        ("eye_right", 2, "normal", "eye", 46, 23, 4),
        ("mouth", 2, "normal", "mouth", 0, -43, 4),
        ("blush_left", 2, "normal", "blush", -75, -16, 3),
        ("blush_right", 2, "normal", "blush", 75, -16, 3),
        ("ornament", 2, "normal", "ornament", 83, 80, 5),
    ]
    pack = ET.Element("SpriteStudioAnimePack", version="2.00.03")
    settings(pack)
    node(pack, "name", "character")
    part_list = node(node(pack, "Model"), "partList")
    for index, (name, parent, kind, _, _, _, _) in enumerate(parts):
        part = node(part_list, "value")
        for tag, value in {"name": name, "arrayIndex": index, "parentIndex": parent,
                           "type": kind, "boundsType": "none", "inheritType": "self",
                           "alphaBlendType": "mix", "show": 1, "locked": 0, "maskInfluence": 0}.items():
            node(part, tag, value)
        inherit = node(part, "ineheritRates")  # SpriteStudio's on-disk spelling.
        node(inherit, "ALPH", 1)
    node(node(pack, "cellmapNames"), "value", "atlas.ssce")
    anime = node(node(pack, "animeList"), "anime")
    node(anime, "name", "idle")
    node(anime, "overrideSettings", 1)
    settings(anime)
    node(anime, "isSetup", 0)
    anime_parts = node(anime, "partAnimes")
    for name, _, _, cell_name, x, y, priority in parts:
        part = node(anime_parts, "partAnime")
        node(part, "partName", name)
        attrs = node(part, "attributes")
        for tag, value in {"POSX": x, "POSY": y, "ROTZ": 0, "SCLX": 1, "SCLY": 1,
                           "ALPH": 1, "PRIO": priority, "HIDE": 0}.items():
            values = [(0, value)]
            if name == "root" and tag == "POSY":
                values = [(0, 0), (30, 3), (59, 0)]
            key(attrs, tag, values)
        if cell_name:
            attr = node(attrs, "attribute", tag="CELL")
            value = node(node(attr, "key", time="0"), "value")
            node(value, "mapId", 0)
            node(value, "name", cell_name)
    write_xml("character.ssae", pack)

    def binding(part, signal, prop, operation, in_min, in_max, out_min, out_max):
        return {"part": part, "signal": signal, "property": prop, "operation": operation,
                "input_min": in_min, "input_max": in_max, "output_min": out_min, "output_max": out_max}
    mapping = {"schema": 1, "bindings": [
        binding("head_pivot", "roll", "rotation_z", "add", -0.6, 0.6, 34, -34),
        binding("head_pivot", "yaw", "position_x", "add", -0.7, 0.7, -22, 22),
        binding("head_pivot", "pitch", "position_y", "add", -0.7, 0.7, -15, 15),
        binding("eye_left", "left_eye", "local_scale_y", "multiply", 0, 1, 0.06, 1),
        binding("eye_right", "right_eye", "local_scale_y", "multiply", 0, 1, 0.06, 1),
        binding("mouth", "mouth_open", "local_scale_y", "multiply", 0, 1, 0.12, 1),
        binding("mouth", "mouth_smile", "local_scale_x", "multiply", 0, 1, 0.9, 1.3),
    ]}
    (HERE / "mapping.json").write_text(json.dumps(mapping, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
