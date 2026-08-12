#!/usr/bin/env python3
from __future__ import annotations

import datetime as dt
import html
import zipfile
from dataclasses import dataclass, field
from pathlib import Path


EMU_PER_INCH = 914400
SLIDE_CX = 12192000
SLIDE_CY = 6858000


def xml_escape(value: str) -> str:
    return html.escape(value, quote=True)


def inches(value: float) -> int:
    return int(value * EMU_PER_INCH)


def detect_image_ext(path: Path) -> str:
    data = path.read_bytes()
    if data.startswith(b"\x89PNG\r\n\x1a\n"):
        return "png"
    if data[:2] == b"\xff\xd8":
        return "jpeg"
    raise ValueError(f"unsupported image format: {path}")


@dataclass
class TextBox:
    x: int
    y: int
    cx: int
    cy: int
    text: str
    font_size: int = 2200
    color: str = "1F2937"
    fill: str | None = None
    line: str | None = None
    radius: bool = False
    align: str = "l"
    bold: bool = False


@dataclass
class Picture:
    path: Path
    x: int
    y: int
    cx: int
    cy: int
    line: str | None = "D7DEE8"


@dataclass
class SlideSpec:
    title: str = ""
    subtitle: str = ""
    text_boxes: list[TextBox] = field(default_factory=list)
    pictures: list[Picture] = field(default_factory=list)
    bg_color: str = "F6F8FB"
    header_accent: str = "0F766E"
    footer_text: str = "AURIX MediRun"
    variant: str = "content"


class PptxBuilder:
    def __init__(self, output_path: Path) -> None:
        self.output_path = output_path
        self.slides: list[SlideSpec] = []

    def add_slide(self, slide: SlideSpec) -> None:
        self.slides.append(slide)

    def build(self) -> None:
        with zipfile.ZipFile(self.output_path, "w", zipfile.ZIP_DEFLATED) as zf:
            self._write_root_files(zf)
            self._write_theme(zf)
            self._write_master_and_layout(zf)
            self._write_presentation(zf)
            media_written: dict[Path, str] = {}
            for idx, slide in enumerate(self.slides, start=1):
                self._write_slide(zf, idx, slide, media_written)

    def _write_root_files(self, zf: zipfile.ZipFile) -> None:
        now = dt.datetime.now().replace(microsecond=0).isoformat()
        overrides = [
            '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>',
            '<Default Extension="xml" ContentType="application/xml"/>',
            '<Default Extension="png" ContentType="image/png"/>',
            '<Default Extension="jpg" ContentType="image/jpeg"/>',
            '<Default Extension="jpeg" ContentType="image/jpeg"/>',
            '<Override PartName="/ppt/presentation.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.presentation.main+xml"/>',
            '<Override PartName="/ppt/slideMasters/slideMaster1.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slideMaster+xml"/>',
            '<Override PartName="/ppt/slideLayouts/slideLayout1.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slideLayout+xml"/>',
            '<Override PartName="/ppt/theme/theme1.xml" ContentType="application/vnd.openxmlformats-officedocument.theme+xml"/>',
            '<Override PartName="/docProps/core.xml" ContentType="application/vnd.openxmlformats-package.core-properties+xml"/>',
            '<Override PartName="/docProps/app.xml" ContentType="application/vnd.openxmlformats-officedocument.extended-properties+xml"/>',
        ]
        for idx in range(1, len(self.slides) + 1):
            overrides.append(
                f'<Override PartName="/ppt/slides/slide{idx}.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slide+xml"/>'
            )
        zf.writestr(
            "[Content_Types].xml",
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/content-types\">"
            + "".join(overrides)
            + "</Types>",
        )
        zf.writestr(
            "_rels/.rels",
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
            "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument\" Target=\"ppt/presentation.xml\"/>"
            "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/package/2006/relationships/metadata/core-properties\" Target=\"docProps/core.xml\"/>"
            "<Relationship Id=\"rId3\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/extended-properties\" Target=\"docProps/app.xml\"/>"
            "</Relationships>",
        )
        zf.writestr(
            "docProps/core.xml",
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            "<cp:coreProperties xmlns:cp=\"http://schemas.openxmlformats.org/package/2006/metadata/core-properties\" "
            "xmlns:dc=\"http://purl.org/dc/elements/1.1/\" xmlns:dcterms=\"http://purl.org/dc/terms/\" "
            "xmlns:dcmitype=\"http://purl.org/dc/dcmitype/\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\">"
            "<dc:title>AURIX MediRun 발표자료</dc:title>"
            "<dc:creator>Codex</dc:creator>"
            "<cp:lastModifiedBy>Codex</cp:lastModifiedBy>"
            f"<dcterms:created xsi:type=\"dcterms:W3CDTF\">{now}</dcterms:created>"
            f"<dcterms:modified xsi:type=\"dcterms:W3CDTF\">{now}</dcterms:modified>"
            "</cp:coreProperties>",
        )
        zf.writestr(
            "docProps/app.xml",
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            "<Properties xmlns=\"http://schemas.openxmlformats.org/officeDocument/2006/extended-properties\" "
            "xmlns:vt=\"http://schemas.openxmlformats.org/officeDocument/2006/docPropsVTypes\">"
            "<Application>Microsoft Office PowerPoint</Application>"
            f"<Slides>{len(self.slides)}</Slides>"
            "<PresentationFormat>On-screen Show (16:9)</PresentationFormat>"
            "</Properties>",
        )

    def _write_theme(self, zf: zipfile.ZipFile) -> None:
        zf.writestr(
            "ppt/theme/theme1.xml",
            """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<a:theme xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" name="AurixTheme">
  <a:themeElements>
    <a:clrScheme name="AurixColors">
      <a:dk1><a:srgbClr val="0B1320"/></a:dk1>
      <a:lt1><a:srgbClr val="FFFFFF"/></a:lt1>
      <a:dk2><a:srgbClr val="1E293B"/></a:dk2>
      <a:lt2><a:srgbClr val="F8FAFC"/></a:lt2>
      <a:accent1><a:srgbClr val="14B8A6"/></a:accent1>
      <a:accent2><a:srgbClr val="2563EB"/></a:accent2>
      <a:accent3><a:srgbClr val="F97316"/></a:accent3>
      <a:accent4><a:srgbClr val="EC4899"/></a:accent4>
      <a:accent5><a:srgbClr val="8B5CF6"/></a:accent5>
      <a:accent6><a:srgbClr val="F43F5E"/></a:accent6>
      <a:hlink><a:srgbClr val="2563EB"/></a:hlink>
      <a:folHlink><a:srgbClr val="7C3AED"/></a:folHlink>
    </a:clrScheme>
    <a:fontScheme name="AurixFonts">
      <a:majorFont><a:latin typeface="Aptos Display"/><a:ea typeface="Malgun Gothic"/><a:cs typeface="Arial"/></a:majorFont>
      <a:minorFont><a:latin typeface="Aptos"/><a:ea typeface="Malgun Gothic"/><a:cs typeface="Arial"/></a:minorFont>
    </a:fontScheme>
    <a:fmtScheme name="AurixFmt">
      <a:fillStyleLst><a:solidFill><a:schemeClr val="phClr"/></a:solidFill></a:fillStyleLst>
      <a:lnStyleLst><a:ln w="9525" cap="flat" cmpd="sng" algn="ctr"><a:solidFill><a:schemeClr val="phClr"/></a:solidFill></a:ln></a:lnStyleLst>
      <a:effectStyleLst><a:effectStyle><a:effectLst/></a:effectStyle></a:effectStyleLst>
      <a:bgFillStyleLst><a:solidFill><a:schemeClr val="lt1"/></a:solidFill></a:bgFillStyleLst>
    </a:fmtScheme>
  </a:themeElements>
  <a:objectDefaults/>
  <a:extraClrSchemeLst/>
</a:theme>""",
        )

    def _write_master_and_layout(self, zf: zipfile.ZipFile) -> None:
        zf.writestr(
            "ppt/slideMasters/slideMaster1.xml",
            """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<p:sldMaster xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships" xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main">
  <p:cSld name="Master">
    <p:bg><p:bgPr><a:solidFill><a:srgbClr val="F6F8FB"/></a:solidFill></p:bgPr></p:bg>
    <p:spTree>
      <p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr>
      <p:grpSpPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="0" cy="0"/><a:chOff x="0" y="0"/><a:chExt cx="0" cy="0"/></a:xfrm></p:grpSpPr>
    </p:spTree>
  </p:cSld>
  <p:clrMap bg1="lt1" tx1="dk1" bg2="lt2" tx2="dk2" accent1="accent1" accent2="accent2" accent3="accent3" accent4="accent4" accent5="accent5" accent6="accent6" hlink="hlink" folHlink="folHlink"/>
  <p:sldLayoutIdLst><p:sldLayoutId id="2147483649" r:id="rId1"/></p:sldLayoutIdLst>
</p:sldMaster>""",
        )
        zf.writestr(
            "ppt/slideMasters/_rels/slideMaster1.xml.rels",
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
            "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideLayout\" Target=\"../slideLayouts/slideLayout1.xml\"/>"
            "<Relationship Id=\"rId2\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/theme\" Target=\"../theme/theme1.xml\"/>"
            "</Relationships>",
        )
        zf.writestr(
            "ppt/slideLayouts/slideLayout1.xml",
            """<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<p:sldLayout xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships" xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main" type="blank" preserve="1">
  <p:cSld name="Blank">
    <p:spTree>
      <p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr>
      <p:grpSpPr><a:xfrm><a:off x="0" y="0"/><a:ext cx="0" cy="0"/><a:chOff x="0" y="0"/><a:chExt cx="0" cy="0"/></a:xfrm></p:grpSpPr>
    </p:spTree>
  </p:cSld>
  <p:clrMapOvr><a:masterClrMapping/></p:clrMapOvr>
</p:sldLayout>""",
        )
        zf.writestr(
            "ppt/slideLayouts/_rels/slideLayout1.xml.rels",
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
            "<Relationship Id=\"rId1\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideMaster\" Target=\"../slideMasters/slideMaster1.xml\"/>"
            "</Relationships>",
        )

    def _write_presentation(self, zf: zipfile.ZipFile) -> None:
        slide_ids = [
            f'<p:sldId id="{255 + idx}" r:id="rId{idx + 1}"/>'
            for idx in range(1, len(self.slides) + 1)
        ]
        zf.writestr(
            "ppt/presentation.xml",
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            "<p:presentation xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
            "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" "
            "xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\" "
            f"cx=\"{SLIDE_CX}\" cy=\"{SLIDE_CY}\">"
            "<p:sldMasterIdLst><p:sldMasterId id=\"2147483648\" r:id=\"rId1\"/></p:sldMasterIdLst>"
            f"<p:sldIdLst>{''.join(slide_ids)}</p:sldIdLst>"
            "<p:sldSz cx=\"12192000\" cy=\"6858000\" type=\"screen16x9\"/>"
            "<p:notesSz cx=\"6858000\" cy=\"9144000\"/>"
            "<p:defaultTextStyle/>"
            "</p:presentation>",
        )
        rels = [
            '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideMaster" Target="slideMasters/slideMaster1.xml"/>'
        ]
        for idx in range(1, len(self.slides) + 1):
            rels.append(
                f'<Relationship Id="rId{idx + 1}" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide" Target="slides/slide{idx}.xml"/>'
            )
        zf.writestr(
            "ppt/_rels/presentation.xml.rels",
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
            + "".join(rels)
            + "</Relationships>",
        )

    def _write_slide(self, zf: zipfile.ZipFile, idx: int, slide: SlideSpec, media_written: dict[Path, str]) -> None:
        rel_entries = [
            '<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slideLayout" Target="../slideLayouts/slideLayout1.xml"/>'
        ]
        shape_xml = [self._base_shapes(slide, idx)]
        if slide.title:
            title_color = "FFFFFF" if slide.variant == "cover" else "0F172A"
            subtitle_color = "D7E7F4" if slide.variant == "cover" else "64748B"
            shape_xml.append(
                self._text_shape_xml(
                    10,
                    "Title",
                    TextBox(inches(0.9), inches(0.7), inches(8.5), inches(0.75), slide.title, 3000, title_color, None, None, False, "l", True),
                )
            )
            if slide.subtitle:
                shape_xml.append(
                    self._text_shape_xml(
                        11,
                        "Subtitle",
                        TextBox(inches(0.92), inches(1.45), inches(8.8), inches(0.45), slide.subtitle, 1450, subtitle_color),
                    )
                )

        next_shape_id = 20
        next_rel_id = 2
        for box in slide.text_boxes:
            shape_xml.append(self._text_shape_xml(next_shape_id, f"TextBox {next_shape_id}", box))
            next_shape_id += 1

        for pic in slide.pictures:
            media_target = media_written.get(pic.path)
            if media_target is None:
                ext = detect_image_ext(pic.path)
                media_target = f"image{len(media_written) + 1}.{ext}"
                media_written[pic.path] = media_target
                zf.writestr(f"ppt/media/{media_target}", pic.path.read_bytes())
            rel_entries.append(
                f'<Relationship Id="rId{next_rel_id}" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/image" Target="../media/{media_target}"/>'
            )
            shape_xml.append(self._picture_shape_xml(next_shape_id, f"Picture {next_shape_id}", pic, next_rel_id))
            next_shape_id += 1
            next_rel_id += 1

        slide_xml = (
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            "<p:sld xmlns:a=\"http://schemas.openxmlformats.org/drawingml/2006/main\" "
            "xmlns:r=\"http://schemas.openxmlformats.org/officeDocument/2006/relationships\" "
            "xmlns:p=\"http://schemas.openxmlformats.org/presentationml/2006/main\">"
            "<p:cSld><p:spTree>"
            "<p:nvGrpSpPr><p:cNvPr id=\"1\" name=\"\"/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr>"
            "<p:grpSpPr><a:xfrm><a:off x=\"0\" y=\"0\"/><a:ext cx=\"0\" cy=\"0\"/><a:chOff x=\"0\" y=\"0\"/><a:chExt cx=\"0\" cy=\"0\"/></a:xfrm></p:grpSpPr>"
            + "".join(shape_xml)
            + "</p:spTree></p:cSld>"
            "<p:clrMapOvr><a:masterClrMapping/></p:clrMapOvr>"
            "</p:sld>"
        )
        zf.writestr(f"ppt/slides/slide{idx}.xml", slide_xml)
        zf.writestr(
            f"ppt/slides/_rels/slide{idx}.xml.rels",
            "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
            "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/2006/relationships\">"
            + "".join(rel_entries)
            + "</Relationships>",
        )

    def _base_shapes(self, slide: SlideSpec, page_number: int) -> str:
        if slide.variant == "cover":
            return (
                self._rect_xml(2, "Background", 0, 0, SLIDE_CX, SLIDE_CY, slide.bg_color, None)
                + self._rect_xml(3, "Accent Left", 0, 0, inches(0.18), SLIDE_CY, "22D3EE", None)
                + self._rect_xml(4, "Hero Card", inches(0.7), inches(1.95), inches(5.5), inches(3.55), "102235", "22D3EE", True)
                + self._rect_xml(5, "Footer Band", inches(0.7), inches(6.5), inches(10.6), inches(0.12), "2DD4BF", None)
            )
        return (
            self._rect_xml(2, "Background", 0, 0, SLIDE_CX, SLIDE_CY, slide.bg_color, None)
            + self._rect_xml(3, "Top Band", 0, 0, SLIDE_CX, inches(0.26), slide.header_accent, None)
            + self._rect_xml(4, "Title Accent", inches(0.9), inches(1.72), inches(2.15), inches(0.08), slide.header_accent, None)
            + self._rect_xml(5, "Footer Line", inches(0.9), inches(6.63), inches(10.2), inches(0.025), "D6DCE5", None)
            + self._text_shape_xml(
                6,
                "Footer",
                TextBox(inches(0.92), inches(6.39), inches(2.6), inches(0.18), slide.footer_text, 980, "64748B"),
            )
            + self._text_shape_xml(
                7,
                "Page",
                TextBox(inches(10.95), inches(6.32), inches(0.4), inches(0.22), f"{page_number:02d}", 1100, "94A3B8", None, None, False, "ctr", True),
            )
        )

    def _rect_xml(self, shape_id: int, name: str, x: int, y: int, cx: int, cy: int, fill: str, line: str | None, radius: bool = False) -> str:
        line_xml = "<a:noFill/>" if line is None else f"<a:solidFill><a:srgbClr val=\"{line}\"/></a:solidFill>"
        prst = "roundRect" if radius else "rect"
        return (
            f'<p:sp><p:nvSpPr><p:cNvPr id="{shape_id}" name="{xml_escape(name)}"/><p:cNvSpPr/><p:nvPr/></p:nvSpPr>'
            "<p:spPr><a:xfrm>"
            f'<a:off x="{x}" y="{y}"/><a:ext cx="{cx}" cy="{cy}"/>'
            f"</a:xfrm><a:prstGeom prst=\"{prst}\"><a:avLst/></a:prstGeom>"
            f'<a:solidFill><a:srgbClr val="{fill}"/></a:solidFill>'
            f"<a:ln>{line_xml}</a:ln></p:spPr>"
            "<p:txBody><a:bodyPr/><a:lstStyle/><a:p/></p:txBody></p:sp>"
        )

    def _text_shape_xml(self, shape_id: int, name: str, box: TextBox) -> str:
        fill_xml = "<a:noFill/>" if box.fill is None else f"<a:solidFill><a:srgbClr val=\"{box.fill}\"/></a:solidFill>"
        line_xml = "<a:noFill/>" if box.line is None else f"<a:solidFill><a:srgbClr val=\"{box.line}\"/></a:solidFill>"
        prst = "roundRect" if box.radius else "rect"
        paragraphs = []
        for line in box.text.split("\n"):
            bold_attr = ' b="1"' if box.bold else ""
            paragraphs.append(
                "<a:p>"
                f'<a:pPr algn="{box.align}"/>'
                f'<a:r><a:rPr lang="ko-KR" sz="{box.font_size}"{bold_attr}><a:solidFill><a:srgbClr val="{box.color}"/></a:solidFill></a:rPr><a:t>{xml_escape(line)}</a:t></a:r>'
                f'<a:endParaRPr lang="ko-KR" sz="{box.font_size}"/>'
                "</a:p>"
            )
        return (
            f'<p:sp><p:nvSpPr><p:cNvPr id="{shape_id}" name="{xml_escape(name)}"/><p:cNvSpPr txBox="1"/><p:nvPr/></p:nvSpPr>'
            "<p:spPr><a:xfrm>"
            f'<a:off x="{box.x}" y="{box.y}"/><a:ext cx="{box.cx}" cy="{box.cy}"/>'
            f"</a:xfrm><a:prstGeom prst=\"{prst}\"><a:avLst/></a:prstGeom>{fill_xml}<a:ln>{line_xml}</a:ln></p:spPr>"
            f'<p:txBody><a:bodyPr wrap="square" rtlCol="0" anchor="t"/><a:lstStyle/>{"".join(paragraphs)}</p:txBody></p:sp>'
        )

    def _picture_shape_xml(self, shape_id: int, name: str, pic: Picture, rel_id: int) -> str:
        line_xml = "<a:noFill/>" if pic.line is None else f"<a:solidFill><a:srgbClr val=\"{pic.line}\"/></a:solidFill>"
        return (
            f'<p:pic><p:nvPicPr><p:cNvPr id="{shape_id}" name="{xml_escape(name)}"/><p:cNvPicPr/><p:nvPr/></p:nvPicPr>'
            f'<p:blipFill><a:blip r:embed="rId{rel_id}"/><a:stretch><a:fillRect/></a:stretch></p:blipFill>'
            "<p:spPr><a:xfrm>"
            f'<a:off x="{pic.x}" y="{pic.y}"/><a:ext cx="{pic.cx}" cy="{pic.cy}"/>'
            "</a:xfrm><a:prstGeom prst=\"rect\"><a:avLst/></a:prstGeom>"
            f"<a:ln>{line_xml}</a:ln></p:spPr></p:pic>"
        )


def build_slides(root: Path) -> list[SlideSpec]:
    screenshots = root / "docs" / "screenshots" / "gui"
    assets = root / "assets"
    hardware = assets / "arduino_module"

    return [
        SlideSpec(
            title="AURIX MediRun",
            subtitle="지하철 역사 응급 AED 운반 보조 로봇",
            bg_color="07111E",
            footer_text="AURIX",
            variant="cover",
            text_boxes=[
                TextBox(inches(0.95), inches(2.12), inches(4.8), inches(0.32), "TEAM AURIX", 1200, "67E8F9", None, None, False, "l", True),
                TextBox(inches(0.95), inches(2.82), inches(5.1), inches(0.95), "도희정\n최치언", 2350, "FFFFFF", None, None, False, "l", True),
                TextBox(inches(0.95), inches(4.1), inches(5.25), inches(1.0), "SOS 버튼 호출 이후\nAED를 목적지까지 자율 주행으로 운반하고\n반출·복귀·기록까지 이어지는 ROS 2 서비스", 1180, "D4E2EE"),
                TextBox(inches(0.95), inches(5.58), inches(2.6), inches(0.38), "2026 AURIX DECK", 1050, "08111C", "5EEAD4", None, True, "ctr", True),
            ],
            pictures=[
                Picture(assets / "aurix_logo.png", inches(7.0), inches(1.45), inches(3.95), inches(2.15), None),
                Picture(screenshots / "v2" / "02_live_monitor.png", inches(6.25), inches(3.38), inches(5.05), inches(2.82), "22D3EE"),
            ],
        ),
        SlideSpec(
            title="Project Overview",
            subtitle="docs 00, 01 기준 서비스 목적과 범위",
            bg_color="F7FAFC",
            header_accent="0F766E",
            text_boxes=[
                TextBox(inches(0.95), inches(2.0), inches(4.9), inches(2.5),
                        "목적\n지하철 역사에서 SOS 버튼이 눌린 구역까지\nAED를 자율 주행으로 운반해\n목격자가 환자 곁을 떠나는 시간을 줄입니다.",
                        1900, "0F172A"),
                TextBox(inches(6.0), inches(2.0), inches(5.1), inches(1.25),
                        "운용 범위\n호출 접수 · 출동 · 반출 감지 · 복귀 · 도킹 · 로컬 기록",
                        1550, "134E4A", "ECFDF5", "99F6E4", True),
                TextBox(inches(6.0), inches(3.45), inches(5.1), inches(1.25),
                        "제외 범위\n환자 상태 판단, 의료 행위, 119 자동 신고는 수행하지 않습니다.",
                        1550, "7C2D12", "FFF7ED", "FDBA74", True),
                TextBox(inches(0.95), inches(5.02), inches(10.15), inches(0.95),
                        "시연 기준은 버튼 3개와 목적지 3개의 1:1 매핑이며, 평상시에는 홈 위치에서 AED 적재·배터리·센서 상태를 점검하며 대기합니다.",
                        1450, "334155", "FFFFFF", "CBD5E1", True),
            ],
            pictures=[Picture(assets / "robot.jpg", inches(0.95), inches(5.2), inches(2.95), inches(1.05), None)],
        ),
        SlideSpec(
            title="Workspace Architecture",
            subtitle="README와 코드 기준 패키지 구성",
            bg_color="F3F7FF",
            header_accent="2563EB",
            text_boxes=[
                TextBox(inches(0.95), inches(2.0), inches(3.2), inches(2.72),
                        "medirun_sensor\n버튼·초음파·홀센서·LED 브리지\nserial / mock 모드 지원\nArduino 입력을 ROS 2 토픽으로 변환",
                        1850, "0F172A", "FFFFFF", "BFDBFE", True),
                TextBox(inches(4.12), inches(2.0), inches(3.25), inches(2.72),
                        "medirun_mission\nemergency_dispatcher\nwaypoint_manager\nmission_manager\npayload_monitor\nsafety_manager",
                        1720, "0F172A", "FFFFFF", "BFDBFE", True),
                TextBox(inches(7.35), inches(2.0), inches(3.8), inches(2.72),
                        "medirun_gui + navigation\n운영 대시보드\n웨이포인트 관리\n원격 제어\nSLAM 매핑\n정밀도 시험 러너",
                        1650, "0F172A", "FFFFFF", "BFDBFE", True),
                TextBox(inches(0.95), inches(5.2), inches(10.2), inches(0.72),
                        "현재 저장소는 ROS 2 Humble, Qt6, PostgreSQL, TurtleBot3 벤더 패키지를 한 워크스페이스 안에서 같이 관리하도록 정리되어 있습니다.",
                        1450, "1E3A8A", "DBEAFE", "93C5FD", True, "ctr"),
            ],
        ),
        SlideSpec(
            title="End-to-End Mission Flow",
            subtitle="docs 01, 04, 05와 현재 노드 구조를 합친 실행 흐름",
            bg_color="ECFEFF",
            header_accent="0891B2",
            text_boxes=[
                TextBox(inches(0.95), inches(2.0), inches(2.1), inches(1.55),
                        "1\nSOS 입력\n버튼 모듈 또는 GUI\n호출 이벤트 수신",
                        1700, "083344", "FFFFFF", "A5F3FC", True),
                TextBox(inches(3.3), inches(2.0), inches(2.1), inches(1.55),
                        "2\n출동 검증\npayload·배터리·ready 확인\nmission_id 생성",
                        1700, "083344", "FFFFFF", "A5F3FC", True),
                TextBox(inches(5.65), inches(2.0), inches(2.1), inches(1.55),
                        "3\n경로 확정\nbutton_id를 route에 매핑\nnavigation goal 발행",
                        1700, "083344", "FFFFFF", "A5F3FC", True),
                TextBox(inches(8.0), inches(2.0), inches(2.1), inches(1.55),
                        "4\n도착·반출\n홀센서 0.5초 확인\n자동 복귀 시작",
                        1700, "083344", "FFFFFF", "A5F3FC", True),
                TextBox(inches(0.95), inches(4.28), inches(10.15), inches(1.45),
                        "토픽 기준 흐름\n/medirun/sensor/sos_button → /medirun/mission/dispatch_request → /medirun/mission/dispatch_command → /medirun/navigation/goal → /medirun/vision/dock_request",
                        1550, "155E75", "CFFAFE", "67E8F9", True),
                TextBox(inches(0.95), inches(5.95), inches(10.15), inches(0.35),
                        "문서의 서비스 시나리오와 현재 Python 노드 구현이 가장 직접적으로 만나는 구간입니다.", 1200, "0F172A"),
            ],
        ),
        SlideSpec(
            title="Mission State Machine",
            subtitle="현재 mission_manager와 payload_monitor 기준 상태 전이",
            bg_color="F8FAFC",
            header_accent="7C3AED",
            footer_text="MISSION STATE",
            text_boxes=[
                TextBox(inches(0.95), inches(2.0), inches(1.55), inches(1.15),
                        "IDLE", 1900, "FFFFFF", "64748B", None, True, "ctr", True),
                TextBox(inches(2.7), inches(2.0), inches(1.7), inches(1.15),
                        "DISPATCHED", 1700, "FFFFFF", "2563EB", None, True, "ctr", True),
                TextBox(inches(4.65), inches(2.0), inches(1.8), inches(1.15),
                        "NAVIGATING", 1650, "FFFFFF", "0EA5E9", None, True, "ctr", True),
                TextBox(inches(6.7), inches(2.0), inches(1.55), inches(1.15),
                        "ARRIVED", 1850, "FFFFFF", "F59E0B", None, True, "ctr", True),
                TextBox(inches(8.5), inches(2.0), inches(2.05), inches(1.15),
                        "PAYLOAD_REMOVED", 1350, "FFFFFF", "F97316", None, True, "ctr", True),
                TextBox(inches(1.8), inches(4.0), inches(2.2), inches(1.15),
                        "RETURNING_HOME", 1450, "FFFFFF", "22C55E", None, True, "ctr", True),
                TextBox(inches(4.55), inches(4.0), inches(1.75), inches(1.15),
                        "DOCKING", 1750, "FFFFFF", "10B981", None, True, "ctr", True),
                TextBox(inches(6.7), inches(4.0), inches(1.55), inches(1.15),
                        "DOCKED", 1850, "FFFFFF", "14B8A6", None, True, "ctr", True),
                TextBox(inches(8.5), inches(4.0), inches(1.85), inches(1.15),
                        "COMPLETED", 1700, "FFFFFF", "059669", None, True, "ctr", True),
                TextBox(inches(0.95), inches(5.62), inches(10.15), inches(0.72),
                        "추가 분기\n도착 후 180초 timeout 또는 운영자 return_home 명령 시 복귀, cancel 또는 navigation FAILED 시 FAILED 상태로 종료",
                        1320, "4C1D95", "F5F3FF", "C4B5FD", True, "ctr"),
            ],
        ),
        SlideSpec(
            title="Operator Dashboard",
            subtitle="docs 03과 현재 Qt 페이지 구조를 기준으로 한 운영 화면",
            bg_color="0B1320",
            header_accent="22C55E",
            footer_text="GUI OVERVIEW",
            text_boxes=[
                TextBox(inches(0.95), inches(2.05), inches(2.15), inches(0.42), "LOGIN", 1180, "A7F3D0", None, None, False, "l", True),
                TextBox(inches(3.65), inches(2.05), inches(2.35), inches(0.42), "OVERVIEW", 1180, "93C5FD", None, None, False, "l", True),
                TextBox(inches(6.65), inches(2.05), inches(2.4), inches(0.42), "LIVE MONITOR", 1180, "FDE68A", None, None, False, "l", True),
                TextBox(inches(9.3), inches(2.05), inches(1.75), inches(0.42), "MISSIONS", 1180, "F9A8D4", None, None, False, "l", True),
                TextBox(inches(0.95), inches(5.77), inches(2.1), inches(0.42), "권한 기반 진입", 1380, "FFFFFF", None, None, False, "l", True),
                TextBox(inches(3.65), inches(5.77), inches(2.6), inches(0.42), "연결·배터리·상태 요약", 1380, "FFFFFF", None, None, False, "l", True),
                TextBox(inches(6.65), inches(5.77), inches(2.35), inches(0.42), "맵·카메라·이벤트", 1380, "FFFFFF", None, None, False, "l", True),
                TextBox(inches(9.05), inches(5.77), inches(2.05), inches(0.42), "큐·이력·명령", 1380, "FFFFFF", None, None, False, "l", True),
            ],
            pictures=[
                Picture(screenshots / "v2" / "00_login.png", inches(0.95), inches(2.42), inches(2.25), inches(2.82), "1F2937"),
                Picture(screenshots / "v2" / "01_overview.png", inches(3.65), inches(2.42), inches(2.55), inches(2.82), "1F2937"),
                Picture(screenshots / "v2" / "02_live_monitor.png", inches(6.65), inches(2.42), inches(2.35), inches(2.82), "1F2937"),
                Picture(screenshots / "v2" / "03_missions.png", inches(9.3), inches(2.42), inches(1.8), inches(2.82), "1F2937"),
            ],
        ),
        SlideSpec(
            title="Remote Control And Mapping",
            subtitle="admin 화면과 주행 운영 보조 기능",
            bg_color="F8FAFC",
            header_accent="8B5CF6",
            footer_text="CONTROL VIEW",
            pictures=[
                Picture(screenshots / "v2" / "04_remote_control.png", inches(0.95), inches(2.0), inches(5.35), inches(3.95), "C4B5FD"),
                Picture(screenshots / "v2" / "10_slam_mapping.png", inches(6.55), inches(2.0), inches(4.55), inches(2.15), "C4B5FD"),
                Picture(screenshots / "v3" / "01_slam_mapping_stable.png", inches(6.55), inches(4.45), inches(4.55), inches(1.5), "DDD6FE"),
            ],
            text_boxes=[
                TextBox(inches(0.95), inches(6.0), inches(5.35), inches(0.35),
                        "RemoteControlPage는 카메라·미니맵·수동 조작·초기 위치 지정 기능을 한 화면에서 제공합니다.", 1260, "475569"),
                TextBox(inches(6.55), inches(6.0), inches(4.55), inches(0.35),
                        "SlamMappingPage는 Cartographer launch와 저장 절차를 GUI에서 다루도록 구성돼 있습니다.", 1260, "475569"),
            ],
        ),
        SlideSpec(
            title="Sensors And Safety",
            subtitle="docs 04, 05와 medirun_sensor / safety_manager 기준",
            bg_color="FFF7ED",
            header_accent="F59E0B",
            text_boxes=[
                TextBox(inches(0.95), inches(4.45), inches(2.2), inches(1.15),
                        "LED\n상태 표시", 1500, "7C2D12", "FFFFFF", "FDBA74", True, "ctr", True),
                TextBox(inches(3.15), inches(4.45), inches(2.2), inches(1.15),
                        "ULTRASONIC\n근접 감지", 1500, "7C2D12", "FFFFFF", "FDBA74", True, "ctr", True),
                TextBox(inches(5.35), inches(4.45), inches(2.2), inches(1.15),
                        "BUTTON\n호출 / SOS", 1500, "7C2D12", "FFFFFF", "FDBA74", True, "ctr", True),
                TextBox(inches(7.55), inches(4.45), inches(2.2), inches(1.15),
                        "HALL\nAED 적재 확인", 1500, "7C2D12", "FFFFFF", "FDBA74", True, "ctr", True),
                TextBox(inches(9.92), inches(2.15), inches(1.18), inches(3.45),
                        "Safety\nManager\n\nfront 35cm\nrear 25cm\n기준 차단", 1450, "FFFFFF", "F59E0B", None, True, "ctr", True),
            ],
            pictures=[
                Picture(hardware / "led_module.jpg", inches(0.95), inches(2.0), inches(2.0), inches(2.15), "FDBA74"),
                Picture(hardware / "Ultrasonic_module.jpg", inches(3.15), inches(2.0), inches(2.0), inches(2.15), "FDBA74"),
                Picture(hardware / "button_module.jpg", inches(5.35), inches(2.0), inches(2.0), inches(2.15), "FDBA74"),
                Picture(hardware / "analog_hall_module.jpg", inches(7.55), inches(2.0), inches(2.0), inches(2.15), "FDBA74"),
            ],
        ),
        SlideSpec(
            title="Navigation Test Results",
            subtitle="docs/tests 시험 요약 문서 기준 정밀도 결과",
            bg_color="F0FDF4",
            header_accent="16A34A",
            text_boxes=[
                TextBox(inches(0.95), inches(2.0), inches(2.6), inches(1.7),
                        "0.13°", 3200, "166534", None, None, False, "l", True),
                TextBox(inches(0.95), inches(3.15), inches(3.35), inches(0.7),
                        "T-00 회전 정확도\n90도 10회 절대평균오차", 1500, "14532D"),
                TextBox(inches(4.2), inches(2.0), inches(2.6), inches(1.7),
                        "0.2cm", 3200, "166534", None, None, False, "l", True),
                TextBox(inches(4.2), inches(3.15), inches(3.35), inches(0.7),
                        "T-01 직진 정확도\n0.45m 10회 절대평균오차", 1500, "14532D"),
                TextBox(inches(7.6), inches(2.0), inches(2.9), inches(1.7),
                        "10 / 10\nPASS", 2400, "FFFFFF", "16A34A", None, True, "ctr", True),
                TextBox(inches(7.55), inches(4.0), inches(3.05), inches(1.2),
                        "종합 판단\n정밀도 기준 충족\n실내 호출 시나리오 근거 확보", 1450, "14532D", "DCFCE7", "86EFAC", True, "ctr"),
                TextBox(inches(0.95), inches(4.45), inches(6.0), inches(1.42),
                        "시험 문서 핵심\n회전은 2026-07-24 기준 10회 모두 성공,\n직진은 0.451~0.453m 범위에 수렴했고\n횡방향 절대평균오차는 0.001m 수준입니다.",
                        1360, "14532D", "FFFFFF", "BBF7D0", True),
            ],
        ),
        SlideSpec(
            title="Documentation And Rules",
            subtitle="docs 02, 03, 04, 06이 코드 구조와 연결되는 방식",
            bg_color="FEFCF3",
            header_accent="D97706",
            text_boxes=[
                TextBox(inches(0.95), inches(2.0), inches(2.3), inches(2.25),
                        "요구사항·설계\n호출 경로, 책임 분리,\n인터페이스와 선행 조건을 정의",
                        1600, "78350F", "FFFFFF", "FDE68A", True, "ctr", True),
                TextBox(inches(3.55), inches(2.0), inches(2.3), inches(2.25),
                        "GUI HMI\n역할 기반 접근,\n운영 화면 구성,\n표시 규칙을 정리",
                        1600, "78350F", "FFFFFF", "FDE68A", True, "ctr", True),
                TextBox(inches(6.15), inches(2.0), inches(2.3), inches(2.25),
                        "ROS2 설계\n노드 책임,\n토픽 인터페이스,\n상태 머신 문서화",
                        1600, "78350F", "FFFFFF", "FDE68A", True, "ctr", True),
                TextBox(inches(8.75), inches(2.0), inches(2.3), inches(2.25),
                        "개발 규칙\n패키지 네이밍,\n브랜치·커밋 규칙,\n파일 관리 기준",
                        1600, "78350F", "FFFFFF", "FDE68A", True, "ctr", True),
                TextBox(inches(0.95), inches(4.75), inches(10.15), inches(1.2),
                        "발표자료도 같은 원칙으로 정리했습니다.\n문서의 이상 설계만 나열하지 않고, 현재 저장소에 실제 있는 패키지·화면·시험·트러블슈팅까지 맞춰서 재구성했습니다.",
                        1450, "92400E", "FFFBEB", "FCD34D", True, "ctr"),
            ],
        ),
        SlideSpec(
            title="Recent Real-World Fixes",
            subtitle="docs/troubleshooting와 log.md에서 확인되는 실제 이슈 대응",
            bg_color="111827",
            header_accent="F43F5E",
            footer_text="FIELD ISSUES",
            text_boxes=[
                TextBox(inches(0.95), inches(2.0), inches(2.45), inches(1.75),
                        "OpenCR\nbattery_state\n미수신", 1800, "FFFFFF", "1F2937", "334155", True, "ctr", True),
                TextBox(inches(3.65), inches(2.0), inches(2.45), inches(1.75),
                        "RMW\n불일치\n조사", 1800, "FFFFFF", "1F2937", "334155", True, "ctr", True),
                TextBox(inches(6.35), inches(2.0), inches(2.45), inches(1.75),
                        "Mission Ops\n세그폴트\n수정", 1800, "FFFFFF", "1F2937", "334155", True, "ctr", True),
                TextBox(inches(9.05), inches(2.0), inches(2.05), inches(1.75),
                        "헤더 DB\n상태 반영\n수정", 1800, "FFFFFF", "1F2937", "334155", True, "ctr", True),
                TextBox(inches(0.95), inches(4.25), inches(10.15), inches(1.25),
                        "이 저장소는 단순 화면 목업이 아니라 실제 운영 문제를 추적한 기록을 갖고 있습니다.\n하드웨어 연결, DDS 설정, GUI 크래시, 헤더 상태 동기화까지 문서와 코드가 함께 남아 있습니다.",
                        1620, "E5E7EB", "0F172A", "374151", True),
                TextBox(inches(0.95), inches(5.82), inches(2.35), inches(0.36),
                        "2026-08-06", 1100, "111827", "FDA4AF", None, True, "ctr", True),
                TextBox(inches(3.65), inches(5.82), inches(2.35), inches(0.36),
                        "2026-08-06", 1100, "111827", "F9A8D4", None, True, "ctr", True),
                TextBox(inches(6.35), inches(5.82), inches(2.35), inches(0.36),
                        "2026-08-09", 1100, "111827", "C4B5FD", None, True, "ctr", True),
                TextBox(inches(9.05), inches(5.82), inches(2.05), inches(0.36),
                        "log.md", 1100, "111827", "93C5FD", None, True, "ctr", True),
            ],
        ),
        SlideSpec(
            title="Closing Message",
            subtitle="현재 코드와 문서 기준으로 정리한 발표 결론",
            bg_color="FDF2F8",
            header_accent="EC4899",
            text_boxes=[
                TextBox(inches(0.95), inches(2.0), inches(2.25), inches(1.5),
                        "SOS\nTO\nMISSION", 2100, "831843", "FFFFFF", "F9A8D4", True, "ctr", True),
                TextBox(inches(3.6), inches(2.0), inches(2.25), inches(1.5),
                        "ROS 2\nNODE\nCHAIN", 2100, "831843", "FFFFFF", "F9A8D4", True, "ctr", True),
                TextBox(inches(6.25), inches(2.0), inches(2.25), inches(1.5),
                        "GUI\nCONTROL\nLOOP", 2100, "831843", "FFFFFF", "F9A8D4", True, "ctr", True),
                TextBox(inches(8.9), inches(2.0), inches(2.2), inches(1.5),
                        "TESTED\nAND\nLOGGED", 2100, "831843", "FFFFFF", "F9A8D4", True, "ctr", True),
                TextBox(inches(0.95), inches(4.28), inches(10.15), inches(1.1),
                        "AURIX MediRun은 문서용 아이디어가 아니라,\n버튼 입력, 미션 상태 전이, 운영 대시보드, 센서 안전, 정밀도 시험, 문제 해결 기록까지 한 저장소 안에서 이어지는 응급 운반 로봇 프로젝트입니다.",
                        1700, "4A044E", "FCE7F3", "F5A8D6", True),
                TextBox(inches(0.95), inches(5.78), inches(6.3), inches(0.42),
                        "다음 발표 포인트: 실제 브링업 데모 · 버튼 호출 시연 · 미션 관제와 라이브 모니터 연동", 1220, "9D174D"),
            ],
            pictures=[Picture(assets / "aurix_logo.png", inches(9.5), inches(5.55), inches(1.5), inches(0.68), None)],
        ),
    ]


def main() -> None:
    root = Path(__file__).resolve().parents[1]
    output = root / "AURIX_MediRun_발표자료_자동생성.pptx"
    builder = PptxBuilder(output)
    for slide in build_slides(root):
        builder.add_slide(slide)
    builder.build()
    print(output)


if __name__ == "__main__":
    main()
