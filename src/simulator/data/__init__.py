"""Parametre dosyalarını okuma. Değerlerin kaynakları dosyaların içinde yorum olarak durur."""
from dataclasses import dataclass, fields
from pathlib import Path

import yaml

DATA_DIRECTORY = Path(__file__).resolve().parent


@dataclass(frozen=True)
class MissileProfile:
    name: str
    mass_launch_kg: float
    propellant_kg: float
    thrust_n: float
    burn_s: float
    diameter_m: float
    drag_coeff_base: float
    plume_drag_factor: float
    max_g: float
    min_guide_mach: float
    self_destruct_s: float
    arming_s: float
    launcher_field_of_regard_deg: float
    pitbull_nm: float
    seeker_field_deg: float
    seeker_range_nm: float
    navigation_constant: float
    fuze_radius_m: float
    sustain_thrust_n: float = 0.0     # ikinci kademe (yoksa 0)
    sustain_s: float = 0.0


def load_missile(name="aim120c5"):
    """data/missiles/<name>.yaml → MissileProfile. Bilinmeyen alan hata verir."""
    with (DATA_DIRECTORY / "missiles" / f"{name}.yaml").open(encoding="utf-8") as file:
        values = yaml.safe_load(file)
    unknown = set(values) - {field.name for field in fields(MissileProfile)}
    if unknown:
        raise ValueError(f"{name}.yaml: bilinmeyen alanlar {sorted(unknown)}")
    return MissileProfile(**values)
