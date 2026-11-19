"""Check the promised complete source coverage, not just HTML generation."""
from html import unescape
from pathlib import Path
import re
import sys
import unittest
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "docs"))
import otopilot_uml as uml


class OtopilotCoverage(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.types, cls.relations = uml.inventory(ROOT)
        cls.by_name = {item.name: item for item in cls.types}
        cls.body, cls.stats = uml.section(ROOT)

    def test_every_definition_including_cpp_implementation_is_present(self):
        # An independent, line-based scan checks that the declaration reader
        # did not silently skip any of the project's actual type definitions.
        names = set()
        for path in (ROOT / "src/otopilot").rglob("*"):
            if path.suffix not in {".hpp", ".cpp"} or {"third_party", "build", "fsm"}.intersection(path.parts):   # fsm: inventory gibi atlanır
                continue
            names.update(re.findall(r"^[ \t]*(?:enum class|class|struct)\s+([\w:]+)\s*(?::[^\n{]+)?\s*(?:\{|$)",
                                    path.read_text(encoding="utf-8"), re.M))
        self.assertEqual(names, set(self.by_name))
        self.assertIn("Connection::Impl", names)
        self.assertEqual(len(names), 63)

    def test_every_member_against_independent_line_scan(self):
        count = 0
        for item in self.types:
            if item.values or item.name == "CountingManeuver":  # yerel test sınıfı, girinti farklı
                continue
            # All current direct fields use four spaces and one declaration
            # per line. Inline method locals are more deeply indented.
            fields = re.findall(r"^ {4}(?! )(?![^\n]*\()(\w[^\n;{}()=]*?[ \t*&])([A-Za-z_]\w*)[ \t]*(?:=[^;\n]*|\{[^;\n]*\})?;[ \t]*$",
                                item.body, re.M)
            expected = [(name, uml.compact(kind)) for kind, name in fields
                        if kind.strip() not in {"struct", "class"}]
            actual = [(member.name, member.type) for member in item.members]
            self.assertEqual(expected, actual, item.name)
            count += len(expected)
        self.assertEqual(count, 237)
        self.assertEqual(count + 2, self.stats["members"])  # +2: CountingManeuver

    def test_all_fields_methods_and_enum_values_in_diagram_and_cards(self):
        svg = uml.complete_svg(self.types, self.relations)
        ET.fromstring(svg)
        for item in self.types:
            self.assertIn(f'data-type="{item.name}"', svg)
            self.assertIn(f'id="{item.key}"', self.body)
            for member in item.members:
                self.assertIn(member.name + ": " + member.type, unescape(svg))
            for value in item.values:
                self.assertIn(value, unescape(svg))
            for method in item.methods:
                # Long signatures wrap visually but do not omit words.
                self.assertTrue(all(line in unescape(svg) for line in uml.row_lines(method)), (item.name, method))
        self.assertEqual(len(re.findall(r'class="uml-edge ', svg)), len(self.relations))

    def test_ownership_inheritance_and_dependencies_have_correct_meaning(self):
        relations = {(r.source, r.target, r.kind, r.label) for r in self.relations}
        for target, member in [("World", "world_"), ("Otopilot", "otopilot_fighter_"),
                               ("Otopilot", "otopilot_hostile_")]:
            self.assertIn(("Sim", target, "composition", member+" [1]"), relations)
        for name in ["Hold", "Out", "Crank", "Notch", "Beam", "Commit"]:
            self.assertIn((name, "Maneuver", "inheritance", "kalıtım"), relations)
        self.assertIn(("Weapons", "Missile", "composition", "missiles [0..*]"), relations)
        self.assertTrue(any(r.source == "Missile" and r.target == "Aircraft" and r.kind == "association" for r in self.relations))
        self.assertFalse(any(r.source == "Phase" for r in self.relations))

    def test_initializers_and_access_are_preserved(self):
        fighter = {m.name: m for m in self.by_name["Otopilot"].members}
        self.assertEqual(fighter["state_"].initial, "OtopilotState::Commit")
        self.assertEqual(fighter["state_"].visibility, "−")
        crank = {m.name: m for m in self.by_name["Crank"].members}
        self.assertEqual(crank["side_"].initial, "ManeuverSide::Left")
        self.assertIn("+ virtual ~Maneuver() = default", self.by_name["Maneuver"].methods)
        self.assertIn("+ Connection& operator=(const Connection&) = delete", self.by_name["Connection"].methods)


if __name__ == "__main__":
    unittest.main()
